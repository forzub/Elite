# NavigationWorld v2 — ship-centered runtime navigation architecture

**Status:** current architecture contract  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`  
**Current stage:** `NAV-V2-LOCAL-1` — dynamic conflict + local receding horizon

Repository/branch authority is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. `main` is the only canonical game-development branch.

## 1. Decision summary

Navigation v2 uses one shared **NavigationWorld** for the active play area. It is not a complete independent navigation world/planner per NPC.

```text
AUTHORITATIVE SYSTEM/WORLD STATE
            |
            v
ship-centered NavigationWorld
    + static NavigationSpace
    |   free-space / clearance / portals / corridor
    |
    + dynamic NavigationMap
    |   P/V/A / prediction / swept bounds / bins
    |   compact relevant/conflict candidates
    |
    + local horizon consumer
        bounded conflict assessment
        temporary target state
            |
            v
    RuckigTrajectorySolver / flight control
```

Accepted hybrid ownership:

```text
CPU
    persistent static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    deterministic precision/local reference work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

The public boundaries remain backend-neutral. No backend-specific actor tables, cells, graph nodes or GPU buffers cross them.

## 2. Coordinate contract

Authoritative physical/system state and navigation working space are separate.

The NavigationWorld origin may translate/rebase with the active ship/domain. Its axes are stable navigation/travel axes and do **not** roll/pitch/yaw with the hull. Hull-local coordinates belong to flight control; render/player-relative coordinates belong to presentation.

Hub/station/carrier/interior geometry remains owned by its local domain. Only the relevant static/dynamic subset is transformed and published into the active ship-centered NavigationWorld.

## 3. Accepted dynamic boundary — `NAV-V2-MAP-2` CLOSED

Production API:

```text
src/world/navigation/map/NavigationMap.h
```

Ingress is a whole dynamic snapshot with source revision, `WorkingFrame`, and actors carrying P/V/A/radius/flags/revision. Egress is compact `queryCorridor()` / `querySphere()` candidate data by value.

`NavigationMap::Candidate` contains map-space P/V/A, predicted endpoint, conservative swept sphere, radius, flags and motion revision. Internal cells and backend state remain private.

Accepted target-machine evidence at 10k actors:

```text
CPU compact corridor/sphere queries <0.2 ms p95
GPU cruise total median 0.6840 ms, p95 1.3226 ms
GPU hub    total median 1.6097 ms, p95 1.6258 ms
```

No synchronous frame-thread dispatch/wait/bulk-readback path is allowed. Dynamic results are asynchronous/double- or triple-buffered; result age contributes to physical safety margin.

## 4. Accepted static boundary — `NAV-V2-SPACE-1` CLOSED

Production API:

```text
src/world/navigation/space/NavigationSpace.h
```

Accepted capabilities:

- sparse free-space regions + explicit portals;
- agent-envelope clearance;
- traversable apertures/tunnels/canyons;
- deterministic topology and costed corridors;
- local fail-closed invalidation + transactional patch;
- static turn-cost semantics preserving arrival direction;
- private dense/BVH acceleration behind the same public API.

Accepted 10k reference evidence includes:

```text
coarse BFS corridor             ≈7.9-8.0 ms
automatic point p95             <=0.0353 ms
bounded invalidation p95        <=0.0115 ms
costed corridor v1 p95          <=9.6103 ms
```

Positive static turn penalty uses semantic state:

```text
(RegionSlot, incoming PortalId)
```

Final target-machine turn acceptance on commit `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

The pinned gate was `<=40 ms p95`, so `NAV-V2-SPACE-1` is closed. Tree-backed expanded state and weak Euclidean A* remain rejected historical experiments. No ship velocity, braking, traffic or pursuit state belongs in persistent static cost.

## 5. Current stage — `NAV-V2-LOCAL-1`

The current task is the composition boundary between accepted static intent and accepted dynamic reduction.

```text
cached static corridor / nominal local target
        +
NavigationMap compact candidates
        +
agent P/V/A + envelope
        +
completed-result age / latency budget
        |
        v
bounded local conflict assessment
        |
        v
receding-horizon temporary safe target state
        |
        v
RuckigTrajectorySolver
        |
        v
