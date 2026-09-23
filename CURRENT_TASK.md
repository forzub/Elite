# CURRENT TASK — rebuild and validate corrected navigation M1

Date: 2026-09-23

Status: **M1 CORRECTION COMPLETE / TARGET EVIDENCE REQUIRED**

The first target attempt found one obsolete E2E write to
`ScenarioRunSettings::pilot`. It has been removed; the test already supplies
the correct resolved `pilotExecutionProfile`. The runtime output produced after
that compile failure was from a stale binary and must not be used as evidence.

## Required target gate

Run the following as one chained command in MSYS2 MinGW64. Do not run CTest if
any earlier step fails:

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

Return the complete output beginning with the printed commit hash.

## Required M1 evidence

The rebuilt executable must print:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

Interpretation:

- compilation fails: repair the reported compile defect and keep M1 open;
- the marker is absent after a successful rebuild: verify test registration and
  binary provenance; do not infer a frame result;
- the marker fails: diagnose the coordinate/frame chain and keep M1 open;
- the marker passes but a later physical case fails: accept M1 separately and
  retain the physical-authoring failure for M3/M4;
- the complete suite passes: accept M1 and activate M2.

Do not weaken the frame fixture, restore broad `pilot` input, tune physical
control, or start the M2 split before this evidence is classified.
