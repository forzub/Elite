# Coordinate / spatial architecture audit

**Status:** audit + migration proposal; not yet an accepted implementation contract  
**Repository baseline reviewed:** `15cdea707ed8210276cb719b1b71d7cf106df6b8`  
**Date:** 2026-09-19

## Executive conclusion

The recurring MAP/WORLD bugs are not isolated mistakes. The repository already
contains most of the correct primitives, but spatial ownership is transitional
and duplicated.

The correct conceptual split is:

~~~text
GALACTIC CATALOG
      |
      v
SYSTEM / GLOBAL DYNAMICS            server authority
      |
      | explicit FrameSnapshot / FieldSample boundary
      v
LOCAL INTERACTION DOMAIN            shared deterministic server/client
      |
      +-- navigation
      +-- collision / damage
      +-- ship physics
      +-- control execution
      +-- short-horizon trajectory
      |
      v
PRESENTATION / ASSET / BODY spaces  leaf conversions only
~~~

The main recommendation is NOT two code libraries only. Use two physical
simulation domains plus one tiny shared spatial/frame kernel:

1. `EliteSpatialCore` -- strong spatial types, KinematicFrame, transforms,
   rebasing and domain IDs. Shared by server and client.
2. `EliteGlobalDynamics` -- server-authoritative galactic/system state,
   ephemerides, gravity/field sampling, coarse inactive propagation and frame
   production.
3. `EliteLocalSimulation` -- deterministic local interaction sandbox shared
   by server/client: local physics, collisions, NavigationWorld, PilotSkill,
   accepted trajectory execution.

Presentation remains client-only and may consume derived world/render views,
but may never feed visual/model coordinates back into simulation authority.

## What is already good

### KinematicFrame is close to the desired boundary

`src/game/navigation/KinematicFrame.h` already owns the correct transforms for
a moving/rotating/accelerating local frame:

- position;
- velocity including `omega x r`;
- acceleration including frame linear acceleration;
- angular acceleration;
- centrifugal term;
- Coriolis term;
- LocalKinematicState <-> WorldKinematicState;
- exact rebasing between two frames in the same system.

This should become SpatialCore rather than remain one navigation helper among
several competing frame APIs.

### TravelFrameSystem has the right ownership idea

An entity owns its travel frame and may match/detach from an external reference
without aliasing that reference. This is suitable for local domains and future
detached/J-transit frames.

### WorldFrame render conversion is correctly one-way

`src/world/coordinates/WorldFrame.h` converts authoritative WorldPosition into
camera-relative float coordinates at the presentation boundary. Rendering does
not need global floats.

### Client prediction already shares local physics

`ClientHubTacticalPrediction` and `SharedShipPhysics` demonstrate that the
local deterministic simulation naturally belongs in a shared server/client
library.

## Confirmed architectural hazards

### 1. Two hub-local bases leak into simulation

The repository intentionally defines:

~~~text
tactical / kinematic: X=prograde, Y=radial, Z=normal
visual / model:       X=normal,   Y=radial, Z=-prograde
~~~

The visual basis is legitimate for authored assets. It is NOT a second physics
frame.

Stage-12 NavigationRuntimeLab currently constructs its NavigationMap working
frame from the visual basis while DynamicMotionState.travelFrame and client
prediction use the tactical KinematicFrame basis. This is the direct structural
source of repeated MAP/WORLD/visual conversion bugs.

Target invariant:
- local simulation has one physical LocalFrame;
- asset/model basis is converted exactly once on asset/attachment ingress;
- NavigationMap/Space never use model/visual coordinates as their state frame.

### 2. DynamicMotionState mixes authoritative local and derived world state

It contains local position/velocity, travel frame, world velocity, reference
velocity, engine accelerations and global gravity in one record.

The runtime currently treats local position/velocity as authoritative while
materializing world state each tick, but the type system does not enforce that.
A caller can read or write either representation.

Target invariant:
- LocalBodyState owns local kinematics only;
- SystemKinematicState is a derived publication while local authority is active;
- exactly one side owns integration.

