# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** isolated runtime navigation backend  
**Stage:** NAV-RUCKIG-1B — live corridor regression / Hub-map cleanup re-acceptance

## Non-negotiable architecture

The custom spline navigator is retired. Runtime responsibilities are:

```text
GeometricPathPlanner      -> collision-free coarse spatial topology
RuckigRoutePlanner        -> physically bounded motion along that topology
RuckigTrajectorySolver    -> state-to-state primitive
NavigationObstacleGeometry-> swept safety authority
GuidanceTunnel            -> presentation only
```

Do not restore `SmoothPathOptimizer` to fix this stage.

## What passed

The user locally validated the previous focused compile gate:

- all four Ruckig/live architecture scripts PASS;
- `ruckig_route_planner` PASS;
- `guidance_tunnel_local_horizon` PASS;
- `EliteGame` MinGW compile/link PASS.

## What failed in the real game

Runtime immediately showed:

```text
НАВИГАЦИЯ НЕДОСТУПНА [Ruckig leg leaves the collision-free coarse corridor]
```

The Hub diagnostic scene also still had two rejected presentation defects:

1. 16 stress objects were arranged as a wall/corridor across player -> dock;
2. Hub Map still rendered permanent object names instead of hover-only labels.

Therefore the previous compile gate is not runtime acceptance.

## Root cause of the Ruckig corridor failure

`RuckigTrajectorySolver` previously solved directly in world/planning XYZ.
Ruckig synchronizes independent scalar DoFs; consequently a diagonal
rest-to-rest move can spatially bow away from the straight coarse segment even
when both endpoints are valid. `NavigationObstacleGeometry` correctly rejected
that bow as leaving the collision-free corridor.

This is a coordinate-seam error: the geometric planner owns the path. Ruckig
must not make arbitrary world axes redefine that path.

## Current fix

Every Ruckig state-to-state solve now uses a deterministic leg-aligned
`MotionBasis`:

```text
coarse leg / relative state displacement -> local +X
transverse basis                         -> local Y/Z
world states/gravity deltas              -> local basis
Ruckig<3> solve                          -> local basis
samples                                  -> world/planning frame
swept obstacle validation                -> unchanged
```

Added focused regression:

```text
diagonal stopped leg stays on coarse chord
```

It requires every sample of a diagonal rest-to-rest leg to remain within
`1e-5 m` of the requested coarse chord.

## Hub diagnostic field

The old four route-crossing bands are deleted. The same 16 deterministic stress
objects now occupy two staggered shells around the Hub with vertical variation.
The +/-X docking service axes stay open and several routes through the field are
possible.

## Hub Map label policy

Persistent names are removed. One generic rule applies to all shared overlay
objects on Hub Map:

```text
not hovered -> no text
hovered     -> one semi-transparent name above the object
```

The renderer chooses one nearest/highest-priority visible object. Infrastructure,
ships and the Hub reference use the same path.

## Re-acceptance commands

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py
python tests/architecture_contracts/check_ruckig_live_navigation.py
python tests/architecture_contracts/check_navigation_stress_field.py
python tests/architecture_contracts/check_live_docking_guidance.py

bash tests/navigation_guidance/run_ruckig_mingw64.sh
cmake --build build --target EliteGame
```

Then run the same Hub scenario and verify:

1. obstacles visibly surround the station instead of forming a line/wall;
2. labels are absent at rest and appear semi-transparently only under mouse;
3. CALCULATE ROUTE becomes available and produces a tunnel;
4. no repeated one-second navigation stalls return.

If route generation still fails, capture the exact UI failure text plus the
single `[RuckigRoutePerf]` line from the announced `navigation_perf.log`. Do not
reduce obstacle clearance or disable swept safety to force acceptance.

## After this runtime gate

Only after the live Hub scenario passes:

1. remove the remaining test-only `SmoothPathOptimizer` shim/source/CMake entry;
2. migrate the obsolete aggregate B-spline assertion;
3. benchmark 1 / 5 / 20 / 100 navigation agents on the shared backend;
4. move genuinely heavy invalidated route solves to worker/latest-wins if the
   measured main-thread budget still requires it.
