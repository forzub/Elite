# Galactic Route Planning Contract

**Status:** DEFERRED / FUTURE TASK  
**Scope:** strategic interstellar navigation above the current System / Local / Precision NavigationWorld layers

## 1. Purpose

Galactic navigation is a separate strategic planning problem. It must not be implemented by stretching LocalAvoidance, exact HitVolume queries, or the station-scale NavigationMap across light-year distances.

The shared product model remains:

```text
RoutePlan / player intent
    -> accepted navigation solution
        -> ManualGuidance
        OR
        -> Autopilot execution
```

Manual and automatic flight must consume the same accepted route/trajectory product. They differ only in execution:

```text
MANUAL
    accepted route/trajectory
        -> GuidanceCorridor / jump alignment / next-leg cues
        -> player controls the ship

AUTOPILOT
    accepted route/trajectory
        -> trajectory/control follower
        -> PilotSkillExecutor
        -> ShipControlState
        -> authoritative physics
```

A second planner for manual presentation is forbidden.

## 2. Hierarchical navigation

The intended navigation hierarchy is:

```text
GALAXY
    strategic system-to-system / jump route

SYSTEM
    interplanetary / orbital route inside one star system

LOCAL
    station / asteroid / traffic / moving-obstacle navigation

PRECISION
    moving gap / docking / terminal 6DoF capture
```

Each lower layer refines only the currently relevant leg. A galactic route does not need centimeter-scale geometry, and a docking planner does not decide which star system to visit next.

The existing `RoutePlan` remains the durable player intent container across Galaxy / System / Detail / Hub maps.

## 3. Galactic graph / strategic route

The future GalacticRoutePlanner should operate on systems / jump-capable navigation nodes and legal jump transitions.

A candidate jump edge must be able to express at least:

```text
distance
jump-drive type
maximum jump range
ship mass / loaded mass
fuel required
fuel available
fuel reserve policy
charge time
cooldown / recovery time
jump duration / transition cost
navigation uncertainty
known / unknown destination
hazard / anomaly cost
restricted / forbidden space
refuel / resupply availability
required departure / arrival conditions
```

The route cost must be multi-objective rather than pure geometric distance. Possible products include fastest, minimum-fuel, safest, or balanced routes, while hard vehicle/safety constraints remain non-negotiable.

## 4. Gravity and trajectory coupling

Interstellar / long-system legs cannot assume a straight Euclidean line when gravity materially changes the physical trajectory.

The strategic planner may first choose a coarse sequence of systems / jump nodes, but every non-instantaneous physical leg must be validated by the shared trajectory-prediction layer.

The future solver must account for:

- gravitational acceleration from relevant bodies;
- time-varying body positions / ephemerides where required;
- gravity-assist or gravity-induced trajectory curvature;
- propulsion acceleration and velocity limits;
- proper acceleration / crew-load limits where applicable;
- arrival-state constraints;
- route timing effects on the next leg.

Gravity remains physics/environment input, not a presentation correction.

## 5. Fuel / delta-v ownership

Fuel cannot be treated as a cosmetic cost attached after trajectory planning.

The accepted trajectory solution must expose enough information to derive or directly publish:

```text
proper delta-v
propellant/fuel consumed
fuel remaining
reserve margin
refuel requirement
mass change if the propulsion model requires it
```

Fuel consumption must be coupled to the selected propulsion / jump-drive model. The solver may reject a geometrically valid route when the vehicle cannot execute it with available fuel plus required reserve.

For jump drives, the fuel model may be discrete per jump, continuous with jump distance, mass-dependent, or drive-specific. The GalacticRoutePlanner must not hard-code one universal formula into the route graph.

## 6. Jump-drive planning

A jump route is a sequence of accepted jump legs, not merely nearest-neighbour star hopping.

A jump leg should eventually carry:

```text
origin node
destination node
planned departure state
required alignment / orientation if applicable
range / drive capability check
fuel cost
charge requirement
cooldown/recovery state
arrival uncertainty / arrival envelope
next-leg readiness
```

The strategic planner selects the sequence. A lower execution layer performs the actual alignment, charge, jump, arrival, recovery, and handoff to the next leg.

## 7. Manual versus automatic galactic travel

The route is identical for manual and automatic modes.

Manual example:

```text
NEXT JUMP: System B
ALIGNMENT ERROR: ...
RANGE: ...
FUEL AFTER JUMP: ...
CHARGE: ...
READY
```

The player performs alignment / charge / jump commands.

Automatic example:

```text
accepted galactic route
    -> align
    -> charge
    -> jump
    -> recover
    -> validate next leg
    -> repeat
```

After arrival in the destination system, control passes to the System planner; near local traffic/geometry it passes to NavigationWorld Local/Precision planning.

## 8. Replanning

The galactic solution must be revisioned and re-evaluated when material inputs change, for example:

- fuel state differs from plan;
- vehicle mass / jump capability changes;
- destination or intermediate system becomes unavailable;
- hazard / restriction data changes;
- an arrival state invalidates the next jump;
- target is dynamic and its predicted state changes;
- the ship deviates too far from the accepted physical leg.

Replanning changes the navigation solution, not the authored RoutePlan unless the player explicitly edits route intent.

## 9. Reuse of existing project foundations

The future implementation should reuse, not duplicate:

- `RoutePlan` as durable cross-map route intent;
- `GalaxyNavigationGrid` / `GalaxyNavigationConfig` for galactic spatial hierarchy and map-addressing support;
- `TrajectoryPredictor` for physical propagation;
- common navigation solution / revision semantics;
- `GuidanceCorridor` and HUD guidance concepts for manual execution;
- the accepted runtime control chain for automatic execution where physical flight rather than jump transition is required.

The existing local NavigationWorld remains responsible for bounded local geometry and dynamic obstacle safety. It is not promoted into a galactic all-space obstacle database.

## 10. Deferred implementation tasks

This work starts only after the current Stage-12 local NavigationWorld authority, manual-guidance, and end-to-end ownership gates are stable.

Planned sequence:

1. define `GalacticRouteSolution` / jump-leg DTO and revision semantics;
2. define drive/fuel capability interface independent of a specific drive model;
3. define galactic node/edge source from the system catalog;
4. implement bounded strategic route search with hard range/fuel constraints;
5. couple route cost to fuel, time, hazards and reserve policy;
6. connect long physical legs to gravity-aware trajectory prediction;
7. add time-varying ephemerides where long-leg accuracy requires them;
8. add manual jump guidance from the accepted route;
9. add automatic jump-leg executor;
10. add replanning after arrival, fuel/capability changes, or route invalidation;
11. add target-machine performance and deterministic regression gates.

## 11. Non-goals for the current Stage 12

The current Stage 12 must not be delayed by implementing galactic routing.

Current work remains focused on:

```text
NavigationWorld local moving/static safety
    -> authoritative planner intent
    -> PilotSkillExecutor
    -> real physics
    -> replication
    -> same accepted product for manual guidance/debug
```

Galactic gravity/fuel/jump routing is deliberately tracked here as a separate future task.
