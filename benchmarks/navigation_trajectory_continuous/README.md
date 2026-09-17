# Navigation trajectory continuous-passage benchmark

This harness measures the isolated CPU cost of `ContinuousPassageTrajectoryEvaluator::evaluate()` after behavior acceptance.

It does **not** measure world discovery, `NavigationMap`, `NavigationSpace`, GPU work, collision response, docking, or live controller integration. Scenario/query construction and one-time contract validation occur outside the timed region.

## What one verifier call does

Each full verifier call uses the accepted bounded reference:

```text
33 pose samples
32 continuous interval proofs
cubic-Hermite translation
shortest-arc smoothstep attitude
continuous center-curve bound
continuous oriented-hull rotation inflation
body-axis linear authority checks
angular authority checks
optional Elite-assisted slip policy
```

The benchmark intentionally measures complete full-verifier paths rather than early invalid/angular-rejection exits.

## Scenarios

```text
straight_newton
    centered Newtonian pass, no rotation

rolled_newton
    collision-free 90-degree roll through a wide slot

lateral_newton
    full Hermite lateral correction with sufficient side-thrust authority

elite_aligned
    full assisted-policy branch with aligned velocity/body-forward

geometry_blocked_roll
    endpoints fit but the continuous rotating hull is blocked

full_precision_batch8
    eight complete verifier calls in one batch
    mirrors the hard <=8 surviving precision-gap candidate budget
```

The `full_precision_batch8` timing is the primary integration signal because the upstream `BoundedGapCandidateBuilder` is capped at eight candidates.

## Reported timing

Each row reports:

```text
queries_per_batch
calls_per_sample
median_batch_us
p95_batch_us
median_per_query_us
p95_per_query_us
```

The runner uses enough repeated calls inside each timer sample to reduce timer noise while preserving a fixed amount of verifier work per sample.

## Decision rule

Navigation-wide main-thread design budget remains:

```text
<0.5 ms typical
<1.0 ms normal peak
```

Interpret the bounded precision fallback as follows:

```text
full_precision_batch8 p95 < 0.5 ms
    -> freeze the static continuous verifier; no micro-optimization needed

0.5 ms <= batch8 p95 < 1.0 ms
    -> still within normal peak; inspect candidate ordering/rate before changing math

batch8 p95 >= 1.0 ms
    -> optimize or introduce a stricter precision-work budget before live integration
```

This is a target-machine benchmark, not a portable CI performance assertion.

## Run

```bash
bash benchmarks/navigation_trajectory_continuous/run_mingw64.sh
```
