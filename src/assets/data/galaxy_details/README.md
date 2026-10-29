# Galaxy details catalog (physical universe)

This directory is the authoritative physical catalog of star systems. It is
read identically by the client and the server (`StarAtlasDatabase`, catalog
schema 2, content fingerprint) and feeds the Galaxy Map, System / Detail /
Hub maps, the celestial runtime and the gravity field. There is one position
and one set of bodies per system; maps and flight never see different worlds.

## Layout: one system = one directory

```text
systems/<NNN>_<slug>/            60 local systems (Galaxy Atlas)
distant_systems/<id>_<slug>/     distant quest / route destinations (id >= 100000, hidden from the atlas)
    system.json                  INPUT  identity, ICRS position, catalog flags, stellar hierarchy,
                                        survey completeness, reserved designations
    stars/<key>.json             INPUT  one stellar component each, {"value", "source"} per number
    observed/<kind>_<slug>.json  INPUT  observational anchors (NASA Exoplanet Archive) / authored Sol bodies
    generated/                   OUTPUT of src/world/generation, replaced as a whole
        system.json              galactic context, present-day stars, barycenters, disks, stability report
        bodies/<kind>_<slug>.json planets, dwarf planets, moons (physical, thermal, atmosphere, surface descriptor)
        reservoirs/<slug>.json   asteroid / outer belts, debris disks, comet clouds, trojans (statistical)
objects_details/*.json           authored free interstellar objects (schema kind "galaxy_object")
objects_details/interstellar_cells/block_<tag>.json
                                 OUTPUT: 25-ly generation cells of the navigation domain in
                                 4x4x4 blocks (background medium lattice + compact bodies)
objects_details/interstellar_medium/<id>.json
                                 OUTPUT: medium structures (IrregularDensityVolume lattices)
```

Inputs are owned by the catalog author; `generated/` is owned by the generator
and never edited by hand. Regenerate with the DEV tool:

```bash
build/headless_server/EliteUniverseGenerator.exe materialize src/assets/data/galaxy_details
build/headless_server/EliteUniverseGenerator.exe interstellar src/assets/data/galaxy_details
build/headless_server/EliteUniverseGenerator.exe determinism src/assets/data/galaxy_details
```

The same UniverseSeed + generator versions + inputs always give the same files
(`determinism` regenerates in forward and reverse order and compares bytes).

## Identity contract

- System ids are the physical StarAtlas ids (0-60 without 18, distant 100062).
  The political `assets/data/galaxy/systems.json` ids are a separate namespace.
- Runtime body ids: star `system_<id>.<Name>`, planet `<parent id>.<Designation>`,
  moon `<planet id>.<Designation>` (spaces and `()/\` become `_`), barycenter
  `system_<id>.barycenter[_<node>]`, reservoir `<parent id>.<kind>`.
- Observed planets keep their catalogue designation; procedural planets take
  the next free letters (catalogued designations, also refuted ones, stay
  reserved). Sol keeps its legacy names, so `system_0.Sol.Земля` etc. are stable.

## Positions

`position_ly` is ICRS (J2000) Cartesian light-years from Sol:
x = d cos(dec) cos(ra), y = d cos(dec) sin(ra), z = d sin(dec). It is the only
source of distance; a stored `distance_ly` is rejected. `position_source`
names the catalogue identifier; `legacy_position_ly` keeps the pre-2026-10-29
value for the migration record.

## History

The legacy flat documents (`systems_details/`, `distant_systems_details/`,
`migration_review/`) were replaced on 2026-10-29; they remain in git history.
Their planets were not used as inputs (most were fictional); see
`tools/universe_generator/` and PROJECT_STATE.md.
