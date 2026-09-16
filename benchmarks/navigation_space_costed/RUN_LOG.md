# NavigationSpace costed corridor benchmark log

**Stage:** `NAV-V2-SPACE-1`  
**Date:** 2026-09-16  
**Machine:** user's Windows 10 / MSYS2 MinGW64 target machine

## Contract

```text
NAVIGATION SPACE COSTED BENCHMARK CONTRACT: PASS
```

The benchmark uses the same published static snapshot for two policy profiles:

```text
distance_only
clearance_aware
```

The topology generator includes deterministic reduced-clearance portals so the clearance-aware policy performs real alternate-route evaluation rather than only adding a constant.

Default run:

```text
warmup=1
iterations=5
```

## Accepted target-machine measurements

```text
scenario   regions portals  distance med/p95 ms  clearance med/p95 ms  regions visited  portals examined  path regions  distance cost  clearance cost
open_1k       1000    2650    0.5340 / 0.5763      0.5723 / 0.6244         1009            5297             33          38400          39150
open_5k       5000   14050    3.4395 / 4.1467      3.9125 / 4.6831         5006           28097             53          62400          63150
open_10k     10000   28600    7.6855 / 8.1733      8.3785 / 8.5020        10006           57197             63          74400          75150
hub_1k        1000    2700    0.5168 / 0.5172      0.5546 / 0.6471         1009            5397             28           1350           1395
hub_5k        5000   14050    3.5556 / 3.7932      3.7808 / 4.0511         5006           28097             53           2600           2645
hub_10k      10000   28600    7.4664 / 7.7609      9.3099 / 9.6103        10006           57197             63           3100           3145
```

Every case returned `found=1`.

## Decision

The previously pinned acceptance rule was:

```text
10k p95 <= 15 ms
    accept deterministic Dijkstra reference as-is
    proceed to turn/curvature cost
```

Worst measured 10k p95 is:

```text
9.6103 ms  hub_10k / clearance_aware
```

Therefore costed corridor v1 is **ACCEPTED** as an asynchronous worker/reference solve. No priority-queue/A* optimization is required before adding the next static route-quality term.

This result does not mean every active NPC should run a full 10k costed solve every frame. Production still relies on cached/reused coarse branches, bounded local horizons and selective replanning.

## Next candidate

Static turn cost is defined by:

```text
src/world/navigation/STATIC_TURN_COST.md
```

`turnPenaltyMetersPerRadian == 0` must preserve this accepted v1 path. Positive turn penalty uses expanded `(RegionSlot, incoming PortalId)` state so route history required for turn angle is not lost.
