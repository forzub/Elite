# CURRENT TASK — rebuild M1 after TrajectoryGenerator cleanup

Date: 2026-09-23

Status: **SECOND COMPILE CORRECTION COMPLETE / TARGET EVIDENCE REQUIRED**

The previous target run confirms the stale `ScenarioRunSettings::pilot` blocker
is gone. It then exposed dead implementation references left after wall-clock
and filesystem performance diagnostics were removed from the pure trajectory
generator. Those references and an unused helper input have been removed
without changing trajectory behavior.

## Required target gate

Run this as one command in MSYS2 MinGW64. Keep the printed commit hash and do
not run CTest if any build step fails:

```bash
cd /d/__elite/work && \
git pull --ff-only && \
git rev-parse HEAD && \
bash tests/navigation_runtime/run_stage1_mingw64.sh && \
cmake --build build/tools/navigation_runtime \
  --target navigation_runtime_pipeline_tests && \
ctest --test-dir build/tools/navigation_runtime \
  -R "^navigation_runtime_pipeline$" \
  --output-on-failure
```

Return the complete output beginning with `git rev-parse HEAD`.

## Required classification

First establish:

1. `TrajectoryGenerator.cpp` compiles and the pipeline executable links;
2. the rebuilt executable prints:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

Then classify any later failure independently:

- failure before the marker keeps M1 open;
- marker PASS accepts the M1 coordinate/frame boundary even if the later known
  physical-authoring E2E fails;
- full pipeline PASS accepts M1 and permits activation of M2.

Do not restore removed timing diagnostics, reintroduce internal file I/O, weaken
the frame fixture, or tune physical control while resolving this gate.
