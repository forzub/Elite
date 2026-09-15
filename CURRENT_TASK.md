# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** live docking guidance correctness + performance  
**Stage:** NAV-LIVE-2 — trajectory smoother profiling + deterministic obstacle field

## Measured runtime result

NAV-LIVE-1 did not remove the visible freeze. The latest runtime capture shows the
first `CALCULATE ROUTE` request dominated by trajectory construction, not global
geometric search:

```text
[DockingPerf]
total_ms      ~= 198.1
snapshot_ms   ~=   0.10
geometric_ms  ~=   0.47
trajectory_ms ~= 106.5
tunnel_ms     ~=  15.7
```

While manual guidance is active, repeated frames also show roughly
`dock_ms ~= 110..126 ms` with `tunnel_builds=1`. Therefore the active performance
problem is synchronous smoothing / tunnel rebuilding on the client frame, not
render submission and not the current three-obstacle visibility search.

## NAV-LIVE-2 changes

### 1. SmoothPathOptimizer instrumentation

`SmoothPathOptimizer` now appends detailed timings to:

```text
navigation_perf.log
```

For every optimize call it records:

- total optimizer time;
- source path point count;
- obstacle count;
- requested spacing/chord error/support level;
- candidates evaluated / safe candidates / selected support level;
- output sample count;
- per-candidate control count and sample count;
- per-candidate spline sampling time;
- per-candidate collision/safety validation time;
- per-candidate quality/curvature scoring time.

The initial trajectory call is easy to distinguish from rolling tunnel calls:

```text
initial trajectory: spacing 3..7 m, chord error 0.03 m, max support 5
rolling reconnect: spacing 14 m, chord error 0.15 m, max support 1
```

### 2. Safe span-lookup optimization

The cubic B-spline knot-span lookup was previously a linear scan for every spline
evaluation. At high support levels this creates avoidable work across thousands
of adaptive samples.

It now uses binary search (`upper_bound`) while preserving the same half-open knot
span semantics. This changes lookup complexity only; it does **not** reduce:

- support levels;
- spline precision;
- adaptive sampling criteria;
- obstacle checks;
- collision radius / clearance;
- curvature acceptance.

### 3. Deterministic navigation stress field

The Hub Motion Lab now contains:

```text
1 station
2 existing docking targets
16 stress obstacles: 8 cubes + 8 cylinders
```

The 16 new objects use fixed positions and orientations and are attached to the
same Hub. They are real `StaticObject`s, therefore
`ClientNavigationPlanningSnapshotFactory` includes them through the normal
`NavigationObstacleFactory` path. The layout is deterministic so timing and path
changes can be compared between builds.

Expected normal route snapshot in this scene: approximately `obstacles=19`.

## Logging policy for this iteration

Detailed navigation performance belongs in `navigation_perf.log`, not in a huge
console paste.

The `[M8E-XPROC]` / `[M8E-STARTUP]` stream is controlled by environment variable
`ELITE_TRACE_RUNTIME`. For normal navigation profiling run with it disabled.

Failure/error messages remain on console. Existing low-rate `[FramePerf]`,
`[DockingPerf]` and `[GuidanceReplan]` summaries remain useful until ownership of
the stall is closed.

## Acceptance

Under MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

unset ELITE_TRACE_RUNTIME
rm -f navigation_perf.log

python tests/architecture_contracts/check_live_docking_guidance.py
python tests/architecture_contracts/check_ruckig_navigation_spike.py
python tests/architecture_contracts/check_ruckig_navigation_integration.py

bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
D:/__elite/work/build/EliteGame.exe
```

Then reproduce one route calculation and several seconds of manual flight with
the tunnel active.

Capture only:

```text
[DockingPerf] ...
[GuidanceReplan] ...
[FramePerf] ...
```

and the generated `navigation_perf.log`.

Acceptance questions:

1. Do the 16 added stress objects appear visually around the diagnostic Hub?
2. Does the route snapshot report about 19 navigation obstacles?
3. Did the knot-span optimization materially reduce `trajectory_ms`?
4. In `navigation_perf.log`, is the remaining cost dominated by spline sampling
   or collision/safety validation?
5. For rolling reconnect calls, which candidate/phase explains the 100+ ms
   `dock_ms` spikes?

## Next optimization decision

- If spline sampling dominates: remove redundant full-resolution candidate work
  while preserving final selected geometry and safety; consider coarse ranking
  followed by full-resolution validation of finalists.
- If collision validation dominates: introduce a spatial broadphase / candidate
  obstacle set while retaining exact swept narrow-phase checks.
- If rolling tunnel still dominates after local solving: stop rebuilding the
  full presentation tail; keep accepted trajectory separately and emit only the
  nearby visible gate window.
- If the initial synchronous solve is still visually unacceptable even after CPU
  reduction: move immutable route/trajectory generation to a worker with
  generation IDs and latest-request-wins publication.

Do not reduce collision/safety fidelity to hide a frame stall.
