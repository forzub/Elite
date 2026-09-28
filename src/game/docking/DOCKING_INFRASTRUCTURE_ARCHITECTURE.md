## Implementation status — 2026-09-28 first separation slice

Implemented as a candidate:
- generic public RoutePlanner facade in its own planner directory;
- generic public RouteFollower/Autopilot facade in its own autopilot directory;
- GameServer/SpaceState migrated off direct legacy Planner/Follower backend
  dependencies;
- Timeline/Sampler/Tracker access hidden behind RouteFollowerApi;
- DockFacility public semantic descriptor;
- independent DockTrafficController API/implementation with provisional vs
  controlled-horizon queue semantics and physical/class policy;
- independent DockLandingController API/implementation with LandingHandoff and
  no-main-engine terminal-control contract;
- separate `EliteDockingInfrastructure` build target;
- native/API tests plus forbidden-dependency architecture gate.

Not implemented yet:
- authoritative facility catalog/runtime population from station models;
- live dispatcher replication/UI;
- pad occupancy and timed portal/lane reservations;
- traffic-controller integration into START DOCKING;
- EntryHoldPoint execution;
- LandingController RCS control, gear/contact/latch;
- authoritative DOCKED/undock state.

Windows compile/test evidence is pending before live integration.

# Docking infrastructure architecture

Status: design contract for the next docking slice. This document intentionally
separates navigation, flight execution, terminal landing and traffic allocation
so the route-planning/autopilot stack can be reused for non-docking tasks.

## 1. Hard module boundaries

Docking must not be implemented as one large state machine that owns route
construction, flight control, traffic allocation, pad geometry and landing gear.
The required ownership split is:

```text
DockTrafficController
        |
        | DockingClearance
        v
RoutePlanner
        |
        | PlannedRoute / AcceptedManeuverProgram
        v
RouteFollower / Autopilot
        |
        | NavigationControlIntent
        v
vehicle flight law / physics

Landing handoff:
RouteFollower / Autopilot
        |
        | LandingHandoff
        v
DockLandingController
        |
        | terminal manoeuvre + gear/latch commands
        v
vehicle flight law / physics
```

These are separate entities and must be physically separated in source files.
The target layout is:

```text
src/game/navigation/planner/
    RoutePlannerApi.h
    RoutePlanner.cpp

src/game/navigation/autopilot/
    RouteFollowerApi.h
    RouteFollower.cpp
    ... sampler/tracking implementation owned by this module

src/game/docking/traffic/
    DockTrafficControllerApi.h
    DockTrafficController.cpp

src/game/docking/landing/
    DockLandingControllerApi.h
    DockLandingController.cpp

src/game/docking/model/
    DockFacilityDescriptor.h
    DockFacilityRuntime.h
```

Existing classes may be migrated incrementally, but the dependency direction
above is mandatory. A temporary compatibility adapter may exist during migration;
it must not become a permanent cross-module shortcut.

## 2. API leakage is forbidden

Each module exposes small DTO/API headers. Private implementation types may not
cross another module boundary.

Architecture-contract tests must enforce at least:
- Planner headers do not include traffic, landing, gear or latch internals.
- Autopilot/Follower headers do not include Planner implementation headers.
- DockLandingController does not include Planner or Follower private headers.
- DockTrafficController does not include Planner/Follower/Landing internals.
- GameServer may orchestrate public APIs but may not reach into module-private
  state to change route progress, queue state or landing state.
- The HUD/client receives replicated public state only; it may not reconstruct
  traffic allocation or a second route.
- No shared mutable singleton or raw pointer may be used as a hidden data path
  across these boundaries.

The architecture gate should explicitly scan forbidden include/dependency pairs,
not merely check for class names.

## 3. RoutePlanner responsibility

RoutePlanner is generic navigation infrastructure. Docking is only one caller.

Input is a public `RoutePlanRequest` containing:
- authoritative start pose/velocity/angular state;
- goal pose/goal region;
- ship motion/collision envelope;
- obstacle/world snapshot;
- optional permitted infrastructure corridor/portal/hold target from traffic;
- behavior profile (ordinary/extreme/tight-passage).

Output is route geometry plus its physically valid dynamic profile. It may
contain ordered route points, corridor/tolerance data, speed/orientation/accel
profile and proof/revision metadata.

