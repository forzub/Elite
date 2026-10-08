# Traffic Route Graph and Navigation Volume Contract

Status: **PROTECTED NAVIGATION CONTRACT**

## Separation of responsibilities

`NavigationVolume` owns physical navigable geometry only.
`TrafficLane` references a volume and assigns traversal policy/direction.
`TrafficRouteGraph` owns semantic portal/lane connectivity only.
`RoutePlanner` owns geometric route construction only and must not mutate topology.

## One geometry, multiple policies

A navigation volume has no color by itself. Runtime/planning policy assigns meaning:

- `KeepOutside` — RED: hard exclusion; hull + clearance must remain outside.
- `KeepInside` — BLUE: hard containment; hull + clearance must remain inside.
- `PreferInside` — GREEN: soft preference; leaving is allowed but increases route cost.
- `None` — unrestricted.

Red and blue are inverse uses of the same volume-membership geometry. Green uses the same inside test as blue but as a cost rather than a hard rejection.

## Clearance rule

For forbidden/red space, inflate the forbidden volume by hull envelope + authored clearance.
For required/blue space, erode the allowed volume by hull envelope + authored clearance.
Therefore the planner never intentionally flies the hull surface on a boundary.

## Physical representation

A simple straight cylinder may be represented as a swept corridor with two circular sections.
A bent tunnel, canyon or cave uses the same `SweptCorridor` representation with additional ordered sections.
Each section owns center, forward, up and circle/rectangle dimensions.
Interior shaping sections are geometry only; they do not need to be public traffic graph nodes.

Render meshes are never authoritative navigation geometry.

## Ownership

Hub-module navigation volumes are authored in module-local coordinates and move with the module reference frame.
Terrain/canyon/cave volumes use an appropriate world-region or kinematic reference frame.
The traffic graph does not duplicate volume geometry; it references volume IDs.

## Portals

Entry/exit portals are semantic crossing planes, not merely points.
A crossing contract includes aperture/frame, crossing direction, clearance and optional speed ceiling.
Docking ports and transit gates remain distinct semantic anchors even if they are physically co-located.

## Traffic lanes

A lane binds:

- entry portal;
- exit portal;
- navigation volume;
- one-way/two-way direction policy;
- `KeepInside` (blue) or `PreferInside` (green) policy;
- optional lane speed ceiling;
- future traffic-resource/conflict IDs.

`KeepOutside` red zones are global planning constraints and are not represented as traversable graph edges.

## Stage compilation

The current ship position is not a topology node. A resolved route prepends a synthetic `FreeApproach` stage.
A volume-transit stage carries a `NavigationVolumeConstraint` into a later stage compiler.
That compiler may create one final continuous `RoutePlan`, but it may not discard the stage's containment/preference semantics.

## Current protected test topology

`earth_orbital_hub_test_traffic_v1`

`FREE SPACE`
→ `cylinder_b.entry_front`
→ `cylinder_b.blue_lane` using `cylinder_b.transit_volume` with `KeepInside`
→ `cylinder_b.exit_rear`
→ `cube_a.dock_front`

The physical cylinder volume is currently a straight swept corridor with two circular sections. Turning it into a bent tunnel must require only additional volume sections, not a traffic-graph redesign.

## Hard architecture rules

1. Built `TrafficRouteGraph` is immutable.
2. `RoutePlanner` may not directly own or mutate `TrafficRouteGraph`.
3. Traffic topology is resolved before geometric stage planning.
4. Invalid IDs, portal order, lane direction or broken graph references fail closed.
5. Physical volume definitions and traffic policy remain separate.
6. A future dynamic traffic controller may change lane availability/policy without changing physical volume geometry.
7. Unrelated work on follower gains, speed profiles, roll, rendering or docking UI must not modify this contract.

Any contract change requires a concrete traffic/navigation use case and regression-test updates.
