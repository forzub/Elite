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

## World economy / traffic direction

The formalized game-design direction for trade flows, causal NPC traffic, civilized navigation infrastructure, taxes/fees/services, insurance and the core-vs-frontier scale contrast is now recorded in:

```text
WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md
```

Reference observations from `Objects in Space` are kept separately in:

```text
Notes/OBJECTS_IN_SPACE_ECONOMY_REFERENCE.md
```

Important provenance: the core trade-flow / corridor / beacon / causal-traffic ideas predate that reference in this project. The reference is used to sharpen and formalize the presentation. The major deliberate divergence is scale: Elite should preserve much larger distances, stronger isolation and a sharper transition from infrastructure-rich civilization to self-navigated frontier/deep space.

This design note does **not** change the active `NAV-V2-LOCAL-1` implementation milestone.

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

## `NAV-V2-LOCAL-1` — horizon reference ACCEPTED

Target-machine behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39` accepted the backend-neutral `LocalHorizonPlanner` boundary.

Compact-candidate benchmark on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
clear_1024 p95       36.9641 us
conflict_1024 p95    31.8656 us
stale_1024 p95        0.0359 us / 0 candidates examined
```

The one-pass dynamic reference is not a CPU bottleneck.

## `LocalAvoidancePlanner` behavior — ACCEPTED

Fresh target-machine gate on `bdec064d152050b4bc199b2657f14b3f577dcba3`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
NAVIGATION LOCAL AVOIDANCE BOUNDARY CONTRACT: PASS
navigation_local: PASS
navigation_local_avoidance: PASS
100% tests passed, 0 failed
Total Test time: 0.06 sec
```

Accepted behavior:

```text
nominal Clear
    -> zero avoidance probes

nominal ConflictHold
    -> query start point from public NavigationSpace
    -> deterministic 15 deg x 8 + 30 deg x 8 fan
    -> same-region + same-publication static proof
    -> dynamic recheck through accepted LocalHorizonPlanner
    -> first proven target = AdjustedClear / PassThrough
    -> otherwise ConflictHold
```

The same-region rule remains deliberately conservative. Portal-crossing avoidance is not yet claimed. Current-P/V/A head-on/crossing remains fail-closed until a trajectory-aware maneuver is separately demonstrated.

## Active local gate — multiplied avoidance probe cost

Dedicated harness:

```text
benchmarks/navigation_local_avoidance/
```

It measures:

```text
nominal_clear_64
    0 probes, 1 horizon evaluation

early_adjust_64
    first probe accepted
    1 probe, 2 horizon evaluations, 2 static point queries

all_static_rejected_64
    16 probes rejected statically
    1 horizon evaluation, 17 static point queries

all_dynamic_rejected_16/64/256/1024
    16 statically valid probes
    16 failed dynamic rechecks
    17 horizon evaluations, 17 static point queries
```

The `1024` case is a deliberate ceiling/stress measurement. It is not an expected normal local-neighbor count.

The benchmark reports median/p95 timing plus probe counts, static/dynamic rejections, total horizon-evaluation count, estimated compact-candidate visits and static point queries. Scenario/static-space construction is outside the timed region.

Performance is pending target-machine evidence. Existing local CPU design budgets remain:

```text
<0.5 ms typical
<1.0 ms normal peak
```

## Planned trajectory/control fidelity

Architecture contract:

```text
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
```

The post-benchmark trajectory-aware layer must consume authoritative vehicle capability rather than assuming an instant acceleration-vector change. Planned fidelity includes:

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

1. run `check_navigation_local_avoidance_benchmark.py` and `benchmarks/navigation_local_avoidance/run_mingw64.sh` on the target machine;
2. if the 16-probe fan fits the accepted local CPU budget, keep it unchanged; otherwise change ordering/budget only from measured evidence;
3. begin the trajectory-aware vehicle/control layer from `TRAJECTORY_CONTROL_MODEL.md`;
4. add pursuit/receding-intercept as a later consumer;
5. implement raw NavigationWorld debug visualization from the same completed snapshot;
6. integrate accepted NavigationWorld products into live game/server;
7. retire legacy route-wide navigation only after v2 owns the live path.

Do not add velocity, braking, dynamic traffic or pursuit prediction to persistent `NavigationSpace` static cost.
