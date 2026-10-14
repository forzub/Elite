# Ship simulation tier and response-physics proving ground

The runtime separates three independent concerns:

```text
identity/actor       simulation tier          materialized fidelity
--------------       ---------------          ---------------------
NPC pilot       +    TrafficShell
traffic actor   +    MaterializedShip    +    Response
player          +    MaterializedShip    +    Detailed
no pilot        +    Derelict            +    Response/Detailed
no pilot        +    WreckShell
```

Normal large-population lifecycle:

```text
TrafficShell -> MaterializedShip(Response) -> Derelict -> WreckShell
```

A shell has identity, descriptor, transform, formation membership and coarse
damage. It has no ShipCore/module graph/reactor/thermal/autopilot runtime.

A Response materialized NPC still has authoritative motion, AI/control and a
real ShipState, but the expensive physical description is resolved into a
cached capability/response profile. Reactor, thermal, life-support and
per-observer full signal/radar scans are skipped. The response profile is
recomputed only when construction, actuator damage or mass properties change.

Detailed fidelity is reserved for the player and for an explicitly promoted
ship when gameplay really needs module-level state (module damage/repair,
ejection, structural inspection, etc.).

## Visibility and maps

TrafficShell and WreckShell are ordinary authoritative ship identities.

- 3D scene: rendered through the existing instanced whole-ship proxy lane.
  They are not expanded into per-module real-ship mesh work.
- System Map: sampled from the same ShipSnapshot stream as materialized ships.
- Detail Map: same rule; shell identity/tier is retained.
- Hub Map: a non-player ship appears only when it actually belongs to that hub
  frame, exactly like a materialized ship.

## Commands

Every runtime command below builds `EliteGame` first. This is deliberate:
running a stale binary invalidates the test.

```bash
scripts/run-ship-simulation-tier-tests.sh contracts
```

Builds EliteGame, the focused navigation/runtime contracts, plus System/Detail
map shell-presence contracts.

```bash
scripts/run-ship-simulation-tier-tests.sh shell500
```

Spawns 500 TrafficShell ships in 3-ship formations on a loop in front of the
player. Expected: visible proxy traffic and map entries, without 500 ShipCore
instances.

```bash
scripts/run-ship-simulation-tier-tests.sh mixed500
```

Spawns 500 total entities: 24 MaterializedShip(Response) and 476 TrafficShell.
This is the normal interaction-budget test.

```bash
scripts/run-ship-simulation-tier-tests.sh tier
```

Spawns one standalone shell about 900 m ahead and automatically performs:

- ~0 s: TrafficShell, visible/map-visible
- ~2 s: TrafficShell -> MaterializedShip(Response), same EntityId
- ~5 s: cockpit eject -> Derelict, AI/formation authority removed
- ~8 s: Derelict -> WreckShell, same EntityId

Expected console markers begin with `[ShipTierTest]`.

```bash
scripts/run-ship-simulation-tier-tests.sh combined
```

Runs the physical ShipState flight specimen together with 500 shells and the
standalone lifecycle probe.

```bash
scripts/run-ship-simulation-tier-tests.sh fullstress
```

Intentional hard-ceiling test: 500 ShipCore/materialized actors. They use
Response fidelity unless explicitly promoted, so this measures motion/AI/entity
overhead rather than 500 detailed reactors/thermal systems. This is not the
normal population path.

## GPU policy

Use the GPU where the work is coherent and massively parallel:

- whole-ship proxy instancing (already used by shells);
- future GPU culling/transform evaluation for very large shell populations;
- possibly a GPU spatial broad-phase if results can stay on-GPU.

Do not move detailed ship simulation to GPU merely because it is expensive.
Module graphs, AI, damage transitions, activation and service systems are
branch-heavy and require authoritative CPU state. At the expected number of
interactive ships, cached response coefficients plus a CPU spatial broad-phase
are the preferred path.

## 500-ship CSV

`herd_500.csv` reports:

- total entities
- shell count
- materialized count
- valid full ShipState count
- Active / Prewarm / Coarse materialized counts
- mean/max formation-slot error
- mean speed
