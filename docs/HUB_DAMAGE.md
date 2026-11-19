# Damaged hub modules

Generators: `tools/models/units/hub_damage.py` (residential / office buildings), `tools/models/units/hub_damage_heavy.py`
(industrial halls, the ship salon, the lattice mast, the city frame), names in `tools/models/units/hub_damage_names.py`.
Every variant is its own module folder `<module>_dmg_<kind>` under `<assets_source>/models/hub/` with the intact
module's hit volumes; build with `python tools/models/build_equipment_models.py --only _dmg_`, then package the modules
in the mesh editor (`assets/compiled/hub_modules/<id>`).

A damaged module has no power: dead facades (no shader windows), no lit windows, beacons or floodlights.
LOD discipline: furniture / machine tools / spare parts only on LOD0 (part group `<module>_furniture`), partitions,
rebar and hanging debris LOD0/1, LOD2+ the shell, the slabs, the back walls and the big volumes.
Inner walls stand `WALL_IN` (0.15 m) inside their own room, hall cladding `SHELL_T` (0.5 m) inside the shell: no two
interior planes share a plane (no z-fighting).

## Residential / office (`hubb_hab_tower`, `hubb_hab_slab`, `hubk_hab_a/b/c`)

| kind | what |
|---|---|
| `missile` | a blast crater several floors high, hanging slabs, burnt ring |
| `laser` | two burn cuts across the facade + a railgun burst with exits |
| `gutted` | the facade torn off over large areas, sections open floor by floor |
| `torn` | (2026-11-19) about half the building ripped away along a jagged diagonal tear (from ~1/3 of the height on one side to near the top on the other); a staircase of open rooms floor by floor, slab stubs with rebar over the void, broken slabs and chunks hanging, broken columns, rubble on the torn floors; the stair / lift core of the big tower stands a few floors higher |
| `breach` | (2026-11-19) a big jagged hole straight through the building's thinner dimension - one sees through it; the rooms open on all sides of the hole (floors, ceilings, walls, partitions, furniture), slab stubs and hanging slabs in it, a burnt rim; slim towers keep their corner piers |

The severe kinds turn the facade boxes into voxel grids (6 m bay x 4 m floor x 6 m bay, rooms of 2 x 2 voxels) for the
rooms, but the slabs are a fine raster (1.5 / 2 / 3 m per LOD) and the facade a 2 m x floor grid, both ending along a
per-floor value-noise jag (`gone_fine`), so the tear is off the voxel grid: slabs stop at different depths, broken tips
hang angled down, rebar tufts at the edges (LOD0). Soot is a gradient band round the opening (`eq_hub_soot`; pure
`eq_hub_burnt` only on a breach's rim), no isolated burnt squares. `torn` cuts from ~1/4 to ~3/4 of the height (about
half the volume); a `breach` is an ellipse at least 1/3 of the facade wide and 1/4 of its height.
Triangles: the big tower `torn` LOD0 ~31k, LOD3 ~3.2k; `breach` ~19k / ~2.8k.

## Heavy variants (`HEAVY_KINDS`, also in `DAMAGED_OF`)

| module | kinds | inside |
|---|---|---|
| `hubb_works` (300 m works) | `breach`, `torn` | furnaces under their stacks (a wrecked one burst, its stack lying in the hall), frame presses, lathes, CNC centres, mills, coil stacks; overhead cranes (one fallen), roof trusses snapped and hanging, torn pipes, rubble, roof sections slid into the collapsed end (`torn`), radiator wings hanging off |
| `hubk_factory` | `breach` | melting furnace, presses, lathes, CNC, a gantry crane (bridge fallen), roof tanks toppled, a vent stack fallen |
| `hubb_ship_salon` (new intact module, not yet in any zone) | `breach` | glass front smashed; three display ships on turntables: one hull torn to the ribs (a pod beside it), one knocked off its table with a wing broken off, one burnt; nozzles on cradles, hull panels, landing gear, crates, a spare engine, the mezzanine broken; the pylon ship fallen on the forecourt |
| `hubv_mast` (450 m lattice mast) | `bent` | bent over at two kinks, chords crushed, braces snapped and dangling |

The server's damage accounting (`GameSimulationCityDamage.cpp`) uses only `_dmg_missile/_laser/_gutted`: modules
without them (the heavy ones) still go intact -> destroyed there; the ruined city (`hub_plane._ruin`) picks from every
variant in `DAMAGED_OF`.

## City frame (`FRAME_KINDS` -> `DAMAGED_FRAME`, not in `DAMAGED_OF`)

| module | kinds |
|---|---|
| `hubb_girder` (30 x 30 m, 100 m) | `bent` (sagging buckle, ends in place, chords bulging, section twisted, braces gone / dangling), `snapped` (both halves hang from their ends, 1-2 chords still bridge, ragged curled ends) |
| `hubk_truss` (8 x 8 m, 100 m) | `bent`, `snapped` |

The ruined city swaps a frame girder within `RUIN_BLOWN` of an epicentre for one of these (60 %, seeded per node).
