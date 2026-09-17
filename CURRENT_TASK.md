# Elite — CURRENT TASK

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Track:** Navigation v2 / shared NavigationWorld  
**Stage:** `NAV-V2-TRAJECTORY-1` — emergency passage mitigation gate active

## Closed evidence

### Local avoidance — CLOSED / ACCEPTED

Target-machine behavior is accepted. Fresh multiplied-probe benchmark on `e0817d157ba5d8c9c329576236310507bda13364`:

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

The `1024 x 17` path is deliberate stress. It remains below the `<1.0 ms normal peak` budget. The deterministic 16-probe fan stays unchanged.

Authority:

```text
benchmarks/navigation_local_avoidance/RUN_LOG.md
```

### Oriented passage / bounded gap / attitude reachability — behavior green

On the same target-machine commit:

```text
navigation_trajectory_passage      PASS
navigation_trajectory_gap          PASS
navigation_trajectory_reachability PASS
100% tests passed, 0 failed
Total Test time: 0.12 sec
```

Passage and reachability architecture contracts passed. The gap architecture checker failed only on an obsolete exact Markdown sentence; the runtime test passed. Current `main` repairs that checker to assert stable markers (`hard candidate cap = 8`, one primary conflict, no all-pairs scan).

### Bounded gap performance — ACCEPTED

Target-machine worst stress result:

```text
top8_1024 p95 = 24.0699 us = 0.0241 ms
```

Full reference is recorded in:

```text
benchmarks/navigation_trajectory_gap/RUN_LOG.md
```

Do not optimize the gap builder further without contrary evidence.

## Active Gate — emergency contact mitigation

User requirement:

```text
safe route unavailable
    != navigation disabled
```

When a gap exists but the ship is too fast/close to reach the requested collision-free orientation, the system must still attempt to minimize consequences. A glancing contact/ricochet is an acceptable emergency outcome if stopping/avoidance is physically impossible.

New candidate:

```text
src/world/navigation/trajectory/EmergencyPassageMitigator.h
src/world/navigation/trajectory/EmergencyPassageMitigator.cpp
```

Tests/contracts:

```text
tests/navigation_trajectory/NavigationTrajectoryEmergencyPassageTests.cpp
tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
```

Semantics:

```text
SafeEntryPose
    a physically reachable sampled attitude fits the entry cross-section
    continuous swept-body safety is still not claimed

EmergencyStopBeforeEntry
    no sampled passage pose fits, but braking can stop before contact
    -> stop/replan; do not intentionally hit

EmergencyMitigatedContact
    no collision-free entry pose and no pre-entry stop are possible
    -> navigation remains active
    -> maximum useful braking
    -> aim at gap center
    -> desired travel along passage axis
    -> choose best reachable hull attitude
    -> contact/ricochet explicitly expected

InvalidInput
    no command may be claimed
```

The first implementation samples exactly `17` attitudes along the physically reachable portion of the shortest orientation arc. It scores them by passage clearance / geometric deficit. Equal-severity poses prefer greater correction toward the passage attitude.

This is intentionally a bounded pre-6DoF policy. It does **not** yet claim that requested gap-center translation or passage-axis velocity is instantly reachable.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_trajectory_gap.py
python tests/architecture_contracts/check_navigation_trajectory_emergency_passage.py
bash tests/navigation_trajectory/run_mingw64.sh
```

Send complete output.

## Next after green gate

Implement continuous bounded 6DoF maneuver feasibility:

1. consume authoritative body-axis linear thrust/braking + angular capability;
2. distinguish assisted `Elite` versus raw `Newton` reachability;
3. propagate position + velocity + attitude + angular state through the narrow-gap horizon;
4. prove swept oriented hull safety for collision-free candidates;
5. for emergency candidates, rank by predicted relative normal contact speed / impact-energy proxy in addition to geometric overlap;
6. allow real physics contact/ricochet and continue navigation from the actual post-impact state;
7. reuse the same moving-frame machinery for moving gaps and moving/rotating docking.

No live `EliteGame` / `EliteServer` integration until this isolated trajectory gate is green.
