# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — continuous static-passage performance gate

## Closed foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted target-machine evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Multiplied-probe target-machine reference on `e0817d157ba5d8c9c329576236310507bda13364`:

```text
nominal_clear_64 p95               1.8333 us
all_dynamic_rejected_64 p95       35.8900 us
all_dynamic_rejected_256 p95     128.9708 us
all_dynamic_rejected_1024 p95    519.9286 us
```

The `1024 x 17` case is deliberate stress and remains inside the `<1.0 ms normal peak` budget. Keep the deterministic 16-probe fan unchanged.

## `NAV-V2-TRAJECTORY-1` — active precision/control layer

Architecture authorities:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/CONTINUOUS_PASSAGE_MODEL.md
NAVIGATION_WORLD_V2.md
```

Accepted isolated components:

```text
BoundedGapCandidateBuilder
OrientedPassageEvaluator
AttitudeReachabilityEvaluator
EmergencyPassageMitigator
ContinuousPassageTrajectoryEvaluator
```

### Bounded gap — ACCEPTED

Worst deliberate stress:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

### Emergency passage mitigation — ACCEPTED

Target-machine acceptance on `b29a03d3d84f4d6575cbbc5166cbd7547b5ce0d8`:

```text
NAVIGATION TRAJECTORY BOUNDED GAP CONTRACT: PASS
NAVIGATION TRAJECTORY EMERGENCY PASSAGE CONTRACT: PASS
4/4 trajectory CTest PASS
```

Accepted invariant:

```text
no collision-free proof != no navigation command
```

If stopping is impossible, `EmergencyMitigatedContact` remains an explicitly non-safe control intent. Physics/collision owns actual contact/ricochet and damage owns consequences.

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

The accepted verifier checks one analytic static/extruded passage segment:

```text
center translation: cubic Hermite start P/V -> end P/V
attitude: shortest-arc smoothstep rotation
33 pose samples
32 conservative continuous interval proofs
```

Continuous safety is not sample-only. Every interval includes:

```text
center curve deviation <= M * dt^2 / 8
rotation sweep inflation <= 2 * R * sin(deltaTheta / 2)
```

Physical gates include forward/reverse/lateral/vertical acceleration plus angular speed/acceleration. `Newtonian` permits velocity/attitude divergence; `EliteAssisted` keeps the same physical authority plus a supplied slip-angle policy.

## Active performance gate

Dedicated harness:

```text
benchmarks/navigation_trajectory_continuous/
```

Scenarios:

```text
straight_newton
rolled_newton
lateral_newton
elite_aligned
geometry_blocked_roll
full_precision_batch8
```

`full_precision_batch8` mirrors the upstream hard `<=8` surviving precision-candidate ceiling and is the primary integration signal.

Decision rule:

```text
batch8 p95 < 0.5 ms  -> freeze static continuous verifier
batch8 p95 < 1.0 ms  -> acceptable normal peak; inspect scheduling/order before math changes
batch8 p95 >= 1.0 ms -> optimize/budget before live integration
```

## Docking continuation

Docking remains terminal relative 6DoF pose/motion matching against a stationary/moving/rotating port with explicit mating frames and `bottom of ship -> bottom of dock` orientation. For a rotating port:

```text
v_port = v_origin + omega x r
```

## Immediate next step

```bash
python tests/architecture_contracts/check_navigation_trajectory_continuous_benchmark.py
bash benchmarks/navigation_trajectory_continuous/run_mingw64.sh
```

After performance acceptance: extend emergency selection with relative normal contact-speed / impact-energy ranking, then move to time-varying gaps and moving/rotating docking.
