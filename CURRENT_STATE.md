# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 12 — end-to-end runtime/stress/debug + legacy retirement

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed

1–10 ACCEPTED
11   ACCEPTED — live game/server/guidance + physics
     11A runtime control seam                  ACCEPTED
     11B-1 authoritative NPC runtime ownership ACCEPTED
     11B-2 replicated guidance/debug truth     ACCEPTED
12   ACTIVE — end-to-end runtime/stress/debug + legacy retirement
```

## Stage 11 acceptance

Target-machine evidence on:

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713
```

passed:

```text
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
NAVIGATION LIVE NPC OWNERSHIP CONTRACT: PASS
NAVIGATION LIVE REPLICATION/GUIDANCE CONTRACT: PASS
wire schema architecture PASS
navigation_runtime 2/2 PASS
navigation_trajectory/pilot 11/11 PASS
wire_data_plane_contracts 1/1 PASS
EliteGame build PASS
EliteServer build PASS
```

The sparse execution invariant is accepted:

```text
ShipSnapshot.navigationExecution =
    variant<monostate, NavigationExecutionSnapshot>

absent execution -> exactly one variant-tag byte
```

Stage 11 is closed. The old direct NPC steering authority remains retired.

## Accepted live ownership chain

```text
NpcAiSystem
    goal + policy only
        |
        v
Navigation v2 planning / accepted maneuver
        |
        v
NavigationRuntimeControlBridge
        |
        v
PilotSkillExecutor
        |
        v
ShipControlState direct navigation demand
        |
        v
SharedShipPhysics / ShipController / DynamicMotionSystem
        |
        v
authoritative motion
        |
        +--> replication
        +--> read-only guidance/debug truth
```

Client planners cannot consume or rewrite replicated server execution truth.

## Stage 12 authority

```text
src/game/navigation/STAGE12_END_TO_END.md
```

Stage 12 now proves the accepted pieces as one live system using deterministic station-adjacent clutter, actual hit-volume/navigation geometry, moving conflicts, narrow passages, docking, post-impact replan, replicated execution truth, manual guidance and raw NavigationWorld debug.

## First Stage 12 slice

Build one reusable deterministic proving-ground scenario and one headless end-to-end fixture:

```text
station-adjacent clutter
A -> B
forced coarse detour
narrow traversable opening
opening too small for configured hull
moving crossing obstacle
```

The fixture must traverse production ownership rather than call only isolated trajectory evaluators.

After the headless gate is green, expose the same scenario in EliteGame and make `Shift+F12` visualize the accepted live NavigationWorld truth.
