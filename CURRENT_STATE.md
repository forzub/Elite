# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted exact target-machine baseline

Exact tested checkout:

```
213bbfb62ff7dcb8e553c06bdca09d99d2d1fd56
```

Evidence:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **16/16 PASS**.
- total runtime test time: ~0.43 s.
- new `maneuver_fly_through_3d`: PASS.

This accepts the continuous 3D fly-through strict expert gate.

## What the accepted fly-through gate proves

The same accepted 5-segment 3D route is executed by Newtonian and Assisted through:
- ~35 deg turn;
- ~60 deg turn;
- ~90 deg turn;
- ~120 deg turn.

The test enforces for strict Expert rows:
- all 9 moving phases complete;
- no hidden StopTurnGo substitution;
- minimum route/corner speed >=3 m/s;
- complete Cobra OBB remains inside 32 m half-width;
- zero tracking-envelope exceed ticks;
- final P/V/attitude remain inside strict bounds;
- each corner yields a finite measured turn radius;
- sampled planned corner acceleration remains <=2 m/s2.

Because the test passed, all of those strict conditions were satisfied for both Newtonian and Assisted.

## Diagnostic limitation in the accepted log

The acceptance run proved the strict gate, but the old `run_mingw64.sh` did not execute `maneuver_fly_through_3d` in verbose mode after the main CTest pass. Therefore the uploaded log contains the 16/16 result but not the detailed `[FLY3D]` and `[FLY3D-CORNER]` rows.

As a result, exact Newtonian-vs-Assisted comparisons for:
- minimum speed;
- slip angle;
- observed radius;
- hull required half-width;
- forward tracking error

cannot yet be quoted from target-machine evidence.

## Current unverified diagnostics-only HEAD

Runner patch:

```
4cd9c4a14c8f2e4ce033082633766a21fece9331
```

It adds a verbose `ctest -R maneuver_fly_through_3d -V` diagnostic pass to `tests/navigation_runtime/run_mingw64.sh` and records its timing.

No navigation algorithm, acceptance rule, planner, follower, physics or test logic changed in this patch.

## Current task

Rerun target diagnostics so the accepted fly-through can be characterized quantitatively.

After the FLY3D metrics are captured:
1. compare Newtonian vs Assisted radius/speed/slip/hull envelopes;
2. record the measured conclusion in all MDs;
3. proceed to speed/doctrine coverage.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
