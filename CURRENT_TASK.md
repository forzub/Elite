# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-MAP-2` — isolated NavigationMap CPU/GPU comparison before live integration

## Accepted direction

Navigation v2 is no longer built around the legacy route-wide synchronous pipeline.
The active runtime navigation domain is ship-centered; Hub keeps its own local
space and publishes only the subset that becomes relevant to the active ship
NavigationWorld.

Canonical architecture:

```text
src/world/navigation/NAVIGATION_PLANNING_ARCHITECTURE.md
```

## Whole-game space decision — accepted

The navigation design must follow where gameplay actually happens, not the Hub
visualization coordinate system.

The accepted domain split is:

```text
AUTHORITATIVE SYSTEM/WORLD STATE
        |
        | transform only at domain boundary
        v
SHIP-CENTERED NAVIGATIONWORLD
    origin follows/rebases with active ship/domain
    axes remain stable system/travel/navigation axes
    nearby static navigation geometry
    dynamic actors { P, V, A, bounds, flags, revision }
    predicted swept volumes
    active route corridors / local horizon
```

Important consequences:

- ship-centered is a **working navigation frame**, not replacement for precise
  system/world authority;
- the origin may translate/rebase with the ship, but the working axes do not roll,
  pitch or yaw with hull attitude;
- hull-local coordinates remain execution/flight-control detail;
- ordinary deep-space flight does not inherit Hub-local axes or Hub ownership;
- Hub is a private local domain for station geometry, docking ports and scheduled
  local bots;
- when Hub interaction becomes relevant, only the Hub subset that can affect the
  active ship is transformed/published into the ship NavigationWorld;
- the same ownership rule applies to other persistent local domains such as a
  carrier, capital ship, settlement or interior.

## Shared NavigationWorld — accepted

Do not give every NPC an independent full-world planner/full-scene scan.

The intended runtime product is one shared, data-oriented NavigationWorld for the
active domain. Player and NPC navigation consume the same spatial/prediction
layer. The shared world reduces the scene first; individual steering/precision
planning receives only compact relevant candidates and temporary target states.

Conceptually:

```text
                    SHIP NAVIGATIONWORLD
                            |
               +------------+------------+
               |                         |
         STATIC SPACE              DYNAMIC ACTORS
       free-space/clearance          P/V/A/bounds
               |                         |
               +------------+------------+
                            |
                  spatial index/prediction
                            |
                   compact conflict set
                            |
             +--------------+---------------+
             |                              |
      mass NPC steering             precision planner
      cheap local targets            docking/repair
             |                              |
             +--------------+---------------+
                            |
                     temporary target
                            |
                    local kinematics
                            |
                      flight control
```

## NavigationMap module boundary

The separate block is now under:

```text
src/world/navigation/map/NavigationMap.h
src/world/navigation/map/NavigationMap.cpp
src/world/navigation/map/CMakeLists.txt
src/world/navigation/map/README.md
```

The public API is intentionally narrow.

Ingress is one owned snapshot by value:

```text
DynamicWorldUpdate
    sourceRevision
    WorkingFrame
    actors[] { id, P, V, A, radius, flags, motionRevision }
```

`NavigationMap` transforms and owns all internal data. Actor tables, prediction
cache, sparse cells and future GPU backend state never cross the module boundary.
The public header has no GLM/OpenGL/GLFW/game/scene dependency and uses PImpl.

Egress is only compact derived data by value:

```text
queryCorridor()
querySphere()
stats()
```

Planners receive only relevant candidates plus revisions/diagnostics; they do not
receive internal map storage.

## Current CPU reference backend

The behavior oracle currently provides:

```text
constant-acceleration endpoint prediction
conservative swept sphere
sparse 3D cell hash
corridor broadphase + conservative sphere/segment test
sphere/local broadphase + conservative sphere/sphere test
```

No fixed actor-per-cell correctness cap is used.

The standalone acceptance layer is:

```text
python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
```

User MinGW64 evidence on 2026-09-16:

```text
navigation_map: 1/1 PASS
100% tests passed
```

This confirms the current CPU reference contract compiles/runs on the target
machine. It is not yet the CPU/GPU performance decision.

## GPU role — accepted candidate, not production decision yet

GPU acceleration is appropriate for the massively parallel reduction stages:

```text
nearby navigation geometry -> local occupancy/free-space support data
actor P/V/A table           -> prediction / conservative swept bounds
dynamic actors              -> spatial hash/binning
active corridor/horizon     -> relevant-actor filtering
all active agents           -> neighbor/conflict candidate pairs
```

