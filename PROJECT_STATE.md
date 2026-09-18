# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / replicated live navigation truth  
**Canonical development branch:** `main`  
**Active stage:** 11B-2

## Progress

```text
[█████████████████████░░] 10 / 12 major stages closed
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

Live integration:

```text
11A  runtime control seam                  ACCEPTED
11B-1 authoritative NPC runtime ownership ACCEPTED
11B-2 replicated guidance/debug truth     ACTIVE
```

Stage 12 remains end-to-end stress/debug/performance and legacy retirement.

## Latest accepted evidence

```text
fb83b8d80f29c6c5e4e12b8af3794182790
```

Correction: the accepted 11B-1 commit is:

```text
fb83b8d80f29c6c5e4e12b8a2fca731ffea7b8e8
```

It passed both live architecture contracts, runtime 1/1, trajectory/pilot 11/11, and canonical client/server builds.

## 11B-2 architecture

The same server execution product now crosses replication:

```text
GameSimulation execution snapshot
 -> ShipSnapshot.navigationExecution
 -> binary wire schema v8
 -> ClientWorldState
 -> ReplicatedNavigationExecutionState
 -> ClientNavigationWorkspace
 -> GuidanceHudPresentation
```

Stable route-executor identity uses `ShipInstanceId`, while current runtime binding retains `EntityId`.

The client mirror is read-only to planning code.

Manual/advisory client planners continue to exist for player guidance but cannot consume or mutate server-executed NPC truth.

## Compatibility

Because `ShipSnapshot` wire layout changed, the simulation snapshot data-plane schema version is bumped from 7 to 8. Client/server mismatch is rejected explicitly rather than decoded ambiguously.

## Acceptance

11B-2 must pass:
- new replication/guidance architecture contract;
- canonical wire schema architecture contract;
- runtime control + replication truth tests;
- trajectory regression;
- canonical wire data-plane round-trip;
- EliteGame + EliteServer production build.

## Next

If 11B-2 is green, stage 11 closes and stage 12 begins immediately.
