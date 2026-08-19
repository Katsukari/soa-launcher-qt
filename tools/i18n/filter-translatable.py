#!/usr/bin/env python3
"""Keep machine tokens out of the translation catalogues.

The launcher looks strings up at runtime via QCoreApplication::translate with a
non-literal source, so lupdate cannot maintain these files. Whatever populates
them sweeps up registry fragments, environment variable names, command-line
flags and paths along with the real UI text. This decides which is which.

    filter-translatable.py --check translations/*.ts     # CI: fail on machine tokens
    filter-translatable.py --missing translations/*.ts   # CI: fail on missing UI literals
    filter-translatable.py --prune translations/*.ts     # remove them, keep files in sync
    filter-translatable.py --list  translations/soa_launcher_en.ts

--prune edits every file given in the same way, so the catalogues do not drift
apart. It refuses to run unless all of them contain the same source strings.
"""

from __future__ import annotations

import argparse
import ast
import html
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict
from pathlib import Path

# Environment variables and runtime constants the launcher passes around.
MACHINE_NAMES = {
    "WINEPREFIX", "WINEARCH", "WINEDEBUG", "WINEDLLOVERRIDES", "WINESERVER",
    "WINETRICKS", "WINEESYNC", "WINEMSYNC", "WINE_FULLSCREEN_FSR", "LD_PRELOAD",
    "PROTONPATH", "PROTON_", "PROTON_USE_WINED3D", "DYLD_",
    "DYLD_FALLBACK_LIBRARY_PATH", "STEAM_COMPAT_",
    "STEAM_COMPAT_CLIENT_INSTALL_PATH", "STEAM_COMPAT_DATA_PATH",
    "STEAM_COMPAT_LIBRARY_PATHS", "CX_ROOT", "REG_SZ", "SET_ACTIVITY",
    "GAMEID", "PATH", "HOME", "PASSWORD", "SECRET", "TOKEN", "STORE",
    "WINE", "PROTON", "UMU", "CSV", "DNS", "SPDisplaysDataType",
    "OffscreenRenderingMode", "VideoMemorySize", "MacCompatibilityProfile",
    "Direct3DCreate9", "CreateDevice",
}

# Wine and macOS log markers that are matched against text, never shown.
LOG_MARKERS = {
    "Backtrace", "Register dump", "Stack dump", "Unhandled exception",
    "Exception frame is not in stack limits", "[REDACTED]",
    "Chipset Model:", "Model:", "device name", "vga compatible controller",
    "3d controller", "display controller", "advanced micro devices",
    "Apple GPU",
}

# Looks like a constant but is UI text. All-caps button and section labels are
# the reason this list exists: a naive "all caps means constant" rule deletes
# ADVANCED, CANCEL and CHECKING along with WINEPREFIX.
KEEP = {
    "ABOUT", "ADVANCED", "CANCEL", "CHECKING", "CONFIRMATION", "COPIED",
    "CREDITS", "DOWNLOADING", "ERROR", "FAILED", "HOME", "INFORMATION",
    "INSTALL", "LAUNCHER", "MENU", "PLAYER", "PLAYTEST", "PREPARING",
    "PROTON", "READY", "REPAIRING", "RESUMING", "RETRY", "RUNTIME", "STORE",
    "UMU", "WINE", "%1 B", "%1 GB", "%1 KB", "%1 MB", "%1 MB of %2 MB", "%1/s",
    "%1h", "%1h %2m", "%1m", "%1m %2s", "%1s",
    "VERSION", "WARNING", "Normal", "English", "Norsk", "Nederlands",
}

WORD = re.compile(r"[A-Za-z]{3,}")
PLACEHOLDER = re.compile(r"%\d+|%[a-z]")
TAG = re.compile(r"<[^>]+>")
CPP_STRING_PATTERN = r'(?:u8|u|U|L)?"(?:\\.|[^"\\])*"'
TRANSLATE_LITERAL = re.compile(
    rf'\b(?:util::)?i18n::translate\s*\(\s*((?:{CPP_STRING_PATTERN}\s*)+)\)',
    re.S,
)
CPP_STRING = re.compile(CPP_STRING_PATTERN)


