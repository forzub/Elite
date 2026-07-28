# Galaxy details catalog

This directory is the authoritative physical catalog for star systems and free interstellar objects.

- `systems_details/*.json`: 60 local star systems shown in the ordinary Galaxy Atlas and decorative starfield.
- `distant_systems_details/*.json`: distant quest and long-range route destinations. They are loaded and searchable, but hidden from the ordinary atlas.
- `objects_details/*.json`: free interstellar objects such as rogue planets, brown dwarfs, comets, nebulae, anomalies, derelicts and deep-space stations.
- `migration_review/`: preserved legacy records that still cannot be matched safely to an active system.

A star-system file owns both its galactic summary and its internal celestial-body details. `id`, `name` and `position_ly` must not be duplicated elsewhere.

`position_ly` is the only source of truth for galactic distance. Do not add `distance_ly` to a system document: distance is derived as `length(position_ly)` by consumers. The loader rejects a stored `distance_ly` field so coordinates and displayed distance cannot diverge again.

The loader rejects malformed files, unsupported schema versions, duplicate system IDs, duplicate normalized system names, identical system coordinates, inconsistent `stars_count`, and duplicate galaxy-object IDs.

## Observational data versus the 3026 game model

For systems corrected from the NASA Exoplanet Archive, `observational_reference_2026` records the current observation status and source. Objects marked `procedural_future_gameplay` belong to the simulated year 3026 and must not be interpreted as NASA-confirmed planets in 2026.

Radial-velocity detections often provide only minimum mass (`M sin i`) and no physical radius. In that case `diameter_km` remains `0.0`; the catalog does not invent a physical radius. A separate rendering policy can later provide a non-physical marker or display proxy without contaminating the scientific fields.

## Corrected migration facts

- `Alpha Centauri A+B` contains only the G2 V and K1 V components. Proxima Centauri is system 60 and has its own coordinates.
- System 20 (`HD 93083`, alias `GJ 1137`) now contains the two planets listed as confirmed by NASA in 2026: `GJ 1137 c` and `HD 93083 b`.
- System 60 (`Proxima Centauri`) now contains confirmed planets b and d plus candidate c, with separate ICRS-derived Cartesian coordinates.
- System 39 is the canonical nearby `HD 114613`. NASA classifies the old `HD 114613 b` radial-velocity signal as a false positive; the retained 3026 bodies are explicitly marked as procedural gameplay data.
- `Kepler-62` is loaded from `distant_systems_details/100062_kepler_62.json` as a hidden quest destination. It is available to route and quest code but is not emitted by the current local Galaxy Atlas snapshot.

## Unresolved legacy slot

The duplicate physical record formerly loaded as system ID 18 has been moved to `migration_review/018_duplicate_hd_114613.json`. NASA data identify system ID 39 as the canonical nearby HD 114613. The intended astronomical identity of physical slot 18 cannot be recovered from the legacy files, so the active physical catalog currently contains 60 unique systems and intentionally leaves ID 18 unused.

`assets/data/galaxy/systems.json` and `nodes.json` belong to the separate political `GalaxyDatabase` layer. Their `systemId` values are local to that dataset and are not StarAtlas physical IDs.

## Distant-system visibility

`StarAtlasDatabase::systems()` contains only the 60 local systems. The current Galaxy Map and starfield use this local collection, so distant quest destinations do not clutter the ordinary atlas.

`StarAtlasDatabase::distantSystems()`, `findSystemSummary()` and `findSystem()` expose distant systems to quest logic, generation-ship routing and future long-range navigation layers.
