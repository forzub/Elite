# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / live NPC ownership  
**Canonical development branch:** `main`  
**Active stage:** 11B-1

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed
```

Closed: NavigationMap, NavigationSpace, LocalAvoidance, precision passage, emergency/contact severity, moving gaps/passages, moving/rotating docking, deterministic PilotSkillProfile.

Stage 11:

```text
11A runtime control seam                  ACCEPTED
11B-1 authoritative NPC runtime ownership ACTIVE
11B-2 replicated guidance/debug truth     PENDING
```

Stage 12: end-to-end scenarios, stress/performance, debug truth, post-impact replan and legacy retirement.

## Latest accepted live evidence

```text
d7c77d5868b3178be3c392f0a8fecad5b57e3b69
NAVIGATION LIVE RUNTIME CONTROL CONTRACT: PASS
runtime 1/1 PASS
trajectory/pilot 11/11 PASS
EliteGame + EliteServer build PASS
```

## 11B-1 architecture

The server no longer accepts steering commands from `NpcAiSystem`.

```text
NpcAiSystem
    goal + pilot profile only
        |
        v
NpcNavigationIntentController
    nominal Navigation v2 acceleration intent
        |
        v
per-NPC NavigationRuntimeControlBridge
        |
        v
accepted live capability/physics seam
```

The initial `MaintainForwardCruise` / `Hold` goals are only ownership fixtures. Higher mission/traffic/repair/combat systems can later choose goals without becoming steering solvers.

`GameSimulation` retains the exact latest execution snapshot/revision used for each NPC. 11B-2 will replicate this product to guidance/debug.

## Safety/runtime invariants

- no fallback to old `sin(position)` steering;
- no direct P/V/angular-rate mutation by navigation;
- no lost elapsed time under activation decimation;
- capability and physics remain downstream authorities;
- stale bridge state is discarded on failure/player takeover.

## Next

Pass 11B-1 target-machine full build/regression gate, then immediately implement replicated guidance/debug truth.
