# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-LOCAL-1` — same-region lateral avoidance behavior accepted; multiplied-probe performance gate active

## Accepted Navigation v2 ownership

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    deterministic precision/local reference work

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

`RuckigTrajectorySolver` remains downstream local kinematics, not path search.

## Closed foundations

### `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU dynamic total below `1.7 ms p95` on the target machine.

Current dynamic broadphase actor geometry is radius + conservative swept sphere. This is safe/cheap but not final oriented-hull maneuver fidelity.

### `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware target-machine evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

Pinned gate was `<=40 ms p95`. Static turn-aware search is accepted and should not be reopened without runtime evidence.

## `NAV-V2-LOCAL-1` — horizon reference ACCEPTED

Target-machine compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass local reference is not a CPU bottleneck.

## `LocalAvoidancePlanner` behavior — ACCEPTED

Fresh target-machine behavior gate on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

Accepted bounded behavior:

```text
nominal Clear
    -> NominalClear / zero lateral probes

nominal ConflictHold
    -> NavigationSpace start point
    -> 15 deg x 8 + 30 deg x 8 deterministic fan
    -> same-region / same-publication static proof
    -> accepted LocalHorizonPlanner dynamic recheck
    -> first proven target = AdjustedClear / PassThrough
    -> otherwise ConflictHold
```

Important limits remain intentional:

- portal-crossing adjusted targets may be conservatively rejected;
- stale/static-unsafe input fails closed;
- current-P/V/A head-on or crossing conflicts remain `ConflictHold` until a trajectory-aware maneuver is demonstrated.

## Active measurement — avoidance fan cost

Dedicated target-machine harness now lives at:

```text
benchmarks/navigation_local_avoidance/
```

It separates four cost classes:

```text
nominal_clear_64
    one horizon pass, zero lateral/static work

early_adjust_64
    first lateral probe succeeds

all_static_rejected_64
    all 16 probes fail NavigationSpace before dynamic recheck

all_dynamic_rejected_16/64/256/1024
    all 16 probes are statically valid
    all 16 dynamic rechecks fail
    17 total horizon evaluations
```

The `1024` case is deliberate stress. Performance is **not yet accepted** until fresh target-machine median/p95 output is captured in `benchmarks/navigation_local_avoidance/RUN_LOG.md`.

Current local CPU design budgets remain:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## Planned next physical-control layer

`src/world/navigation/TRAJECTORY_CONTROL_MODEL.md` is the accepted post-benchmark architecture contract.

The next trajectory-aware stage must add, through the authoritative flight/physics boundary:

- oriented hull dimensions/proxy and attitude;
- angular state and rotation authority;
- body-axis/thruster acceleration authority;
- rotation time before braking/vector change;
- assisted `Elite` versus Newtonian flight behavior;
- airplane-like / lateral / rotate-then-thrust / flip-and-burn maneuver feasibility;
- deterministic NPC `PilotSkillProfile` for reaction delay, control smoothness, damping/overshoot, anticipation and precision.

A low-skill NPC may genuinely oscillate or collide through delayed/poor control execution; geometry and physical capability remain truthful.

## Immediate next step

Run the dedicated avoidance performance gate:

```bash
python tests/architecture_contracts/check_navigation_local_avoidance_benchmark.py
bash benchmarks/navigation_local_avoidance/run_mingw64.sh
```

Do not integrate live game/server control or pursuit until this gate is closed.
