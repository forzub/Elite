# Navigation Planning Architecture

**Updated:** 2026-09-16  
**Status:** Navigation v2 architecture authority  
**Current implementation wave:** `NAV-V2-MAP-1` isolated NavigationMap API + CPU reference

## Decision: the previous live planner is not the v2 foundation

The previous runtime chain proved useful as an experiment but is rejected as the
architecture to scale to NPC traffic:

```text
GeometricPathPlanner
    -> route-wide TrajectoryGenerator / RuckigRoutePlanner
    -> dense route sampling
    -> route-wide obstacle validation
    -> GuidanceTunnel
```

Replacing the custom spline with Ruckig did not remove the dominant structural
cost. A successful six-point route still materialized roughly ten thousand
samples in one observed run, while synchronous docking/navigation work could
block the client update for hundreds of milliseconds. The problem is therefore
not "make the old route solver faster". Navigation v2 starts from a shared
spatial world, reduces the scene first, and performs expensive planning only on
the small relevant subset.

`SmoothPathOptimizer` remains retired. `GeometricPathPlanner`, the old route-wide
sampling path and current guidance plumbing are legacy/migration code, not v2
architecture authority. Do not build new NavigationWorld code around them.

Ruckig remains useful only as a possible final local kinematic primitive after a
route/local-avoidance layer has selected a target state. Ruckig is not a free-
space/path-search engine.

## Coordinate authority

Elite has multiple coordinate domains by design. Navigation v2 must not collapse
them into one universal frame.

### Authoritative system state

Long-lived simulation state remains in precise system/world coordinates. This is
the stable source of truth for replication, celestial motion and transforms
between independently moving domains.

### Active ship-centered NavigationWorld

The player spends ordinary flight in an active working space centered on the
ship/navigation domain. Navigation v2 therefore builds its dynamic working set in
**ship-centered NavigationWorld** coordinates.

The working origin may translate/rebase with the active ship/domain, but the
navigation axes must remain stable travel/system axes. They must not rotate every
time the hull rolls, pitches or yaws. Hull-local coordinates belong to flight
control, not to the shared spatial index.

### Hub-local space

A Hub owns its own authored/local geometry, docking ports and scheduled local
traffic. Hub-local data stays **Hub-local**. When the Hub becomes relevant to the
active ship, only the subset that can affect the active NavigationWorld is
transformed/published into that working space.

The same rule applies to other persistent local domains: station interior,
carrier, capital ship, settlement, etc. Their private navigation state is not
rebased around the player every frame.

## NavigationMap block boundary

`NavigationMap` is the first concrete v2 ownership boundary. It is intentionally a
separate block under:

```text
src/world/navigation/map/
```

The only production-facing header is:

```text
NavigationMap.h
```

The boundary is designed so storage/backend decisions cannot leak into gameplay,
planners, Hub code or presentation. Callers publish an authoritative snapshot by
value and receive only compact derived query products by value:

```text
authoritative snapshot
    position / velocity / acceleration
    active WorkingFrame
            |
            v
+---------------------------------------+
| NavigationMap                         |
|                                       |
| owns working-frame conversion         |
| owns actor storage                    |
| owns prediction cache                 |
| owns spatial cells/index              |
| owns future CPU/GPU backend state     |
+---------------------------------------+
            |
            +-> corridor candidate result
            +-> local sphere candidate result
            +-> aggregate stats/revisions
```

No caller receives references, pointers or views into actor tables, cells,
prediction caches or future GPU buffers. No Store/scene/render/Hub object is
retained inside the map. This is a strict copy/move-in, compact-result-out API.

The API owns the system/world -> ship-centered transform. Callers supply a
`WorkingFrame` in authoritative coordinates; the map converts points/vectors to
its stable local navigation basis internally. This avoids duplicating rebase
logic in every consumer.

The current `NAV-V2-MAP-1` implementation is a deterministic CPU reference. It is
not the final backend. Its purpose is to pin behavior and provide a correctness
oracle for a future GPU implementation without changing planner call sites.

Current internal reference representation:

```text
owned dynamic actor table
+ constant-acceleration conservative prediction
+ sparse 3D cell hash
+ corridor query
+ sphere query
```

The CPU reference has no fixed actor-per-cell correctness cap. Out-of-bounds and
rejected records remain explicit statistics.

The GPU backend must stay behind this exact block boundary. `NavigationMap.h`
must not include OpenGL/GLFW/render/game-state headers.

## NavigationWorld v2

The intended runtime authority is one shared data-oriented NavigationWorld for
the active domain, consumed by the player and NPC navigation systems. The
NavigationMap block is the spatial-data layer inside that authority.

