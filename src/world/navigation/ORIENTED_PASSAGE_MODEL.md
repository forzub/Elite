# Navigation v2 — oriented passage / emergent gap model

**Status:** geometry, bounded-gap behavior, attitude reachability and gap-builder performance measured on target machine; emergency contact-mitigation candidate prepared  
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

A second accepted semantic rule is equally important:

```text
no collision-free proof != no navigation command
```

If the ship is physically too close/fast to become collision-free, navigation/control continues with an explicit **emergency impact-mitigation intent**. It may accept contact/ricochet as preferable to planner shutdown or a worse head-on impact. Such a result is never labeled safe; collision response and damage remain authoritative downstream physics responsibilities.

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
                |  coast / brake / unreachable-safe-pose
                v
        +-- safe entry pose can still be proven
        |       -> full 6DoF trajectory / swept-body feasibility
        |
        +-- safe entry pose cannot be proven in time
                -> EmergencyPassageMitigator
                   fixed reachable-attitude sampling
                   max useful braking
                   aim at gap center
                   travel intent along passage axis
                   explicit contact-expected result when unavoidable
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

A passage may fit geometrically but still be unreachable as a **collision-free entry pose** because the ship is too fast and too close to rotate into the required attitude.

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
    even maximum declared braking cannot keep the ship before the entry until the requested safe attitude is ready
```

`UnreachableBeforeEntry` is **not** a command to disable navigation. It means only that this evaluator cannot prove the requested collision-free attitude before the entry plane. The emergency layer may still produce a physically meaningful best-effort maneuver.

This is still a precheck, not the complete maneuver proof. It does not replace flight control or thruster allocation.

## Emergency passage / impact mitigation

Candidate code:

```text
src/world/navigation/trajectory/EmergencyPassageMitigator.h
src/world/navigation/trajectory/EmergencyPassageMitigator.cpp
```

When safe entry cannot be proven in time, the system still returns an active command intent whenever the inputs are valid.

The emergency policy is:

```text
1. keep trying to rotate toward the preferred passage attitude;
2. use maximum useful longitudinal braking when it buys time or lowers impact energy;
3. aim translation at the gap center;
4. bias desired travel along the passage axis to reduce side-normal incidence;
5. evaluate only physically reachable attitudes before the entry plane;
6. choose the reachable attitude with the best passage clearance / smallest geometric deficit;
7. if stopping before entry is physically possible, stop instead of intentionally colliding;
8. if stopping is impossible and no collision-free entry pose exists, continue with
   EmergencyMitigatedContact rather than returning no navigation command.
```

The initial implementation samples a fixed `17` attitudes along the shortest orientation arc from current pose toward the preferred passage pose. This is deliberately constant-size and avoids assuming that OBB clearance is monotonic along an arbitrary 3D rotation.

Emergency result classes are:

```text
SafeEntryPose
    a physically reachable sampled entry attitude fits the passage cross-section
    (continuous swept-body safety still belongs to the next solver)

EmergencyStopBeforeEntry
    no sampled entry pose fits, but maximum braking can stop the ship before contact
    navigation remains active and can replan from the stopped/slow state

EmergencyMitigatedContact
    collision-free entry and pre-entry stop are both unavailable
    navigation keeps issuing best-effort intent
    contact is explicitly expected, never mislabeled Clear/safe

InvalidInput
    no physical/geometric command may be claimed
```

For `EmergencyMitigatedContact`, collision and ricochet are allowed outcomes. Navigation supplies intent; physics/collision computes the actual contact/CCD/TOI/impulse/ricochet and damage/structural systems process the result. The next navigation solve consumes the actual post-contact state rather than pretending the emergency trajectory stayed collision-free.

The first mitigation score uses passage-plane clearance deficit. The later continuous 6DoF layer must additionally minimize **relative normal contact speed** against the constraining boundary. Desired travel along the passage axis and aim at the gap center are already exposed explicitly for that purpose.

## Kinematic trigger

An `ObstacleGap` becomes especially important when normal avoidance has run out of room.

Conceptually:

```text
if ordinary avoidance proves a safe side-step:
    use it
