# Recovery point — Assisted tunnel-safe route planner accepted

Date: 2026-10-12

Status: ACCEPTED / restore-safe.

This recovery point freezes the Assisted continuous route-planner state after
the hot-replan + mandatory BLUE transit stress scenario completed successfully
and after a static navigation-boundary audit.

## Accepted runtime evidence

Accepted stress log:

- `elite_navigation_stress(20261012-083812).log`
- scenario: `hot-replan-blue-transit-v2`
- requests: 12
- failures: 0
- planner retries: 0
- emergency stops: 0
- BLUE entered: 1
- BLUE exited: 1
- status: COMPLETE
- final rebound approach: 0 m
- final rebound minimum radius: approximately 954.85 m
- configured tunnel radius floor: approximately 954.93 m
- final peak lateral utilization: 1.000000
- final peak angular-velocity utilization: 0.150385
- final peak angular-acceleration utilization: 0.990025

The route-length increase is accepted when it is justified by tunnel
readability / turn safety. The rejected historical pathology was an unjustified
very long route, not long routes in principle.

## Accepted geometric policy

Manual/autopilot guidance geometry has a speed-independent tunnel-readability
floor. With 500 m sparse guidance frames and the current 30 degree maximum turn
per characteristic frame interval, the corresponding radius floor is about
955 m.

This is a geometric safety constraint. Slowing the ship down may satisfy
dynamic authority, but it may not buy a visually unreadable / operationally
unsafe tunnel with adjacent frames making an abrupt heading change.

Dynamic feasibility remains separate:

- lateral acceleration authority;
- angular-velocity authority;
- angular-acceleration authority;
- acceleration/braking reachability.

## Module boundary audit

The accepted ownership is:

```text
TrafficRouteGraph / NavigationVolume / semantic portals
        |
        v
RouteStageCompiler
        |
        v
CompiledTrafficRoute
        |
        +--> SegmentedRouteComposer
        |       |
        |       +--> planner::RoutePlanRequest --> RoutePlanner
        |       +<-- planner::RoutePlan geometry
        |
        +--> TrafficRoutePlanAdapter
                |
                +--> generic RouteStageSpan / RouteFrameAnchor metadata
                            |
                            v
                  ClientRouteAutopilot / follower
```

Required boundaries:

1. `RoutePlannerApi.h` is value-only and has no traffic implementation
   dependency.
2. `ContinuousAssistedPathOptimizer` is a private implementation detail of
   `RoutePlanner.cpp`; production orchestration, traffic and follower code
   must not include it directly.
3. `SegmentedRouteComposer` owns traffic-aware geometry composition only.
   It may consume `CompiledTrafficRoute` and call the generic planner through
   `RoutePlanRequest/RoutePlan`, but it does not publish semantic
   `RouteStageSpan` / frame metadata and performs no direct I/O.
4. `TrafficRoutePlanAdapter` is the sole owner of compiled-traffic to generic
   stage/frame metadata conversion.
5. `RouteEntryBinder` operates only on generic planner values. It may shift
   already-generic stage/frame progress while splicing a retained tail, but it
   does not branch on traffic stage kinds, lane IDs or volume IDs.
6. Autopilot/follower code consumes generic route geometry and execution data
   only. It has no traffic header/type dependency and does not inspect semantic
   route stages for planning decisions.
7. `SpaceState` remains the composition root. It may own traffic compilation,
   invoke composer/adapter/planner and publish the returned products, but it
   does not construct `RouteStageSpan` or route-stage progress itself.
8. Pure route calculations do not read clocks, files, environment state,
   presentation state or concrete ship descriptors.

The static guard
`tests/architecture_contracts/check_navigation_route_boundaries.py` now
enforces the new SegmentedRouteComposer ownership, internal optimizer privacy,
follower traffic isolation, SpaceState composition-only rule and direct-I/O
ban for route calculation modules.

## Important implementation decisions

- Endpoint speed is explicit in `RoutePlanRequest::terminalSpeedMps`.
- BLUE free legs optimize for the real handoff speed instead of pretending to
  stop at the semantic boundary.
- Late-entry rebound does not invent a second braking straight before an
  already-authored traffic tail.
- Post-splice speed profiling reapplies lateral, angular-velocity and
  angular-acceleration limits.
- Final rebound routes exceeding dynamic authority are rejected rather than
  published as Ready.
- Curvature-uniformity cost discourages flattened Bezier turns without
  prescribing one preferred radius.
- Tunnel safety is a hard geometric rule and therefore cannot be defeated by
  lowering local speed.

## Verification commands

```bash
python tests/architecture_contracts/check_navigation_route_boundaries.py

cmake --build build \
  --target ordinary_route_policy_tests \
           continuous_assisted_optimizer_lab_tests \
           route_planner_api_tests \
           segmented_route_composer_tests \
           EliteGame \
  -j 8

ctest --test-dir build/tests/navigation_runtime \
  -R "^(ordinary_route_policy|continuous_assisted_optimizer_lab|route_planner_api|segmented_route_composer)$" \
  --output-on-failure

powershell.exe -ExecutionPolicy Bypass \
  -File scripts/run-navigation-stress.ps1
```

The restore branch named in the commit history is intended to remain immutable.
