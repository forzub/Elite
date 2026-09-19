# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Validate the new capability-derived moving attitude capture for DriftTurn exit.

## Current candidate

```
31a66a3eb462df6b5a60b2da2aca9f018d8aa332
```

## Mechanism

The capture:
- starts from actual yaw and actual yaw rate after the drift arc;
- keeps translation at 10 m/s;
- targets the outgoing corridor yaw and zero terminal yaw rate;
- uses a quintic boundary-value profile;
- computes the shortest feasible duration from real effective angular acceleration/rate limits;
- reserves B10 angular tracking authority instead of consuming the whole physical envelope.

This directly addresses the two defects exposed by prior experiments:
- fixed-time authoring ignored actual angular state;
- raw target-heading step incorrectly delegated the whole maneuver to B10.

## Diagnostics to inspect

For expert newtonian and assisted DriftTurn:
- attitude_capture_program_s
- attitude_capture_start_yaw_rate_radps
- attitude_capture_peak_ff_yaw_rate_radps
- attitude_capture_peak_ff_yaw_accel_radps2
- tracking_envelope_exceeded_ticks
- outgoing_attitude_captured
- final_forward_error_deg
- final_pos_error_m
- final_velocity_error_mps

## Acceptance

Need:
- 15/15 runtime;
- expert DriftTurn final attitude <=5 deg;
- terminal angular convergence;
- final P <=1.5 m;
- final V <=1.0 m/s;
- zero corridor violation;
- long arc remains green.

If this passes, close the corner-family execution defect and move to the next mixed-angle multi-segment 3D corridor stage.

## Iteration rule

After each code/evidence change, update all state MD files and recreate CONTINUE_PROMPT.md from scratch.
