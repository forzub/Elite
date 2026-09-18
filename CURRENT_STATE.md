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

Target-machine run on
`80b90a6abc43982f3c6da9f76e20a05b2cd25d46` produced mixed but useful
evidence.

### Passed

- architecture contract PASS;
- NavigationSpace 1/1 PASS;
- production EliteGame and EliteServer builds PASS;
- live server published 17 exact static obstacles;
- live planner queried them and observed nominal static blocking;
- ship passed the obstacle plane and made 6382.44 m of goal progress.

### Failures and interpretation

`navigation_local_avoidance` failed because a newly-authored dynamic fixture
used an invalid conservative swept radius smaller than the actor radius. The
production fail-closed validator behaved correctly; the fixture is corrected.

`navigation_runtime_planner` failed because exact same-region endpoint
clearance rejected the centre of a valid corridor portal. Portal centres lie on
the shared boundary by definition. The runtime now carries the already-proven
corridor portal clearance into the precision segment query; only that nominal
portal endpoint may touch the boundary. Adjusted probes remain strict.

The live self-test failed only because the old
`minimumConservativeClearanceMeters > 0` condition was still treated as
acceptance truth. It reported `-128.338 m`, meaning the ship entered the
conservative enclosing sphere. Under 12A-4 that sphere is broadphase only and
may overlap free space outside the real OBB.

### Corrected acceptance

Current candidate:

```text
a40d41bcfff4aecee94b495c0c5dd64223c7fe2b
```

The server now sweeps every actual fixed-step motion segment against the exact
HitVolume static layer. Live acceptance requires:

```text
exact_static=1
exact_static_obstacles>0
exact_static_query=1
exact_static_block=1
max_exact_static_examined>0
exact_static_motion_samples>0
exact_static_violation=0
```

`min_conservative_clearance_m` remains printed as a broadphase diagnostic and
is no longer an exact-static safety verdict.

Stage remains **12A-4 CANDIDATE** pending the corrected target-machine gate.