### 3. Propulsion demand bounces Local -> WORLD -> Local

DynamicMotionSystem computes/accepts acceleration with world-oriented ship axes,
stores main/RCS acceleration as world vectors, then updateLocalFrameMotion
converts those vectors back through `frame.worldToLocalVector()` before
integrating local velocity.

That round trip is unnecessary and is the seam that allowed a field named
`...MapMps2` to contain a WORLD vector.

Target invariant:
- controller output entering LocalSimulation is LocalFrame acceleration;
- body-axis thruster allocation converts Body <-> Local only;
- no WORLD acceleration is required to integrate local ship motion.

### 4. Global gravity is sampled but not coupled into local fixed-step physics

GameSimulation computes authoritative gravity and writes
`motion.gravityAccelerationMps2`.

The current HubTactical DynamicMotionSystem local fixed step does not consume
that field. It integrates propulsion only.

KinematicFrame already contains the mathematics needed to convert between
global and local acceleration, but that mathematics is not the actual coupling
API of the production local integrator.

This is a real missing GlobalDynamics -> LocalSimulation contract.

### 5. HubNavigationFrame duplicates KinematicFrame

HubNavigationFrame stores the same origin/velocity/acceleration/basis/angular
state and re-implements position/vector/velocity transforms, then can construct
a KinematicFrame.

Target invariant:
- registry owns KinematicFrame;
- hub metadata references a FrameId;
- no second frame implementation.

### 6. Snapshot wire carries duplicate spatial authority

ShipSnapshot contains a full ShipTransform whose DynamicMotionState already
contains travel-frame/local/world data, and also publishes a separate
ShipReferenceFrameSnapshot containing the same local/frame state.

ClientWorldState copies the reference-frame payload back into ShipTransform.

Target invariant:
- one canonical replicated LocalBodyState + FrameSnapshot;
- derived world/render state is not separately authoritative.

### 7. WorldPosition represents more than one domain

PlayerSpatialDomainResolver interprets the same WorldPosition type as:
- system-local when sourceSystemId >= 0;
- galactic-absolute when sourceSystemId < 0.

The meaning depends on a side-channel ID.

Target invariant:
- GalacticPosition and SystemPosition are distinct types;
- LocalPosition is a third distinct type tied to FrameId;
- conversion cannot happen by accidentally passing a raw glm::dvec3.

### 8. Transitional velocity-match hack proves sequencing is not explicit

updateShipReferenceFrames contains a pendingReferenceVelocityMatch branch that
waits for a reference speed above 10000 m/s before accepting it as a complete
world velocity.

This is a staging workaround, not a stable spatial contract. FrameSnapshot
publication needs an explicit valid/epoch/revision state instead.

### 9. Old and new navigation stacks use different spatial philosophies

The older LocalGuidancePlanner / TrajectoryPredictor operate largely on
system/world kinematic states including gravity.

NavigationWorld v2 operates on NavigationMap local coordinates.

Both are useful at different scales, but their names do not expose the scale
boundary and both accept raw dvec3 vectors.

Recommended ownership:
- long-range/gravity trajectory -> GlobalDynamics/SystemTrajectory;
- short-horizon collision/avoidance/docking -> LocalSimulation.

## Global <-> local coupling contract

When a LocalInteractionDomain is active, local state is authoritative for its
members. Global state is derived, not independently integrated.

Global -> Local supplies at an explicit epoch:
- FrameSnapshot (origin P/V/A, basis, omega, alpha, SystemId, FrameId);
- global field samples or a field evaluator handle;
- objects entering the interaction domain;
- coarse route/goal intent.

Local -> Global publishes:
- materialized SystemKinematicState for indexing/network/system map;
- impulses / delta-v / state changes that survive domain deactivation;
- membership and lifecycle changes.

The conversion must use KinematicFrame's complete kinematic transform. For an
accelerating/rotating frame, global acceleration is not just rotated local
acceleration. Frame acceleration, angular acceleration, centrifugal and Coriolis
terms are part of the transform.

