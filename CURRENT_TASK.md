# CURRENT TASK — Observe Ruckig reference vs physical execution, then fix maneuver ownership

Date: 2026-09-22

Status: **DIAGNOSTIC VIEWER READY / TARGET RUN REQUIRED**

Code baseline before documentation commits:

```text
620b59ebbb6937727c5c3e3a47f78884ae3fd99f
```

## New visual markers

During execution:

```text
PINK CROSS
    = instantaneous programReferencePosition
    = current Ruckig-derived reference point B9/B10 asks follower to track

VIOLET CROSS
    = end of active AcceptedManeuverProgram phase
    = post-split endpoint on the already calculated trajectory
```

Important: current multi-point Ruckig solve has no true "target of current
section". It solves one scalar path progress `s(t)` to the end of the complete
execution guide. The phase endpoint is created afterward when the full
trajectory is divided by retained-route progress.

## Architecture decision

Do not treat RCS as the sole source of turn acceleration.

The physical maneuver planner/compiler is authoritative for:
- flyable geometry;
- tangent/velocity state;
- hull attitude schedule;
- angular reachability;
- aft-main + RCS allocation;
- turn/braking lead distance.

Ruckig is an inner numerical solver for timing/state transition inside that
compiled maneuver.

Follower/autopilot only executes the accepted program. If it cannot track, it
holds/reacquires or requests recompile; it does not become a second path
planner.

## Terminal requirement in current scenario

`scenario.json` explicitly provides `finish.forward` and `finish.up`.
Parser therefore sets both terminal orientation requirements.

Current acceptance:
- position <= 5 m;
- speed within +/-1.5 m/s;
- forward error <= 0.25 rad;
- up error <= 0.25 rad.

For non-zero requested finish speed, the trajectory request also sets terminal
velocity in `finish.forward`.

So the current test is an oriented moving fly-through.

No braking is required merely because there is a turn. If a broad free-space
arc can arrive at the requested speed and final direction, preserve speed.
Brake only when required by:
- requested lower terminal speed;
- curvature/available acceleration;
- narrow corridor / obstacle clearance;
- finite attitude acquisition / propulsion reachability.

## Run next

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Use Newtonian / Expert / Standard and reproduce the higher-speed case.

Observe frame-by-frame around the first miss:
- pink reference cross;
- violet active-phase endpoint;
- hull nose;
- actual velocity vector;
- main/RCS lamps.

The immediate question is:
**does the pink reference itself demand a physically unreasonable turn, or does
the follower fail to realize a reasonable reference?**

Do not tune follower envelopes until this is answered.
