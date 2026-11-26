#!/usr/bin/env bash
# Shared settings for the Newtonian pilot diagnostics (run from the MSYS2
# MinGW64 shell). Transient outputs go to $ELITE_DIAG_DIR (default D:/tmp)
# with the elite_ prefix; the game itself writes its DEV logs there too.
NEWTONIAN_TOOLS="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ELITE_ROOT="$(cd "$NEWTONIAN_TOOLS/../../.." && pwd)"
export ELITE_DIAG_DIR="${ELITE_DIAG_DIR:-/d/tmp}"
# ELITE_NEWTONIAN_BUILD: the build dir of the replay tools (default build/headless_server; 2026-11-26 its -O0
# GameServer.cpp stopped linking - build-relwithdebinfo works)
NEWTONIAN_BUILD="${ELITE_NEWTONIAN_BUILD:-$ELITE_ROOT/build/headless_server}"
REPLAY_EXE="$NEWTONIAN_BUILD/EliteNewtonianReplay.exe"
CASES_DIR="$NEWTONIAN_TOOLS/cases"
