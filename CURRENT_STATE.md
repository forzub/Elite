# Elite — CURRENT STATE

**Updated:** 2026-09-16  
**Canonical branch:** `main`  
**Editor baseline:** v0.10.86 accepted  
**Renderer:** OpenGL 4.3 Core + GPU-P0/P0.1 accepted  
**Navigation:** `NAV-V2-SPACE-1` — static-space reference accepted; adjacency + dense-slot graph measured; private spatial-index candidate pending target-machine rerun

## Repository source of truth

`main` is the only canonical game-development branch. `REPOSITORY_SOURCE_OF_TRUTH.md` governs branch/recovery policy.

## Navigation v2 accepted architecture

Navigation v2 uses one shared ship-centered `NavigationWorld`. The old route-wide synchronous chain remains migration code; `RuckigTrajectorySolver` is downstream local kinematics after navigation selects a safe temporary target.

Accepted hybrid ownership from `NAV-V2-MAP-2`:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local search

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

GPU work remains asynchronous/double- or triple-buffered; no frame-thread `dispatch -> wait -> bulk readback` path is allowed.

Accepted moving-goal design is documented in:

```text
src/world/navigation/PURSUIT_HORIZON.md
```

Pursuit is a receding moving-goal/intercept horizon using available target P/V/A plus known static/dynamic feasibility. It does not rebuild a complete global route every frame and does not automatically consume an adversary's private route intent.

## `NAV-V2-MAP-2` — CLOSED

Accepted 100-iteration 10k evidence:

```text
CPU cruise corridor p95=0.1713 ms, sphere p95=0.1329 ms, rebuild p95=3.0958 ms
CPU hub    corridor p95=0.1551 ms, sphere p95=0.0471 ms, rebuild p95=2.4924 ms
GPU cruise total median=0.6840 ms, p95=1.3226 ms
GPU hub    total median=1.6097 ms, p95=1.6258 ms
```

## `NAV-V2-SPACE-1` boundary/reference — ACCEPTED

Canonical block:

```text
src/world/navigation/space/
```

Fresh target-machine behavior has passed:

```text
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

The public `NavigationSpace.h` boundary remains backend-neutral/PImpl. The deterministic CPU reference uses free-space AABB regions + explicit portals internally; this is not a permanent storage commitment.

## Static-space scaling history

### Baseline

```text
10k open corridor median/p95 = 1991.0357 / 2008.2074 ms
10k hub  corridor median/p95 = 1951.6453 / 1956.0686 ms
portal examinations          = 285,913,245
```

Root cause: full portal-map scan for every BFS region.

### Optimization 1 — per-region adjacency — ACCEPTED

```text
10k open corridor median/p95 = 21.7848 / 22.1695 ms
10k hub  corridor median/p95 = 22.3689 / 22.7493 ms
portal examinations          = 57,189
```

Adjacency reduced 10k corridor median by roughly `87–91x` and portal examinations by roughly `5000x`.

### Optimization 2 — dense RegionSlot graph — ACCEPTED

The first architecture-contract rerun falsely failed because the contract searched for `std::vector<RegionSlot> frontier` while the valid implementation used `std::vector<Impl::RegionSlot> frontier`. C++ behavior and benchmark both passed; the contract marker is fixed on `main`.

Measured 10k results after dense slots:

```text
open_10k
    replace med/p95      14.8300 / 15.1018 ms
    point med/p95         0.2783 / 0.2814 ms
    corridor med/p95      7.9396 / 7.9735 ms
    invalidate med/p95    3.6275 / 3.7643 ms
    patch med/p95        16.0010 / 16.0111 ms

hub_10k
    replace med/p95      14.1335 / 14.9556 ms
    point med/p95         0.4301 / 0.7043 ms
    corridor med/p95      7.9981 / 8.4877 ms
    invalidate med/p95    3.2642 / 3.3515 ms
    patch med/p95        16.1113 / 16.1715 ms

portal examinations     57,189
corridor_found           1
```

Dense slots reduced the post-adjacency corridor by another roughly `2.7–2.8x`. A ~8 ms 10k coarse global BFS is acceptable as worker-side reference work; further unweighted-BFS micro-optimization is not the active priority.

Raw history: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 3 candidate — private RegionSlot spatial BVH

Implemented on `main` without changing `NavigationSpace.h`.

The private CPU reference now owns:

```text
RegionSlot[] -> AABB BVH
```

Used by:

```text
queryPoint candidate reduction
corridor start/end region localization
invalidateBounds candidate reduction
```

Portal invalidation also uses a private endpoint-incidence list:

```text
RegionSlot -> PortalId[] touching the region
```

so local damage no longer requires scanning every portal merely to invalidate the six portals touching one changed region.

Exact AABB/clearance tests remain authoritative after candidate reduction, and candidate slots are sorted before semantic evaluation to preserve deterministic RegionId behavior.

Full publication/local patch now rebuild graph + BVH transactionally. Those operations are update/worker-path work and may become more expensive; ordinary point/invalidation work is the optimization target.

## Active gate

Rerun architecture + behavior + the same benchmark workload after the spatial-index change.

Do not yet wire NavigationSpace into live `EliteGame` / `EliteServer`. Weighted/costed corridor search, pursuit implementation, final NPC steering, precision docking/repair and Shift+F12 debug rendering remain later stages.