else if collision/hold is near and bounded local gaps exist:
    for each of at most 8 candidates:
        build oriented passage
        test geometric hull fit / candidate attitudes
        test attitude reachability before entry
        if a collision-free entry pose is reachable:
            submit to full 6DoF swept-body trajectory proof
            if full trajectory is safe:
                use safe passage maneuver
        else:
            build emergency passage intent

    choose best safe maneuver if one exists
    otherwise choose best emergency mitigation candidate

    emergency candidate:
        if can stop before entry -> brake/stop/replan
        else -> brake + center on gap + align travel with passage axis
                + rotate to best reachable attitude
                + allow explicit contact/ricochet
else:
    maximum braking / least-severity emergency response
```

Therefore "avoidance impossible" does not automatically mean "collision unavoidable", and "collision unavoidable" does not mean "navigation unavailable".

## Dynamic gaps

Two moving objects can also form a time-varying gap. That is **not** proven by the current static-witness builder or geometry/reachability prechecks.

The later 6DoF layer must evaluate the gap at time `t` using predicted obstacle poses/bounds and prove or mitigate:

- whether the gap remains sufficiently open during the complete crossing;
- how much attitude correction is physically reachable before/during entry;
- whether relative motion closes the gap onto the hull;
- uncertainty/result age in clearance;
- the complete swept oriented hull;
- predicted relative normal speed for any unavoidable contact;
- post-contact continuation after a real ricochet/impact.

This naturally generalizes moving/rotating docking corridors, although docking should normally abort/go-around rather than intentionally use a destructive contact unless the game explicitly enters an emergency collision case.

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
zero angular authority -> InvalidInput for collision-free reachability proof
```

Pinned emergency-passage fixtures include:

```text
enough room -> SafeEntryPose
full preferred roll too late but reachable partial roll already fits -> SafeEntryPose
safe roll unreachable -> EmergencyMitigatedContact / command remains active
emergency intent -> gap center + passage axis
can stop before an impossible slot -> EmergencyStopBeforeEntry / no intentional impact
zero angular authority -> braking/contact mitigation still returns an active command
```

Architecture contracts:

```text
tests/architecture_contracts/check_navigation_trajectory_passage.py
tests/architecture_contracts/check_navigation_trajectory_gap.py
tests/architecture_contracts/check_navigation_trajectory_reachability.py
tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
```

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

## Target-machine evidence — 2026-09-17

On public commit `e0817d157ba5d8c9c329576236310507bda13364` the MinGW64 trajectory suite built successfully and all three then-registered tests passed:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time 0.12 sec
```

The passage and attitude-reachability architecture contracts passed. The bounded-gap architecture checker reported one documentation-string mismatch (`small candidate budget (initial target <= 4-8)`), while the bounded-gap C++ test itself passed. The checker is repaired to test stable architecture markers rather than that obsolete exact sentence; no runtime algorithm failure was observed.

## Performance measurement — ACCEPTED

Dedicated bounded-gap builder harness:

```text
benchmarks/navigation_trajectory_gap/
```

Target-machine p95 evidence on `e0817d157ba5d8c9c329576236310507bda13364`:

```text
scenario       p95_us
reject_16       0.2459
reject_64       0.7550
reject_256      4.6499
reject_1024    11.6730

top8_16         0.7883
top8_64         1.8260
top8_256        6.3609
top8_1024      24.0699
```

`1024` is deliberate stress and is not an expected normal precision-fallback input. Even continuous top-8 maintenance over 1024 already-reduced neighbors remains only `0.0241 ms p95`, far below the existing main-thread local-navigation design budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

The bounded gap builder is therefore performance-accepted. Do not optimize it further without contrary runtime evidence.

## What is not claimed yet

The current candidate still does **not** prove:

- body-axis thrusters can execute every requested translational correction toward the gap center/axis;
- a continuous swept hull stays clear while rotating and translating;
- moving obstacles keep the gap open;
- exact minimum-damage impact orientation/relative normal speed under full 6DoF dynamics;
- `Elite` versus `Newton` control policy chooses/executes the same maneuver identically;
- a complete moving/rotating docking capture trajectory is feasible.

Those belong to the next vehicle-capability and continuous-sweep slices. Current accepted/prepared facts are:

1. nearby obstacles can form a bounded positive passage candidate;
2. hull orientation can turn a sphere-rejected apparent no-route case into a geometric fit;
3. high speed / short distance can invalidate a requested collision-free entry attitude;
4. that invalidation does **not** disable navigation: an explicit emergency stop or mitigated-contact command can still be produced;
5. contact/ricochet may be an intentional least-severity emergency outcome, while physics/damage remain authoritative for the actual impact.
