# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — continuous static-passage performance gate

## Accepted preconditions

### `NAV-V2-LOCAL-1` — CLOSED

The deterministic 16-probe fan is accepted. Deliberate `1024 x 17` stress remains below the `<1.0 ms normal peak` budget.

### Bounded gaps — ACCEPTED

Worst measured stress:

```text
top8_1024 p95 = 24.0699 us
```

### Emergency passage mitigation — ACCEPTED

Target-machine gate on `b29a03d3d84f4d6575cbbc5166cbd7547b5ce0d8` passed all contracts and `4/4` trajectory CTest.

Accepted invariant:

```text
safe trajectory unavailable != navigation disabled
```

### Continuous static passage — BEHAVIOR ACCEPTED

Fresh target-machine gate on `574a2e98fd7a75ebf562bbfa476fba367fb888d1`:

```text
NAVIGATION TRAJECTORY CONTINUOUS PASSAGE CONTRACT: PASS

navigation_trajectory_passage               PASS
navigation_trajectory_gap                   PASS
navigation_trajectory_reachability          PASS
navigation_trajectory_emergency_passage     PASS
navigation_trajectory_continuous_passage    PASS

100% tests passed, 0 failed
Total Test time: 0.24 sec
```

Behavior now accepted:

```text
33 pose samples
32 continuous interval proofs
Hermite translation
shortest-arc smoothstep attitude
continuous OBB sweep bound
body-axis linear authority
analytic angular authority
Newtonian vs Elite-assisted slip semantics
```

Do not alter the continuous verifier until its measured cost is known.

## Active Gate — continuous verifier microbenchmark

Harness:

```text
benchmarks/navigation_trajectory_continuous/
```

Contract:

```text
tests/architecture_contracts/check_navigation_trajectory_continuous_benchmark.py
```

Timed scenarios:

```text
straight_newton
rolled_newton
lateral_newton
elite_aligned
geometry_blocked_roll
full_precision_batch8
```

Scenario/query construction and one-time expected-status validation occur outside the timed region.

`full_precision_batch8` represents the hard `<=8` surviving gap-candidate ceiling from `BoundedGapCandidateBuilder`.

Reported:

```text
queries_per_batch
calls_per_sample
median_batch_us
p95_batch_us
median_per_query_us
p95_per_query_us
```

Navigation CPU design budget remains:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Decision rule:

```text
batch8 p95 < 0.5 ms
    -> performance-accept and freeze static continuous verifier

0.5 ms <= batch8 p95 < 1.0 ms
    -> acceptable peak; inspect scheduling/candidate ordering before changing math

batch8 p95 >= 1.0 ms
    -> optimize or introduce stricter precision-work budget before live integration
```

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_continuous_benchmark.py
bash benchmarks/navigation_trajectory_continuous/run_mingw64.sh
```

Send complete output.

## Next after performance gate

1. record exact benchmark evidence in `benchmarks/navigation_trajectory_continuous/RUN_LOG.md`;
2. if accepted, freeze the static continuous reference;
3. rank emergency candidates by predicted **relative normal contact speed / impact-energy proxy** so glancing/ricochet contact beats a normal hit;
4. generalize passage state to moving/time-varying obstacle gaps;
5. reuse the moving-frame machinery for moving/rotating docking with explicit bottom-to-bottom mating frames;
6. add deterministic NPC `PilotSkillProfile` execution later;
7. keep live `EliteGame` / `EliteServer` integration after isolated trajectory gates.
