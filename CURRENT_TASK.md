# Elite — CURRENT TASK

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-SPACE-1` — persistent static free-space / clearance / connectivity

## Source-of-truth rule

`main` is the only game-development baseline. Read and obey `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not continue feature work on parallel `chatgpt/*`, rescue, staging or other branches.

## Previous gate — accepted

`NAV-V2-MAP-2` is closed.

Target-machine evidence includes:

```text
NAVIGATION MAP BOUNDARY CONTRACT: PASS
navigation_map: 1/1 PASS
```

Accepted 100-iteration CPU results at 10k actors:

```text
cruise: rebuild 2.7892 / 3.0958 ms med/p95
        corridor 0.0552 / 0.1713 ms
        sphere   0.0346 / 0.1329 ms
hub:    rebuild 2.1775 / 2.4924 ms med/p95
        corridor 0.0328 / 0.1551 ms
        sphere   0.0324 / 0.0471 ms
```

Accepted Quadro RTX 3000 GPU 100-iteration results at 10k actors:

```text
cruise: bin 0.0106 ms, neighbor 0.6737 ms, total 0.6840 ms, p95 1.3226 ms
hub:    bin 0.0105 ms, neighbor 1.5991 ms, total 1.6097 ms, p95 1.6258 ms
```

GPU correctness/structure:

```text
reference_ok=1 for both 1k scenarios
overflow=0
out_of_bounds=0
valid=1
readback_bytes=32
10k memory≈16.6321 MiB
```

The GPU GLSL identifier repair is canonical on `main`:

```text
48c8dbfaefc1a2bda82af762f46a5b38400033ac
```

Raw GPU evidence is recorded in `benchmarks/navigation_gpu/RUN_LOG.md`.

## Backend decision — HYBRID

Accepted ownership direction:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

The public `NavigationMap` boundary remains backend-neutral. GPU scheduling must be asynchronous/double- or triple-buffered; no frame-thread synchronous wait/bulk readback.

## Active objective — `NAV-V2-SPACE-1`

Build the static spatial half of NavigationWorld in isolation before live integration.

The stage must answer these questions in code/tests, not prose only:

1. how static obstacles/free regions are represented sparsely;
2. how an agent envelope requests clearance;
3. how traversable regions connect through portals/narrow passages;
4. how a local damage/geometry change invalidates only a bounded portion of the structure;
5. how a coarse global corridor is returned without materializing a dense full trajectory;
6. how outside↔inside transitions are represented for stations/carriers/interiors;
7. how the static space shares the same ship-centered working-frame contract without duplicating NavigationWorld state.

## First implementation slice

Create a backend-neutral static-space block under a dedicated navigation namespace/directory, analogous to the `NavigationMap` ownership boundary.

Minimum public concepts:

```text
StaticSpace / NavigationSpace
WorkingFrame or compatible frame identifier
AgentEnvelope / clearance radius
RegionId / PortalId
Region
Portal
StaticSpaceUpdate / local invalidation revision
PointLocation or containing-region query
Clearance query
Coarse corridor query -> ordered regions/portals
Stats / diagnostics
```

The exact names may differ, but the ownership contract must hold:

- public headers do not expose renderer/OpenGL/game-state dependencies;
- internal acceleration structures are private;
- callers publish authored/runtime static geometry through an explicit boundary;
- callers receive compact route/topology products, not internal cell arrays;
- queries are deterministic for the CPU reference implementation.

## Reference representation requirements

Do not start with a giant dense system voxel field.

The first CPU reference should favor a sparse/hierarchical or region/portal representation that can express:

```text
large open volumes
station exterior obstacles
interior rooms/corridors
narrow docking/repair passages
breaches/openings after damage
clearance by agent size
```

A hybrid static representation is allowed. Region/portal topology may sit above local sparse clearance geometry if that gives better scaling and authoring/runtime behavior.

## Required tests for the first slice

At minimum:

```text
open-space point is traversable
solid obstacle rejects insufficient clearance
same geometry admits/rejects different agent envelopes
connected regions return deterministic coarse corridor
disconnected regions return no route
narrow portal rejects oversized agent
local invalidation increments revision without full-world semantic reset
outside -> interior portal path works
public boundary has no GLM/OpenGL/GLFW/render/game dependency
```

Add a dedicated architecture contract for `NAV-V2-SPACE-1` and a standalone MinGW64 test runner consistent with the NavigationMap test layout.

## Not in this slice

Do not yet:

- wire the new static space into `EliteGame` / `EliteServer`;
- repair the old route-wide planner;
- implement final mass-NPC steering;
- implement precision docking SIPP/Theta*;
- render the Shift+F12 debug view;
- delete migration navigation code.

Those follow after the isolated static-space boundary and behavior are accepted.

## Performance contract

Keep current targets:

```text
main-thread navigation CPU       < 0.5 ms typical, < 1.0 ms normal peak
GPU dynamic NavigationWorld      < 1.0 ms preferred, < 2.0 ms heavy-scene target
precision/global route solve     async only
```

Static-space rebuild/invalidation may run asynchronously; ordinary clearance/region/corridor queries must be bounded and cheap enough for shared NavigationWorld use.

## Completion gate for `NAV-V2-SPACE-1`

The stage is not complete until:

1. public static-space API/ownership contract exists;
2. deterministic CPU reference implementation exists;
3. architecture + behavioral tests pass on the target machine;
4. local invalidation behavior is demonstrated;
5. narrow-portal/agent-envelope behavior is demonstrated;
6. a small standalone benchmark or diagnostics proves query/invalidation scale is plausible;
7. `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, `NAVIGATION_WORLD_V2.md` and project-context docs are synchronized.
