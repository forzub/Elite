# CURRENT TASK — validate canonical page timeline and M1 frame invariance

Date: 2026-09-23

Status: **THIRD M1 CORRECTION COMPLETE / TARGET EVIDENCE REQUIRED**

The target now builds and links. Its first real M1 execution exposed duplicate
storage-page time ownership: runtime selected page 1 just before that page's own
start, and Sampler correctly returned `BeforeStart`.

`ManeuverProgramTimeline` now owns page windows, page-local elapsed time and
active-page selection. Runtime, Sampler, Follower and phase gate consume it.

The blueprint has additionally incorporated the later navigation-map,
portal/risk and pilot-skill requirements. That documentation does not change
this immediate M1 gate: validate the already implemented timeline/frame
candidate before beginning M2 or any octree/portal implementation.

## Required target gate

Run as one chained command in MSYS2 MinGW64:

```bash
cd /d/__elite/work && \
git pull --ff-only && \
git rev-parse HEAD && \
bash tests/navigation_runtime/run_stage1_mingw64.sh && \
cmake --build build/tests/navigation_runtime \
  --target maneuver_program_sampler_tests && \
ctest --test-dir build/tests/navigation_runtime \
  -R "^maneuver_program_sampler$" \
  --output-on-failure && \
cmake --build build/tools/navigation_runtime \
  --target navigation_runtime_pipeline_tests && \
ctest --test-dir build/tools/navigation_runtime \
  -R "^navigation_runtime_pipeline$" \
  --output-on-failure
```

Return the complete output beginning with the commit hash.

## M1 acceptance evidence

Required marker:

```text
[PASS] non-identity translated/rotated/moving frame preserves NavLocal product-chain execution
```

The marker now means:

- identity and non-identity runs have the same terminal outcome;
- their complete NavLocal position, velocity, attitude and clock histories agree
  within explicit tolerances;
- storage pages use one maneuver timeline.

It does not claim that the current 78-infeasible-segment physical plan is valid.
That failure must appear later and be classified for M3/M4, not used to reject
M1.

If the marker passes, accept M1 and activate M2 even if a subsequent known
physical-authoring case fails. If timeline or frame equivalence fails, keep M1
open and repair the exact boundary defect.