def classify(source: str) -> str | None:
    """Return why this string is a machine token, or None if it is UI text."""
    text = source.strip()
    if not text:
        return "empty"
    if text in KEEP or (len(text.split()) > 1 and text.upper() == text
                        and all(w in KEEP or w.isalpha() for w in text.split())):
        return None

    if text in MACHINE_NAMES:
        return "environment variable or runtime constant"
    if text in LOG_MARKERS:
        return "log marker matched in text, never displayed"

    # Nothing left to translate once markup and placeholders are removed.
    bare = PLACEHOLDER.sub("", TAG.sub("", text))
    if not WORD.search(bare):
        return "no translatable words"

    if re.search(r'^"[A-Za-z0-9_ ]+"\s*=', text):
        return "registry line"
    if re.fullmatch(r'"[A-Za-z0-9_ ]+"', text):
        return "registry key name"
    if text.startswith("REGEDIT4") or "HKEY_" in text:
        return "registry"
    if re.fullmatch(r"-{1,2}[A-Za-z][\w-]*(=.*)?", text):
        return "command-line flag"
    if re.fullmatch(r"/[A-Z]{1,2}", text):
        return "command-line flag"
    if re.fullmatch(r"[\w.\-]+\.(exe|dll|log|json|sh|app|drv|so|dylib|png|"
                    r"icns|plist|reg|bat|jsonl)", text, re.I):
        return "filename"
    # Qt file-dialog filters look like globs but the labels around them are
    # user-visible: "Applications (*.app);;All Files (*)".
    if ";;" not in text and not re.search(r"\)\s*$", text):
        if re.search(r"\*\.[a-z]+|[\w.\-]+-\*", text):
            return "glob pattern"
    if text.startswith(("../", "./", "~/", "/.")):
        return "relative path"
    if re.match(r"^\.?[\w ]*/[\w /.]*$", text) and " " not in text.split("/")[0]:
        return "path fragment"
    if text.startswith(("<?xml", "<!DOCTYPE")):
        return "xml"
    # Inline CSS or block-level markup should never reach a translator: one
    # mangled tag breaks the layout, and the styling is not theirs to change.
    # Build the markup in the source and pass the prose through %1 instead.
    if "style=" in text or re.search(r"<(h[1-6]|div|table|tr|td|p)\b", text, re.I):
        return "markup with styling - split it out of the source"
    # key=value diagnostic payloads: two or more pairs and no prose
    pairs = re.findall(r"\b\w+=(?:%\d+|[\w./\-]*)", text)
    if len(pairs) >= 2 and len(WORD.findall(re.sub(r"\b\w+=\S*", "", text))) == 0:
        return "diagnostic key=value payload"
    if re.fullmatch(r"[yMdHmsz\-]{8,}", text):
        return "date format pattern"
    if re.search(r"\breg\.exe\b|\bcmd\b|>nul|exit /b", text):
        return "shell command"
    return None


def sources(text: str) -> list[str]:
    return [html.unescape(m) for m in re.findall(r"<source>(.*?)</source>", text, re.S)]


def context_sources(text: str, context_name: str) -> set[str]:
    root = ET.fromstring(text)
    for context in root.findall("context"):
        if context.findtext("name") == context_name:
            return {
                message.findtext("source") or ""
                for message in context.findall("message")
            }
    return set()


def literal_translation_sources(source_root: Path) -> dict[str, list[tuple[Path, int]]]:
    found: dict[str, list[tuple[Path, int]]] = defaultdict(list)
    suffixes = {".cpp", ".h", ".hpp", ".m", ".mm"}
    for path in sorted(source_root.rglob("*")):
        if path.suffix not in suffixes:
            continue
        text = path.read_text(encoding="utf-8")
        for match in TRANSLATE_LITERAL.finditer(text):
            parts: list[str] = []
            for token_match in CPP_STRING.finditer(match.group(1)):
                token = re.sub(r"^(?:u8|u|U|L)", "", token_match.group(0))
                value = ast.literal_eval(token)
                if not isinstance(value, str):
                    raise ValueError(f"unexpected non-string literal in {path}")
                parts.append(value)
            line = text.count("\n", 0, match.start()) + 1
            found["".join(parts)].append((path, line))
    return found


