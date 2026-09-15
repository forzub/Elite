# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** live docking guidance correctness + performance  
**Stage:** NAV-LIVE-3 — collision broadphase + replan-storm containment

## Measured runtime result

The 19-obstacle diagnostic run made the remaining ownership explicit:

```text
[DockingPerf]
total_ms      ~= 470.3
snapshot_ms   ~=   0.41
geometric_ms  ~=   7.42
trajectory_ms ~= 373.3
tunnel_ms     ~=  15.5
```

`navigation_perf.log` shows the initial trajectory smoother at about 367 ms.
For each support-level candidate, spline sampling itself is only about 7..8 ms,
while collision/safety validation costs about 39..60 ms. The dominant cost is
therefore repeated segment-vs-obstacle narrow-phase work, not B-spline math.

The same runtime file also exposes a worse rolling-guidance defect:

```text
1 initial SmoothPathOptimizer call
273 rolling SmoothPathOptimizer calls
= 39 rolling tunnel rebuild attempts * 7 spatial candidates
```

The rolling calls consumed roughly 14.4 seconds of smoother CPU in the captured
short run. A reconnect attempt took about 0.37 s, longer than the old 0.25 s
replan period, so another synchronous attempt could become due on the next frame.
This is the immediate cause of the near-unusable motion after the stress field was
introduced.

## NAV-LIVE-3 changes

### 1. Conservative collision broadphase

`src/world/navigation/NavigationObstacleGeometry.cpp` now performs a cheap
segment-vs-enclosing-sphere broadphase before the exact obstacle test.

- Box: sphere encloses the fully inflated OBB.
- Capsule: sphere encloses the fully inflated capsule.
- Sphere: normal inflated radius.

A broadphase miss can safely skip the exact test. A broadphase hit still runs the
existing exact OBB/capsule/sphere intersection. This does **not** change collision
radius, clearance, route sampling, or final safety semantics.

The current implementation is deliberately the first acceleration layer. If the
19-obstacle test still spends material time in collision validation, the next
step is a spatial obstacle index / candidate set rather than weakening checks.

### 2. Contain synchronous rolling replan storm

`ManualDockingGuidancePlan` now limits rolling replan checks to 1 Hz while the
solver is still synchronous on the client frame.

The previous hard-envelope branch bypassed the cadence timer entirely. That
out-of-band heavy solve is temporarily disabled by making the hard-envelope
scale unreachable in normal flight. Manual guidance is advisory; departures are
still detected by the normal periodic pose/predicted-exit/course checks.

This is a runtime guard, not the final architecture. The intended final form is:

```text
frame thread: cheap tracking / invalidation
worker: immutable reconnect solve
publication: generation id / latest-request-wins
```

Once that exists, an immediate hard-envelope event can be restored without
blocking the frame.

### 3. Stress field corrected

The first NAV-LIVE-2 stress layout proved visually wrong: it was a cloud near the
Hub that did not meaningfully cross the player-to-dock route.

The same 16 objects are now arranged as four deterministic obstacle bands across
the baseline route. Each band has one object centred on the direct path and
neighbouring cube/cylinder obstacles that make the bypass non-trivial while
leaving a safe route around the band.

The two authored docking targets remain unchanged. Expected route snapshot is
still approximately `obstacles=19`.

### 4. Perf log path is explicit

At process startup the executable/tests now print the exact absolute log path:

```text
[NavigationPerf] log_path=<absolute path>/navigation_perf.log
```

The user observed two files because the logger was relative to the process
working directory: test executables and `EliteGame` can run with different CWDs.
The startup line removes that ambiguity.

## Acceptance

Under MSYS2 MinGW64:

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

unset ELITE_TRACE_RUNTIME

python tests/architecture_contracts/check_navigation_stress_field.py
python tests/architecture_contracts/check_live_docking_guidance.py
bash tests/navigation_guidance/run_mingw64.sh
cmake --build build --target EliteGame
D:/__elite/work/build/EliteGame.exe
```

At startup record the `[NavigationPerf] log_path=...` line, remove that exact file
before a clean timing run if desired, then:

1. visually confirm that the four obstacle bands cross the route rather than sit
   harmlessly near the station;
2. calculate one route to cube A;
3. fly/rotate for several seconds with the tunnel active;
4. capture `[DockingPerf]`, `[GuidanceReplan]`, `[FramePerf]` and the exact
   `navigation_perf.log` announced by the executable.

## Acceptance questions

1. Does the direct route now visibly detour around the stress objects?
2. Does the snapshot still report about 19 obstacles?
3. How far does broadphase reduce candidate `safety_ms` from the old 39..60 ms?
4. Is `trajectory_ms` materially below the old 373 ms stress baseline?
5. Is `tunnel_builds=1` no longer present frame after frame?
6. Is manual flight usable between the throttled reconnect events?

## Next optimization decision

- If collision validation remains dominant: add a spatial broadphase/candidate
  obstacle set shared by geometric, trajectory and local guidance.
- If rolling reconnect remains expensive: stop evaluating broad/loop candidate
  families synchronously; keep the accepted topology and move reconnect to a
  worker/latest-wins job.
- If the initial solve is still visibly blocking after CPU reduction: move the
  whole immutable route+trajectory solve off the frame thread.
- Keep exact swept collision checks as the final narrow phase. Do not trade
  collision fidelity for frame time.
