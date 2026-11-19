# Asteroid mine kit (hubq_*)

Author's request (2026-11-19): models for mines. A big bucket-wheel excavator, carts and rails, open pits on big
asteroids, burrows, and the settlement next to the mine: residential blocks, warehouses, pyramid ore piles, a terminal
for large transports, and a small pier for small ships and guard ships. Addition: one mine that looks like an offshore
drilling platform from outside, lots of lattice trusses, an industrial look, and gorges or canyons to fly through.

- Generator: `tools/models/units/hub_mine.py`. It uses the eqlib kernel, like the other hub kits (`hub_belt`,
  `hub_district`, `hub_plane`). The models are scripted geometry. Every detail is geometry and every colour is an MTL
  material, so there are no textures.
- The existing hub modules and the existing mine models are not touched. The new modules use the `hubq_` prefix.
- Sources are written to `<assets_source>/models/hub/hubq_*/intact/assembly/LOD0..LOD3/*.obj`, with `hit_volumes.json`
  and `model.json` (the passport).
- The mesh editor lists the modules under «Модули хабов» (`hub_modules`).

## Frame and scale

- Units are metres. OBJ +Y points away from the rock, and the module stands on y = 0 (the asteroid surface).
- +Z is the long side of the module or its direction of travel.
- Rails:
  - gauge 2.4 m;
  - rail top 1.0 m above the surface (sintered bed, sleepers, rail);
  - a power third rail on the +X side.
- Rolling stock and the excavator are placed at y = 0 on a rail line, like the rails themselves.
- LODs follow the kit convention:
  - asset LOD0: close-up extras (`close()`), such as sleepers every 1.5 m, bolt heads and bucket teeth;
  - LOD1: the main look;
  - LOD2: lattice girders reduced to chords plus side diagonals, nets and boulders removed;
  - LOD3: plain volumes (a lattice becomes a solid box or a tapered prism).

## Low / micro gravity: what replaces Earth weight (plausible-physics principle)

A big asteroid has a surface gravity of about 0.01-0.3 m/s². Vesta and Ceres are about 0.25 m/s². At that gravity a
13 500 t Bagger-class machine weighs from 1/40 to 1/1000 of its Earth weight. Each mechanism below therefore has a
cause that does not depend on weight:

| Earth mechanism | Asteroid version in the kit |
|---|---|
| Crawlers and the machine's weight take the digging reaction | The excavator rides two rock-anchored tracks. Bogies wrap the rail head with upstop rollers, as on a roller coaster. Four hydraulic spud anchors are drilled into the bench while it cuts. |
| Ore falls from the buckets onto an open belt | The wheel discharges into a shroud. The ore then moves on pipe conveyors, where the belt is rolled into a closed tube. |
| Open hopper wagons | Hopper wagons have clamshell lids, upstop rollers and bottom-dump doors. |
| Free-standing stockpiles | Piles still hold their shape: the angle of repose barely depends on g. They lie under orange tether nets on anchor posts, because impacts or electrostatic charge would loft the fines. A stacker boom lowers the ore slowly through a sealed spout. |
| Gravity hoist with a counterweight | A friction hoist with a tail rope (a closed rope loop) pulls the cage both ways. |
| Shiploader pouring ore into a hold | A travelling loader couples a sealed articulated arm to the ship's bulk port, like an oil loading arm. Mooring clamp arms hold the ship alongside a berth girder 90 m above the rock. |
| Houses | Regolith-covered habitat rows, with 2-3 m of cover as radiation shielding and radiator fins to dump heat. The dormitory has water-wall shield cassettes and a storm-shelter tank. |
| Oil rig flare | A volatiles vent / gas separator flare. Borehole mining lifts a volatile-bearing slurry (water ice, carbonaceous rock). |

## References used

