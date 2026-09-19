# CONTINUE PROMPT — Elite Navigation v2

Continue work directly in GitHub repository `forzub/Elite`, branch `main`.

**Workflow rule:** after every state-affecting iteration, update `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, the active Stage-12 document, and recreate this entire `CONTINUE_PROMPT.md` **from scratch** from the new current truth. Do not incrementally patch stale prompt prose. The newly recreated prompt must contain this same rule again.

## Latest target-machine evidence

The latest run exercised the continuous outgoing-tracking candidate. The pasted log did not include a printed checkout hash, so do not claim a new exact full tested HEAD. The candidate code commit was:

```
24fce76b30d2448a4b94c93ad78dd8d37e5102df
```

Results:
- Stage-12 architecture contract PASS.
- `navigation_runtime` 14/15.
- StopTurnGo expert healthy.
- RadiusTurn healthy.
- Long 180 deg arc healthy.
- Only strict expert failure remains DriftTurn exit attitude.

Latest DriftTurn experiment:
- old x=60 checkpoint no longer ends control;
- outgoing reference extends to x=120 at 10 m/s;
- desired outgoing heading is held continuously;
- target angular velocity is zero.

Result:
- expert final P ~0.463 m;
- expert final V ~0.0014 m/s;
- zero corridor violation;
- expert final attitude **48.287 deg**;
- 439 tracking-envelope exceeded ticks;
- no outgoing attitude capture.
Competent/rookie also fail strongly (~108 deg / ~107 deg).

## Correct interpretation

The checkpoint/deadline hypothesis was incomplete.

B10 is a bounded residual tracking controller. In this fixture its angular feedback reserve is only 0.35 rad/s². Replacing the planner-authored 90 deg attitude trajectory with an instantaneous target-heading step asks B10 to execute the whole maneuver from tracking reserve. That violates the planner/follower contract.

The long arc proves the intended architecture works: planner supplies a continuous angular reference + feed-forward; B9/B10 track it accurately.

## Current task

Implement a planner-authored **moving attitude-capture** for DriftTurn exit.

Requirements:
- translation continues along the outgoing straight at 10 m/s;
- angular transition starts from actual/planned current attitude and angular velocity;
- target is outgoing attitude with terminal angular velocity 0;
- angular acceleration/rate stay inside capability;
- maneuver horizon is computed from state/capability and braking requirement, not a fixed arbitrary 4 s;
- accepted program contains the angular reference/feed-forward;
- B10 only performs bounded residual correction.

Do not:
- use a raw 90 deg heading step and expect tracking reserve to execute it;
- inflate generic tracking reserve;
- weaken 5 deg terminal attitude;
- reintroduce x=60 as an angular deadline.

## Acceptance

Need:
- architecture PASS;
- runtime 15/15;
- expert DriftTurn P <=1.5 m;
- V <=1.0 m/s;
- final attitude <=5 deg;
- terminal angular rate within tolerance;
- zero corridor violation;
- sustained-speed/material-slip behavior preserved;
- long arc remains green.

## After green

Record exact tested HEAD and evidence, explain that the closed defect was missing planner-authored exit attitude capture, then move to mixed-angle multi-segment 3D corridor testing.

**Again:** recreate this entire prompt from scratch after every state-affecting iteration.
