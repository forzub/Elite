# CURRENT TASK — validate viewer build fix, then localize speed constraints

**Date:** 2026-09-22  
**Status:** BUILD FIX COMMITTED / TARGET VALIDATION REQUIRED

## Target failure just received

The user's MinGW64 run reached the viewer build and failed only in
`NavigationRuntimeViewer.cpp`:

- `recalculationRequired` was used by `drawHud` before declaration;
- a dangling `if (speedChanged)` remained after removing slider auto-refresh.

Fixed in:
- `e2597a7714f5e7fee2200e7f47f9bfb9bafb2734`;
- `b15d0e97c6897d2b099e0c6e41d60830c45776b9`.

The unused `buildReferenceAttitudes` acceleration warning was also removed.

## Canonical speed rule

Speed is not constant by style and is not required to be constant along a route.

- START/FINISH speed = boundary constraints only.
- Intermediate speed may be lower or higher.
- Braking must have a concrete local reason.
- A hard/acute maneuver may slow heavily or stop.
- A clear section may exceed FINISH speed substantially.
- Local speed constraints must remain local.
- No invented braking merely to match an arbitrary uniform reference.
- STANDARD/EXTREME only trade clearance/risk; they never define nominal speed.

Recorded in:
- `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`;
- `tools/navigation_runtime/README.md`.

## Implementation already corrected

- Runtime execution hard speed ceiling now comes from vehicle capability, not
  `max(start, finish)`.
- Exact moving FINISH speed is no longer converted into a whole-route scalar speed cap.
- Focused regression added: 500 m three-point straight route, START=10, FINISH=10,
  vehicle max=80; calculated peak must exceed 12 while terminal remains exactly 10.

Relevant commits:
- `336b48a293dca0306847afbe3bc00e5787713a2e`;
- `16fe6a8918fa1a9f03fce3dd2f814987198f31e8`;
- `22133bbe5c9ef61a52e839338e88b44a29061f22`;
- `49009e6f480c7a70848f9a89c5cb4a0f7d4c8dab`.

## Known remaining mechanism defect

`globalGuideSpeedLimit()` still collapses the worst curvature on a multi-point guide
into one route-wide maximum speed.

That is safe but wrong under the new contract: a sharp bend may force local braking, but
must not cap unrelated straight sections.

After target compile/build is green, the next mechanism slice is:
- local scalar speed restrictions by progress;
- forward/backward braking feasibility;
- accelerate on clear sections;
- brake only for the upcoming local restriction;
- full stop permitted if required;
- accelerate again afterward.

Do not solve this in Follower or by adding style speeds.

## Immediate target command

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD
bash tests/navigation_runtime/run_stage1_mingw64.sh
```

If green, also run the focused Ruckig regression:

```bash
bash tests/navigation_guidance/run_mingw64.sh
```

Then launch viewer:

```bash
./build/tools/navigation_runtime/bin/navigation_runtime_viewer.exe tools/navigation_runtime/scenario.json
```

Validate Calculate UX and speed/style route-coordinate behavior as previously specified.

Do not claim target PASS until user evidence is received.

## Mandatory state protocol

Every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
