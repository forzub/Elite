# Spatial architecture decision — Global dynamics / local free-fall domains

**Decision date:** 2026-09-19  
**Status:** project direction selected; implementation not started  
**Navigation freeze point:** current 12A-6b3b recovery candidate is allowed to
finish its already-running target-machine gate, but no further Navigation v2
feature/recovery patch is to be added before Spatial Phase A.

## Decision

Pause further Navigation v2 implementation after recording the result of the
current target-machine run.

The next implementation task is to establish one protected spatial foundation
before Navigation v2 continues:

~~~text
EliteSpatialCore
    |
    +-- typed Galactic / System / Local / Body quantities
    +-- FrameId / LocalDomainId / SystemId
    +-- KinematicFrame transforms
    +-- exact rebase
    +-- the only System <-> Local conversion API

EliteGlobalDynamics                 SERVER AUTHORITY
    |
    +-- star-system ephemerides / celestial motion
    +-- global P/V/A
    +-- large-scale gravity / fields
    +-- inactive/coarse entities
    +-- local-domain carrier P/V/A
    |
    +---------- explicit boundary ----------+
                                           |
                                           v
EliteLocalSimulation                SHARED SERVER + CLIENT
    |
    +-- local P/V/A
    +-- ship physics / controls
    +-- collisions / damage
    +-- NavigationMap / NavigationSpace
    +-- local trajectory / docking / combat
~~~

This is a domain split, not a server/client split. Local deterministic simulation
must remain available on both server and client prediction paths.

## Local sandbox model

A local interaction domain is treated as a moving object in System/Global
coordinates.

It owns a carrier frame:

~~~text
LocalDomainFrame
    systemId
    domainId
    epoch
    global origin position
    global origin velocity
    global origin acceleration
    basis
    angular velocity / angular acceleration (normally zero or minimal)
~~~

All entities capable of direct local physical interaction belong to the same
LocalDomainId for that interaction epoch.

The domain may follow an anchor ship, station, convoy or combat cluster. The
anchor is a placement/reference choice, not the coordinate axes of the ship
body.

Do not create independently rotating per-ship physics cubes for ships that may
collide with each other.

## Free-fall simplification

The preferred local domain is a free-fall, non-rotating or minimally rotating
frame.

GlobalDynamics advances the domain carrier under the large-scale gravitational
field. LocalSimulation then cancels the common-mode acceleration of that frame.

Conceptually:

~~~text
planet / star gravity
        |
        v
GlobalDynamics accelerates LocalDomainFrame
        |
        v
all local bodies share that common acceleration
        |
        v
LocalSimulation does NOT need to integrate that common acceleration again
~~~

This matches the intended intuition that near a planet the whole local sandbox
falls together.

For an object at local offset x, the exact external residual is approximately:

~~~text
a_residual(x) =
    R^T * ( g(P + R*x) - g(P) )
~~~

for a non-rotating free-fall frame whose origin acceleration is g(P).

The residual is the tidal/differential field. It may be deliberately ignored
when bounded below the gameplay/physics error budget.

Therefore the local-domain environment policy should be explicit, for example:

~~~text
IgnoreExternalFieldResidual
ApplyTidalResidual
FullExternalField
~~~

The default short-range navigation/combat sandbox can use
IgnoreExternalFieldResidual when its size and lifetime keep the resulting
position error below tolerance.

Do not silently ignore the residual for arbitrarily large domains, very long
simulation horizons, near compact/high-gradient bodies, atmospheric descent,
surface contact, or gameplay that explicitly depends on gravity gradients.

A useful admission/error criterion is based on predicted residual displacement:

~~~text
0.5 * maxDifferentialAcceleration * horizon^2 <= allowedPositionError
~~~

If the bound is exceeded, shrink/rebase the local domain or enable the tidal
residual. This makes the simplification measurable rather than heuristic.

## Rotation policy

A local domain does not need to rotate with a hub or ship.

Preferred default:

~~~text
origin: follows free-fall carrier trajectory
basis: inertial/non-rotating over the local horizon
~~~

Then:
- common gravity is cancelled by carrier acceleration;
- Coriolis/centrifugal terms disappear or are minimized;
- stations/ships may rotate normally inside the sandbox;
- NavigationMap and collision geometry share one simple basis.

