"""Stabilization stage 1 barrier: no mutable state of a WORLD in function statics or namespace globals of the authoritative
code (game/simulation, game/server, game/economy, game/traffic).

A world's state must belong to its owner (GameSimulation, GameServer, EconomyServer ...): a function static or a namespace
global outlives its world - a second world in the same process (exit to the menu, a new game; a future Host Game) reuses
ship ids and restarts server time and generations, and inherited the first world's pad-service places, take-off state,
strike radii and construction overrides (all fixed in stage 1).

Allowed: constants (const / constexpr / static const lambdas), log rate limiters and diagnostics (names with log / trace /
diag / timing / profile / nag / said / printed), and the explicit ALLOW list below - each with its reason."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DIRS = ["src/game/simulation", "src/game/server", "src/game/economy", "src/game/traffic"]
DIAG = re.compile(r"log|trace|diag|timing|profile|nag|said|printed|warned|dev[A-Z]|worst|lastMiss|empty|ticks|maxQueued",
                  re.I)
ALLOW = {
    # process-wide caches over IMMUTABLE asset data (not world state)
    ("GameSimulationCityDamage.cpp", "cache"): "has-damaged-variants per asset folder (the files on disk)",
    # the DEV economy load test (ELITE_DEV_ECONOMY_TICK_S only)
    ("GameServer.cpp", "economyNext"): "DEV economy tick load test",
    ("GameServer.cpp", "economyDayOffset"): "DEV economy tick load test",
    # a construction-override snapshot cache keyed by the store's generation (the store itself is reset per world)
    ("GameServer.cpp", "builtFor"): "keyed by hubExpansionOverrideGeneration, reset with the store",
    ("GameServer.cpp", "cached"): "keyed by hubExpansionOverrideGeneration, reset with the store",
    # the server worker's timing samples (DEV ELITE_DEV_WORKER_TIMING)
    ("ServerWorker.cpp", "t"): "worker timing diagnostics",
    ("ServerTickProfile.h", "profile"): "tick profile diagnostics",
    # the economy service's binding to the ONE installed world (install / uninstall) - a service locator, one world
    # at a time (stage 10 notes it for concurrent worlds)
    ("EconomyService.cpp", "b"): "installed-world binding with an explicit install / uninstall lifecycle",
    # the DEV city-track output
    ("GameSimulationCombat.cpp", "acc"): "DEV ELITE_DEV_CITY_TRACK sampler",
    ("GameSimulationCombat.cpp", "closest"): "closest-approach log of a pass (diagnostic)",
    ("GameSimulation.cpp", "tracks"): "DEV ELITE_DEV_EPOCH_CHECK diagnostic",
    ("GameServer.cpp", "timing"): "jump-entry timing diagnostic",
    ("GameServer.cpp", "timedDamage"): "timed damage diagnostic",
    # DEV scripted schedules (ELITE_DEV_CITY_STRIKE / ELITE_DEV_CITY_FIRE) with their done flags - test scenarios only
    ("GameSimulationCityDamage.cpp", "strikes"): "DEV scripted strike schedule",
    ("GameSimulationCityDamage.cpp", "events"): "DEV scripted fire schedule",
}
static_rx = re.compile(r"^\s+static\s+(?!const\b|constexpr\b|inline\b|void\b|_assert)([\w:<>,\s\*&]+?)\s+(\w+)\s*(=|;|\{|\()")
global_rx = re.compile(r"^(?:static\s+)?(?:thread_local\s+)?std::(?:unordered_map|map|vector|set|unordered_set|deque|atomic)<.*>\s+(\w+)\s*[;={]")

problems = []
for d in DIRS:
    for dirpath, _, files in os.walk(os.path.join(ROOT, d)):
        for f in files:
            if not f.endswith((".cpp", ".h")):
                continue
            p = os.path.join(dirpath, f)
            for n, line in enumerate(open(p, encoding="utf-8", errors="replace"), 1):
                if line.lstrip().startswith("//"):
                    continue
                m = static_rx.match(line)
                name = None
                if m and "(" not in m.group(1) and m.group(3) != "(":
                    name = m.group(2)
                else:
                    g = global_rx.match(line)
                    if g and "const" not in line.split(g.group(1))[0]:
                        name = g.group(1)
                if not name or DIAG.search(name) or (f, name) in ALLOW:
                    continue
                if "g_devCommandsAllowed" in line or "g_lastWriteError" in line or "g_diagnosticClock" in line:
                    continue   # process policy flag / diagnostics
                problems.append(f"{os.path.relpath(p, ROOT)}:{n}: mutable static / global '{name}': {line.strip()[:110]}")
if problems:
    print("[FAIL] world state outside its owner (stabilization stage 1):")
    for x in problems:
        print("  " + x)
    sys.exit(1)
print("[PASS] world state ownership: no mutable function statics / namespace globals in the authoritative code beyond the "
      "allow list")