```text
AUTHORITATIVE SYSTEM STATE
            |
            v
ship-centered NavigationWorld
    +------------------------------+
    | NavigationMap                |
    | static free-space/clearance  |
    | dynamic actor table P/V/A    |
    | dynamic spatial index        |
    | predicted swept bounds       |
    +------------------------------+
    | active route corridors       |
    | local conflict candidates    |
    +------------------------------+
            |
      +-----+-------------------+
      |                         |
      v                         v
mass-agent steering       precision local planner
/NPC navigation           docking / repair / special
      |                         |
      +-----------+-------------+
                  v
          temporary target state
                  |
             local motion
          (Ruckig candidate)
                  |
             flight control
```

The dynamic map is **not** a dense voxel field containing velocity and
acceleration in every cell. P/V/A remain actor-owned. Spatial cells contain only
indexing/occupancy information needed to find relevant actors.

## Static navigation representation

The ordinary render scene is not itself a navigation map. Render geometry answers
"what triangles exist"; navigation representation must answer "what free volume
can this agent occupy, with what clearance, and how are free regions connected".

The exact production representation is intentionally not selected before
benchmarking. Candidates include sparse voxel/octree bricks, convex free-space
cells and a hybrid authored/generated region graph. Required properties are:

- sparse/hierarchical representation rather than a uniform meter-scale universe;
- per-region clearance or an equivalent agent-envelope query;
- explicit connectivity/portals between free regions;
- local invalidation/rebuild when topology changes;
- ability to represent outside <-> inside passages and authored narrow routes;
- fast spatial query from a route corridor or local physical horizon.

Static station/hub geometry should be baked/cached. It must not be rediscovered
from hundreds of obstacle primitives for every ship route request.

`NAV-V2-MAP-1` does not pretend this decision is already made. Static
free-space/clearance is the next NavigationMap capability after the dynamic
boundary and backend measurements are accepted.

## Dynamic actor layer

Dynamic actors are kept as compact records such as:

```text
position
velocity
acceleration
angular velocity where needed
collision/navigation bounds
motion mode / flags
prediction confidence or route revision
```

A shared spatial index determines which actors can interact. No agent may scan
all actors in the scene as its normal update path.

Prediction is lazy in the final design. A detailed trajectory tube is generated
only for actors that can enter a selected route corridor/local horizon. Actors
outside that corridor are discarded before expensive space-time planning.

The current CPU reference uses a deliberately conservative first-stage envelope:

```text
p1 = p0 + v*T + 0.5*a*T^2
travel_bound = |v|*T + 0.5*|a|*T^2
swept_sphere = sphere(p0, radius + travel_bound)
```

This envelope is broadphase data, not the final trajectory tube.

## GPU ownership candidate

`NAV-V2-GPU-0` already provides an isolated OpenGL 4.3 compute benchmark for the
dynamic layer. It performs, in ship-centered coordinates:

```text
P/V/A actor table
    -> conservative future swept bounds
    -> 3D spatial bins
    -> corridor relevance filter
    -> all-agent neighbor/conflict candidate query
    -> tiny aggregate result / GPU-resident follow-up
```

The prototype deliberately uses a conservative swept sphere and fixed-capacity
3D cells so overflow and scaling are measurable. It does not claim this is the
final representation.

The next GPU experiment should implement the same externally visible
`NavigationMap` behavior rather than create another public GPU-specific API.
That gives a direct CPU-reference/GPU comparison and prevents backend details
from propagating through the game.

GPU work must be asynchronous/double- or triple-buffered in the final runtime.
A compute dispatch followed by synchronous full readback is not an acceptable
replacement for a CPU stall. The benchmark reads back only a tiny aggregate
statistics block.

## Performance contract

Navigation must fit inside the game, not consume the game frame.

Design targets:

| Work | Target |
| --- | ---: |
| Main-thread navigation CPU, typical | < 0.5 ms |
| Main-thread navigation CPU, normal peak | < 1.0 ms |
| GPU dynamic NavigationWorld update | < 1.0 ms preferred |
| GPU heavy dynamic scene | < 2.0 ms target ceiling |
| Full/precision route solve | async worker/GPU job; never synchronous frame blocker |

The 1 ms / 2 ms GPU values are design budgets, not portable test assertions.
Rendering competes for the same GPU. If 10k actors consume several milliseconds,
a CPU spatial-index baseline or a different GPU representation must be compared
before integration.

Different layers run at different rates. Physics/control may run at 60-120 Hz;
dynamic broadphase may run at 30-60 Hz; mass-agent avoidance can run at a lower
staggered rate; global route validity is rare/event-driven. A new global route is
not generated every frame and not automatically every second.

## Global route and local physical horizon

Navigation v2 separates distant intent from near-term physically relevant work.

A global route is a sparse corridor through connected free-space regions/portals.
It can be cached and invalidated by region revisions. It does not become a dense
trajectory to the final destination.

The local planner operates on a receding physical horizon, conceptually:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

Only the next section requires high-resolution dynamic reasoning. Farther route
regions remain coarse intent. This prevents route length from linearly exploding
per-frame collision/sample work.

