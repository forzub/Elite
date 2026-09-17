# Navigation v2 — oriented passage / emergent gap model

**Status:** first precision geometry + attitude-reachability candidate; target-machine gates pending  
**Updated:** 2026-09-17 Europe/Kyiv  
**Parent contracts:** `NAVIGATION_WORLD_V2.md`, `src/world/navigation/TRAJECTORY_CONTROL_MODEL.md`

## Core decision

A narrow passage is not limited to an authored door, tunnel or docking aperture.

Two or more nearby obstacles may form a **positive free-space opportunity** between them. If ordinary lateral avoidance cannot demonstrate a safe bypass, the system may treat the local free space between nearby boundaries as an oriented passage candidate instead of treating every obstacle only as a repulsive keep-out source.

Example:

```text
high speed / short remaining distance
    ordinary side-step avoidance cannot clear either obstacle

        obstacle A      obstacle B
            |   narrow gap   |
            |       ^        |
                    |
              ship can fit
          only at correct roll
```

The same pose geometry also serves authored flat apertures and narrow docking corridors.

## Performance rule — precision is a fallback, not the default broadphase

Do **not** replace the accepted sphere/swept-sphere broadphase with oriented-body checks for every actor.

The intended layering is:

```text
cheap NavigationMap broadphase
        |
        v
LocalHorizonPlanner / LocalAvoidancePlanner
        |
        +-- ordinary clear/adjusted path -> done
        |
        +-- ConflictHold / explicit narrow route / docking corridor
                |
                v
        bounded gap-candidate builder
                |
                |  one primary conflict + already reduced neighbors
                |  no unbounded all-pairs scan
                |  hard candidate cap = 8
                v
        OrientedPassageEvaluator
                |  O(1) projected OBB fit per candidate
                v
        AttitudeReachabilityEvaluator
                |  O(1) angle / angular authority / distance precheck
                |  coast / brake / unreachable
                v
        only physically plausible passage candidates
                |
                v
        full 6DoF trajectory / swept-body feasibility
```

A naive `N x N` obstacle-pair search on the frame path is explicitly rejected.

Candidate generation must use already-reduced local data: the primary conflict, neighboring static surfaces, spatial-bin adjacency, authored aperture metadata, or a similarly bounded source.

The ordinary clear/adjusted path does not execute the precision-gap builder at all.

## Passage sources

The precision layer recognizes the same abstract passage regardless of origin:

```text
AuthoredAperture
    door / hangar mouth / station slot / tunnel

ObstacleGap
    transient or static free space between nearby objects/boundaries

DockingCorridor
    approach tunnel leading to a terminal docking pose
```

After construction, all three are evaluated with the same oriented-hull mathematics.

## Oriented passage geometry boundary

Candidate code:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h
src/world/navigation/trajectory/OrientedPassageEvaluator.cpp
```

It deliberately owns only constant-size geometry:

```text
HullProxy
    body-local OBB half extents
    extra precision clearance

Pose
    center position
    proper right-handed orthonormal body orientation basis

Passage
    center
    proper right-handed orthonormal passage frame
    half width / half height
    source kind
```

For an OBB hull, projected half extent on a passage-plane axis `a` is:

```text
h(a) = |bodyRight . a| * halfX
     + |bodyUp    . a| * halfY
     + |bodyFwd   . a| * halfZ
     + clearance
```

The hull fits the passage cross-section only when projected size plus center offset stays within both passage half extents.

This is O(1) per candidate and allocation-free.

A reflected orientation basis fails closed because it would invert roll/up semantics needed by narrow passages and docking.

## Why the old conservative sphere is insufficient

For a flat ship:

```text
half extents = 4 x 1 x 6 m
```

its conservative sphere radius is more than 7 m. A flat aperture only 3 m high would therefore be rejected by a sphere even though the real hull fits with its thin axis aligned to the aperture height.

The precision evaluator records both answers:

```text
oriented hull fits = true
conservative sphere fits = false
```

This is intentional. Sphere broadphase remains safe for finding possible conflicts; precision geometry recovers valid motion that the sphere would conservatively discard.

## Bounded two-obstacle gap candidate builder

Candidate code:

```text
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h
src/world/navigation/trajectory/BoundedGapCandidateBuilder.cpp
```

The builder receives:

```text
one primary conflict witness
already reduced local neighbor witnesses
one snapshot revision
reference point + travel direction
bounded search policy
```

It does **not** discover all obstacle pairs.

Complexity is:

```text
O(local_neighbors * hardCandidateLimit)
hardCandidateLimit = 8
```

The constant factor stays bounded because only the deterministic best eight candidates are retained.

Each `ObstacleWitness` currently carries a conservative center/radius plus snapshot revision. The pair is accepted only when:

- both witnesses are valid and come from the same snapshot revision;
- conservative inflated bounds do not overlap;
- clear separation lies inside the configured gap-width interval;
- pair separation is sufficiently transverse to requested travel, so front/back obstacles are not misclassified as a slot;
- gap center lies inside the bounded forward and centerline windows.

The builder returns compact `ObstacleGap` products. It never claims that the complete maneuver is feasible.

## Two-obstacle gap semantics

`ObstacleGap` carries:

```text
center
travel direction through the gap
separation axis between the constraining boundaries
clear separation
secondary clearance
```

`OrientedPassageEvaluator::makeObstacleGapPassage()` converts that product into a normal passage frame.

The secondary clearance is not invented from the obstacle pair: it must come from already reduced local/static evidence. Two objects constrain one passage-plane direction; the surrounding free-space owner remains authoritative for the orthogonal direction.

## Attitude reachability before entry

Candidate code:

```text
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.cpp
```

A passage may fit geometrically but still be unusable because the ship is too fast and too close to rotate into the required attitude.

The evaluator consumes:

```text
current body orientation
required passage orientation
current angular velocity
max angular acceleration
max angular speed
distance to passage entry
closing speed
available longitudinal braking acceleration
```

It computes a conservative minimum attitude-ready time.

The required rest-to-rest angular move uses bounded angular acceleration/rate. Existing angular velocity is conservatively settled first because it may be around an unhelpful axis; settle time and possible orientation drift consume additional margin.

Longitudinal result classes are:

```text
AlreadyReady
    orientation and angular rate are already acceptable

