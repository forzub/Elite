# Navigation route stress: clutter and forced turns

Deterministic top-down scene: 10 × 10 capsule obstacles, 500 ships entering
from the left and aiming for different positions on the right. Each query sees
the same 100 obstacles and uses the current `GeometricPathPlanner` with a
configurable per-route obstacle limit (production default: 32). Ship requests
are generated outside the timed region; the planner's own obstacle selection,
support nodes, visibility checks and A* remain inside it.
`full_scene_clear` separately checks the returned segments against all 100
barrels, including the ones the route planner did not select.

Compile in a checkout with GLM headers available:

```bash
g++ -std=c++17 -O2 -DNDEBUG -I. -I/path/to/glm \
    benchmarks/navigation_route_stress/RouteStress.cpp \
    src/world/navigation/GeometricPathPlanner.cpp \
    src/world/navigation/NavigationObstacleGeometry.cpp \
    src/game/navigation/OrdinaryPhysicalManeuverCompiler.cpp \
    -o route_stress
./route_stress 500 32
./route_stress 500 32 dogleg
./route_stress 500 32 open chain
./route_stress 500 32 open_separated chain
./route_stress 500 32 dogleg chain
./route_stress 500 32 barrels chain
```

The second scene has four tall static walls: two offset barriers with opposite
openings and upper/lower boundaries. It forces a top-down S turn and keeps the
same 500 independent starts/goals. It measures a corner-heavy case separately
from the 100-barrel throughput case. These are 500 **independent route queries**,
not simultaneous dynamic collision avoidance for 500 moving ships.
The `open_separated` control uses 25 × 20 parallel lanes in three dimensions,
30 m apart. Its 500 starts and 500 goals are distinct and do not overlap under
the 13 m geometric hull radius. Ship-to-ship motion is still outside this
benchmark; the control isolates the terminal-state calculation.

Run 500/100 as an additional worst-case comparison only with an explicit
time budget: the visibility search is dense in support-node pairs. The result
measures route-construction latency and geometric route coverage, **not**
physical feasibility, optimality or flight time. Later physical planners must
reuse the same starts, goals, obstacles, vehicle and terminal contracts and
report feasibility, simulated arrival time, collision/rejection counts, and
their own latency distributions. A route that hits an unconsidered obstacle
must not be counted as safe merely because the per-query obstacle limit is 32.

The benchmark also times one `OrdinaryPhysicalManeuverCompiler` request per
valid route: initial 20 m/s along +X, first non-start waypoint as target,
20 m/s desired velocity toward it, 4-second horizon, Newtonian capability
numbers from the current Cobra physical test fixture. This is **only the first
unproved primitive**. By default no subsequent corner is chained. The optional
`chain` diagnostic transfers the exact B5 terminal sample position, velocity,
body basis and angular velocity to the next route-point query. It tries a
bounded speed/horizon frontier, checks sampled center segments against the
whole scene, and requires an 8 m capture radius at each waypoint. The terminal
contract is <=2 m/s speed and +X hull direction within 10 degrees. The `open`
scene has no obstacles and isolates arrival quality.

This bounded greedy point-chain is not the production coordinator, a corridor
search, literal actuator simulation, or continuous swept-hull proof. Failure
cannot establish that a better speed schedule, turn region or alternate
corridor is impossible. It shows where this specific point-following attempt
runs out of options.

## Local measurement (Linux container, g++ -O2, 2026-09-23)

| Scene | Ships | Route wall time | Route p50 | Route p95 | Full-scene-clear | Turns >45° | First B5 candidates |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 100 barrels, 32-obstacle cap | 500 | 17,074.610 ms | 31.941 ms | 63.243 ms | 500/500 | 0 | 500/500 unproved |
| Four-wall dogleg | 500 | 37.596 ms | 0.061 ms | 0.133 ms | 500/500 | 1,000 | 500/500 unproved |

The original measurement used a 5 m agent radius; this was inconsistent with
the active Cobra 13 m geometric projection and was superseded. The table uses
the corrected radius. The dogleg has 153 bends over 90° across 142 routes;
its first physical probe still reports 500 unproved candidates because it has
not reached any later corner. First-probe p50/p95 are 0.009/0.011 ms for the
barrels and 0.002/0.002 ms for the dogleg. These times **do not** represent
full physical route planning. The 500-ship results are single sequential cold
batches on this container, not target MinGW results,
not an FPS claim, and not evidence that all possible 32-obstacle subsets are
safe. Physically proved flight and complete alternative-planner cost are still
unmeasured.

## Chained diagnostic (same local compiler, 2026-09-23)

Every ship still gets an unproved first candidate. Chaining through all route
points produces a different result:

| Scene | Geometric routes | Final position reached | Full terminal state | Sampled-clearance failures | Chain p95 |
| --- | ---: | ---: | ---: | ---: | ---: |
| Open, no obstacles | 500/500 | 500/500 | **0/500** | 0 | 0.345 ms |
| Four-wall dogleg | 500/500 | 25/500 | **0/500** | 178 | 1.047 ms |
| 100 barrels | 500/500 | 13/500 | **0/500** | 406 | 3.884 ms |

Empty space: all 500 miss final speed (median 5.110 m/s; p95 12.409 m/s),
448 miss body orientation. Dogleg: 177 no-candidate, 178 sampled-contact and
120 no-progress stops. Barrels: 79 no-candidate, 406 sampled-contact and two
no-progress stops. A sampled-clearance failure rejects the diagnostic attempt;
passing that check would still not prove continuous oriented-hull clearance.

## Input-contract audit and separated control (2026-09-24)

The original random fixtures do give 500 distinct starts and 500 distinct
goals, but they place every start on one line and every goal on another. With
two 13 m-radius hulls overlapping below 26 m separation, the 100-barrel
fixture has 10,401 overlapping start pairs and 10,077 overlapping goal pairs;
the 140 m-wide dogleg/open fixture has 41,706 and 41,926 respectively. These
are **independent workload queries**, not a valid simultaneous 500-ship
traffic scene. Earlier replies describing their outcomes as ships physically
flying together were wrong.

`open_separated` repairs this control: zero coincident or overlapping start
and goal pairs, minimum separation 30 m. All 500 geometric routes are direct,
all 500 point-chain attempts enter the final 8 m position sphere, but **0/500**
meet the <=2 m/s terminal speed (median 19.214 m/s). Body orientation is
correct in all 500. Thus the empty-space terminal-speed failure is reproducible
with distinct, adequately separated endpoints. It belongs to the greedy B5
diagnostic/terminal authoring, not to shared destinations, traffic control or
an intrinsic limitation of A*.
