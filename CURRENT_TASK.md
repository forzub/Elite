# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 end-to-end integration  
**Stage:** 12A — deterministic runtime proving ground

## Accepted baseline

Stage 11 is accepted on target-machine evidence from:

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713
```

with all architecture contracts green, runtime 2/2, trajectory/pilot 11/11, wire-data-plane 1/1 and both canonical production builds passing.

Stage 12 contract:

```text
src/game/navigation/STAGE12_END_TO_END.md
```

## Implement now

Create a reusable deterministic station-adjacent proving-ground scenario consumed by a headless end-to-end runtime fixture.

Minimum field:

```text
boxes / slabs
cylinders / pylons
offset walls
one forced detour
one narrow traversable gap
one gap too small for the configured hull
one moving crossing obstacle
```

Do not create a presentation-only obstacle list. The scenario must publish the same geometry/hit-volume truth used by the live NavigationWorld adapter.

## Required production chain

```text
scenario / goal
 -> NavigationWorld
 -> global/local/precision accepted navigation
 -> NavigationRuntimeControlBridge
 -> PilotSkillExecutor
 -> ShipControlState navigation demand
 -> SharedShipPhysics / ShipController / DynamicMotionSystem
 -> authoritative motion
 -> replication
 -> read-only guidance/debug truth
```

A test that stops at `LocalGuidancePlanner` or a trajectory evaluator does not satisfy this task.

## First acceptance gate

The deterministic fixture must prove:

- a normal configured ship reaches B without collision;
- at least one route revision/detour is caused by actual obstacle geometry;
- a genuinely traversable narrow opening is accepted;
- the same opening is rejected for an oversized hull;
- the moving obstacle causes a real avoidance/replan response;
- server execution truth remains the replicated presentation truth;
- no retired direct-steering fallback takes authority;
- work remains bounded and no synchronous full-world rescan is introduced.

## After green

1. expose the same proving ground in the interactive game;
2. implement/complete `Shift+F12` raw NavigationWorld visualization from the same completed snapshot;
3. extend fixtures to moving gap, docking, unavoidable-contact mitigation and post-impact replan;
4. collect live CPU/GPU/cadence measurements;
5. retire remaining legacy route-wide navigation only after v2 proves all required live scenarios.
