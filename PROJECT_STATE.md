# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / end-to-end runtime proving ground  
**Canonical development branch:** `main`  
**Active stage:** 12A

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed
```

Closed:
1. NavigationMap
2. NavigationSpace
3. LocalHorizon / LocalAvoidance
4. oriented passage / bounded gaps / attitude
5. continuous static passage
6. emergency mitigation / contact severity
7. moving-gap prediction
8. moving continuous passage
9. moving/rotating docking
10. deterministic PilotSkillProfile
11. live runtime ownership + replicated guidance/debug truth

Active:
12. end-to-end runtime/stress/debug + legacy retirement

## Latest accepted evidence

```text
9650c44cca23741dae3f4acf2c9a96a4ab4c5713
```

Target-machine acceptance passed:

```text
live runtime architecture PASS
live NPC ownership architecture PASS
live replication/guidance architecture PASS
wire schema architecture PASS
navigation_runtime 2/2 PASS
navigation_trajectory/pilot 11/11 PASS
wire_data_plane_contracts 1/1 PASS
EliteGame build PASS
EliteServer build PASS
```

This closes all of stage 11.

## Accepted replicated execution architecture

```text
GameSimulation execution snapshot
 -> ShipSnapshot.navigationExecution sparse variant
 -> binary wire schema v8
 -> ClientWorldState
 -> ReplicatedNavigationExecutionState
 -> ClientNavigationWorkspace
 -> GuidanceHudPresentation
```

Stable route-executor identity uses `ShipInstanceId`, while current runtime binding retains `EntityId`. The client mirror is read-only to planning code and updates only on accepted server snapshot ticks. An absent execution costs exactly one variant-tag byte.

## Stage 12

Authority:

```text
src/game/navigation/STAGE12_END_TO_END.md
```

The first target is a deterministic station-adjacent obstacle proving ground exercised through the production runtime chain. It must use actual navigation/collision geometry, not a visual-only obstacle list, and must include a forced detour, a narrow passable opening, an opening that fails for a larger hull, and a moving crossing obstacle.

After the headless end-to-end gate, the exact same scenario becomes the interactive proving ground and `Shift+F12` debug source.

Legacy route-wide navigation remains quarantined until the v2 runtime proves ordinary travel, obstacle avoidance, narrow passages, moving conflicts, docking, post-contact replan and guidance presentation.
