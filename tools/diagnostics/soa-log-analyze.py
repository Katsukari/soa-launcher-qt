#!/usr/bin/env python3
"""Story of Alicia launch log analyser (macOS + Linux).

Reads a Wine log (and optionally the Alicia hook log) and answers, without
anyone having to grep 200 MB by hand:

  * which modules loaded, in what order, builtin or native, at what base
  * which loads FAILED, split into "expected on this platform" and "not expected"
  * which thread stopped first, and what it was doing
  * every exception and Wine assertion, with the NTSTATUS decoded
  * which graphics backend and which audio driver actually won
  * a plain-English verdict

Streams the file, so a multi-gigabyte relay log is fine.

  soa-log-analyze.py wine.log
  soa-log-analyze.py wine.log --alicia alicia.log --json report.json
  soa-log-analyze.py <run-directory>          everything in one diagnostic run
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from collections import Counter, OrderedDict, defaultdict, deque

# ---------------------------------------------------------------- constants

# Wine probes every audio backend in turn and uses whichever exists. The others
# are simply not built on this platform. Reporting them as failures is the
# single most common false lead in this project's history, so they are named
# explicitly rather than guessed at.
EXPECTED_MISSING = {
    "darwin": {
        "winepulse.drv", "winealsa.drv", "wineoss.drv", "wineandroid.drv",
        "wineps.drv", "winex11.drv",
    },
    "linux": {
        "winecoreaudio.drv", "wineandroid.drv", "winemac.drv", "wineps.drv",
    },
}
# Never platform-specific: these are probed and optional everywhere.
EXPECTED_MISSING_ANY = {
    "wineps.drv", "winebus.sys", "winehid.sys", "mscoree", "mscoree.dll",
}

NTSTATUS = {
    "c0000005": "STATUS_ACCESS_VIOLATION",
    "c0000006": "STATUS_IN_PAGE_ERROR",
    "c000001d": "STATUS_ILLEGAL_INSTRUCTION",
    "c0000025": "STATUS_NONCONTINUABLE_EXCEPTION",
    "c0000029": "STATUS_INVALID_UNWIND_TARGET",
    "c000008c": "STATUS_ARRAY_BOUNDS_EXCEEDED",
    "c000008e": "STATUS_FLOAT_DIVIDE_BY_ZERO",
    "c0000094": "STATUS_INTEGER_DIVIDE_BY_ZERO",
    "c0000096": "STATUS_PRIVILEGED_INSTRUCTION",
    "c00000fd": "STATUS_STACK_OVERFLOW",
    "c0000135": "STATUS_DLL_NOT_FOUND",
    "c0000139": "STATUS_ENTRYPOINT_NOT_FOUND",
    "c0000142": "STATUS_DLL_INIT_FAILED",
    "c0000374": "STATUS_HEAP_CORRUPTION",
    "c0000409": "STATUS_STACK_BUFFER_OVERRUN",
    "80000003": "EXCEPTION_BREAKPOINT",
    "80000004": "EXCEPTION_SINGLE_STEP",
    "80000026": "STATUS_LONGJUMP",
    "80000101": "EXCEPTION_WINE_ASSERTION",
    "40010006": "DBG_PRINTEXCEPTION_C (OutputDebugString)",
    "406d1388": "MS_VC_EXCEPTION (SetThreadName)",
    "e06d7363": "C++ EH exception (throw)",
}

WIN32_ERROR = {
    "2": "ERROR_FILE_NOT_FOUND",
    "5": "ERROR_ACCESS_DENIED",
    "8": "ERROR_NOT_ENOUGH_MEMORY",
    "87": "ERROR_INVALID_PARAMETER",
    "126": "ERROR_MOD_NOT_FOUND",
    "127": "ERROR_PROC_NOT_FOUND",
    "193": "ERROR_BAD_EXE_FORMAT",
    "1114": "ERROR_DLL_INIT_FAILED",
}

# Wine emits four different line shapes depending on which of +timestamp, +pid
# and +tid are enabled. Handling only the richest one means a default-configured
# log parses as zero events and the tool reports "nothing wrong" — the worst
# possible failure for a diagnostic. All four are accepted.
_TAIL = r"(?P<cls>trace|warn|err|fixme):(?P<chan>[a-z0-9_]+):(?P<fn>\S+)\s*(?P<msg>.*)$"
LINE_FORMS = (
    re.compile(r"^(?P<ts>\d+\.\d+):(?P<pid>[0-9a-f]+):(?P<tid>[0-9a-f]+):" + _TAIL),
    re.compile(r"^(?P<pid>[0-9a-f]+):(?P<tid>[0-9a-f]+):" + _TAIL),
    re.compile(r"^(?P<tid>[0-9a-f]+):" + _TAIL),
    re.compile(r"^" + _TAIL),
)


def parse_line(line):
    for form in LINE_FORMS:
        match = form.match(line)
        if match:
            return match
    return None

RE_LOAD_FOUND = re.compile(r'Found L"(?P<path>[^"]+)" for L"(?P<name>[^"]+)" at (?P<base>[0-9A-Fa-f]+)')
RE_LOAD_FAIL = re.compile(r'Failed to load module L"(?P<name>[^"]+)"; status=(?P<status>[0-9a-f]+)')
RE_BUILTIN_MISS = re.compile(r'cannot find builtin library for L"(?P<name>[^"]+)"')
RE_EXC = re.compile(r"code=(?P<code>[0-9a-f]+)")
# Wine writes debug output unbuffered. When two threads write at once — and
# especially when the launcher captures stderr through a pipe, where only
# PIPE_BUF bytes are atomic — one message lands in the middle of another. Those
# lines are real log corruption, not a parse failure, so they get counted and
# truncated at the splice rather than reported as fact.
RE_SPLICE = re.compile(r"(trace|warn|err|fixme):[a-z0-9_]+:\S+\s")

RE_HOOK = re.compile(r"^\[(?P<ts>[0-9:.]+)\]\s+\[(?P<tag>[^\]]+)\]\s+(?P<msg>.*)$")
RE_HOOK_LOADER = re.compile(r"LoadLibrary[AW]\((?P<name>[^)]*)\)\s*->\s*(?P<result>OK|FAILED)")


def base_name(path: str) -> str:
    return path.replace("\\", "/").rsplit("/", 1)[-1].lower()


class Report:
    def __init__(self, platform_hint: str) -> None:
        self.platform = platform_hint
        self.modules: "OrderedDict[str, dict]" = OrderedDict()
        self.failed: "OrderedDict[str, dict]" = OrderedDict()
        self.builtin_misses: list[str] = []
        self.exceptions: Counter = Counter()
        self.exception_first: dict[str, dict] = {}
        self.assertions: list[dict] = []
        self.diagnostics: Counter = Counter()
        self.diag_example: dict[str, str] = {}
        self.last_per_thread: dict[str, dict] = {}
        self.tail_per_thread: dict[str, deque] = defaultdict(lambda: deque(maxlen=6))
        self.thread_lines: Counter = Counter()
        self.channels: Counter = Counter()
        self.first_ts = None
        self.last_ts = None
        self.wine_build = None
        self.renderer_hints: Counter = Counter()
        self.audio_driver = None
        self.total = 0
        self.unparsed = 0
        self.spliced = 0


def scan_wine_log(path: str, report: Report) -> None:
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            report.total += 1
            line = raw.rstrip("\n")

            # Assertions and libc aborts are printed without the Wine prefix.
            if line.startswith("Assertion failed:") or "libc++abi:" in line:
                report.assertions.append({"line": report.total, "text": line.strip()})
                continue

            match = parse_line(line)
            if not match:
                report.unparsed += 1
                low = line.lower()
                if "mvk-info" in low or "moltenvk" in low:
                    report.renderer_hints["MoltenVK / Vulkan"] += 1
                continue

            groups = match.groupdict()
            ts = groups.get("ts")
            tid = groups.get("tid") or "----"
            pid = groups.get("pid") or "----"
            chan = match.group("chan")
            cls = match.group("cls")
            fn = match.group("fn")
            msg = match.group("msg")

            if ts is not None:
                if report.first_ts is None:
                    report.first_ts = ts
                report.last_ts = ts
            else:
                ts = f"#{report.total}"   # no timestamps enabled; use line order
            report.channels[f"{cls}:{chan}"] += 1
            report.thread_lines[f"{pid}:{tid}"] += 1

            splice = RE_SPLICE.search(msg)
            if splice:
                report.spliced += 1
                msg = msg[:splice.start()].rstrip() + "  <<SPLICED>>"

            entry = {"ts": ts, "line": report.total, "text": f"{cls}:{chan}:{fn} {msg}"[:220]}
            report.last_per_thread[f"{pid}:{tid}"] = entry
            report.tail_per_thread[f"{pid}:{tid}"].append(entry)

            # ---- module inventory
            found = RE_LOAD_FOUND.search(msg)
            if found:
                name = base_name(found.group("name"))
                if name not in report.modules:
                    report.modules[name] = {
                        "name": name,
                        "path": found.group("path"),
                        "base": found.group("base"),
                        "first_ts": ts,
                        "line": report.total,
                        "kind": "native" if ":\\users\\" in found.group("path").lower()
                                or "\\game\\" in found.group("path").lower() else "system",
                    }
                continue

            failed = RE_LOAD_FAIL.search(msg)
            if failed:
                name = base_name(failed.group("name"))
                status = failed.group("status")
                report.failed.setdefault(name, {
                    "name": name,
                    "status": status,
                    "status_name": NTSTATUS.get(status, "unknown"),
                    "ts": ts,
                    "line": report.total,
                })
                continue

            builtin = RE_BUILTIN_MISS.search(msg)
            if builtin:
                report.builtin_misses.append(base_name(builtin.group("name")))
                continue

            # ---- exceptions
            if chan == "seh" and "dispatch_exception" in fn:
                code = RE_EXC.search(msg)
                if code:
                    value = code.group("code").lower()
                    report.exceptions[value] += 1
                    report.exception_first.setdefault(value, {
                        "ts": ts, "line": report.total, "tid": tid,
                        "text": msg[:220],
                    })
                continue

            # ---- anything the developers flagged
            if cls in ("err", "fixme", "warn"):
                key = f"{cls}:{chan}:{fn}"
                report.diagnostics[key] += 1
                report.diag_example.setdefault(key, msg[:200])

            low = msg.lower()
            if "wine-" in low and report.wine_build is None and chan == "winediag":
                report.wine_build = msg[:120]
            if "adapter_vk" in fn or "vulkan" in low:
                report.renderer_hints["Vulkan (wined3d adapter_vk)"] += 1
            if "wgl" in fn or "opengl" in low or "gl_vendor" in low:
                report.renderer_hints["OpenGL (wined3d adapter_gl)"] += 1
            if chan == "coreaudio":
                report.audio_driver = "winecoreaudio"
            elif chan in ("pulse", "alsa", "oss"):
                report.audio_driver = f"wine{chan}"


# Milestones the launcher writes to timeline.jsonl. How far the run got is a
# different question from what the Wine log says went wrong, and answering both
# in one place is why this absorbed the old analyze-macos-launch.py.
TIMELINE_STAGES = [
    ("rendering_started", "d3d9_draw_observed"),
    ("after_present_before_draw", "d3d9_present_observed"),
    ("after_device_before_present", "d3d9_create_device_observed"),
    ("before_d3d9_device", "alicia_process_observed"),
]


def scan_timeline(path: str) -> dict:
    events = []
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            try:
                item = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(item, dict):
                events.append(item)

    names = {str(e.get("event", "")) for e in events}
    stage = "unknown"
    for label, marker in TIMELINE_STAGES:
        if marker in names:
            stage = label
            break
    else:
        if "wine_wrapper_finished" in names:
            stage = "process_creation"

    finished = next((e for e in reversed(events)
                     if e.get("event") == "session_finished"), None)
    return {
        "events": len(events),
        "stage": stage,
        "milestones": {marker: (marker in names)
                       for _, marker in TIMELINE_STAGES},
        "wrapper_finished": "wine_wrapper_finished" in names,
        "session_details": str(finished.get("details", "")) if finished else "",
    }


def scan_alicia_log(path: str) -> dict:
    result = {"loaded": [], "failed": [], "tags": Counter(), "last": None, "lines": 0}
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n").rstrip("\r")
            if not line:
                continue
            result["lines"] += 1
            result["last"] = line[:220]
            match = RE_HOOK.match(line)
            if not match:
                continue
            result["tags"][match.group("tag")] += 1
            loader = RE_HOOK_LOADER.search(match.group("msg"))
            if loader:
                name = base_name(loader.group("name"))
                bucket = "loaded" if loader.group("result") == "OK" else "failed"
                if name not in result[bucket]:
                    result[bucket].append(name)
    return result


def expected_missing(name: str, platform_hint: str) -> bool:
    if name in EXPECTED_MISSING_ANY:
        return True
    return name in EXPECTED_MISSING.get(platform_hint, set())


def emit(report: Report, alicia: dict | None, timeline: dict | None, stream) -> dict:
    def out(text: str = "") -> None:
        print(text, file=stream)

    summary: dict = {}
    out("=" * 78)
    out("STORY OF ALICIA — LAUNCH LOG ANALYSIS")
    out("=" * 78)
    out(f"platform assumed : {report.platform}")
    out(f"lines parsed     : {report.total} ({report.unparsed} not in Wine format)")
    if report.spliced:
        out(f"CORRUPTED LINES  : {report.spliced} — two threads wrote at once and Wine")
        out(f"                   spliced their output. Messages marked <<SPLICED>> are")
        out(f"                   truncated at the join. Capture Wine's stderr straight to")
        out(f"                   a file instead of through a pipe to reduce this.")
    if report.first_ts:
        span = float(report.last_ts) - float(report.first_ts)
        out(f"log span         : {report.first_ts} -> {report.last_ts}  ({span:.1f}s)")
    else:
        out("log span         : no timestamps in this log "
            "(add +timestamp to WINEDEBUG; ordering below is by line number)")
    if report.renderer_hints:
        winner = report.renderer_hints.most_common(1)[0]
        out(f"graphics backend : {winner[0]}  ({winner[1]} references)")
        summary["renderer"] = winner[0]
    if report.audio_driver:
        out(f"audio driver     : {report.audio_driver}")
        summary["audio"] = report.audio_driver

    # ---- the question that keeps getting asked
    out()
    out("-" * 78)
    out("MODULES LOADED  (%d)" % len(report.modules))
    out("-" * 78)
    for entry in report.modules.values():
        out(f"  {entry['first_ts']:>12}  {entry['name']:<28} @{entry['base']:>8}  {entry['kind']}")

    unexpected = [e for n, e in report.failed.items() if not expected_missing(n, report.platform)]
    benign = [e for n, e in report.failed.items() if expected_missing(n, report.platform)]

    out()
    out("-" * 78)
    out("MODULES THAT FAILED TO LOAD")
    out("-" * 78)
    if unexpected:
        out("  !! NOT EXPECTED — investigate these:")
        for entry in unexpected:
            out(f"     {entry['name']:<28} status={entry['status']} ({entry['status_name']})")
    else:
        out("  none unexpected.")
    if benign:
        out()
        out("  expected on this platform (Wine probes every backend; ignore):")
        out("     " + ", ".join(sorted(e["name"] for e in benign)))
    summary["failed_unexpected"] = [e["name"] for e in unexpected]

    if report.builtin_misses:
        out()
        out("  'no builtin library' notices — normal for the game's own DLLs:")
        out("     " + ", ".join(sorted(set(report.builtin_misses))))

    # ---- what died
    out()
    out("-" * 78)
    out("EXCEPTIONS AND ASSERTIONS")
    out("-" * 78)
    if report.assertions:
        for item in report.assertions:
            out(f"  !! ASSERTION (line {item['line']}): {item['text']}")
        summary["assertions"] = [i["text"] for i in report.assertions]
    if report.exceptions:
        for code, count in report.exceptions.most_common():
            name = NTSTATUS.get(code, "unknown")
            first = report.exception_first[code]
            flag = "  " if name in ("STATUS_LONGJUMP", "DBG_PRINTEXCEPTION_C (OutputDebugString)",
                                    "MS_VC_EXCEPTION (SetThreadName)", "C++ EH exception (throw)") else "!!"
            out(f"  {flag} {code} {name:<38} x{count:<7} first line {first['line']} tid {first['tid']}")
        summary["exceptions"] = {c: n for c, n in report.exceptions.items()}
    if not report.assertions and not report.exceptions:
        out("  none recorded.")

    # ---- who stopped first
    out()
    out("-" * 78)
    out("LAST ACTIVITY PER THREAD  (latest first — the quiet ones died earliest)")
    out("-" * 78)
    def order_key(item):
        value = item[1]["ts"]
        try:
            return float(value)
        except ValueError:
            return float(item[1]["line"])

    ordered = sorted(report.last_per_thread.items(), key=order_key, reverse=True)
    for key, entry in ordered[:12]:
        out(f"  {entry['ts']:>12}  {key}  {entry['text'][:120]}")
    if ordered:
        quietest = ordered[-1]
        out()
        out(f"  earliest to go silent: {quietest[0]} at {quietest[1]['ts']}")
        out(f"    {quietest[1]['text'][:150]}")
        summary["first_silent_thread"] = quietest[0]

    # ---- developer complaints
    if report.diagnostics:
        out()
        out("-" * 78)
        out("err / fixme / warn  (top 20 by count)")
        out("-" * 78)
        for key, count in report.diagnostics.most_common(20):
            marker = "!!" if key.startswith("err:") else "  "
            out(f"  {marker} x{count:<6} {key}")
            out(f"        {report.diag_example[key][:140]}")

    if timeline:
        out()
        out("-" * 78)
        out("LAUNCH TIMELINE")
        out("-" * 78)
        out(f"  events recorded : {timeline['events']}")
        out(f"  reached stage   : {timeline['stage']}")
        for marker, seen in timeline["milestones"].items():
            out(f"    {'reached' if seen else '   -   '}  {marker}")
        if timeline["session_details"]:
            out(f"  session details : {timeline['session_details'][:150]}")
        summary["stage"] = timeline["stage"]

    if alicia:
        out()
        out("-" * 78)
        out("ALICIA HOOK LOG")
        out("-" * 78)
        out(f"  lines: {alicia['lines']}   tags: " +
            ", ".join(f"{t}={c}" for t, c in alicia["tags"].most_common()))
        if alicia["failed"]:
            out("  LoadLibrary FAILED: " + ", ".join(alicia["failed"]))
        out(f"  last line: {alicia['last']}")
        summary["alicia_last"] = alicia["last"]

    # ---- verdict
    out()
    out("=" * 78)
    out("VERDICT")
    out("=" * 78)
    verdict = []
    if report.assertions:
        verdict.append("Wine hit an internal ASSERTION — this is a bug in the Wine build, "
                       "not in the game or the launcher. See the assertion text above.")
    fatal = [c for c in report.exceptions
             if NTSTATUS.get(c, "") in ("STATUS_ACCESS_VIOLATION", "STATUS_STACK_OVERFLOW",
                                        "STATUS_HEAP_CORRUPTION", "STATUS_ILLEGAL_INSTRUCTION")]
    if fatal:
        verdict.append("Fatal-class exceptions present: " +
                       ", ".join(f"{c} ({NTSTATUS[c]})" for c in fatal) +
                       ". Note many can be caught and handled — check whether one is the LAST event.")
    if unexpected:
        verdict.append("Unexpected module load failures: " +
                       ", ".join(e["name"] for e in unexpected))
    if not verdict:
        verdict.append("No assertion, no unexpected module failure, no fatal exception recorded. "
                       "If the run still failed, the cause is above the noise floor of this log — "
                       "raise the log mode and re-run.")
    for item in verdict:
        out("  * " + item)
    if report.spliced:
        verdict.append(f"{report.spliced} log lines were spliced by concurrent threads; "
                       "some messages above are truncated at the join.")
    summary["verdict"] = verdict
    summary["spliced_lines"] = report.spliced
    out()
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description="Analyse a Story of Alicia Wine log.")
    parser.add_argument("wine_log",
                        help="wine.log, or a diagnostic run directory containing it")
    parser.add_argument("--alicia", help="path to alicia.log")
    parser.add_argument("--timeline", help="path to timeline.jsonl")
    parser.add_argument("--platform", choices=["darwin", "linux", "auto"], default="auto")
    parser.add_argument("--json", help="also write the machine-readable summary here")
    args = parser.parse_args()

    platform_hint = args.platform
    if platform_hint == "auto":
        platform_hint = "darwin" if sys.platform == "darwin" else "linux"

    # A whole run directory is the common case; find the pieces inside it.
    wine_log = args.wine_log
    alicia_log = args.alicia
    timeline_path = args.timeline
    if os.path.isdir(wine_log):
        run = wine_log
        wine_log = os.path.join(run, "wine.log")
        alicia_log = alicia_log or os.path.join(run, "alicia.log")
        timeline_path = timeline_path or os.path.join(run, "timeline.jsonl")

    if not os.path.exists(wine_log):
        print(f"error: no such file: {wine_log}", file=sys.stderr)
        return 2

    report = Report(platform_hint)
    scan_wine_log(wine_log, report)

    # A macOS log gives itself away; trust the content over the host.
    if any("winemac" in n or "coreaudio" in n for n in list(report.modules) + list(report.failed)):
        report.platform = "darwin"
    elif any("winex11" in n or "winealsa" in n for n in report.modules):
        report.platform = "linux"

    alicia = (scan_alicia_log(alicia_log)
              if alicia_log and os.path.exists(alicia_log) else None)
    timeline = (scan_timeline(timeline_path)
                if timeline_path and os.path.exists(timeline_path) else None)
    summary = emit(report, alicia, timeline, sys.stdout)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(summary, handle, indent=2)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
