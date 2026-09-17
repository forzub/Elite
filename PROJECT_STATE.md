# Project State

**Updated:** 2026-09-17 Europe/Kyiv  
**Current focus:** NavigationWorld v2 / local dynamic avoidance  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-LOCAL-1`

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior domains publish only relevant subsets.

Legacy route-wide navigation remains migration code. `RuckigTrajectorySolver` is downstream local kinematics.

Accepted hybrid ownership:

```text
CPU: static free-space, portals, corridor search, precision local search
GPU: dynamic P/V/A prediction, swept bounds, spatial bins, conflict reduction
```

Moving-goal pursuit is specified in `src/world/navigation/PURSUIT_HORIZON.md`; runtime pursuit remains later.

## `NAV-V2-MAP-2` — CLOSED

Dynamic-map CPU/GPU boundary and hybrid ownership are accepted. `NavigationMap` returns compact dynamic candidates by value and hides backend/cell/GPU state.

Current mass/broadphase actor geometry is radius + conservative swept sphere. It is intentionally cheap and conservative, not a final oriented-hull trajectory model.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted target-machine turn-aware implementation on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero-turn p95    8.4498 ms
open_10k turn-aware p95  12.0072 ms
hub_10k  zero-turn p95    8.4125 ms
hub_10k  turn-aware p95  11.9065 ms
turn portals examined    329,660
```

Static turn search is closed; do not spend more work there without new runtime evidence.

## `NAV-V2-LOCAL-1` — horizon reference accepted

Target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39` accepted the backend-neutral `LocalHorizonPlanner` boundary.

Compact-candidate benchmark on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass dynamic reference is not a CPU bottleneck.

## Active local slice — conservative adjusted target

`LocalAvoidancePlanner` tries at most sixteen deterministic target probes after nominal `ConflictHold`:

```text
15 degree deflection x 8 azimuths
30 degree deflection x 8 azimuths
```

A candidate must be envelope-safe in the same NavigationSpace region, use the same static space/source revision as the start evidence, and pass the accepted dynamic recheck.

The same-region rule is intentionally conservative. Portal-crossing avoidance is not yet claimed. Current-kinematics head-on/crossing remains fail-closed until a trajectory-aware maneuver is separately demonstrated.

### Latest target-machine attempt

Run on `f27006ccbba1a6faaa3d3000dedf8d8dee5f02e8` did not execute the new avoidance behavior binary:

```text
horizon architecture check -> FAIL on stale exact README phrase
avoidance architecture     -> PASS
navigation_local           -> PASS
navigation_local_avoidance -> NOT RUN, executable absent
```

This was test infrastructure, not avoidance behavior evidence. The architecture check has been repaired to pin semantic ownership, and `tests/navigation_local/run_mingw64.sh` now builds the complete CMake project before CTest instead of building only the historical horizon executable.

Avoidance status remains **pending target-machine behavior rerun**, not failed.

## Planned trajectory/control fidelity

Architecture contract:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

The post-avoidance trajectory-aware layer must consume authoritative vehicle capability rather than assuming an instant acceleration-vector change. Planned fidelity includes:

```text
oriented hull proxy + attitude / angular state
body-axis or thruster acceleration authority
rotation time before braking / vector change
assisted Elite-style versus Newtonian free-flight behavior
airplane-like / lateral / rotate-then-thrust / flip-and-burn strategies
oriented swept-body safety checks
NPC PilotSkillProfile: reaction, decision rate, smoothing, damping/overshoot, precision
```

Poor NPC skill is modeled through delayed/under-damped control execution, not by falsifying geometry or actor dimensions. A low-skill pilot may genuinely oscillate or collide when its corrections consume the remaining safety margin.

## Next order

1. rerun local horizon + avoidance architecture/behavior gate with repaired test infrastructure;
2. benchmark multiplied avoidance probe cost for nominal-clear, early-adjust, static-reject and dynamic-reject paths;
3. if accepted, begin the trajectory-aware vehicle/control layer from `TRAJECTORY_CONTROL_MODEL.md`;
4. add pursuit/receding-intercept as a later consumer;
5. implement raw NavigationWorld debug visualization from the same completed snapshot;
6. integrate accepted NavigationWorld products into live game/server;
7. retire legacy route-wide navigation only after v2 owns the live path.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