ReachableCoast
    required attitude is ready before entry without longitudinal braking

ReachableWithBraking
    coasting would reach the entry too soon, but available braking creates enough time

UnreachableBeforeEntry
    even maximum declared braking cannot keep the ship before the entry until attitude is ready
```

This is still a precheck, not the complete maneuver proof. It does not replace flight control or thruster allocation.

## Kinematic trigger

An `ObstacleGap` becomes especially important when normal avoidance has run out of room.

Conceptually:

```text
if ordinary avoidance proves a safe side-step:
    use it
else if collision/hold is near and bounded local gaps exist:
    for each of at most 8 candidates:
        test oriented hull fit
        if pose fits:
            test attitude reachability before entry
            if reachability is coast/braking feasible:
                submit to full 6DoF swept-body trajectory proof
                if full trajectory is safe:
                    use passage maneuver
    if no candidate survives:
        fail closed / maximum braking / emergency response
else:
    fail closed / maximum braking / emergency response
```

Therefore "avoidance impossible" does not automatically mean "collision unavoidable". A passage through the free space between conflicts is a separate maneuver class, but it is accepted only if physical timing also works.

## Dynamic gaps

Two moving objects can also form a time-varying gap. That is **not** proven by the current static-witness builder or geometry/reachability prechecks.

The later 6DoF layer must evaluate the gap at time `t` using predicted obstacle poses/bounds and prove that:

- the gap remains sufficiently open during the complete crossing;
- the ship can reach the required attitude before entry;
- relative motion does not close the gap onto the hull;
- uncertainty/result age is included in clearance;
- the swept oriented hull remains collision-free for the complete maneuver.

This naturally generalizes moving/rotating docking corridors.

## Behavior fixtures

Target-machine suite:

```text
tests/navigation_trajectory/
```

Pinned geometry fixtures include:

```text
flat hull + flat slot, correct attitude
    -> Fits
    -> conservative sphere rejects

same hull rolled 90 degrees
    -> rejected by aperture height

two nearby obstacles -> narrow ObstacleGap
    wide attitude -> rejected
    rolled thin attitude -> Fits

correct attitude but excessive lateral offset
    -> rejected

degenerate gap frame
    -> InvalidInput / fail closed
```

Pinned bounded-gap fixtures include:

```text
one primary + one neighbor -> one gap
no neighbor-neighbor pair generation
hard cap of 8 candidates
deterministic top-K ordering
mixed snapshot revision -> reject
overlapping conservative bounds -> reject
front/back pair -> not a transverse slot
built gap -> oriented precision evaluator
far-side irrelevant gap -> reject
```

Pinned attitude-reachability fixtures include:

```text
already aligned -> AlreadyReady
90 degree roll + enough distance -> ReachableCoast
same roll + less distance -> ReachableWithBraking
same roll + high speed / too little distance -> UnreachableBeforeEntry
existing angular motion consumes additional settle margin
zero angular authority -> InvalidInput / fail closed
```

Architecture contracts:

```text
tests/architecture_contracts/check_navigation_trajectory_passage.py
tests/architecture_contracts/check_navigation_trajectory_gap.py
tests/architecture_contracts/check_navigation_trajectory_reachability.py
```

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

## Performance measurement

Dedicated bounded-gap builder harness:

```text
benchmarks/navigation_trajectory_gap/
```

It measures two classes at `16/64/256/1024` local neighbors:

```text
reject_N
    cheap rejection scan

top8_N
    many plausible pair candidates; continuously maintain deterministic best 8
```

`1024` is deliberate stress and is not an expected normal precision-fallback input.

The existing main-thread local-navigation design budget remains:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Do not optimize the precision fallback before target-machine evidence shows a need.

## What is not claimed yet

The current candidate still does **not** prove:

- body-axis thrusters can execute every required translational correction;
- a continuous swept hull stays clear while rotating and translating;
- moving obstacles keep the gap open;
- `Elite` versus `Newton` control policy chooses/executes the same maneuver identically;
- a complete moving/rotating docking capture trajectory is feasible.

Those belong to the next vehicle-capability and continuous-sweep slices. The current candidate establishes three narrower facts:

1. nearby obstacles can form a bounded positive passage candidate;
2. hull orientation can turn a sphere-rejected apparent no-route case into a geometric fit;
3. high speed / short distance can still reject that gap when the required attitude cannot be achieved in time.
