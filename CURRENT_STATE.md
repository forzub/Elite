# CURRENT STATE

**Updated:** 2026-09-20 Europe/Kyiv

## Latest exact target-machine evidence

Tested checkout:

```
a5e44cdc2fb8eaa312ca788ae4b53a9985df3cae
```

- Stage-12 architecture contract: **PASS**.
- `navigation_runtime`: **14/15 PASS**.
- Only failing target: `maneuver_corner_family_matrix`.
- StopTurnGo expert remains healthy.
- RadiusTurn remains healthy.
- Long 180 deg arc remains healthy for all PilotSkill profiles and both laws.
- Remaining strict failure is still expert DriftTurn exit attitude.

## Latest DriftTurn experiment

Candidate tested in this checkout:
- one coherent 4 s / 40 m recovery;
- smooth 90 deg yaw commanded over 2.5 s;
- final 1.5 s continues at 10 m/s while holding exit yaw.

Result: **worse**, not accepted.

Expert DriftTurn:
- previous final attitude error: ~10.325 deg;
- new final attitude error: **16.889 deg**;
- final P error remains ~0.473 m;
- final V error remains ~0.032 m/s;
- corridor violation remains 0;
- tracking envelope remains unexceeded for expert.

Competent DriftTurn also worsened to ~52.744 deg and 193 envelope-exceeded ticks. Rookie DriftTurn ends ~21.643 deg with 127 exceeded ticks.

## Interpretation

The long arc still proves the follower can rotate accurately while translating:
- expert final attitude error 0.052 deg;
- max in-flight forward/tangent error 3.221 deg;
- zero tracking-envelope exceed ticks.

Therefore the failure is not a general inability to rotate in motion.

The 2.5 s recovery compresses the same 90 deg yaw change into a more aggressive angular profile. Although the nominal profile remains inside the published angular capability, the closed-loop execution leaves a larger terminal attitude residual. A 1.5 s zero-feed-forward settle with the current bounded angular feedback is not sufficient to remove that residual.

Do **not** tune durations blindly or weaken the 5 deg gate.

## Next diagnostic/mechanism step

Instrument the DriftTurn recovery boundary with:
- terminal attitude error;
- terminal angular-velocity error / actual yaw rate;
- reference yaw rate;
- peak angular tracking residual during recovery.

Then choose the correction from evidence.

Likely mechanism if the terminal state shows residual angular motion:
- planner-authored moving terminal capture / recovery continuation that keeps the translational reference advancing at 10 m/s while holding final yaw and damping angular rate;
- not the existing frozen-position StateCapture;
- no follower-side maneuver selection.

This gate must be fixed before moving to the next major 3D/speed-doctrine stage.

## Architecture invariants

- Planner owns maneuver choice, trajectory/corridor and proof.
- Follower owns sampling/tracking, bounded feedback and safety monitoring; it must not silently choose a different maneuver family.
- AcceptedManeuverProgram remains the planner/follower contract.
- Manual guidance must visualize the same accepted route/trajectory.
- Newtonian and Assisted physical laws remain distinct.
- Planner cannot mutate authoritative physics state.

## Documentation protocol

After every state-affecting iteration, synchronize:
- `CURRENT_STATE.md`
- `CURRENT_TASK.md`
- `PROJECT_STATE.md`
- active Stage-12 document

And recreate `CONTINUE_PROMPT.md` **from scratch** from current truth.
