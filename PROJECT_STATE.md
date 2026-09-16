# Project State

**Updated:** 2026-09-16 Europe/Kyiv  
**Current project focus:** NavigationWorld v2 / static-space spatial indexing  
**Canonical development branch:** `main`  
**Active stage:** `NAV-V2-SPACE-1`

## Repository state

`main` is the only canonical game-development branch. Branch governance is defined by `REPOSITORY_SOURCE_OF_TRUTH.md`. Do not resume feature work on historical parallel refs.

## Navigation v2 direction

Navigation v2 uses one shared ship-centered `NavigationWorld`. Authoritative system/world state remains physical truth; relevant state is published into a translating working space with stable navigation/travel axes. Hub/station/carrier/interior spaces remain local domains and publish only relevant subsets.

The old route-wide chain is migration code only. `RuckigTrajectorySolver` remains downstream local kinematics, not path-search authority.

Accepted hybrid ownership:

```text
CPU
    static free-space / clearance
    connectivity / portals
    sparse cached global corridor search
    precision local planning

GPU
    dynamic P/V/A prediction
    conservative swept bounds
    spatial binning
    all-agent neighbor/conflict reduction
```

## Accepted moving-goal contract

Moving target pursuit is documented in:

```text
src/world/navigation/PURSUIT_HORIZON.md
```

A pursuer uses a receding predicted intercept state/region rather than repeatedly routing to stale current target position. Prediction uses available target P/V/A plus static/dynamic feasibility and does not require a complete global route rebuild every frame. Private hostile target intent is not automatically shared.

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k long-run evidence:

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

Public `NavigationSpace` API remains backend-neutral/PImpl. The CPU reference uses free-space AABB regions + explicit portals internally with agent-envelope clearance, deterministic corridor output, fail-closed invalidation and transactional local patching.

Target-machine behavior test:

```text
navigation_space: 1/1 PASS
100% tests passed, 0 failed
```

## Static-space scaling progression

### Baseline

```text
10k corridor ≈1.95-1.99 s
portal examinations=285,913,245
```

### Optimization 1 — per-region adjacency — ACCEPTED

```text
10k corridor ≈21.8-22.4 ms
portal examinations=57,189
```

### Optimization 2 — dense RegionSlot graph — ACCEPTED

Target-machine rerun:

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
```

The first dense-slot architecture check falsely reported a missing `std::vector<RegionSlot> frontier` marker. The implementation correctly used `std::vector<Impl::RegionSlot>`; behavior/benchmark passed and the test marker is corrected on `main`.

Combined graph optimization moved the 10k coarse corridor from about two seconds to about eight milliseconds. That is acceptable as asynchronous worker/reference work; unweighted BFS micro-optimization is no longer the active priority.

Raw history: `benchmarks/navigation_space/RUN_LOG.md`.

## Optimization 3 candidate — private static-space BVH

The remaining interactive/static measurement showed:

```text
open_10k point p95=0.2814 ms, invalidate p95=3.7643 ms
hub_10k  point p95=0.7043 ms, invalidate p95=3.3515 ms
```

`NavigationSpace::Impl` now builds a private AABB BVH over dense `RegionSlot` identity.

Uses:

```text
queryPoint candidate reduction
corridor start/end region localization
invalidateBounds candidate reduction
```

A separate private endpoint-incidence list invalidates only portals touching changed regions, including incoming one-way portals.

Exact region intersection/clearance semantics remain authoritative after candidate reduction. Public `NavigationSpace.h` is unchanged.

Full static publication and local patch rebuild graph + BVH transactionally. They remain update/worker-path operations and may cost more; the current optimization targets ordinary point/invalidation work.

## Active gate

Rerun on the target MinGW64 machine:

```bash
python tests/architecture_contracts/check_navigation_space_boundary.py
bash tests/navigation_space/run_mingw64.sh
python tests/architecture_contracts/check_navigation_space_benchmark.py
bash benchmarks/navigation_space/run_mingw64.sh
```

After the measurement, accept/reject the spatial index from evidence. If ordinary queries become cheap, proceed to weighted/costed corridor semantics plus aperture/canyon route-choice acceptance cases. Live runtime integration remains later.

## Documentation Definition of Done

Meaningful NavigationWorld iterations synchronize current state/task, project state, affected contracts and project-context evidence before handoff.
