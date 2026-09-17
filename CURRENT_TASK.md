# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — moving / time-varying gap prediction gate

## Newly closed gate — emergency contact severity

Target-machine run on `7f1bccd4e8b91c72e4fc5f9e6d1329260790aa8e`:

```text
NAVIGATION TRAJECTORY EMERGENCY CONTACT SEVERITY CONTRACT: PASS
navigation_trajectory_passage                       PASS
navigation_trajectory_gap                           PASS
navigation_trajectory_reachability                  PASS
navigation_trajectory_emergency_passage             PASS
navigation_trajectory_continuous_passage            PASS
navigation_trajectory_emergency_contact_severity    PASS
100% tests passed, 0 failed out of 6
Total Test time: 0.27 sec
```

`EmergencyContactSeverityScorer` is **behavior/architecture accepted**. Keep its hard bound `<=8 candidates x <=4 witnesses`. Do not benchmark it separately unless later live composition shows material cost.

## Active candidate — `MovingGapPredictor`

Public contract:

```text
src/world/navigation/MOVING_GAP_MODEL.md
```

Code:

```text
src/world/navigation/trajectory/MovingGapPredictor.h
src/world/navigation/trajectory/MovingGapPredictor.cpp
```

Test/architecture gate:

```text
tests/navigation_trajectory/NavigationTrajectoryMovingGapTests.cpp
tests/architecture_contracts/check_navigation_trajectory_moving_gap.py
```

### Ownership

The predictor receives **one pair already selected by the accepted bounded gap stage**.

It does not:

```text
discover pairs
scan NavigationMap / NavigationSpace
run all-pairs work
synthesize the ship trajectory
claim CCD/TOI/contact response
```

### Motion model

Each boundary supplies:

```text
P / V / A
angular velocity
conservative radius
snapshot revision
```

Center prediction:

```text
p(t) = p0 + v0*t + 0.5*a*t^2
v(t) = v0 + a*t
```

Mixed revisions fail closed before prediction.

### Fixed bounded work

```text
33 samples
32 continuous intervals
one already-selected pair
```

Per sample the predictor publishes:

```text
gap center
gap-center velocity
separation axis
clear separation / separation rate
primary + secondary surface point
normal toward free space
surface material velocity = v_center + omega x r
```

The boundary surface output is suitable for later composition into the already accepted `EmergencyContactSeverityScorer` witness contract. The predictor itself does not invent a ship contact point.

### Continuous gap proof

Samples alone are not accepted.

For each constant-relative-acceleration interval:

```text
relative trajectory deviation from endpoint chord
    <= |a_rel| * dt^2 / 8
```

The exact minimum distance from origin to that chord segment minus this deviation provides a conservative whole-interval center-separation lower bound. Inflated radii are then subtracted to obtain the continuous free-gap lower bound.

This pins the important failure case:

```text
sample i      gap open
between       gap closes
sample i+1    gap open again

=> GapClosesDuringHorizon
```

Transverse alignment is also bounded continuously from the quadratic `dot(r(t), travel)` numerator and the continuous center-distance lower bound. A pair that rotates into the travel axis returns `AlignmentLost`.

### Pinned fixtures

```text
static transverse pair -> OpenForHorizon
co-moving pair -> moving gap center with preserved width
between-sample hidden closure -> GapClosesDuringHorizon
pair rotates longitudinally -> AlignmentLost
rotating boundary -> surface velocity includes omega x r
mixed revisions -> RevisionMismatch
relative acceleration -> future gap width/rate change
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_moving_gap.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Expected suite after build: `7/7`.

## Next after green gate

1. behavior-accept/freeze the bounded `MovingGapPredictor`;
2. build the moving continuous passage verifier that combines these 33 time-varying gap states with the ship's continuous P/V/attitude/hull sweep and physical authority;
3. reuse the same moving-frame machinery for translating/rotating docking ports;
4. add explicit bottom-to-bottom terminal mating constraints and relative velocity/angular-rate capture gates;
5. add deterministic `PilotSkillProfile` execution;
6. integrate accepted Navigation v2 into live game/server/guidance;
7. run end-to-end stress/debug acceptance;
8. retire legacy route-wide navigation only after the live v2 path is stable.

## Definition of final success

Navigation v2 is finished only when the live runtime demonstrates normal flight, static/dynamic avoidance, static and moving narrow oriented passage, truthful Elite/Newton authority, least-severity unavoidable collision handling, post-impact replanning, moving/rotating docking, shared guidance/debug truth and intended NPC scaling without planner stalls or unbounded precision work.
