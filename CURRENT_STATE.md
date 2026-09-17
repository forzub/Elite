# Elite — CURRENT STATE

**Updated:** 2026-09-17  
**Canonical branch:** `main`  
**Navigation:** Navigation v2 / shared NavigationWorld  
**Active stage:** `NAV-V2-LOCAL-1` — local reference accepted; conservative lateral avoidance candidate pending behavior gate

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

## `NAV-V2-MAP-2` — CLOSED

Accepted 10k evidence includes CPU compact candidate queries below `0.2 ms p95` and GPU dynamic total below `1.7 ms p95` on the target machine.

## `NAV-V2-SPACE-1` — CLOSED / ACCEPTED

Final accepted turn-aware target-machine evidence on `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

Pinned gate was `<=40 ms p95`. Static turn-aware search is accepted. Published dense region/portal state, per-edge geometry/clearance and precomputed turn angles are the accepted reference. Do not continue static turn optimization without new runtime evidence.

## `NAV-V2-LOCAL-1` — REFERENCE ACCEPTED

Behavior gate on `77d794a97f1bbd753a55871ff1ef7f6c21c2ed39`:

```text
NAVIGATION LOCAL HORIZON BOUNDARY CONTRACT: PASS
navigation_local: PASS
```

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
        v
receding-horizon temporary target
        |
        +-> Clear / PassThrough
        +-> Clear / Terminal
        +-> ConflictHold
        +-> StaleHold
```

### Compact-candidate performance — ACCEPTED

Target-machine measurement on `4b94048b15e6e2cd32754b6b8d48daedcb18625f`:

```text
scenario         p95_us     p95_ns/candidate
clear_16          0.4814          30.0873
clear_64          1.8327          28.6362
clear_256         7.9820          31.1798
clear_1024       36.9641          36.0977

conflict_16       0.5073          31.7062
conflict_64       1.9333          30.2078
conflict_256      7.5441          29.4693
conflict_1024    31.8656          31.1188

stale_1024        0.0359          0 candidates examined
```

Even the artificial 1024-candidate stress case is below `0.037 ms p95`. The one-pass local reference is not a performance problem and should not be optimized further without new evidence.

Raw evidence: `benchmarks/navigation_local/RUN_LOG.md`.

## Active candidate — conservative same-region lateral avoidance

`LocalAvoidancePlanner` is now on `main` as the first adjusted-target slice.

It owns no second world or spatial state. It consumes:

- accepted `LocalHorizonPlanner`;
- compact `NavigationMap::QueryResult`;
- public `NavigationSpace::queryPoint()` static checks.

For a nominal `ConflictHold` it probes a bounded deterministic 3D fan:

```text
15 deg ring x 8 azimuths
30 deg ring x 8 azimuths
maximum = 16 target probes
```

A lateral target is considered statically proven only when both current agent point and target are envelope-safe and resolve to the same NavigationSpace region. Because one semantic region is a convex AABB, the whole straight segment then remains inside that free-space region.

The candidate is intentionally conservative:

- a valid portal-crossing maneuver can be rejected;
- stale dynamic data remains `StaleHold` before avoidance work;
- non-traversable static start becomes `StaticHold`;
- current-kinematics head-on/crossing conflicts remain `ConflictHold`;
- only a conflict that can be cleared by a statically proven target and the accepted dynamic reference becomes `AdjustedClear / PassThrough`.

Status: **pending target-machine compile + architecture + behavior gate**. No avoidance performance or production acceptance is claimed yet.

## Immediate next step

Run:

```bash
python tests/architecture_contracts/check_navigation_local_boundary.py
python tests/architecture_contracts/check_navigation_local_avoidance.py
bash tests/navigation_local/run_mingw64.sh
```

If PASS, benchmark the multiplied avoidance probe cost separately before any live game/server integration.
