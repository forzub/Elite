# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** 12A-4 — exact static HitVolume OBB / narrow-passage geometry

## Progress

```text
[██████████████████████░] 11 / 12 major stages closed

1–11 ACCEPTED
12 ACTIVE
   12A-1 live NavigationWorld composition seam       ACCEPTED
   12A-2 authoritative GameSimulation proving actor  ACCEPTED
   12A-3 live CUBE 08 behavior proof                 ACCEPTED
   12A-4 exact static HitVolume OBB layer             CANDIDATE
```

## 12A-2 acceptance

Target-machine evidence on:

```text
7b4db95788d80c95afb3b57c109c671cb7a41366
```

passed:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
navigation_runtime 3/3 PASS
EliteGame build PASS
EliteServer build PASS
```

This accepts the authoritative `GameSimulation` wiring, isolated Active lab
actor, real NAV STRESS hit-volume source, and shared client/server production
build.

## 12A-3 acceptance

Target-machine acceptance on:

```text
9246530e5eb539e227a1af69461113f2b30ec93a
```

passed:

```text
NAVIGATION STAGE 12 RUNTIME PLANNER CONTRACT: PASS
navigation_runtime 3/3 PASS
EliteGame build PASS
EliteServer build PASS
```

Live authoritative evidence:

```text
simulated_s=58.9
plans=2945
executions=2945
obstacle_candidate=1
obstacle_conflict=1
adjusted=1
conflict_hold=0
lateral_exec=1
max_lateral_demand_mps2=11.4807
max_applied_accel_mps2=37.4859
max_applied_lateral_accel_mps2=5.50595
max_relative_speed_mps=59.8769
max_route_deviation_m=740.715
min_center_distance_m=664.211
min_conservative_clearance_m=129.59
progress_m=3502.46
passed_obstacle_plane=1
replication_error_mps2=0
canonical_replication_error_mps2=0
```

This accepts the non-identity working-frame conversion, real authoritative
avoidance behavior, physically bounded speed/acceleration, positive obstacle
clearance and exact same-tick sparse/canonical replication truth.

## 12A-4 candidate

Current candidate adds an exact persistent static geometry layer to
`NavigationSpace`:

```text
HitVolume OBBs
 -> NavigationHitVolumeAdapter
 -> map-frame NavigationObstacle boxes
 -> NavigationSpace exact point/segment queries
 -> LocalAvoidance nominal + adjusted static segment proof
```

The key regression deliberately creates a narrow OBB aperture whose
conservative enclosing spheres overlap the centreline. A fitting envelope must
pass the exact OBB gap; an oversized envelope must fail.

The live NAV STRESS lab now publishes real non-self-rotating HitVolume OBBs
only after authoritative hub/object transforms and HitVolume rebuilds are
current for the fixed step. The server self-test requires:

```text
exact_static=1
exact_static_obstacles>0
```

before the existing CUBE 08 physical/replication evidence can PASS.

Conservative static spheres remain in NavigationMap for this intermediate gate
so 12A-3 behavior is not changed simultaneously with the precision-geometry
publication. After 12A-4 is accepted, the next slice can remove static objects
from the dynamic candidate layer and reserve NavigationMap for genuinely
dynamic/moving actors.
