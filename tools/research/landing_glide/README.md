# Landing glide study (2026-11-07)

2D (vertical plane) model of the Moon de-orbit for a ship whose only thrust is axial
(the Assisted Cobra in normal flight has no keypad RCS): the body's pull less the
orbital lift, the nose slewing at a bounded rate, the main engine gated by the
attitude error - the same pilot logic as `BodyAutopilot`.

- `sim.py` + `variants.py q3` reproduce the live q3 crash (hand-over 597 m / 19.9 km /
  353 m/s vs the live 588 m / 18.8 km / 350 m/s) - the model is trusted.
- `gturn.py`: a pure gravity turn from 82.7 km: reaching 1.5 km needs ~2 m/s2 of
  braking for 600 s and ends at -34..-79 deg path angles (nose into the ground).
- `grid2.py <arc km>`: constant-attitude two-phase plans - within +-25 deg pitch the
  sink cannot be stopped before the ground; ending at 450 m/s ~8 km high is feasible.
- `closed.py`: the closed-loop law shipped as `BodyAutopilot::stepGlide` (phase 1
  nose up + fore bank, phase 2 nose -20 deg braking, both re-solved every second by
  bisection) - all starts 285 / 330 / 400 km end within 0.4 km of the aim.

Run with plain `python` (no dependencies).
