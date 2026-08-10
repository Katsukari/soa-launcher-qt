# Diagnostics

Tools for working out why a launch failed, and for collecting everything needed
to ask someone else. Written by hand with AI assistance.

## Log modes

`wine-debug-profiles.sh` holds the WINEDEBUG channel sets, so the launcher, the
shell and CI all use the same definitions.

| mode | rough size, 60s run | what it gets you |
|---|---|---|
| `off` | – | nothing; use when measuring performance |
| `normal` | 1–3 MB | which modules loaded, which failed, every error |
| `verbose` | 20–60 MB | plus per-thread activity, graphics, audio, SEH |
| `audio` | 3–8 MB | memory mappings and the audio stack only |
| `forensic` | 200–800 MB | plus registry, file I/O, window messages, sync, heap |
| `relay` | GBs | every cross-DLL call; roughly 20x slower |

    source tools/diagnostics/wine-debug-profiles.sh
    export WINEDEBUG="$(soa_wine_debug verbose)"

A `relay` run is a different experiment rather than a slower one, the timing
changes enough that the game often fails somewhere else entirely. `forensic` is
heavy enough to do the same, which is why `audio` exists.

## Reading a run

    tools/diagnostics/soa-log-analyze.py <run-directory>
    tools/diagnostics/soa-log-analyze.py wine.log --alicia alicia.log --timeline timeline.jsonl
    tools/diagnostics/soa-log-analyze.py wine.log --json report.json

Given a diagnostic run directory it finds `wine.log`, `alicia.log` and
`timeline.jsonl` on its own. It streams, so a multi-gigabyte relay log is fine,
and it reads all four Wine line formats — a log recorded without `+timestamp`
still parses.

It reports:

- **How far the launch got**, from the milestones in `timeline.jsonl`
- **Modules loaded** - name, base address, order, native or system
- **Modules that failed**, split into unexpected and expected-on-this-platform.
  Wine probes every audio backend and uses whichever exists, so on macOS
  `winepulse`, `winealsa`, `wineoss` and `wineandroid` always fail and it means
  nothing. Reading those as the fault wasted a lot of time once.
- **Exceptions** with the NTSTATUS decoded, marking which are fatal-class and
  which are routine
- **Wine assertions** - bugs in the Wine build, not in the game
- **Last activity per thread**, newest first; the quiet ones died first
- **A verdict** in plain English

## Collecting a bundle

    tools/diagnostics/collect-macos.sh
    tools/diagnostics/collect-linux.sh

Each gathers logs, host details, prefix registry and graphics/audio settings,
runs the analyser over the newest run, and produces one archive containing
`analysis.txt`.

## A caveat about log quality

Wine writes debug output unbuffered, so when two threads write at once their
lines splice together and a message from one lands inside another. The analyser
flags what it can detect as `<<SPLICED>>`, but not every case is detectable.

It is worse when stderr is captured through a pipe, where only `PIPE_BUF` bytes
are atomic. Writing Wine's stderr straight to a file reduces it noticeably.
