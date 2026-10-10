# Navigation route module boundary contract

Status: enforced architecture contract.

## Purpose

One navigation route is a single value product with ordered semantic stages.
Traffic/world semantics, generic planning, and execution are separate modules.
Information may cross those boundaries only through explicit value contracts.

## Legal dependency direction

```text
TrafficRouteGraph / NavigationVolume / semantic portals
        |
        v
RouteStageCompiler
        |
        v
TrafficRoutePlanAdapter
        |
        +--> planner::RoutePlanRequest
        +--> planner::RoutePlan stage/frame metadata
                    |
                    v
              RoutePlanner
                    |
                    v
          ClientRouteAutopilot / follower
```

`SpaceState` is a composition root. It may invoke both sides, but it must not
reimplement semantic-to-generic conversion.

## Ownership

### Traffic layer

Owns:
- portal/lane/volume identifiers;
- KeepInside / PreferInside semantics;
- authored volume frames;
- semantic stage ordering;
- conversion of those semantics into generic route-stage/frame metadata.

The only production semantic-to-generic route adapter is
`TrafficRoutePlanAdapter`.

### Generic planner contract

`RoutePlannerApi.h` owns value-only planner inputs/outputs.

It may describe generic route stages, frame policy, constraints, curves, speed
and diagnostics. It must not include traffic implementation headers.

The docking backend may consume generic constraints such as mandatory via
points and mandatory tangent straights. It must not know which BLUE/GREEN lane
or portal produced them.

### Autopilot / follower

The follower consumes only generic route geometry, speed/orientation reference,
current vehicle state and explicit dynamic phase input.

It must not:
- include traffic headers;
- inspect portal/lane/volume identifiers;
- branch on `RouteStageKind`;
- read `RoutePlan::stages` to make planning decisions;
- query NavigationVolume or TrafficRouteGraph;
- create a replacement route.

Stage semantics are planner-side metadata. Frame anchors are already-generic
execution reference data and may be consumed by the route-frame machinery.

### SpaceState

`SpaceState` owns orchestration only:
- collect immutable planning snapshot;
- invoke compiler/adapter/planner;
- publish/execute returned products;
- provide explicitly allowed live terminal phase data.

It must not calculate route-stage progress ranges or synthesize
`RouteStageSpan` itself.

## Dynamic data

Dynamic terminal orientation is an explicit execution input. It may be applied
only through the route-frame dynamic-phase contract. Earlier traffic stages
cannot read live dock state through globals, singletons, scene lookups or
presentation data.

No module may use presentation objects, log state, mutable global state, or
another module's private object as an implicit navigation data channel.

## Enforcement

`tests/architecture_contracts/check_navigation_route_boundaries.py` rejects
forbidden include/type/data paths. Any new cross-module information must first
be added to an explicit value API and to this contract.
