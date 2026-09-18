# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / Stage 12A-2 authoritative proving actor  
**Canonical development branch:** `main`

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed
```

Stages 1–11 are accepted. Stage 12 is active.

## Accepted Stage 12A-1

Target-machine gate on
`af58b46cdb01ad254097383e5e4274c733c4e28e` passed:

```text
stage-12 runtime planner architecture PASS
navigation_runtime 3/3 PASS
EliteGame build PASS
EliteServer build PASS
```

This accepts the shared runtime composition from NavigationSpace/NavigationMap
through LocalHorizon/LocalAvoidance into the existing PilotSkillExecutor control
bridge.

## Stage 12A-2 candidate

The accepted planner is now connected to authoritative `GameSimulation` for a
single isolated `NAVIGATION V2 RUNTIME LAB` NPC. The actor is pinned Active;
ordinary NPC behavior remains unchanged.

The existing diagnostic scene already provides the physical proving field:
`NAV STRESS CUBE/CYLINDER` objects. The lab route deliberately crosses
`NAV STRESS CUBE 08` on the unmodified straight line.

A new `NavigationHitVolumeAdapter` makes authoritative damage/collision
`HitVolume` geometry the navigation source. The current lab consumes
hit-volume-derived conservative radii in `NavigationMap`; exact world OBB
conversion is implemented and tested for the subsequent static/precision
topology slice.

## Next evidence

1. target-machine architecture/runtime/build gate for 12A-2;
2. deterministic live simulation evidence that CUBE 08 changes the accepted
   command and the authoritative ship avoids contact;
3. exact hit-volume OBB -> static/precision NavigationSpace publication;
4. extend the same proving field to moving conflict, narrow gap, docking and
   post-impact replan;
5. expose the same accepted truth through Shift+F12 rather than a second
   visualization planner.
