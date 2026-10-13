# Ship simulation tier proving ground

The simulation tier is independent from actor identity.

Normal large-population path:

```text
TrafficShell -> MaterializedShip -> Derelict -> WreckShell
```

A shell has identity, descriptor, transform, formation membership and coarse
damage, but no ShipCore/module graph/reactor/thermal/autopilot runtime.

## Commands

From MSYS2 / Git Bash at the repository root:

```bash
scripts/run-ship-simulation-tier-tests.sh contracts
```

Builds EliteGame and the focused contract tests, then runs CTest.

```bash
scripts/run-ship-simulation-tier-tests.sh shell500
```

Spawns 500 lightweight TrafficShell ships in 3-ship formations on a loop placed
in front of the player. This is the normal bulk-population scalability test.

```bash
scripts/run-ship-simulation-tier-tests.sh mixed500
```

Spawns 500 total entities: 24 materialized ships and 476 shells. This exercises
the normal interaction budget without turning all background traffic into full
ships.

```bash
scripts/run-ship-simulation-tier-tests.sh tier
```

Spawns one standalone shell about 900 m ahead and automatically performs:

- ~2 s: TrafficShell -> MaterializedShip
- ~5 s: cockpit eject -> Derelict, AI and formation ownership removed
- ~8 s: Derelict -> WreckShell

Expected console markers begin with `[ShipTierTest]`.

```bash
scripts/run-ship-simulation-tier-tests.sh combined
```

Runs the physical ShipState flight specimen together with 500 shells and the
standalone lifecycle probe.

```bash
scripts/run-ship-simulation-tier-tests.sh fullstress
```

Intentional torture test: all 500 ships are fully materialized. This is not the
normal runtime architecture and may become extremely slow. Use only when
profiling the hard ceiling of full simulation.

## 500-ship CSV

`herd_500.csv` reports:

- total entities
- shell count
- materialized count
- valid full ShipState count
- Active / Prewarm / Coarse materialized counts
- mean/max formation-slot error
- mean speed