flight control
```

The local layer does **not** own another NavigationWorld, does not scan the full scene and does not expand the global route into thousands of samples. Per-agent work starts after shared reduction has produced compact candidates.

### Local horizon rule

Baseline physical horizon:

```text
D >= v*T_latency + v^2/(2*a_brake) + turn_distance + safety_margin
```

`T_latency` includes asynchronous snapshot/result age. Higher-fidelity relative-motion models may replace the baseline later, but stale-result age may never be ignored.

The first deterministic reference uses bounded relative-motion conflict assessment over compact candidates. It outputs a compact temporary target/safety product, not a trajectory.

### Fail-closed ownership

If a safe local target cannot be demonstrated, the local layer reports a fail-closed result. It must not invent free space from render geometry or bypass `NavigationSpace`/`NavigationMap` authority.

Pursuit is a later consumer: moving target P/V/A -> bounded intercept prediction -> reuse valid coarse branch -> local horizon. Pursuit-specific prediction is not baked into generic conflict logic.

## 6. Legacy navigation status

The previous chain is migration code:

```text
GeometricPathPlanner
 -> route-wide TrajectoryGenerator / RuckigRoutePlanner
 -> dense route sampling
 -> route-wide obstacle validation
 -> GuidanceTunnel
```

Existing `TacticalCollisionMonitor` and `SmallCraftNavigation` are also pre-v2 GLM/old-state implementations. Their algorithms may inform tests/reference math, but the v2 local boundary must remain backend-neutral and must not depend on their old scene/contact ownership.

Ruckig remains useful only downstream after the local navigation layer has selected an accepted temporary target state.

## 7. Performance contract

```text
main-thread navigation CPU       <0.5 ms typical
                                 <1.0 ms normal peak
GPU dynamic NavigationWorld      <1.0 ms preferred
                                 <2.0 ms heavy-scene target
full/precision global solve      asynchronous only
```

These are design budgets, not portable assertions. Numerical acceptance claims come from the user's target-machine output.

Different layers may run at different rates. Global corridor validity is revision/event driven; local physical avoidance is receding-horizon; mass-NPC work may be staggered.

## 8. Navigation / collision / damage boundary

```text
Navigation
    conservative envelopes / free-space / predicted conflicts

Physics / Collision
    broadphase candidates -> exact narrow phase / CCD / TOI / contacts

Damage / Structural
    semantic hit ownership -> detach / breach / destruction
    -> local static-space invalidation
```

A detached fragment becomes a dynamic NavigationMap actor. A topology-changing breach changes NavigationSpace only when the opening has sufficient clearance for the requesting envelope.

## 9. Guidance/debug contract

Manual guidance visualizes the accepted corridor/trajectory actually used by navigation/control. It must not run an unrelated planner.

Ordinary `F12` keeps Hub/local presentation. `Shift+F12` toggles Hub render <-> raw NavigationWorld Debug. Debug consumes the same completed NavigationWorld snapshot used by navigation/control and must not run a second planner or force synchronous readback.

Useful debug data includes static regions/portals/clearance, dynamic actors P/V/A, swept bounds, active corridor, local horizon, conflicts, snapshot generation and age.

## 10. Roadmap

1. **`NAV-V2-MAP-2` — CLOSED:** shared dynamic reduction/backend evidence.
2. **`NAV-V2-SPACE-1` — CLOSED:** static free-space/corridor/turn-aware reference.
3. **`NAV-V2-LOCAL-1` — ACTIVE:** bounded dynamic conflict + temporary target selection.
4. pursuit/receding-intercept consumer.
5. raw NavigationWorld debug visualization.
6. live `EliteGame` / `EliteServer` integration.
7. retire legacy route-wide navigation only after v2 owns the live path.

## 11. Documentation Definition of Done

For meaningful NavigationWorld iterations synchronize, as applicable:

```text
CURRENT_STATE.md
CURRENT_TASK.md
PROJECT_STATE.md
NAVIGATION_WORLD_V2.md
relevant README / benchmark RUN_LOG / contracts
private elite-project-context CURRENT_STATE/CURRENT_TASK/ITERATION_LOG
```

Stale state/task documentation or branch ambiguity blocks handoff.
