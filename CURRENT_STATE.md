# Elite — CURRENT STATE

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Current public HEAD:** `32bb51739ee3512d25dbaa940f94fd3743152eb6`

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — CANDIDATE

## Latest target-machine result

Run on
`54bd19ef647f0eca8dcc738be4342052ee164683`:

```text
architecture PASS
EliteGame PASS
EliteServer PASS

obstacle_candidate=0
obstacle_conflict=0
configured_route_exact_block=1
exact_obstacle_block=0
exact_static_block=0
adjusted=0

exact_static=1
exact_static_obstacles=17
exact_static_query=1
exact_static_motion_samples=6000
exact_static_violation=0

reached_goal=1
remaining_goal_m=0.17053
```

Interpretation:
- ownership cleanup itself works;
- CUBE 08 stays out of NavigationMap;
- authored start->goal centerline intersects exact CUBE 08;
- real ship reaches goal safely;
- but runtime bounded nominal segment never records CUBE 08 as a static blocker.

The frame-axis contract was re-audited and is correct:
visual = X normal, Y radial, Z -prograde;
tactical = X prograde, Y radial, Z normal;
visual->tactical conversion {-z,y,x} is correct.

## Diagnostic candidate

Current candidate adds two independent proofs:

1. a navigation_runtime regression with live-scale numbers:
   - static OBB 1300 m ahead;
   - first physical horizon ~1420 m;
   - distant goal;
   - unrelated dynamic actor;
   - expected AdjustedClear from exact static geometry.

2. a first-live-plan exact probe before Planner::plan:
   - captures actual agent map position;
   - actual goal map position;
   - computed bounded target;
   - horizon distance;
   - exact blocking entity.

Server self-test now fails immediately if:
- the first live bounded segment does not hit exact CUBE 08; or
- the identical exact segment hits CUBE 08 but planner loses that static block.

No stationary conservative sphere has been reintroduced.
