# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-LOCAL-1` — same-region avoidance behavior accepted; multiplied-probe benchmark pending. First oriented-passage precision candidate prepared in parallel.

## Accepted foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU dynamic total below `1.7 ms p95` on the target machine.

Current shared dynamic broadphase remains intentionally cheap/conservative: center P/V/A + radius + swept sphere.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final turn-aware target-machine evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k zero p95     8.4125 ms
hub_10k turn p95    11.9065 ms
turn portals examined 329,660
```

Do not reopen static turn optimization without new runtime evidence.

## `NAV-V2-LOCAL-1` — horizon + avoidance behavior ACCEPTED

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
Total Test time: 0.06 sec
```

Accepted bounded avoidance:

```text
nominal ConflictHold
    -> 15 deg x 8 + 30 deg x 8 deterministic target fan
    -> same-region / same-publication static proof
    -> compact dynamic recheck
    -> first proven target = AdjustedClear
    -> otherwise fail closed
```

Current-P/V/A head-on/crossing remains fail-closed until a trajectory-aware maneuver is demonstrated.

## Active measurement — 16-probe fan cost

Dedicated target-machine harness:

```text
benchmarks/navigation_local_avoidance/
```

It measures nominal clear, first-probe success, 16 static rejections, and 16 repeated dynamic rejections at compact candidate counts 16/64/256/1024.

`1024` is deliberate stress. Performance is not accepted until fresh target-machine median/p95 is captured.

Current local CPU budgets:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## New precision candidate — oriented passages and emergent gaps

Architecture:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

First backend-neutral candidate:

```text
src/world/navigation/trajectory/OrientedPassageEvaluator.h
src/world/navigation/trajectory/OrientedPassageEvaluator.cpp
```

It solves one deliberately small problem in O(1): given a body-local OBB proxy, ship pose, and an already-selected oriented passage cross-section, determine whether the real hull fits at that attitude/offset.

This recovers cases that the conservative broadphase sphere intentionally rejects, such as a flat ship rolling through a flat slot.

Passage sources share one representation:

```text
AuthoredAperture
ObstacleGap
DockingCorridor
```

Important new semantic rule: **two nearby objects may form a positive free-space passage**. If ordinary avoidance cannot clear them because speed is high / distance is short, the free space between them may still be usable when the real hull fits at an appropriate attitude.

Performance protection is explicit:

```text
cheap broadphase/local avoidance
        |
        +-- success -> done
        |
        +-- ConflictHold / explicit aperture / docking
              -> bounded gap candidates only
              -> initial design target <= 4-8
              -> O(1) oriented fit per candidate
              -> full 6DoF proof only for plausible fits
```

Unbounded `N x N` obstacle-pair search on the frame path is rejected.

Behavior candidate is not accepted yet; target-machine architecture/build/tests are pending.

Pinned first fixtures:

```text
flat hull + flat slot, correct attitude -> Fits while sphere rejects
same hull rolled 90 degrees            -> rejected
two-obstacle narrow gap, wide attitude -> rejected
same gap, thin rolled attitude          -> Fits
off-center hull                         -> rejected
degenerate frame                        -> fail closed
```

## Planned 6DoF / docking continuation

After the current gates, precision work continues with:

- vehicle attitude/angular state;
- body-axis thrust and rotation authority;
- rotation time before braking/vector change;
- assisted `Elite` versus Newtonian behavior;
- continuous swept oriented-body proof through narrow gaps;
- time-varying gaps between moving objects;
- moving/rotating docking frames;
- docking relative pose / linear velocity / angular velocity matching;
- explicit `bottom of ship -> bottom of dock` mating-frame orientation;
- deterministic NPC `PilotSkillProfile`, including over-correction/oscillation/go-around behavior.

A low-skill NPC may genuinely crash by consuming its safety margin; geometry and physical capability remain truthful.

## Immediate next step

Run both currently pending target-machine checks:

```bash
python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh

python tests/architecture_contracts/check_navigation_trajectory_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Do not live-wire game/server control or pursuit until the relevant gates are accepted.