For a moving docking target, terminal approach is a separate product. A docking
port may publish a moving final-approach curve/manifold and a moving entry state.
The route planner aims for that entry region/state; the terminal planner owns the
last approach. It is not necessary to regenerate the entire distant route every
time the terminal moves slightly.

## Dynamic obstacles and space-time planning

The normal sequence is:

```text
cached/global corridor
    -> NavigationMap corridor query
    -> predicted swept volumes / safe-time intervals for only returned actors
    -> local route/velocity correction
    -> execute first part
    -> repeat on a receding horizon
```

The production algorithm is not selected yet. Candidate families include
space-time/safe-interval planning for precision traffic and reciprocal local
avoidance/steering for large NPC populations. Selection must be based on isolated
benchmarks and actual game constraints, not on preserving old code.

Mass background NPCs do not need the same solver fidelity as a repair drone or
precision docking autopilot. Navigation LOD is allowed: cheap shared corridor +
local avoidance for ordinary traffic, precise space-time planning only for the
small number of agents that require it.

## Navigation, collision and damage are separate layers

Navigation answers **how not to collide**. Physics answers **whether/where a
collision actually occurred**. Damage answers **what the collision destroyed**.
They may share spatial broadphase data, but they do not share authority.

```text
NavigationWorld / NavigationMap
    conservative navigation envelopes / predicted conflicts

Physics / Collision
    broadphase candidates
    exact narrow phase / CCD / TOI / contacts

Damage / Structural
    hit ownership
    impulse/material consequence
    detach / breach / destruction
    local navigation invalidation
```

Render, collision, hit/damage and navigation geometry intentionally differ.
Navigation geometry may be more conservative than collision geometry; damage can
be semantically detailed without forcing navigation to process render triangles.

## Holes, breaches and detached fragments

A visible hole is not automatically free navigation space.

Small projectile holes normally affect render/hit/pressure state but do not alter
navigation topology. A larger breach becomes traversable only when its clearance
admits the requesting agent envelope plus margin. A repair drone may pass where a
ship cannot.

A topology-changing breach dirties only the affected static navigation region.
The rebuilt region may publish an explicit outside<->inside breach portal with
clearance metadata. The whole station map must not be rebuilt for one damaged
panel.

A detached semantic/structural fragment becomes a new dynamic rigid body and a
new NavigationMap actor. It enters the shared spatial index automatically and
can become an avoidance/collision candidate.

## Current gates

### NAV-V2-MAP-1

Implementation:

```text
src/world/navigation/map/NavigationMap.h
src/world/navigation/map/NavigationMap.cpp
src/world/navigation/map/CMakeLists.txt
src/world/navigation/map/README.md
tests/navigation_map/NavigationMapContractTests.cpp
tests/navigation_map/CMakeLists.txt
tests/navigation_map/run_mingw64.sh
tests/architecture_contracts/check_navigation_map_boundary.py
```

Required evidence:

- public API has no game/render/OpenGL/GLM dependency;
- PImpl hides cells/prediction/backend state;
- whole dynamic snapshot is owned after publication;
- system/world -> ship-centered transform happens inside the block;
- corridor and sphere queries return only compact candidates;
- rejected frame update does not mutate accepted state;
- sparse query path does not intentionally scan the full actor table;
- CPU reference contract passes in MinGW64.

### NAV-V2-GPU-0

The existing benchmark remains valid evidence for choosing dynamic backend
ownership:

```text
benchmarks/navigation_gpu/main.cpp
benchmarks/navigation_gpu/README.md
benchmarks/navigation_gpu/run_mingw64.sh
tests/architecture_contracts/check_navigation_gpu_benchmark.py
```

It evaluates deterministic `cruise` and dense `hub` distributions at 1k / 5k /
10k actors and measures GPU prediction/binning, corridor filtering, all-agent
candidate queries, CPU submission, memory, overflow and readback.

## Non-negotiable v2 rules

- NavigationMap is a strict block boundary; its internal data never becomes a
  cross-module shared structure.
- Ship-centered NavigationWorld is a working domain, not replacement for precise
  authoritative system state.
- Hub-local/private navigation remains Hub-local and publishes only relevant data
  into the active working domain.
- No per-agent full-scene scans.
- No synchronous route-wide dense sampling/validation on the frame thread.
- No fixed arbitrary obstacle-count cap as a correctness strategy.
- GPU acceleration must not introduce synchronous large readbacks.
- Static free space is cached/baked and locally invalidated, not rediscovered on
  every route request.
- Dynamic prediction is corridor/horizon filtered before expensive planning.
- Ruckig does not own obstacle topology.
- Guidance/HUD consumes accepted navigation products and never owns planning.
- Navigation, exact collision and damage remain separate authorities even when
  they share broadphase/spatial data.
- Collision/safety fidelity is not reduced to meet a timing budget; the data
  structure/algorithm/scheduling must change instead.
