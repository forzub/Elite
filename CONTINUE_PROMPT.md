# CONTINUE PROMPT — Elite Navigation: physical maneuver timing is now the blocker

Continue directly in GitHub repository `forzub/Elite`, branch `main`.

Every state-affecting iteration MUST:
1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md`;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. recreate this `CONTINUE_PROMPT.md` from scratch.

Also keep `src/game/navigation/NAVIGATION_PIPELINE_AUDIT.md` synchronized.

## Last target gate

Baseline:
`5cc0b668665f0adc11b160bad3bc2af314cdfe4d`

Passed:
- static-route architecture;
- Stage-1 nominal route;
- follower corridor component;
- viewer/runtime build.

Failed:
- `navigation_runtime_pipeline`.

High-speed Newtonian diagnostic:
- 743 trajectory samples;
- 51 storage pages;
- 742 actuator segments;
- 42 actuator segments infeasible;
- first storage page advance occurred;
- program invalidated for tracking loss at 0.51 s;
- max reference/velocity angle 175.84 deg;
- max body/velocity angle 0.94 deg;
- reference clock is MONOTONIC.

## Important: two failures, not one

### Stale test

The E2E regression still searches for `REFERENCE CLOCK HOLD:`.
That mechanism was intentionally removed.

Update tests to the new contract:
- monotonic reference clock;
- no old hold diagnostics;
- explicit invalidation on prolonged tracking loss.

### Real navigation failure

The accepted translational trajectory is authored/timed before full physical
attitude + propulsion feasibility.

Current order:
```text
route -> guide -> Ruckig -> attitude -> actuator fit
```

Wrong because the scalar trajectory may demand a force vector before the hull
can rotate to provide it.

Target order:
```text
route
 -> physical maneuver compiler
    geometry/tangent
    force requirement
    hull attitude
    angular reachability
    main/RCS allocation
    throttle slew
    lead-rotation and braking boundaries
 -> Ruckig timing inside the feasible envelope
 -> AcceptedManeuverProgram
 -> Autopilot
```

The 42 infeasible actuator segments are evidence of this defect. Do not ignore
or clamp them away.

## Next actions

1. Fix stale E2E assertions.
2. Turn actuator infeasibility into an authoring failure.
3. Start the physical maneuver compiler / timing inversion above.
4. Re-run:
```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Do not tune follower gains or increase the 0.50 s tracking-loss timeout.
Do not enable dynamic avoidance.
