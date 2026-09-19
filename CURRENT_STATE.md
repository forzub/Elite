# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest target-machine evidence

The latest target run exercised the continuous outgoing-tracking candidate introduced by code commit:

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

The pasted log did not include `git rev-parse HEAD`, so the exact full tested checkout hash is not independently recorded in this evidence. The changed diagnostics prove the candidate code was present.

Results:
- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **14/15 PASS**.
- Only failing target: `maneuver_corner_family_matrix`.
- StopTurnGo expert remains healthy.
- RadiusTurn remains healthy.
- Long 180 deg arc remains healthy for all PilotSkill profiles and both laws.
- DriftTurn still fails badly under the constant-heading outgoing reference.

## Continuous outgoing-tracking experiment

The old x=60 checkpoint was successfully decoupled from phase termination and the outgoing reference was extended to x=120.

This did **not** solve DriftTurn.

Expert DriftTurn:
- final P error ~0.463 m;
- final V error ~0.0014 m/s;
- corridor violation 0;
- final attitude error **48.287 deg**;
- tracking-envelope exceeded ticks **439**;
- `outgoing_attitude_captured=0`.

Competent DriftTurn:
- final attitude error ~108.471 deg;
- 494 tracking-envelope exceeded ticks;
- no outgoing attitude capture.

Rookie DriftTurn:
- final attitude error ~107.493 deg;
- 518 tracking-envelope exceeded ticks;
- no outgoing attitude capture.

By contrast, expert StopTurnGo and RadiusTurn capture the outgoing attitude essentially at the old x=60 checkpoint (within ~0.05-0.15 m).

## Interpretation

The previous checkpoint/deadline hypothesis was incomplete.

B10 is a **bounded tracking controller**, not a maneuver generator. Its angular feedback is intentionally clamped by the planner-reserved tracking authority (`0.35 rad/s²` in this fixture). Replacing the planner-authored 90 deg angular trajectory with an instantaneous constant final heading asks B10 to perform the entire 90 deg turn using only tracking reserve.

That violates the intended planner/follower ownership:
- planner must author the large attitude maneuver/reference and feed-forward;
- follower may only reduce residual error around that accepted reference.

The long arc remains the proof: when the planner supplies a continuous angular reference + feed-forward, B9/B10 track it accurately. The direct 90 deg heading step fails because it removes the maneuver program and leaves only bounded correction authority.

## Current problem statement

The remaining issue is not x=60 itself and not a general follower defect. It is the **DriftTurn exit attitude-transition authoring**.

The correct next mechanism is a planner-authored moving attitude-capture segment whose angular profile is derived from:
- current attitude error;
- current angular velocity;
- vehicle angular acceleration/rate capability;
- target outgoing attitude/angular velocity;
while position reference continues along the outgoing straight.

Its duration/horizon must come from physics/capture conditions, not an arbitrary 4 s deadline.

## Do not

- ask B10 tracking reserve to execute the full 90 deg maneuver;
- weaken the 5 deg requirement;
- inflate generic tracking reserve merely to make the fixture pass;
- reintroduce an arbitrary fixed-time attitude deadline.

## Documentation protocol

After every state-affecting iteration, synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- active Stage-12 document

And recreate `CONTINUE_PROMPT.md` **from scratch**.