Planner must NOT own:
- queue order;
- pad assignment;
- dock occupancy;
- landing gear;
- pad contact/latch;
- execution cursor;
- live control law state.

Planner may be reused later for tunnels, canyons, pursuit, repair routing,
formation joins and general NPC navigation without any docking dependency.

## 4. RouteFollower / Autopilot responsibility

Autopilot consumes one accepted route/program and the measured ship state.

It owns:
- ordered spatial progress;
- look-ahead steering;
- route/corridor tracking;
- route-loss diagnostics;
- generation of vehicle-level navigation intent.

It does NOT own:
- route topology;
- traffic priority;
- pad selection;
- queue advancement;
- landing gear;
- final pad capture/latch.

Its output remains vehicle-level intent such as target velocity and bounded
linear/angular acceleration demand. Installed Assisted/Newtonian flight law and
ship physics remain the physical actuator authority.

## 5. DockLandingController responsibility

Landing begins only after RouteFollower reaches a declared
`LandingHandoff` for the assigned pad.

Landing is a different control problem and must be a separate module.

Typical state sequence:

```text
PAD_APPROACH
  -> ALIGN_OVER_PAD
  -> GEAR_DEPLOY
  -> SETTLE_ON_MANOEUVRING_THRUSTERS
  -> GEAR_CONTACT
  -> LATCH
  -> DOCKED
```

Rules:
- main engines are forbidden after landing handoff unless a future dock type
  explicitly declares a safe main-engine landing profile;
- normal pad parking uses manoeuvring/RCS authority;
- LandingController consumes the assigned pad pose/clearance and current ship
  state, not Planner internals;
- landing gear is authoritative ship state
  (Retracted/Deploying/Deployed/Loaded/Retracting), not only animation;
- after successful latch the ship becomes kinematically attached to the pad/dock
  frame and moves/rotates with it;
- undock is a separate traffic-authorized state transition.

## 6. DockFacility is infrastructure, not one mesh

A DockFacility exposes semantic infrastructure independent of art geometry:

```text
DockFacility
  KinematicFrame
  AccessPortal[]
  EntryHoldPoint[]
  TrafficConflictZone[]
  InternalLane[]
  ParkingPad[]
  ServiceZone[]
  DockTrafficController
```

The rendered model may later be a polygonal rotating drum, cylinder, asteroid,
industrial frame or authored Blender model. Navigation/traffic code must consume
semantic anchors and shared collision volumes rather than mesh-specific logic.

A rotating and a non-rotating dock use the same API. Rotation is simply part of
the DockFacility kinematic frame.

## 7. Physical fit and class policy are separate decisions

Physical compatibility is a hard geometric constraint. Example public ship data:

```text
DockingEnvelope:
  width
  height
  length
  sweptWidth
  sweptHeight
  landingFootprint
  landingGearClearance
  mass
```

Portal/pad data contains usable aperture/clearance and pad footprint.

If the ship does not physically fit, traffic control may NEVER assign that
portal/pad, including emergency mode.

Ship class (S/M/L/etc.) is an allocation policy layered on top of physical fit.

Normal policy:
- S -> S
- M -> M
- L -> L

Emergency fallback may use a larger class:
- S -> M/L permitted by policy;
- M -> L permitted by policy;
- L has no smaller fallback.

A smaller aperture/pad is never permitted merely because the dispatcher is in
emergency mode.

## 8. Traffic controller responsibilities

DockTrafficController owns rights to infrastructure, not flight control.

It knows:
- compatible portals and pads;
- pad occupancy/reservation;
- estimated release time;
- arrival/departure queue;
- portal/internal-lane conflict reservations;
- emergency priority;
- current queue position and estimated wait.

It returns a public `DockingClearance`, for example:

```text
DockingClearance
  facilityId
  portalId
  padId
  holdPointId
  allocationReason
  clearanceRevision
  state
  queuePosition
  estimatedWaitSeconds
  entryWindow
  permittedTrafficResources[]
```

Planner receives only the resulting clearance/resources. It never computes who
has priority.

## 9. Long-range docking requests: provisional vs committed

A request can be made many minutes away. Such a request must NOT reserve a pad,
portal or queue position for five minutes and block ships that are already much
closer.

Use a two-stage traffic contract.

### Stage A — remote inquiry / provisional approach

From any distance the dispatcher returns:
- physical compatibility;
- class-policy compatibility;
- currently usable dock classes;
- current occupancy;
- predicted wait/queue;
- a provisional recommended facility/portal.

