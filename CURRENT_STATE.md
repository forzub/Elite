# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Last accepted exact target-machine baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

Accepted:
- Stage-12 architecture contract PASS;
- navigation_runtime 17/17 PASS;
- B7 speed/doctrine select->execute PASS;
- prior maneuver/corridor/fly-through gates accepted.

## Latest target-machine runtime attempt

Exact tested checkout:

```
350f7d593e22b8b89cb3ae4dbfbfbb7fbb53ea03
```

Results:
- architecture contract PASS;
- build PASS;
- navigation runtime 17/18;
- only `maneuver_chained_limit_matrix` failed.

### Newtonian chain

Healthy and unchanged:
- phases 4/4;
- seam P jump 0;
- seam V jump 0;
- seam forward jump ~0.000001 deg;
- seam omega jump 0;
- max hull half-width 17.428330 m inside 25 m;
- max slip 34.491954 deg;
- final P error 0.024611 m;
- final speed 0.024960 m/s;
- final forward error 0.028574 deg;
- tracking envelope exceeded ticks 0.

### Assisted phase-3 failure

Measured:
- entry slip 1.579066 deg;
- max slip 25.788647 deg;
- max slip after 1 s 25.788647 deg;
- final slip 25.788647 deg;
- max forward tracking error 25.964670 deg;
- final forward error 25.964670 deg;
- tracking-envelope exceeded ticks 171.

Therefore the failure is not inherited seam transient. The aligned phase itself receives/executes a bad attitude reference.

## Root cause

The chained test reference builder used a world-up reconstruction:

```
forward -> choose upSeed(Y or X) -> right/up
```

and switched seeds when `abs(dot(forward, Y)) > 0.92`.

Phase 3 bends far enough around +Y to cross that threshold. The forward tangent remains smooth, but right/up can jump to the equivalent opposite roll frame.

This matters because B10 `ManeuverTrackingController` tracks full SO(3) attitude using:
- forward;
- right;
- up.

Thus an artificial right/up discontinuity becomes a real angular feed-forward/feedback command.

This is a reference-authoring defect in the chained test fixture, not evidence that Assisted lacks angular authority.

## Current unverified fix candidate

Commit:

```
b8eb4641b013692c773d087d6cad96756c672b3c
```

Change:
- aligned moving reference now uses a minimal-twist parallel-transport/Bishop frame;
- each new forward tangent projects the previous right axis into the new normal plane;
- no arbitrary world-up seed switch occurs inside a smooth trajectory;
- terminal forward is pinned while transported roll is preserved.

Important semantic rule recorded:
- ordinary velocity-aligned flight should use continuous transported roll;
- exact terminal roll/top orientation for docking, attachment or placement must be an explicit terminal attitude-capture profile, not an accidental world-up reconstruction.

No B10 gains, physical limits, slip limits or terminal tolerances changed.

## Current active block

Chained transitions + physical limits remains open until target evidence passes.

Expected suite: 18 tests.

If the transported-frame fix works:
- Assisted phase 3 must remain <=8 deg slip after 1 s;
- terminal slip <=4 deg;
- zero chain tracking-envelope violations;
- negative/limit cases must then execute and report.

If it still fails:
- investigate quaternion/angular-feed-forward derivation from the continuous transported samples;
- do not loosen criteria.

## Documentation protocol

After every state-affecting iteration:
- update `CURRENT_STATE.md`;
- update `CURRENT_TASK.md`;
- update `PROJECT_STATE.md`;
- update active Stage-12 document;
- recreate `CONTINUE_PROMPT.md` from scratch.
