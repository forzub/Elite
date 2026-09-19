# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Task

Replace the failed constant-heading DriftTurn exit with a planner-authored **moving attitude capture**.

## Latest evidence

The continuous outgoing reference experiment removed x=60 as a control terminus, but DriftTurn still failed:
- expert final attitude ~48.287 deg;
- expert 439 tracking-envelope exceeded ticks;
- no outgoing attitude capture;
- P/V/corridor remained excellent;
- long arc remained clean.

This proves B10 cannot be used as the primary 90 deg maneuver generator. Its bounded angular feedback reserve is only for tracking correction.

## Required mechanism

Author the outgoing attitude transition in B8/B9 as a physical reference derived from state and capability:
- current body attitude;
- current angular velocity;
- desired outgoing attitude;
- desired terminal angular velocity 0;
- angular acceleration and rate capability;
- moving translational reference at 10 m/s.

The profile should accelerate/rotate/brake as required by capability and stop conditions. The horizon should be **computed from the maneuver**, not chosen as a fixed 4 s route-time deadline.

B10 then remains responsible only for residual bounded tracking error.

## Validation goals

- expert DriftTurn final attitude <=5 deg;
- final angular rate within terminal tolerance;
- P <=1.5 m;
- V <=1.0 m/s;
- no corridor violation;
- sustained-speed/material-slip semantics retained;
- long arc remains green.

## Iteration rule

After every code/evidence change, synchronize all state MD files and recreate `CONTINUE_PROMPT.md` from scratch.
