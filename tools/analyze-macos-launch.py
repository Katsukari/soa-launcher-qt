#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path

SIGNATURES = [
    ("runtime_freetype_missing", "cannot find the FreeType font library", "Runtime packaging failure: FreeType is missing or unloadable."),
    ("vulkan_unavailable", "built without Vulkan support", "Runtime has no Vulkan backend; DXVK cannot work in this build."),
    ("dll_load_failure", "failed to load", "A DLL or native dependency failed to load; inspect the first matching line."),
    ("unhandled_exception", "unhandled exception", "Alicia or Wine raised an unhandled exception."),
    ("page_fault", "page fault", "Wine recorded a page fault."),
    ("d3d_device_failure", "CreateDevice failed", "Direct3D 9 device creation failed."),
]


def timeline_events(path: Path) -> list[dict[str, object]]:
    result=[]
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        try:
            item=json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(item, dict):
            result.append(item)
    return result


def main() -> int:
    parser=argparse.ArgumentParser()
    parser.add_argument("timeline", type=Path)
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path)
    args=parser.parse_args()

    events=timeline_events(args.timeline)
    names=[str(e.get("event", "")) for e in events]
    log=args.log.read_text(encoding="utf-8", errors="replace")
    lower=log.lower()

    findings=[]
    for code, needle, explanation in SIGNATURES:
        index=lower.find(needle.lower())
        if index >= 0:
            line=log[:index].count("\n")+1
            findings.append((code, line, explanation))

    process_seen="alicia_process_observed" in names
    create_device="d3d9_create_device_observed" in names
    present="d3d9_present_observed" in names
    draw="d3d9_draw_observed" in names
    wrapper_finished="wine_wrapper_finished" in names
    finished=next((e for e in reversed(events) if e.get("event")=="session_finished"), None)

    if not process_seen and wrapper_finished:
        stage="process_creation"
    elif process_seen and not create_device:
        stage="before_d3d9_device"
    elif create_device and not present:
        stage="after_device_before_present"
    elif present and not draw:
        stage="after_present_before_draw"
    elif draw:
        stage="rendering_started"
    else:
        stage="unknown"

    lines=[
        "Story of Alicia macOS launch diagnosis",
        f"stage={stage}",
        f"process_seen={int(process_seen)}",
        f"create_device_seen={int(create_device)}",
        f"present_seen={int(present)}",
        f"draw_seen={int(draw)}",
        f"wrapper_finished={int(wrapper_finished)}",
        f"session_finished={int(finished is not None)}",
    ]
    if finished:
        lines.append(f"session_details={finished.get('details','')}")
    if findings:
        lines.append("findings:")
        for code, line, explanation in findings:
            lines.append(f"- {code} at log line {line}: {explanation}")
    else:
        lines.append("findings: no known fatal signature found")
    text="\n".join(lines)+"\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
