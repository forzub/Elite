# Turn-aware hot-path publication — ACCEPTED

Stage: `NAV-V2-SPACE-1` — **CLOSED / ACCEPTED**

The accepted positive-turn reference keeps exact dense Dijkstra semantics:

```text
state = (RegionSlot, incoming PortalId)
frontier = binary heap ordered by g
```

Immutable static values are published with the graph instead of recomputed in the query hot loop:

- dense region center and capacity;
- dense invalidation flags synchronized with `invalidateBounds()`;
- dense portal slot, center and invalidation flag;
- per-directed-edge geometric distance;
- per-directed-edge available static clearance;
- precomputed turn angle for `(arrival TurnStateSlot, outgoing adjacency edge)`.

The positive-turn query therefore avoids ordered-map region/portal lookups and avoids repeated `sqrt`/`acos` geometry in its expanded-state loop. Policy-dependent distance/clearance/turn multipliers remain query-time values.

Target-machine acceptance run on commit `1acaddc1771d3b1a9466dfec7b0974379d74fcd1`:

```text
open_10k zero p95    8.4498 ms
open_10k turn p95   12.0072 ms
hub_10k  zero p95    8.4125 ms
hub_10k  turn p95   11.9065 ms
turn portals examined 329,660
```

Architecture, behavior and benchmark contracts all passed; accepted route length, turn burden and cost stayed unchanged. The pinned turn-aware gate was `<=40 ms p95`, so the implementation passes with large margin.

The unchanged `329,660` transition count is useful evidence: after dense state conversion, per-transition work rather than expanded-state semantics was the practical bottleneck in this reference workload.

Historical Euclidean A* remains rejected: it reduced transition work only ~1.75% while regressing 10k p95 to roughly 97 ms.

This optimization changes private representation only. Public API, zero-turn v1 path, aperture/canyon behavior and static turn semantics remain unchanged.

Raw evidence: `benchmarks/navigation_space_turn/RUN_LOG.md`.

No further persistent static turn-search optimization is active. The next stage is `NAV-V2-LOCAL-1`: dynamic conflict assessment + local receding-horizon temporary target selection.
