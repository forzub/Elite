# Project State

**Updated:** 2026-09-18 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / Stage 12A-3 live behavior proof  
**Canonical development branch:** `main`

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed
```

Stages 1–11 are accepted. Stage 12 is active.

### 12A-1 — ACCEPTED

Shared `NavigationRuntimePlanner` composition from NavigationSpace/NavigationMap
through LocalHorizon/LocalAvoidance into PilotSkillExecutor.

### 12A-2 — ACCEPTED

Target-machine gate on
`7b4db95788d80c95afb3b57c109c671cb7a41366` passed architecture,
`navigation_runtime 3/3`, EliteGame and EliteServer.

One isolated Active diagnostic NPC is therefore accepted as authoritative
`GameSimulation` ownership using the existing NAV STRESS physical scene and
HitVolume-derived navigation geometry.

### 12A-3 — ACCEPTED

Target-machine acceptance on
`9246530e5eb539e227a1af69461113f2b30ec93a` proved the complete live CUBE 08
chain through authoritative physics and exact sparse/canonical replication.

The working-frame defect is closed: planner map-frame acceleration is
transformed into the Stage-11 world-space control seam. The accepted run stayed
under the configured ~60 m/s target, produced ~5.51 m/s² physically applied
lateral acceleration, retained 129.59 m conservative clearance, progressed
3.50 km in 58.9 s and replicated execution with zero same-tick error.

### 12A-4 — CANDIDATE

The next geometry slice moves persistent static precision into
`NavigationSpace` using the authoritative HitVolume-derived
`NavigationObstacle` OBBs.

`NavigationSpace` now owns exact static point/segment tests alongside its
coarse region/portal topology. `LocalAvoidance` must prove both the nominal
bounded segment and every adjusted probe against that geometry, including when
the dynamic `NavigationMap` reports no conflict.

A deterministic regression proves the intended narrow-passage rule: overlapping
conservative bounding spheres may not erase a genuine OBB aperture. A fitting
agent crosses; an oversized envelope fails.

The live NAV STRESS fixture publishes the same HitVolume OBBs in the
NavigationMap working frame after authoritative transforms/HitVolume rebuild.
The server self-test requires publication, non-zero exact-static query work and
an observed nominal static block, so live PASS cannot be satisfied by merely
storing unused OBB data.

For this intermediate gate, static objects remain in the existing
NavigationMap sphere candidate set as well. Removing those static spheres is a
separate post-acceptance cleanup so broadphase ownership and precision geometry
are not changed in the same acceptance step.