A rotating domain remains supported only when there is a concrete benefit.
If used, all non-inertial terms must come through SpatialCore/KinematicFrame.
No subsystem may implement its own rotating-frame math.

## Star-system coordinate decision

Do NOT make the Solar System the master physical origin for the whole galaxy.

Also do NOT build an arbitrary recursive hierarchy of star-relative frames.

Use one shallow addressing model:

~~~text
Galaxy catalog:
    SystemId -> GalacticPosition of system origin

Inside an active system:
    SystemId + SystemPosition/SystemVelocity/SystemAcceleration

Interstellar/travel state:
    GalacticPosition or an explicit travel domain
~~~

The Sun is simply the origin/body chosen for the Solar System entry. Another
star system has its own system/barycentric frame. There is no chain
"Galaxy -> Sun -> other star -> planet -> local physics" in ordinary state
storage.

The boundary is only:

~~~text
Galactic system origin + SystemState
                <->
          LocalDomainFrame + LocalState
~~~

Planet/moon/hub relationships are ephemeris/dynamics relationships inside one
System frame, not additional coordinate types exposed throughout the game.

This keeps precision manageable without multiplying coordinate semantics.

## Authority rule

An entity has exactly one integration owner at any instant.

When active in LocalSimulation:

~~~text
LocalBodyState = authoritative
System/Global P/V/A = derived publication
~~~

When dematerialized/coarse:

~~~text
System/Global state = authoritative
LocalBodyState does not continue integrating
~~~

Never integrate both copies and reconcile them later.

## Boundary API target

Conceptually:

~~~text
GlobalDynamics::openLocalDomain(...)
    -> LocalDomainSeed {
           FrameSnapshot,
           LocalBodySeed[],
           EnvironmentPolicy
       }

SpatialCore::toLocal(...)
SpatialCore::toSystem(...)
SpatialCore::rebase(...)

LocalSimulation::step(...)

GlobalDynamics::commitLocalDomain(...)
    <- materialized SystemKinematicState[]
~~~

The actual C++ names may differ, but the ownership must not.

## Compile-time protection

Raw glm::dvec3 must not be the public boundary type between domains.

Required distinct types:

~~~text
GalacticPosition
SystemPosition
SystemVelocity
SystemAcceleration

LocalPosition
LocalVelocity
LocalAcceleration

BodyVector / BodyAcceleration

FrameId
LocalDomainId
SystemId
~~~

A MAP/LOCAL vector must not be assignable to a SYSTEM/WORLD field without an
explicit SpatialCore conversion.

## Navigation freeze state

Last actually target-machine accepted Stage-12 baseline remains:

~~~text
daaf038021cdf8b9561db60fdd35e7cefce0b2df
~~~

Current pre-freeze recovery candidate being run on the target machine:

~~~text
f25a9c2378c45950155db00caf48e71b5a07e3ac
~~~

The candidate adds directional stopping reserve and a short emergency braking
AcceptedShortSegment after exact-static invalidation. It is NOT accepted until
the target-machine result is recorded.

The immediately preceding live witness had already shown that the repaired
WORLD -> MAP conversion makes ideal and executed acceleration agree closely.
The remaining entity-28 failure at that point was dynamic viability: the local
planner woke on exact-static invalidation but ordinary AdjustedClear progress
could not remove existing momentum quickly enough.

The current run is valuable and must be recorded regardless of PASS/FAIL. After
that result, Navigation v2 implementation is paused at this boundary while
Spatial Phase A is implemented.

## Spatial Phase A acceptance

Before resuming Navigation v2:

1. Create EliteSpatialCore shared target.
2. Move/canonicalize KinematicFrame transforms there.
3. Introduce typed System/Local/Body quantities and IDs.
4. Define LocalDomainFrame + LocalBodyState authority.
5. Pin System <-> Local P/V/A round-trip tests.
6. Pin frame-rebase preservation tests.
7. Pin free-fall-domain common-gravity cancellation test.
8. Pin optional tidal-residual test.
9. Make NavigationMap working frame consume canonical LocalDomainFrame only.
10. Prevent visual/model hub basis from entering NavigationMap state.

Only after these invariants are green should Stage 12 resume.
