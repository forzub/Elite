# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted target-machine baseline

Exact tested checkout:

```
b687b9d3189cdfbbca91123b578637f991cbc645
```

Accepted evidence:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **15/15 PASS**.
- Strict expert StopTurnGo / RadiusTurn / DriftTurn are healthy.
- Long 180 deg angular tracking remains healthy.
- DriftTurn moving-attitude capture is accepted.

## New unverified stage: continuous 3D fly-through

Code candidate commits:
- `5c10c19d7fd2eb4b1fb6aa26b903fd55713b6dcf` — new fly-through test.
- `c48a92010350cf12f417aa19f23f75487dfb1459` — CMake registration.

New test:

```
tests/navigation_runtime/ManeuverFlyThrough3dTests.cpp
```

CTest target:

```
maneuver_fly_through_3d
```

The runtime suite is expected to grow from 15 to **16 tests**.

## Test purpose

Prove continuous chained 3D maneuver execution instead of:
`stop -> rotate in place -> next leg`.

The same accepted geometric fly-through is executed by:
- Newtonian;
- Assisted;

for:
- Expert — strict acceptance;
- Competent — diagnostic;
- Rookie — diagnostic.

## Route

Five connected segments with four intended turn angles:
- ~35 deg;
- ~60 deg;
- ~90 deg;
- ~120 deg.

Entry fly-through speed:

```
8 m/s
```

Each corner receives:

```
45 m
```

of cut distance on both incoming/outgoing legs.

The route includes simultaneous X/Y/Z direction changes.

## Corner reference

Each corner is a C2 quintic 3D transition.

Boundary conditions:
- position continuous;
- velocity continuous;
- endpoint velocity magnitude = 8 m/s;
- endpoint acceleration = 0;
- body forward follows the local velocity tangent;
- angular velocity/feed-forward are derived from sampled orientation evolution.

The hard corner is allowed to reduce speed naturally but must not become StopTurnGo.

## Physical/corridor conditions

Vehicle:
- Cobra Mk1 logical hull;
- half extents {13.0, 2.5, 11.1} m.

Corridor:
- 32 m half-width;
- acceptance is based on all eight OBB hull corners, not center only.

Corner linear reference is constrained so sampled planned acceleration stays <= 2 m/s2, matching the manoeuvre/RCS authority used for transverse demand.

## Metrics

Per route:
- completed phases;
- minimum route speed;
- max center cross-track;
- max hull required half-width;
- max corridor violation;
- max forward tracking error;
- tracking-envelope exceeded ticks;
- final P/V/attitude;
- simulated time.

Per corner:
- route angle;
- planned peak acceleration;
- planned minimum speed;
- actual minimum speed;
- max slip angle;
- minimum observed physical turn radius;
- max center cross-track;
- max hull required half-width.

## Strict Expert acceptance

For both Newtonian and Assisted:
- all 9 phases complete;
- full Cobra hull remains within 32 m half-width corridor;
- zero tracking-envelope exceeded ticks;
- route speed never drops below 3 m/s;
- no corner drops below 3 m/s;
- final position error <= 1.5 m;
- final speed error <= 0.75 m/s relative to 8 m/s;
- final forward error <= 5 deg;
- every corner produces a finite measurable turn radius;
- sampled planned corner acceleration <= 2 m/s2.

Competent/Rookie are diagnostic on the first target-machine pass.

## Next validation

Run exact target-machine gates. If the new test passes, compare Newtonian vs Assisted radii, slip, speed loss and hull envelope rather than assuming either law is superior.

If it fails, fix the physical/reference mechanism; do not weaken corridor/speed/tracking criteria just to obtain green.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 documentation;
- recreate `CONTINUE_PROMPT.md` **from scratch**.
