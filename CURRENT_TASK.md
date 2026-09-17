# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-LOCAL-1` — compact-candidate scaling accepted; conservative lateral avoidance behavior gate pending

## Closed local reference gates

`LocalHorizonPlanner` behavior is accepted and its compact-candidate scaling is accepted.

Target-machine reference on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass compact-candidate loop is not a bottleneck. Do not optimize it further without new runtime evidence.

## Active candidate — `LocalAvoidancePlanner`

The first bounded adjusted-target slice composes:

```text
LocalHorizonPlanner
+ compact NavigationMap::QueryResult
+ public NavigationSpace::queryPoint()
```

For nominal `ConflictHold` it probes at most 16 deterministic lateral targets:

```text
15 deg x 8 azimuths
30 deg x 8 azimuths
```

A target must be envelope-safe in the same NavigationSpace region and must use the same static space/source revision as the start-point evidence. It is then rechecked through the accepted dynamic reference. Current-kinematics head-on/crossing conflicts remain fail-closed; a changed target alone cannot erase them.

## Latest target-machine attempt — infrastructure failure, not avoidance failure

Run on `f27006ccbba1a6faaa3d3000dedf8d8dee5f02e8` produced:

```text
check_navigation_local_boundary.py
    FAIL: local horizon ownership documentation missing: already reduced

check_navigation_local_avoidance.py
    PASS

navigation_local
    PASS

navigation_local_avoidance
    NOT RUN: executable not found
```

Diagnosis:

1. the old horizon architecture check was coupled to the exact README phrase `already reduced`; the README had been consolidated to `compact dynamic products` while the actual ownership contract remained unchanged;
2. `tests/navigation_local/run_mingw64.sh` configured both CTest executables but explicitly built only the historical `navigation_local_tests` target, so CTest registered `navigation_local_avoidance` without its executable existing.

Fix now on `main`:

- horizon contract checks semantic ownership markers rather than the stale exact README phrase;
- the header still explicitly pins `already reduced NavigationMap products`;
- local test runner now builds the complete CMake project before CTest, so every registered local executable is produced.

No avoidance behavior acceptance or rejection is inferred from the failed run because the avoidance executable did not execute.

## Trajectory/control requirements recorded for the next stage

New architecture contract:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

Current local navigation remains intentionally conservative: controlled/dynamic actors are represented by radius + swept sphere and do not yet carry hull attitude/control-mode/pilot-skill semantics.

The planned trajectory-aware stage must separate:

```text
world geometry/state
vehicle capability: hull shape, attitude, angular state, thrust authority
flight-control mode: assisted Elite vs Newtonian free-flight behavior
pilot skill: reaction delay, decision rate, input smoothing, damping/overshoot, precision
actual control execution / physics
```

This is where airplane-like steering, free attitude/velocity decoupling, flip-and-burn, rotation time before braking, oriented swept hull checks and deterministic low-skill NPC oscillation belong.

Do not stuff these into the current static/dynamic broadphase or same-region target fan.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_local_boundary.py
python tests/architecture_contracts/check_navigation_local_avoidance.py
bash tests/navigation_local/run_mingw64.sh
```

Send the complete output.

## Decision after behavior gate

If architecture + both local executables PASS:

1. accept the same-region avoidance behavior slice;
2. add a dedicated avoidance benchmark for nominal-clear, early-adjust, all-static-rejected and all-dynamic-rejected cases;
3. decide whether the 16-probe fan needs any ordering/budget change;
4. then begin the trajectory-aware vehicle/control layer defined in `TRAJECTORY_CONTROL_MODEL.md`, including head-on/crossing maneuver feasibility.

Do not wire live `EliteGame` / `EliteServer` or pursuit-specific intercept logic before these gates pass.
