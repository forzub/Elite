# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-LOCAL-1` — avoidance behavior accepted; 16-probe performance gate pending. Trajectory precision candidates are prepared in parallel and remain unaccepted until target-machine gates pass.

## Accepted foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted target-machine evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU 10k heavy-scene totals below `1.7 ms p95`.

Shared dynamic broadphase intentionally remains cheap/conservative: center P/V/A + radius + swept sphere.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

Do not reopen static turn optimization without new runtime evidence.

## `NAV-V2-LOCAL-1` — behavior ACCEPTED / performance pending

Accepted compact-candidate reference on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

Fresh avoidance behavior gate on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
```

Accepted bounded avoidance:

```text
ConflictHold
    -> deterministic 15 deg x 8 + 30 deg x 8 target fan
    -> same-region / same-publication static proof
    -> compact dynamic recheck
    -> AdjustedClear or fail closed
```

Current-P/V/A head-on/crossing remains fail-closed until a trajectory-aware maneuver is demonstrated.

Active timing harness:

```text
benchmarks/navigation_local_avoidance/
```

Local CPU design budget remains:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## Prepared trajectory precision chain — PENDING TARGET-MACHINE GATES

Architecture authorities:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
```

Prepared layering:

```text
accepted cheap broadphase/local avoidance
        |
        +-- normal safe result -> done
        |
        +-- ConflictHold / explicit aperture / docking corridor
                |
                v
        BoundedGapCandidateBuilder
        one primary conflict + already reduced neighbors
        hard cap = 8
        no all-pairs scan
                |
                v
        OrientedPassageEvaluator
        O(1) real oriented hull fit
                |
                v
        AttitudeReachabilityEvaluator
        O(1) angle / angular authority / distance check
        AlreadyReady / ReachableCoast / ReachableWithBraking /
        UnreachableBeforeEntry
                |
                v
        later full continuous 6DoF swept-body proof
```

### Oriented passage geometry

Files:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h/.cpp
```

A flat/non-spherical ship may fit a narrow aperture or obstacle gap only in the correct attitude even when the conservative broadphase sphere rejects it.

Passage sources share one abstraction:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

### Emergent obstacle gaps

Files:

```text
src/world/navigation/trajectory/BoundedGapCandidateBuilder.h/.cpp
```

Project invariant: **two nearby objects may form positive free space between them**. If ordinary lateral avoidance cannot clear either obstacle because speed is high / distance is short, a bounded fallback may test the free gap between them.

Performance protection:

- one already-known primary conflict is compared with already-reduced neighbors;
- no neighbor-neighbor or global `N x N` pair discovery;
- mixed snapshot revisions fail closed;
- conservative overlapping bounds do not invent a gap;
- front/back obstacle pairs are not misclassified as transverse slots;
- deterministic candidate cap is `8`.

Dedicated performance harness:

```text
benchmarks/navigation_trajectory_gap/
```

It measures reject/top-8 paths at 16/64/256/1024 local neighbors. `1024` is deliberate stress.

### Attitude reachability before entry

Files:

```text
src/world/navigation/trajectory/AttitudeReachabilityEvaluator.h/.cpp
```

A gap that fits geometrically may still be unusable if the ship cannot rotate into the required pose before reaching it.

The current conservative precheck consumes current/required attitude, current angular velocity, angular acceleration/rate authority, distance to entry, closing speed and available longitudinal braking.

Pinned semantics:

```text
AlreadyReady
ReachableCoast
ReachableWithBraking
UnreachableBeforeEntry
```

Example fixture: a 90 degree roll needs 2 s with the pinned angular authority. At 10 m/s it needs 20 m by coasting; with 5 m/s^2 braking it can become ready in 10 m; with only 8 m available it fails closed.

This evaluator is not a duplicate flight controller and does not yet prove a continuous swept-body trajectory.

## Docking remains in the same 6DoF layer

Docking is terminal pose/motion matching against a possibly moving/rotating port, including:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

`bottom of ship -> bottom of dock` remains explicit; an upside-down center-point arrival is invalid.

A rotating port target includes tangential point velocity:

```text
v_port = v_origin + omega x r
```

## Immediate next step

Run all pending isolated gates after fast-forwarding local `main`:

```bash
python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_passage.py
python tests/architecture_contracts/check_navigation_trajectory_gap.py
python tests/architecture_contracts/check_navigation_trajectory_reachability.py
bash tests/navigation_trajectory/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_gap_benchmark.py
bash benchmarks/navigation_trajectory_gap/run_mingw64.sh
```

Do not live-wire game/server control or pursuit until the relevant isolated gates are accepted.
