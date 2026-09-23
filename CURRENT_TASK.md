# CURRENT TASK — target-validate navigation blueprint M1

Date: 2026-09-23

Status: **M1 CODE CANDIDATE COMPLETE / TARGET EVIDENCE REQUIRED**

Primary specification:

```text
src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md
```

## What changed

- the private runtime `toSystemIntent()` bypass was removed;
- control intent uses `NavigationFrameBoundary`;
- initial position, velocity, attitude basis and angular-velocity state cross
  the same boundary;
- runtime frame snapshot and epoch are explicit inputs;
- execution deadline includes that non-zero universe-time epoch;
- the moving/accelerating/rotating frame is advanced during execution;
- Follower, trace and terminal checks receive NavLocal state rather than system
  vectors mislabeled as map vectors;
- a non-identity product-chain equivalence E2E was added;
- brittle architecture checks were made formatting-independent and stale
  hardware assumptions were corrected.

## Required target gate

Run from MSYS2 MinGW64:

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

bash tests/navigation_runtime/run_stage1_mingw64.sh

cmake --build build/tools/navigation_runtime \
  --target navigation_runtime_pipeline_tests

ctest --test-dir build/tools/navigation_runtime \
  -R "^navigation_runtime_pipeline$" \
  --output-on-failure
```

The output must contain:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

## Interpretation

- compile failure: fix M1; do not advance;
- non-identity test failure: frame conversion/evolution is still wrong; fix M1;
- non-identity test passes but a later pre-existing physical E2E fails: record
  both facts separately; M1 may be accepted while the physical-planner failure
  remains assigned to M3/M4;
- all gates pass: mark M1 accepted and activate M2.

## Next stage after acceptance

M2 splits `NavigationScenarioRuntime.cpp` into a thin composition root plus
production-owned parsing, route composition, maneuver planning/proof,
execution-harness and diagnostics modules. Do not begin that split before M1
compile/runtime evidence is recorded.
