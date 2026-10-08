# Traffic Route Graph Contract

Status: **PROTECTED NAVIGATION CONTRACT**

This layer owns semantic hub-traffic topology. It answers **which portals must be crossed, in which order, and which transit volume owns a constrained stage**.

It does **not** own sampled route geometry, follower control, speed-controller gains, docking visuals, or obstacle-search internals.

## Hard rules

1. `TrafficRouteGraph` is immutable after validated construction.
2. `RoutePlanner` must not directly own or mutate `TrafficRouteGraph`.
3. Traffic topology is resolved before geometric planning.
4. A blue / mandatory-transit zone is represented by:
   - an explicit entry portal;
   - an explicit exit portal;
   - a constrained transit volume;
   - a mandatory graph edge that references that volume.
5. One-way mandatory transit may not be reversed by generic graph search.
6. Docking ports and transit gates are separate semantic anchors even when they occupy similar physical geometry.
7. A mandatory transit stage must remain inside its authored volume after hull clearance.
8. The current ship position is not a graph node. It creates a synthetic `FreeApproach` stage to the first required portal.
9. Geometric stage outputs may later be concatenated into one continuous `RoutePlan`, but stage semantics may not be discarded.
10. Invalid topology fails closed. Missing IDs, duplicate IDs, broken portal references, orphan mandatory zones, or wrong portal order must reject graph construction.

## Current protected test topology

`earth_orbital_hub_test_traffic_v1`

Flow:

`FREE SPACE`
→ `cylinder_b.entry_front`
→ **MANDATORY** `cylinder_b.blue_transit`
→ `cylinder_b.exit_rear`
→ `cube_a.dock_front`

The cylinder transit is currently one-way.

## Change policy

Do not change this contract as part of unrelated work on:

- autopilot tuning;
- trajectory sampling;
- roll synchronization;
- tunnel rendering;
- speed profiles;
- obstacle avoidance;
- docking UI.

A topology-contract change requires a concrete traffic use case and matching regression-test updates in `TrafficRouteGraphTests.cpp`.
