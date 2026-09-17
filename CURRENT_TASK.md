# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — emergency contact severity ranking

## Newly closed gate — continuous verifier performance

Target-machine run on `6f85436252d36e4b586ab496efe3aea45fb25e79`:

```text
NAVIGATION TRAJECTORY CONTINUOUS BENCHMARK CONTRACT: PASS

straight_newton p95                4.0458 us
rolled_newton p95                  6.5235 us
lateral_newton p95                 4.1593 us
elite_aligned p95                  4.1820 us
geometry_blocked_roll p95          6.5133 us
full_precision_batch8 p95         41.3527 us
```

Decision rule was:

```text
batch8 p95 < 0.5 ms -> freeze verifier
```

Measured batch8 is `0.04135 ms`, so the static `ContinuousPassageTrajectoryEvaluator` is **performance accepted and frozen**.

Do not optimize its math without contrary live-runtime evidence.

## Active task — rank unavoidable contacts by severity

Current `EmergencyPassageMitigator` already guarantees:

```text
safe trajectory if one exists
    -> otherwise stop before impact if physically possible
    -> otherwise keep an explicit non-safe control command alive
```

What is still missing: among several unavoidable-contact commands, choose the one with the least physical consequence rather than only the smallest geometric deficit.

The next isolated scorer must consume predicted contact kinematics and prefer:

```text
small relative normal speed
small impact-energy proxy
more tangential / glancing incidence
useful passage-axis progress
reachable hull attitude
```

A glancing scrape/ricochet must rank ahead of a hard perpendicular hit when both are unavoidable.

The result must remain explicit: contact-expected candidates are never `Clear`; exact CCD/TOI/impulse/ricochet stay physics authority.

## Acceptance order after this task

1. pin deterministic static-contact severity fixtures;
2. benchmark the scorer only if behavior shows nontrivial cost;
3. generalize passage geometry/contact prediction to moving obstacles and time-varying gaps;
4. reuse the same relative-motion machinery for moving/rotating docking;
5. add bottom-to-bottom mating-frame terminal constraints;
6. add deterministic `PilotSkillProfile` execution;
7. integrate accepted Navigation v2 into live game/server/guidance;
8. run end-to-end stress/debug acceptance;
9. retire legacy route-wide navigation only after the live v2 path is stable.

## Definition of final success

The navigation work is finished when the live runtime, not only isolated tests, proves that ships can:

```text
fly normally within CPU/GPU budgets
avoid static and dynamic hazards
use narrow/oriented gaps when physically possible
respect Elite/Newton vehicle authority
keep controlling through unavoidable collisions and minimize impact severity
recover/replan from real post-impact state
rendezvous and dock with stationary/moving/rotating ports in the correct orientation
show the same accepted trajectory in guidance/debug
scale to intended NPC traffic without planner stalls or N^2 precision work
```

Until those live-system gates are green, `NAV-V2-TRAJECTORY-1` and Navigation v2 as a whole are not finished.
