# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-LOCAL-1` — local reference accepted; conservative lateral avoidance candidate pending behavior rerun

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

## Closed stages

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

Pinned gate was `<=40 ms p95`. Static turn-aware search is accepted. Do not continue static turn optimization without new runtime evidence.

## `NAV-V2-LOCAL-1` — horizon reference accepted

Accepted local horizon semantics:

```text
static corridor / nominal local target
        +
NavigationMap Candidate[]
        +
agent P/V/A + result age
        |
        v
bounded dynamic conflict assessment
        |
        +-> Clear / PassThrough
        +-> Clear / Terminal
        +-> ConflictHold
        +-> StaleHold
```

Target-machine compact-candidate scaling on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass local reference is not a CPU bottleneck.

## Active candidate — conservative same-region lateral avoidance

`LocalAvoidancePlanner` probes at most 16 deterministic directions after nominal `ConflictHold`:

```text
15 deg ring x 8 azimuths
30 deg ring x 8 azimuths
```

Each adjusted target must:

- be traversable for the current envelope;
- resolve to the same NavigationSpace region as the start point;
- use the same space/source revision as the start static evidence;
- pass the accepted compact-dynamic recheck.

This is deliberately conservative. Portal-crossing avoidance may be rejected. Current-kinematics head-on/crossing remains fail-closed until a trajectory-aware maneuver is actually demonstrated.

### Latest target-machine run

Run on `f27006ccbba1a6faaa3d3000dedf8d8dee5f02e8` did **not** execute the avoidance behavior binary:

```text
horizon architecture check
    FAIL on stale exact README phrase

avoidance architecture check
    PASS

navigation_local
    PASS

navigation_local_avoidance
    NOT RUN: executable absent
```

Root cause was test infrastructure:

- old horizon checker was coupled to wording that had changed without ownership changing;
- runner built only the historical `navigation_local_tests` target even though CTest registered the new avoidance executable.

Both infrastructure defects are fixed on current `main`: semantic ownership is pinned and the runner builds all registered local test executables.

Avoidance behavior remains **pending rerun**, not failed.

## Planned next physical-control layer

`src/world/navigation/TRAJECTORY_CONTROL_MODEL.md` now records the post-avoidance trajectory/control contract.

Current local navigation uses center P/V/A + radius/swept sphere. The future trajectory-aware layer must add, through the authoritative flight/physics boundary:

- hull dimensions / oriented collision proxy;
- attitude and angular state;
- body-axis/thruster acceleration authority;
- rotation time before braking or thrust-vector change;
- assisted `Elite` versus free Newtonian control behavior;
- airplane-like, lateral, rotate-then-thrust and flip-and-burn maneuver feasibility;
- deterministic NPC `PilotSkillProfile` parameters such as reaction delay, decision rate, input smoothing, damping/overshoot and precision.

A low-skill NPC may therefore genuinely oscillate or collide because it reacts late or controls poorly; world geometry and vehicle dimensions must never be falsified to simulate incompetence.

## Immediate next step

Rerun:

```bash
python tests/architecture_contracts/check_navigation_local_boundary.py
python tests/architecture_contracts/check_navigation_local_avoidance.py
bash tests/navigation_local/run_mingw64.sh
```

If PASS, measure the multiplied avoidance probe cost before implementing the trajectory-aware layer.