- **Bagger 288** ([Wikipedia](https://en.wikipedia.org/wiki/Bagger_288)):
  - 220 m long, 96 m high, 13 500 t;
  - wheel 21.6 m with 18 buckets of 6.6 m³;
  - 12 crawlers in 3 rows;
  - 240 000 m³ per day, 16.56 MW, fed by an external cable.
  - Our machine is a medium SRs-class one: wheel 18 m with 14 buckets, a 45 m wheel boom, A-frame mast stays, a
    counterweight boom, a discharge boom and a cable reel.
- **Open-pit mining** ([Wikipedia](https://en.wikipedia.org/wiki/Open-pit_mining)): benches 12-15 m high and 20-40 m
  wide, sized to the equipment; ramps and haul roads; stepped walls.
  - Ours: 3 benches 15 m high and 40 m wide. Each bench takes the excavator and a track. The batters are 6 m wide
    (68°, steep because slope failure is minor in low g), with a 60 m plateau at the rim.
- **Bingham Canyon** ([Wikipedia](https://en.wikipedia.org/wiki/Bingham_Canyon_Mine)): 4 km wide and 1.2 km deep. Its
  1000-car railroad was replaced by about 8 km of conveyors. This is why the demo uses a rail line plus conveyors.
- **Headframe** ([Wikipedia](https://en.wikipedia.org/wiki/Headframe)): steel and concrete headframes reach up to
  87 m; at Konkola the sheave centres are 71 m above the collar. Ours is 48 m to the sheave deck.
- **Malmbanan / Iron Ore Line** ([Wikipedia](https://en.wikipedia.org/wiki/Iron_Ore_Line)):
  - 68-car trains of about 100 t hopper wagons, 30 t axle load, IORE locomotives;
  - Kiruna mine to the Narvik port.
  - Our wagon is a 100 t class hopper of 12.6 m.
- **Mine cars** ([Wikipedia](https://en.wikipedia.org/wiki/Minecart)): narrow gauge, battery or electric locomotives.
- **Stacker / stockpiles** ([Wikipedia](https://en.wikipedia.org/wiki/Stacker)): luffing and slewing booms, with
  conical and chevron piles. Our piles are square pyramids at about 34°, the angle of repose of crushed ore.
- **Shiploaders** ([Wikipedia](https://en.wikipedia.org/wiki/Shiploader); Pilbara Ports, Port Hedland:
  [port facilities](https://www.pilbaraports.com.au/ports/port-of-port-hedland/port-operations/port-facilities)):
  - travelling shiploaders on 580-623 m berths;
  - 10 000-13 500 t/h;
  - a 272 m multi-user berth.
  - Ours is a 300 m berth girder for carriers up to about 260 m (the T-5000 / HT-20K class, the same as the
    `hubb_pier_l` berth).
- **Pipe conveyors** (rules of thumb, [Overland Conveyor](https://overlandconveyorcompany.freshdesk.com/support/solutions/articles/69000836440-pipe-conveyor-rules-of-thumb)):
  - pipe diameter at least 3× the largest lump;
  - fill at most 66-70 %;
  - curve radius about 300-800× the diameter;
  - hexagonal idler panels.
- **Company / mining towns** ([Wikipedia](https://en.wikipedia.org/wiki/Company_town); Kiruna): a company-built
  settlement beside an isolated mine, with housing, a store and stores yards. Ours has habitat rows, a dormitory and
  vaulted stores with container yards.
- **Offshore jacket platforms and hydraulic borehole mining**, for the rig:
  - 4 battered lattice legs with X bracing;
  - two decks, the derrick and crown block;
  - pedestal cranes, a cantilevered landing pad (as on a helideck), pipe racks, a flare boom and escape capsules;
  - the riser down to a wellhead stack on the rock.

## Models

| Module | What | Size (m) | Tris LOD0 / 1 / 2 / 3 |
|---|---|---|---|
| `hubq_bucket_wheel` | Bucket-wheel excavator on two tracks (x ±8), boom slewed 21° to +X, discharge spout over a track at x −16 | ~110 × 55 × 50 | 19.7k / 17.6k / 6.2k / 2.7k |
| `hubq_rail` | Straight track 50 m (z 0..50) | 4.6 × 1 × 50 | 1.7k / 300 / 300 / 36 |
| `hubq_rail_curve` | Curve R 100 m, 15° (start heading +Z, centre at +X) | | 2.0k / 1.3k / 684 / 108 |
| `hubq_rail_end` | Buffer stop | 5 × 3 × 10 | 608 / 324 / 292 / 168 |
| `hubq_ore_wagon` | Lidded hopper wagon, ~75 m³ / ~100 t | 3.4 × 5.4 × 12.6 | 1.5k / 1.2k / 1.1k / 300 |
| `hubq_loco` | Electric shunting locomotive, cab at −Z | 3.4 × 5 × 14 | 1.2k / 1.1k / 944 / 364 |
| `hubq_conveyor` | Pipe conveyor 50 m on a truss and A-frame trestles (belt centre at 6 m) | 6 × 8 × 50 | 2.8k / 1.7k / 1.6k / 84 |
| `hubq_bridge` | Through-truss conveyor bridge, 200 m span, 14 m deep (ships fly under it) | 18 × 14 × 210 | 11.7k / 7.3k / 6.4k / 168 |
| `hubq_pit_side` | Pit wall: 60 m plateau, 3 benches (15 × 40), floor edge (pit at −X) | 190 × 60 × 100 | 888 / 888 / 492 / 48 |
| `hubq_pit_corner` | 90° pit corner, rim radius 140 m about the origin (+X..+Z) | 200 × 60 × 200 | ~1.2k / 1.2k / 650 / 130 |
| `hubq_pit_cut` | 200 m pit wall with a 60 m slot down to the floor (canyon mouth) | 190 × 60 × 200 | 1.0k / 1.0k / 624 / 108 |
| `hubq_pit_floor` | Pit floor plate (top y 0) | 230 × 15 × 230 | 652 / 652 / 300 / 12 |
| `hubq_canyon` | Straight canyon 100 m: floor 60 m, 45 m deep, stepped walls (top 136 m) | 200 × 60 × 100 | 340 / 340 / 252 / 92 |
| `hubq_ground` | Regolith tile, flat edges, gentle relief | 100 × 12 × 100 | 342 / 342 / 192 / 12 |
| `hubq_portal` | Burrow ("нора") portal: rock hill with a 12 × 12 m arched mouth (−Z), airlock doors, conveyor out | 76 × 46 × 48 | 5.6k / 4.2k / 3.1k / 542 |
| `hubq_headframe` | Shaft headframe 48 m, sheaves, back legs, winding house, collar house | 30 × 57 × 50 | 9.4k / 9.0k / 2.4k / 670 |
| `hubq_hab_block` | Regolith-covered habitat row: 4 cylinders 9 × 36 m, radiators, mast | 52 × 31 × 44 | 2.2k / 1.9k / 1.3k / 230 |
| `hubq_hab_tower` | Dormitory, 8 floors, shield cassettes, stair towers, roof tank | 38 × 43 × 32 | 2.3k / 2.3k / 2.2k / 900 |
| `hubq_store` | Vaulted store hall (r 14, 60 m) plus a netted container yard | 62 × 15 × 70 | 3.7k / 2.8k / 1.8k / 96 |
| `hubq_ore_pile` | Iron-ore pyramid 60 × 20 m under a net, feed post and boom | 80 × 34 × 66 | 5.2k / 5.1k / 1.4k / 66 |
| `hubq_ore_pile_s` | Concentrate pyramid 36 × 12 m (grey) | 56 × 26 × 42 | 4.3k / 4.3k / 1.2k / 66 |
| `hubq_bulk_terminal` | 4 silos, lattice towers, 300 m berth girder at 90 m, travelling loader, clamp arms | 70 × 105 × 300 | 44.7k / 33.7k / 17.6k / 1.0k |
| `hubq_pier_small` | 150 m pier at 22 m: 2 small-ship berths (−X), guard berth (+X), PD turret, control tower | 50 × 46 × 155 | 9.1k / 9.0k / 3.3k / 730 |
| `hubq_drill_rig` | Drilling-platform mine: 4 battered lattice legs, decks at 60 / 73 m, derrick to 142 m, cranes, landing pad, flare boom | ~180 × 150 × 180 | 30.8k / 30.4k / 12.9k / 1.1k |
| `hubq_mine_site` | The assembled demo (below) | ~1.4 × 1.2 km | 277k / 159k / 19k (set views: modules at mid LOD 0 / 1 / 2) |

Triangle counts as of the build of 2026-11-19 (the build prints them: `… LOD<n>: <tris> triangles`).

## Demo layout `hubq_mine_site` (assembly by reference)

The pit centre is at the origin, with the rim at y = 0.

- **Pit.** About 540 m across: 6 straight walls, 4 corners, and the north wall with the canyon cut. The floor is
  45 m down.
- **East bench 1 (y −15).** The bucket-wheel machine works the face on its two tracks. A train (locomotive and 6
  wagons) waits under its discharge spout. The track ends at a buffer stop.
- **NE corner.** The track runs round the corner (6 × 15° curves, R 100). It goes west along the north bench, then
  turns 30° right (2 curves laid in reverse) and enters the burrow portal. A second short train stands on the north
  straight.
- **Canyon.** It runs north out of the pit through the cut: 300 m long, with a 60 m floor 45 m down. Two truss bridges
  (200 m spans) cross it.
- **Ore route.**
  - The headframe (shaft into the burrow) stands west of the canyon.
  - A pipe conveyor runs east from it, crosses the canyon on a bridge and reaches the stockyard (4 iron piles and 2
    concentrate piles).
  - A reclaim conveyor runs on to the bulk terminal at the east edge. Ships berth on its +X side, 90 m up.
- **Settlement.** Between them: 5 habitat rows, 2 dormitories and 3 stores.
- **South-east.** The small pier (deck out to the east) and the drilling rig.
- **Ground.** 100 m regolith tiles fill everything outside the pit and the canyon.

Flying lines for ships:

- the pit itself;
- the canyon under the two bridges (14 m truss depth, 45 m from the rim to the floor, 60 m floor);
- the 60 m gap under the rig decks (the legs stand about 84 m apart at the rock and 44 m at the deck);
- under the terminal girder, between the towers (220 m clear, 82 m high).

## How to regenerate

```sh
python tools/models/build_equipment_models.py --only hubq_ </dev/null      # writes the sources (all hubq_* + the demo)
python tools/materials/seed_material_library.py </dev/null                  # adds the new eq_hubq_* materials (once)
"/c/Program Files/Blender Foundation/Blender 5.0/blender.exe" -b --factory-startup -P tools/models/render_preview.py \
    -- D:/__elite/assets_source/models/hub/hubq_drill_rig 1 D:/tmp/mine_prev/rig.png pad          # a preview
```

To package, open the mesh editor:

1. Run `build/tools/mesh_editor/bin/EliteMeshEditor.exe`, or start it with `--serve <seconds>`. The catalogue is read
   at start, so restart the editor after adding new modules.
2. Open «Модули хабов» → `hubq_*`.
3. Press the shell / package step («Скорлупы»). From a script, send `{"cmd": "shell", "category": "hub_modules",
   "id": "<module>"}` over the page WebSocket.

The packages go to `assets/compiled/hub_modules/<module>/<module>.elmodel`. The demo `hubq_mine_site` is an assembly
of the modules by reference, so package the modules first.

## Status (2026-11-19)

- All 24 modules and `hubq_mine_site` are generated. `build_equipment_models.py --check --only hubq_` passes.
- All 25 are packaged by the mesh editor into `assets/compiled/hub_modules/hubq_*`.
- Previews are in `D:/tmp/mine_prev/` (temporary):
  - per module: `<module>_L1_<view>.png`;
  - the demo site: `site_overview.png`, `site_bench.png`, `site_face.png`, `site_canyon.png`, `site_portal.png`,
    `site_rig.png` and `site_terminal.png`.
- Not yet in the game world. The site is not placed in `initial_world_state` and has no hub map, haze or lanes.
  `tools/models/hub_belt_world.py` is the path once the author picks where the mine goes.