Do not integrate both the local and global copies of the same active entity.
That creates drift and eventually two competing truths.

## Gravity and other global forces

A clean local step should be conceptually:

~~~text
system position derived from LocalState + FrameSnapshot
        |
        v
GlobalFieldSample at that epoch
        |
        v
worldToLocalAcceleration(...)
        |
        + local propulsion / collision / gameplay forces
        v
LocalDynamics fixed-step integration
~~~

If the frame itself follows the same gravity field, its frame acceleration is
already part of the KinematicFrame transformation. This prevents applying
orbital gravity twice.

For a rotating frame the non-inertial terms must remain explicit. If a simpler
local physics domain is desired, choose an inertially oriented local frame and
let stations/hubs move inside it instead of rotating the entire sandbox.

## Local domain is not necessarily "the ship frame"

The local sandbox may be centered near a ship, but its simulation axes should
not automatically rotate with that ship.

A per-ship rotating frame makes every external object acquire pseudo-motion and
makes ship-to-ship collision across different frames expensive.

Use an interaction-domain frame shared by all bodies that can physically
interact:
- hub/station tactical domain;
- battle/formation domain;
- planetary-local domain;
- detached travel domain.

A floating origin may be rebased near the player when required, preserving
kinematics through `rebaseLocalKinematics`.

## Recommended public spatial types

Do not expose raw glm::dvec3 across domain boundaries.

At minimum use distinct records for:
- GalacticPosition;
- SystemPosition / SystemVelocity / SystemAcceleration;
- LocalPosition / LocalVelocity / LocalAcceleration;
- BodyVector;
- FrameId + SystemId.

A stronger implementation may use tagged vector wrappers, but even simple
distinct structs will make accidental MAP/WORLD assignment a compile error.

## Migration order

Do not perform a flag-day rewrite.

Phase A -- SpatialCore:
- move KinematicFrame + kinematic state/rebase math into a neutral shared target;
- add strong global/local/body types and explicit boundary adapters;
- add round-trip invariants/tests.

Phase B -- one local authority:
- make DynamicMotionState local-only for integration;
- keep world position/velocity as read-only derived compatibility views;
- remove Local -> WORLD -> Local propulsion round-trip.

Phase C -- one hub frame:
- replace HubNavigationFrame transform methods with canonical KinematicFrame;
- retain hub metadata separately;
- convert visual/model attachment coordinates only at asset ingress.

Phase D -- Global field coupling:
- define FrameSnapshot + GlobalFieldSample API;
- feed gravity through the complete world->local acceleration transform;
- test orbital co-frame cases and inertial local domains.

Phase E -- replication:
- replace duplicated ShipTransform + ShipReferenceFrameSnapshot authority with
  one canonical local kinematic payload plus FrameSnapshot;
- derive world/presentation state on consumers.

Phase F -- Navigation:
- make NavigationMap/Space consume the canonical LocalFrame only;
- move old world-space long-range trajectory code under explicit system/global
  ownership;
- remove map/world naming ambiguity.

Phase G -- remove compatibility mirrors:
- pendingReferenceVelocityMatch speed heuristic;
- legacy world/local mirrors;
- duplicate manual dot-product frame conversions;
- misleading `MapMps2` fields after boundary conversion.

## Acceptance invariants

- No local simulation module accepts SystemPosition/SystemVelocity directly.
- No global dynamics module accepts LocalPosition without an explicit FrameId.
- Asset/model basis cannot instantiate NavigationMap coordinates.
- Every active entity has exactly one spatial authority owner.
- P/V/A in one kinematic state always share one frame.
- Network carries one canonical spatial state, not two mirrors.
- local->system->local round trip preserves P/V/A within tolerance.
- rebase oldFrame->newFrame preserves materialized System P/V/A.
- gravity in local domain agrees with system-space reference integration.
- server and client local fixed-step use the same LocalSimulation library.
