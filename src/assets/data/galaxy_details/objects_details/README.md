# Interstellar object files

Every `.json` file in this directory is loaded automatically. No registry or manifest is required.

Generated content (`EliteUniverseGenerator interstellar`, never edited by hand)
lives in two subdirectories, also loaded automatically:

- `interstellar_cells/block_<tag>.json` - `elite.interstellar_cell_block.v1`:
  the 25-ly generation cells covering the navigation domain, 4x4x4 per file.
  Each cell holds an `interstellar_medium_background` object (log10 density /
  phase lattice of the continuous background field: Local Bubble cavity,
  supernova-enriched wall, warm neutral medium) and its compact bodies.
- `interstellar_medium/<id>.json` - `interstellar_medium_region` objects:
  irregular density volumes (`geometry.shape = "density_lattice"`, relative
  density lattice, bounding ellipsoid only as metadata).

Lattice contract: `src/world/interstellar/DensityLattice.h`.

Minimal schema:

```json
{
  "schema_version": 1,
  "kind": "galaxy_object",
  "id": "rogue_planet_001",
  "name": "Nomad-01",
  "object_type": "rogue_planet",
  "position_ly": { "x": 7.25, "y": -3.8, "z": 11.4 },
  "description": "",
  "tags": [],
  "properties": {}
}
```

Suggested `object_type` values: `rogue_planet`, `brown_dwarf`, `interstellar_comet`, `nebula`, `black_hole`, `anomaly`, `derelict`, `relay`, `deep_space_station`, `resource_field`.
