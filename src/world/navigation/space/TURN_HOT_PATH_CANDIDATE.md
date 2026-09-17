# Turn-aware hot-path optimization candidate

Stage: `NAV-V2-SPACE-1`

The Euclidean A* candidate is rejected by target-machine evidence: it reduced 10k `turn_portals_examined` only from 329,660 to 323,888 while increasing turn-aware p95 from 67.9647/72.6054 ms to 97.0537/97.6909 ms (open/hub).

The next candidate keeps exact dense turn-state Dijkstra semantics:

```text
state = (RegionSlot, incoming PortalId)
frontier = binary heap ordered by g
```

The optimization target is per-expanded-edge cost, not another heuristic. Static values that do not depend on the query should be published with the graph instead of recomputed in the query hot loop:

- dense region center and capacity;
- dense invalidation flags synchronized with `invalidateBounds()`;
- dense portal slot, center and invalidation flag;
- per-directed-edge geometric distance;
- per-directed-edge available static clearance;
- precomputed turn angle for `(arrival TurnStateSlot, outgoing adjacency edge)`.

The positive-turn query should then avoid ordered-map lookups and avoid `sqrt`/`acos` in its expanded-state loop. Policy-dependent distance/clearance/turn multipliers remain query-time values.

This is a private representation optimization. Public API, accepted zero-turn v1 path, aperture/canyon behavior, turn semantics, and deterministic optimal-cost search remain unchanged.

Target-machine gate remains `<=40 ms p95` at 10k. If this still misses the gate, the next step is search-work reduction (bidirectional/hierarchical turn-state search), not another weak Euclidean heuristic.
