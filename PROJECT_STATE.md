# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / live runtime integration  
**Canonical development branch:** `main`  
**Active stage:** 11A

## Progress

```text
[████████████████████░░░] 10 / 12 major stages closed
```

Closed:

```text
1  NavigationMap / mass dynamic state
2  NavigationSpace / global corridors
3  LocalHorizon / LocalAvoidance
4  oriented passages / gaps / attitude
5  continuous static passage
6  emergency mitigation / contact severity
7  moving gap prediction
8  moving continuous passage
9  moving/rotating docking 6DoF
10 deterministic PilotSkillProfile execution
```

Active:

```text
11 live game/server/guidance + physics hookup
   11A runtime control seam
   11B authoritative NPC/guidance ownership
```

Remaining:

```text
12 end-to-end stress/debug/performance + legacy retirement
```

## Stage 10 acceptance

```text
b042321b65084950aa91784a5e02344430e5c2bc
NAVIGATION PILOT SKILL CONTRACT: PASS
11/11 PASS
```

## Stage 11A design

The live seam keeps responsibilities explicit:

```text
Navigation v2 intent
    -> PilotSkillExecutor
    -> ShipControlState direct acceleration demand
    -> existing ship capability layer
    -> authoritative motion
```

A new `NavigationRuntimeControlBridge` publishes one execution snapshot matching the exact demand sent downstream.

The old keyboard-oriented `ShipControlState` remains compatible, but gains explicit navigation acceleration-demand fields. Existing ships without that flag continue through the established path.

`ShipController` owns angular capability truth. `DynamicMotionSystem` owns linear propulsion split and final speed/resource semantics. `GameSimulation` selects direct navigation demand only when no material manual translation override exists.

## Stage 11B direction

The current `NpcAiSystem` is still a placeholder steering authority. After 11A acceptance it must become goal/policy input rather than a second motion solver. Per-NPC live intent and execution revisions will be the single truth consumed by both control and guidance/debug.

## Final remaining work

After live ownership is stable:

```text
end-to-end scenarios
performance/stress
Shift+F12/debug truth
guidance same accepted intent
collision/post-impact replan
legacy navigation retirement
```
