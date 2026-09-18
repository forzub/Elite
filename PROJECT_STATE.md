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

### 12A-3 — CANDIDATE

`EliteServer --self-test-navigation` now supplies deterministic live evidence
from the real server runtime.

Important repair made before the live gate: candidate broadphase covers the
complete bounded local avoidance fan with `NavigationMap::querySphere()`,
instead of only the nominal straight corridor. Successful adjusted probes also
retain the identity of the obstacle that rejected the nominal target.

The self-test measures:
- nominal CUBE 08 conflict;
- adjusted target;
- exact executed lateral acceleration;
- straight-route deviation;
- minimum conservative clearance;
- obstacle-plane passage;
- goal progress;
- exact authoritative-to-replicated execution-vector equality.

After this passes, proceed to exact transformed HitVolume OBB publication into
static/precision NavigationSpace topology. Conservative spheres are broadphase
only and must not become the accepted narrow-gap geometry.