Do **not** store velocity/acceleration densely in every spatial cell. Dynamic
state remains actor-owned. A dense `256^3` field already contains ~16.8 million
cells; six floats for V+A alone are roughly 384 MiB before occupancy, clearance,
IDs and topology. `512^3` multiplies that by eight. The map therefore remains
sparse/hierarchical and cells only index spatial occupancy/relevance.

Single-agent A*/Theta*/SIPP is not the first GPU target. Those searches are
branch-heavy and use irregular priority/frontier access. First use GPU compute to
reduce the candidate set; keep precision/global graph search on an asynchronous
CPU worker unless batched measurements later prove otherwise.

### No synchronous GPU wait

A GPU backend must never replace a CPU hitch with:

```text
dispatch -> barrier/fence -> blocking readback -> continue frame
```

Production scheduling must be double/triple buffered:

```text
frame N:   submit NavigationWorld update N
frame N+1: consume last completed result; submit N+1
```

Detailed products should remain GPU-resident where useful or cross to CPU only as
bounded asynchronous compact results. Any result latency is covered by the local
physical safety horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

## Current CPU/GPU measurement block

CPU benchmark:

```text
benchmarks/navigation_map/CMakeLists.txt
benchmarks/navigation_map/main.cpp
benchmarks/navigation_map/run_mingw64.sh
benchmarks/navigation_map/README.md
```

GPU compute prototype:

```text
benchmarks/navigation_gpu/
```

Both use deterministic `cruise` / `hub` classes at 1k / 5k / 10k actors. The
comparison must measure at least:

```text
CPU publication/rebuild median + p95
CPU corridor/local query median + p95
GPU compute median + p95
CPU submission cost
candidate/conflict counts
cells visited / actors examined
GPU/CPU memory
cell overflow/rejection diagnostics
readback bytes
```

The CPU benchmark mirrors the GPU scenario classes:

```text
cruise: 1k / 5k / 10k actors, 18 km spawn cube, 250 m/s, 8 m/s^2
hub:    1k / 5k / 10k actors,  7 km spawn cube, 120 m/s, 6 m/s^2
prediction horizon: 3 s
```

Run from MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_navigation_map_boundary.py
bash tests/navigation_map/run_mingw64.sh
bash benchmarks/navigation_map/run_mingw64.sh
bash benchmarks/navigation_gpu/run_mingw64.sh
```

Optional longer CPU run:

```bash
bash benchmarks/navigation_map/run_mingw64.sh --warmup 10 --iterations 100
```

## Backend decision gate

Use measured evidence rather than committing to GPU in advance:

- if CPU corridor/local queries are comfortably below the frame budget and rebuild
  cadence can be asynchronous/incremental, retain CPU ownership initially;
- if CPU publication/query work becomes expensive at 5k/10k while the compute
  prototype stays inside the GPU budget, add a GPU backend behind the same API;
- a hybrid is allowed: CPU owns topology/precision graph search while GPU owns
  actor prediction, binning and conflict reduction;
- never expose GPU buffers/cells/actor tables through the module boundary;
- never require synchronous bulk GPU readback for normal navigation.

## Performance contract

```text
main-thread CPU navigation work          < 0.5 ms typical
                                          < 1.0 ms normal peak
GPU dynamic NavigationWorld update       < 1.0 ms preferred
                                          < 2.0 ms heavy-scene target
full/precision route solve               async only; never frame-thread blocking
```

The GPU budget is a design target, not a portable hard assertion: rendering uses
the same GPU.

## Ordered next work

1. Keep the accepted `NavigationMap` API/CPU reference as correctness oracle.
2. Measure `benchmarks/navigation_map` on the user's MinGW64 machine.
3. Measure the existing compute prototype on the same 1k/5k/10k classes.
4. Compare CPU/GPU/hybrid cost, memory, candidate reduction and transfer cost.
5. Choose the internal dynamic backend **without changing the public
   `NavigationMap` API**.
6. Start `NAV-V2-SPACE-1` inside the same module boundary:
   static free-space / clearance, connectivity/portals, local invalidation and
   agent-envelope queries.
7. Only after these isolated gates integrate NavigationWorld into the live game.
8. Replace legacy synchronous route-wide work incrementally; no heavy periodic or
   frame-thread route solve is allowed in the v2 live path.