def prune(text: str, doomed: set[str]) -> tuple[str, int]:
    kept, removed = [], 0
    for chunk in re.split(r"(<message>.*?</message>\s*)", text, flags=re.S):
        found = re.search(r"<source>(.*?)</source>", chunk, re.S)
        if chunk.startswith("<message>") and found and html.unescape(found.group(1)) in doomed:
            removed += 1
            continue
        kept.append(chunk)
    return "".join(kept), removed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true",
                      help="exit 1 if any machine token is present")
    mode.add_argument("--missing", action="store_true",
                      help="exit 1 if a literal translate() source is missing")
    mode.add_argument("--prune", action="store_true",
                      help="remove machine tokens from every file given")
    mode.add_argument("--list", action="store_true",
                      help="show what would be removed, grouped by reason")
    parser.add_argument("--source-root", type=Path, default=Path("src"),
                        help="source tree scanned by --missing (default: src)")
    parser.add_argument("files", nargs="+", type=Path)
    args = parser.parse_args()

    catalogues = {p: p.read_text(encoding="utf-8") for p in args.files}

    # Catalogues must already agree, or pruning would push them further apart.
    sets = {p: set(sources(t)) for p, t in catalogues.items()}
    reference = next(iter(sets.values()))
    for path, entries in sets.items():
        if entries != reference:
            only = entries ^ reference
            print(f"error: {path.name} does not contain the same sources as the others "
                  f"({len(only)} differ); fix that before pruning", file=sys.stderr)
            return 2

    if args.missing:
        launcher_sets = {
            path: context_sources(text, "Launcher")
            for path, text in catalogues.items()
        }
        launcher_reference = next(iter(launcher_sets.values()))
        for path, entries in launcher_sets.items():
            if entries != launcher_reference:
                only = entries ^ launcher_reference
                print(
                    f"error: {path.name} does not contain the same Launcher-context "
                    f"sources as the others ({len(only)} differ)",
                    file=sys.stderr,
                )
                return 2
        locations = literal_translation_sources(args.source_root)
        missing = {source: refs for source, refs in locations.items()
                   if source not in launcher_reference}
        if not missing:
            print(
                f"complete: {len(locations)} literal translation sources are "
                "catalogued in the Launcher context"
            )
            return 0
        print(f"{len(missing)} literal translation sources are missing:", file=sys.stderr)
        for source, refs in sorted(missing.items(), key=lambda item: item[1][0]):
            path, line = refs[0]
            shown = source.replace("\n", r"\n")
            print(f"  {path}:{line}: {shown!r}", file=sys.stderr)
        return 1

    reasons: dict[str, str] = {}
    for source in reference:
        why = classify(source)
        if why:
            reasons[source] = why

    if not reasons:
        print(f"clean: {len(reference)} messages, no machine tokens")
        return 0

    grouped: dict[str, list[str]] = defaultdict(list)
    for source, why in reasons.items():
        grouped[why].append(source)

    if args.check:
        print(f"{len(reasons)} machine tokens in {len(reference)} messages:", file=sys.stderr)
        for why, items in sorted(grouped.items(), key=lambda kv: -len(kv[1])):
            print(f"  {len(items):>4}  {why}", file=sys.stderr)
            for item in sorted(items)[:3]:
                print(f"          {item[:70]!r}", file=sys.stderr)
        print("\nrun with --prune to remove them", file=sys.stderr)
        return 1

    if args.list:
        for why, items in sorted(grouped.items(), key=lambda kv: -len(kv[1])):
            print(f"{len(items):>4}  {why}")
            for item in sorted(items):
                print(f"        {item[:88]!r}")
        return 0

    counts = Counter()
    for path, text in catalogues.items():
        pruned, removed = prune(text, set(reasons))
        path.write_text(pruned, encoding="utf-8")
        counts[removed] += 1
        print(f"  {path.name}: removed {removed}, "
              f"{len(sources(pruned))} remain")
    if len(counts) != 1:
        print("error: files diverged during pruning", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
