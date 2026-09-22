# CURRENT TASK — Target-validate strict API purity, then resume physical maneuver compiler

Date: 2026-09-22

Status: **SECOND API AUDIT IMPLEMENTED / TARGET BUILD REQUIRED**

Code baseline before documentation commits:

```text
efb3999b71a18186c1e3622c6c51d9b0169b52be
```

## Why the second audit was necessary

The previous cleanup made top-level inputs explicit but still left some
calculation helpers with oversized context objects and a few private behavior
thresholds.

A concrete refactor error was also found: an undeclared `terminal.*` alias had
leaked outside the two helpers that explicitly receive a terminal endpoint.

That is now fixed and statically guarded.

## Active rule

```text
NO hidden data source.
NO ambient clock.
NO implicit file/config read.
NO concrete ship lookup inside navigation.
NO behavioral magic number inside a calculation kernel.
NO oversized Scenario/Settings argument merely to pick a few fields.

orchestration resolves -> narrow API -> calculation -> explicit result
```

## Important code changes

- one `ResolvedRunKinematics` per public Stage-1/Stage-2 call;
- narrow route-clearance API;
- narrow vehicle-profile API;
- explicit attitude-author initial/terminal state;
- explicit accepted-program metadata;
- explicit `ExecutionVehicleInit`;
- narrow execution trace helper;
- hidden trajectory thresholds moved to TrajectoryGenerationPolicy;
- RuntimePlanner hold urgency/emergency doctrine moved to Policy;
- stale reacquisition wording removed from E2E tests;
- new `NAVIGATION_API_CONTRACT.md`;
- expanded `check_navigation_api_purity.py`.

## Target gate

Run exactly:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh

ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

The first script must now fail immediately if a pinned API-purity contract is
broken.

Do not interpret a Stage-2 physical E2E failure as an API-purity failure if the
architecture checker and build pass. The known physics blocker remains:
trajectory timing precedes full hull/thrust feasibility.

## After API/build green

Resume physical maneuver compiler in this order:
1. geometry/tangent candidate;
2. required force vector;
3. reachable hull attitude/omega/alpha;
4. installed main + RCS allocation;
5. throttle slew/resource constraints;
6. lead rotation / braking boundary;
7. only then Ruckig timing;
8. prove and publish ActuatorSegments;
9. Autopilot executes the proved schedule.

Do not loosen follower gains or tracking-loss timeout.
Dynamic avoidance remains disabled.
