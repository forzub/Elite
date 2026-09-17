# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-TRAJECTORY-1` — oriented passage / emergent gap / emergency mitigation

## Closed foundations

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

### `NAV-V2-LOCAL-1` — CLOSED / ACCEPTED

Behavior was accepted earlier. Fresh target-machine multiplied-probe evidence on `e0817d157ba5d8c9c329576236310507bda13364`:

```text
scenario                     p95_us
nominal_clear_64               1.8333
early_adjust_64                4.2795
all_static_rejected_64         4.2655
all_dynamic_rejected_16       12.9500
all_dynamic_rejected_64       35.8900
all_dynamic_rejected_256     128.9708
all_dynamic_rejected_1024    519.9286
```

Design budget:

```text
<0.5 ms typical
<1.0 ms normal peak
```

The `1024 x 17` case is deliberate stress and is not a normal compact-neighbor count. It remains inside the normal-peak budget. Decision: keep the deterministic 16-probe fan unchanged.

Authority:

```text
benchmarks/navigation_local_avoidance/RUN_LOG.md
```

## `NAV-V2-TRAJECTORY-1` — current isolated precision layer

Architecture authorities:

```text
src/world/navigation/ORIENTED_PASSAGE_MODEL.md
src/world/navigation/TRAJECTORY_CONTROL_MODEL.md
NAVIGATION_WORLD_V2.md
```

Current chain:

```text
ConflictHold / explicit aperture / docking corridor
        |
        v
BoundedGapCandidateBuilder
    one primary conflict + already reduced neighbors
    hard candidate cap = 8
    no global/all-pairs scan
        |
        v
OrientedPassageEvaluator
    oriented OBB cross-section fit
        |
        v
AttitudeReachabilityEvaluator
    can the requested collision-free attitude be ready in time?
        |
        +-- yes -> later continuous 6DoF safe proof
        |
        +-- no
              v
EmergencyPassageMitigator
    keep navigation intent alive
    brake when useful
    aim at gap center
    bias travel along passage axis
    choose best physically reachable hull attitude
    explicit contact-expected result when unavoidable
```

### Geometry / bounded-gap behavior — target-machine green

On `e0817d...` all then-registered trajectory C++ tests passed:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time: 0.12 sec
```

The passage and reachability architecture contracts passed. The gap architecture checker had only a stale exact-Markdown marker; runtime behavior was green. That checker is repaired on current `main` to assert stable architecture invariants.

### Bounded gap performance — ACCEPTED

Target-machine p95:

```text
reject_16       0.2459 us
reject_64       0.7550 us
reject_256      4.6499 us
reject_1024    11.6730 us

top8_16         0.7883 us
top8_64         1.8260 us
top8_256        6.3609 us
top8_1024      24.0699 us
```

Even `top8_1024` stress is about `0.024 ms p95`. The bounded-gap builder is not a CPU concern and should not be micro-optimized without contrary evidence.

Authority:

```text
benchmarks/navigation_trajectory_gap/RUN_LOG.md
```

## New emergency semantic — prepared candidate

`UnreachableBeforeEntry` now means only:

```text
collision-free requested entry attitude is not proven reachable in time
```

It does **not** mean planner shutdown.

New isolated files:

```text
src/world/navigation/trajectory/EmergencyPassageMitigator.h
src/world/navigation/trajectory/EmergencyPassageMitigator.cpp
tests/navigation_trajectory/NavigationTrajectoryEmergencyPassageTests.cpp
tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
```

Result classes:

```text
SafeEntryPose
EmergencyStopBeforeEntry
EmergencyMitigatedContact
InvalidInput
```

Emergency policy:

```text
safe maneuver if reachable
else stop before contact if physically possible
else maximum useful braking
     + aim at gap center
     + desired travel along passage axis
     + best physically reachable hull attitude
     + explicit expected contact/ricochet
```

`EmergencyMitigatedContact` is a valid navigation/control intent but never a safe/`Clear` result. Actual collision impulse, ricochet, damage and post-impact state remain physics/damage authority.

The first candidate samples a fixed 17 reachable attitudes along the shortest orientation arc and selects the pose with best passage clearance / smallest geometric deficit. Full 6DoF work must later add translational authority and relative normal impact speed.

## Docking remains in the same 6DoF layer

Docking is terminal pose/motion matching against a possibly moving/rotating port:

```text
relative position
relative linear velocity
relative attitude
relative angular velocity
explicit mating frame / top-bottom convention
```

`bottom of ship -> bottom of dock` remains explicit. A rotating port includes:

```text
v_port = v_origin + omega x r
```

Normal docking should abort/go-around if capture is unsafe; generic emergency impact mitigation only applies if physical collision has truly become unavoidable.

## Immediate next step

Target-machine gate only for the newly changed trajectory slice:

```bash
python tests/architecture_contracts/check_navigation_trajectory_gap.py
python tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

If green, the next implementation is continuous 6DoF translation+rotation through the gap with explicit `Elite`/`Newton` authority and emergency ranking by relative normal contact speed / impact-energy proxy.
