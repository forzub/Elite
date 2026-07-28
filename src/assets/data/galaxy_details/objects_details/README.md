# Interstellar object files

Every `.json` file in this directory is loaded automatically. No registry or manifest is required.

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
