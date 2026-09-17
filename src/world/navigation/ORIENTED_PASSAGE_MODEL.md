# Navigation v2 — oriented passage / emergent gap model

**Status:** first precision-geometry candidate; behavior gate pending target-machine build/test  
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
                |  primary conflict + nearby relevant boundaries
                |  no unbounded all-pairs scan
                |  small candidate budget (initial target <= 4-8)
                v
        OrientedPassageEvaluator
                |  O(1) projected OBB fit per candidate
                v
        only plausible passages
                |
                v
        full 6DoF trajectory / swept-body feasibility
```

A naive `N x N` obstacle-pair search on the frame path is explicitly rejected.

Candidate generation should use already-reduced local data: the primary conflict, neighboring static surfaces, spatial-bin adjacency, authored aperture metadata, or a similarly bounded source. The exact candidate-builder backend remains open until measured.

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

## First geometry boundary

Current candidate code:

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
    orthonormal body orientation basis

Passage
    center
    orthonormal passage frame
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

## Two-obstacle gap semantics

`ObstacleGap` is a compact product of a future bounded gap-candidate builder. It carries:

```text
center
travel direction through the gap
separation axis between the constraining boundaries
clear separation
secondary clearance
```

`OrientedPassageEvaluator::makeObstacleGapPassage()` converts that product into a normal passage frame.

Important ownership rule: the evaluator does **not** scan scene objects or discover pairs. It consumes already-selected local boundaries. This keeps the precision math deterministic and cheap while leaving candidate extraction free to use static adjacency, spatial bins or other measured backends.

## Kinematic trigger

An `ObstacleGap` becomes especially important when normal avoidance has run out of room.

Conceptually:

```text
if ordinary avoidance proves a safe side-step:
    use it
else if collision/hold is near and a bounded local gap exists:
    test passage orientation
    if pose can fit:
        test whether current vehicle can rotate/translate into that pose in time
        if full swept trajectory is safe:
            use passage maneuver
        else:
            fail closed / brake / emergency response
else:
    fail closed / brake / emergency response
```

Therefore "avoidance impossible" does not automatically mean "collision unavoidable". A passage through the free space between conflicts is a separate maneuver class.

## Dynamic gaps

Two moving objects can also form a time-varying gap. That is **not** proven by the current geometry-only evaluator.

The later 6DoF layer must evaluate the gap at time `t` using predicted obstacle poses/bounds and prove that:

- the gap remains sufficiently open during the complete crossing;
- the ship can reach the required attitude before entry;
- relative motion does not close the gap onto the hull;
- uncertainty/result age is included in clearance;
- the swept oriented hull remains collision-free for the complete maneuver.

This naturally generalizes moving/rotating docking corridors.

## Initial behavior fixtures

`tests/navigation_trajectory/NavigationTrajectoryPassageTests.cpp` pins:

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

Architecture contract:

```text
tests/architecture_contracts/check_navigation_trajectory_passage.py
```

Target-machine runner:

```text
tests/navigation_trajectory/run_mingw64.sh
```

## What is not claimed yet

This first slice does **not** yet prove:

- rotation can be completed before reaching the gap;
- body-axis thrusters can execute the required translation;
- a continuous swept hull stays clear while rotating;
- moving obstacles keep the gap open;
- a complete docking capture trajectory is feasible.

Those belong to the next trajectory-aware capability/continuous-sweep slices. The current candidate intentionally isolates and tests the geometric fact that **orientation can turn an apparent collision/no-route case into a valid passage**.
