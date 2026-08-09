#!/usr/bin/env bash
# Story of Alicia — WINEDEBUG channel profiles (macOS + Linux)
#
# Sourcing this file gives you soa_wine_debug <mode>, which prints the WINEDEBUG
# string for that mode. Kept as data in one place so the launcher, the shell,
# and CI cannot drift apart.
#
#   source tools/diagnostics/wine-debug-profiles.sh
#   export WINEDEBUG="$(soa_wine_debug forensic)"
#
# Sizes below are for a ~60 second Alicia run; they scale roughly linearly.

soa_wine_debug() {
    case "${1:-normal}" in
    off)
        # Nothing. Use for performance measurement only.
        echo "-all"
        ;;
    normal)
        # Default. Every error, every module load, cheap enough to leave on.
        # Answers "which DLL loaded, which failed". ~1-3 MB.
        echo "+timestamp,+pid,+tid,err+all,fixme+all,+winediag,+loaddll"
        ;;
    verbose)
        # Adds full module/process/thread tracing and the graphics + audio
        # subsystems. Answers "what was the last thing each thread did".
        # ~20-60 MB.
        echo "+timestamp,+pid,+tid,err+all,fixme+all,+winediag,+loaddll,+module,+process,+thread,+seh,+d3d,+d3d9,+dsound,+mmdevapi,+winmm"
        ;;
    audio)
        # Targeted at the DirectSound/CoreAudio crash: every memory mapping plus
        # the audio stack, and nothing else. forensic answers the same question
        # but +heap/+file/+sync are ~88% of its volume and slow the run ~39x,
        # which is enough to stop the fault reproducing at all.
        # ~3-8 MB.
        echo "+timestamp,+pid,+tid,err+all,fixme+all,+winediag,+loaddll,+virtual,+dsound,+mmdevapi,+winmm,+coreaudio"
        ;;
    forensic)
        # Everything short of instruction tracing: registry, file I/O, window
        # messages, synchronisation. Use when a run fails and you do not yet
        # have a hypothesis. ~200-800 MB.
        echo "+timestamp,+pid,+tid,err+all,fixme+all,+winediag,+loaddll,+module,+process,+thread,+seh,+d3d,+d3d9,+dsound,+mmdevapi,+winmm,+reg,+file,+win,+msg,+sync,+heap,+virtual,+imm,+ntdll"
        ;;
    relay)
        # Every cross-DLL call. This is the deepest Wine goes. Expect several
        # GB and a ~20x slowdown; the game may not reach the same point at all,
        # so treat relay runs as a different experiment, not a slower one.
        echo "+timestamp,+pid,+tid,err+all,fixme+all,+relay,+snoop,+seh,+loaddll,+module"
        ;;
    *)
        echo "soa_wine_debug: unknown mode '$1'" >&2
        echo "modes: off normal verbose audio forensic relay" >&2
        return 2
        ;;
    esac
}

soa_wine_debug_modes() { echo "off normal verbose audio forensic relay"; }

# Executed directly rather than sourced: print the requested mode.
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    soa_wine_debug "$@"
fi
