#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_timeline(path: Path) -> dict[str, object]:
    events: list[dict[str, object]] = []
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.strip():
            continue
        try:
            item = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(item, dict):
            events.append(item)

    finished = next((e for e in reversed(events) if e.get("event") == "session_finished"), None)
    details = str(finished.get("details", "")) if finished else ""
    outcome = "active_or_incomplete"
    for token in details.split():
        if token.startswith("outcome="):
            outcome = token.split("=", 1)[1]
            break

    event_names = {str(e.get("event", "")) for e in events}
    elapsed = max((int(e.get("elapsed_ms", 0)) for e in events), default=0)
    return {
        "file": path.parent.name if path.name == "timeline.jsonl" else path.name,
        "outcome": outcome,
        "duration_s": elapsed / 1000.0,
        "process": "yes" if "alicia_process_observed" in event_names else "no",
        "create_device": "yes" if "d3d9_create_device_observed" in event_names else "no",
        "present": "yes" if "d3d9_present_observed" in event_names else "no",
        "draw": "yes" if "d3d9_draw_observed" in event_names else "no",
        "exception": "yes" if "fatal_exception_observed" in event_names else "no",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", nargs="?", type=Path,
                        default=Path.home() / "Library/Application Support/Story of Alicia/logs/diagnostics")
    parser.add_argument("--limit", type=int, default=20)
    args = parser.parse_args()

    paths = list(args.directory.glob("run-*/timeline.jsonl"))
    paths.extend(args.directory.glob("*.timeline.jsonl"))
    paths = sorted(paths, key=lambda p: p.stat().st_mtime,
                   reverse=True)[: max(args.limit, 1)]
    if not paths:
        print(f"No timeline files found in {args.directory}")
        return 1

    rows = [read_timeline(path) for path in paths]
    headers = ["file", "outcome", "duration_s", "process", "create_device", "present", "draw", "exception"]
    widths = {h: max(len(h), *(len(f"{r[h]:.1f}") if h == "duration_s" else len(str(r[h])) for r in rows)) for h in headers}
    print("  ".join(h.ljust(widths[h]) for h in headers))
    print("  ".join("-" * widths[h] for h in headers))
    for row in rows:
        values = []
        for h in headers:
            value = f"{row[h]:.1f}" if h == "duration_s" else str(row[h])
            values.append(value.ljust(widths[h]))
        print("  ".join(values))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
