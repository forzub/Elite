# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — continuous static-passage gate active

## Accepted preconditions

### `NAV-V2-LOCAL-1` — CLOSED

Target-machine multiplied-probe benchmark on `e0817d157ba5d8c9c329576236310507bda13364` is accepted. Worst deliberate stress:

```text
all_dynamic_rejected_1024 p95 = 519.9286 us
```

This remains below the `<1.0 ms normal peak` budget. Keep the 16-probe fan unchanged.

### Oriented passage / bounded gap / attitude reachability — ACCEPTED

Target-machine C++ suite on `e0817d...` passed:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
```

Bounded-gap builder performance is accepted:

```text
top8_1024 p95 = 24.0699 us
```

### Emergency passage mitigation — ACCEPTED

Fresh target-machine evidence on `b29a03d3d84f4d6575cbbc5166cbd7547b5ce0d8`:

```text
NAVIGATION TRAJECTORY BOUNDED GAP CONTRACT: PASS
NAVIGATION TRAJECTORY EMERGENCY PASSAGE CONTRACT: PASS

navigation_trajectory_passage              PASS
navigation_trajectory_gap                  PASS
navigation_trajectory_reachability         PASS
navigation_trajectory_emergency_passage    PASS

100% tests passed, 0 failed
Total Test time: 0.18 sec
```

Accepted semantic:

```text
no provably safe maneuver
    != no navigation command
```

If stopping is impossible, navigation may emit `EmergencyMitigatedContact`: brake, aim at the gap, align travel with the passage axis as far as possible, choose the best reachable hull attitude, and hand the expected contact/ricochet to physics/damage.

## Active Gate — continuous static passage feasibility

Architecture authority:

```text
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
```

Candidate code:

```text
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.h
src/world/navigation/trajectory/ContinuousPassageTrajectoryEvaluator.cpp
```

Tests/contracts:

```text
tests/navigation_trajectory/NavigationTrajectoryContinuousPassageTests.cpp
tests/architecture_contracts/check_navigation_trajectory_continuous_passage.py
```

### Analytic candidate segment

```text
translation
    cubic Hermite: start P/V -> end P/V

orientation
    shortest rotation arc
    smooth rest-to-rest law s(u)=3u^2-2u^3
```

Exact candidate peak angular requirements:

```text
omega_peak = 1.5 * angle / T
alpha_peak = 6.0 * angle / T^2
```

### Continuous geometry proof

The fixed partition is:

```text
33 pose samples
32 intervals
```

A segment is **not** accepted merely because those 33 poses fit.

Each interval receives conservative continuous inflation:

```text
center curve deviation <= M * dt^2 / 8
rotation sweep inflation <= 2 * R * sin(deltaTheta / 2)
```

This must reject the fixture where both endpoint orientations fit but the hull clips the passage wall during the intermediate roll.

### Body-axis authority

Required Hermite map-space acceleration is projected into the rotating hull frame and checked against declared capability:

```text
forward
reverse / braking
lateral
vertical
```

Between sample endpoints the projection receives a conservative bound from acceleration-vector change plus body-axis rotation.

A clear geometric curve may therefore still return:

```text
LinearAuthorityExceeded
```

### `Elite` / `Newton`

```text
Newtonian
    velocity and attitude may diverge

EliteAssisted
    identical truthful physical thrust limits
    plus supplied controller-policy max velocity/forward slip angle
```

Assisted mode is not allowed to manufacture extra acceleration.

### Result classes

```text
Feasible
GeometryBlocked
LinearAuthorityExceeded
AngularAuthorityExceeded
AssistedSlipExceeded
InvalidInput
```

Pinned fixtures include:

```text
straight centered segment -> Feasible
endpoint-fit / mid-roll wall clip -> GeometryBlocked
same roll / wider slot -> Feasible
insufficient lateral thrust -> LinearAuthorityExceeded
sufficient lateral thrust -> Feasible
insufficient angular rate -> AngularAuthorityExceeded
sideways inertial travel: Newton accepted / Elite slip policy rejected
zero duration -> InvalidInput
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_continuous_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Send complete output.

## Next after green gate

1. add a dedicated microbenchmark for the 33-sample / 32-interval continuous verifier;
2. if timing is comfortably bounded, keep the analytic verifier unchanged;
3. add emergency candidate ranking by predicted **relative normal contact speed / impact-energy proxy**, so a glancing/ricochet contact is preferred over a normal hit;
4. generalize static passage state to time-varying obstacle gaps;
5. reuse the same moving-frame trajectory machinery for moving/rotating docking with bottom-to-bottom mating semantics;
6. then add NPC `PilotSkillProfile` execution and only later live `EliteGame` / `EliteServer` integration.