This is advisory. No hard queue slot and no pad reservation exists yet.

### Stage B — controlled-arrival admission

A ship becomes queue-eligible only when it reaches a configurable controlled
approach horizon. The primary trigger should be ETA-to-facility rather than raw
distance; a safety distance floor may also be used.

At admission:
- the dispatcher assigns a queue token;
- queue ordering is based on arrival-readiness/ETA at the controlled zone, not
  the time a remote request was first clicked;
- emergency priority may explicitly override normal ordering;
- only then may a pad and an entry resource window become hard reservations.

This allows a ship that was already near the station to enter the queue ahead of
a ship that requested docking five minutes earlier from far away.

## 10. Initial holding model: virtual queue + one physical hold point per portal

Do NOT begin with a large free-form holding yard. It adds collision routing and
parking allocation before it provides useful gameplay.

The first implementation should use:
- a virtual queue in DockTrafficController;
- one authoritative `EntryHoldPoint` for each portal/final approach lane;
- at most one ship admitted into that near-entry lane at a time;
- other queued ships remain outside the controlled lane and may stop at a
  Planner-generated safe staging stop on their existing approach route.

The flow is:

```text
remote request
   -> provisional information only
en route
   -> controlled-arrival admission
virtual queue
   -> clearance to EntryHoldPoint
stop/stabilize at EntryHoldPoint
   -> portal + pad reservation becomes executable
clear to enter
   -> internal route
   -> LandingHandoff
   -> DockLandingController
```

This preserves safe sequencing without requiring an orbiting "parking lot".
A later version may add multiple HoldPoints, staging rings or dedicated holding
bays without changing the four-module API.

## 11. Arrival and departure conflict rule

Portal and internal lanes are conflict resources.

An arrival may enter only if every required conflict resource is reserved for its
window. A departure receives the same treatment.

Therefore a docked ship requesting departure may have to remain latched until
the exit path is clear. The dispatcher then issues an UndockClearance; only
after that may DockLandingController release the latch and move the ship toward
the handoff back to Autopilot.

## 12. Map/UI state owned by dispatcher

For the selected ship, every dock/pad presentation must distinguish at least:

- INCOMPATIBLE_PHYSICAL
- CLASS_RESTRICTED
- AVAILABLE
- QUEUED
- OCCUPIED
- RESERVED
- ENTRY_BUSY
- MAINTENANCE
- EMERGENCY_AVAILABLE

The client may display:
- aperture/pad incompatibility;
- class-policy restriction;
- estimated occupied-until time;
- queue length and this ship's queue position;
- estimated wait;
- assigned portal/pad after commitment.

These values come from DockTrafficController public replicated state. The client
must not derive queue/availability from local object inspection.

## 13. Test obligations before implementation is accepted

Architecture:
- compile/public-header dependency gate for the four modules;
- forbidden-include scan;
- no duplicate route/queue/landing state ownership.

Traffic:
- far request does not reserve a pad/portal;
- nearer ship may become queue-eligible first;
- committed queue token cannot be silently displaced by a normal later arrival;
- emergency priority is explicit and logged;
- physically too-large ship is always rejected;
- emergency larger-dock fallback works;
- smaller-dock fallback never works;
- arrival/departure conflict resource cannot be double-booked.

Landing:
- landing cannot start without valid pad assignment + landing handoff;
- main-engine prohibition after handoff;
- gear state transitions are authoritative;
- latch requires pose/velocity/angular/contact tolerances;
- docked ship follows rotating DockFacility frame;
- departure cannot unlatch before dispatcher clearance.

Navigation reuse:
- RoutePlanner and Autopilot tests build without linking docking traffic or
  landing implementation.
- Generic non-docking scenario uses the same Planner/Follower public API.

## 14. Current migration rule

The already successful long SpatialCorridor behavior is not to be broadly
retuned while this architecture is introduced.

The immediate refactor goal is ownership separation first:
1. freeze public DTO/API boundaries;
2. add architecture-contract tests;
3. migrate current Automatic route construction/execution behind
   Planner/Autopilot boundaries;
4. add DockTrafficController data model and provisional/committed queue;
5. replace the current internal-capture completion with LandingHandoff and the
   separate DockLandingController;
6. add authoritative gear/latch/undock;
7. then replace test dock art with a richer polygonal/rotating DockFacility.

