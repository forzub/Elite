# Hub construction kit (hubc_*)

Author's request (2026-11-19): hubs under construction are rebuilt from construction-stage modules: foundation, then
bare frame, then partly clad, then nearly done. The site also needs cranes, material stacks, temporary
loading/unloading platforms, and worker housing that looks completely like barracks («абсолютно барачного внешнего
вида»).

Author correction (2026-11-19): tower cranes and the gantry are used **only on planet and moon surfaces with gravity**.
Orbital and free-space hubs get no cranes. They use the tug, the clawship with its held elements, worker pods and jigs
instead.

- Generator: `tools/models/units/hub_construction.py`. It uses the eqlib kernel, like `hub_district` and `hub_mine`.
  Every detail is geometry and every colour is an MTL material, so there are no textures.
- Sources: `<assets_source>/models/hub/hubc_*/intact/assembly/LOD0..LOD3/*.obj`, plus `hit_volumes.json` and
  `model.json`.
- Packages: `assets/compiled/hub_modules/hubc_*/hubc_*.elmodel`.
- The mesh editor lists the modules under «Модули хабов».

## Frame (contract)

- Units are metres and +Y is up.
- **The origin of every module is the centre of its footprint on the deck plane y = 0.** The module stands on y = 0.
  Rafts and pile caps go down to about y = −3.
- The flyers (`hubc_hauler`, `hubc_tug`, `hubc_clawship`, `hubc_pod`) and the held elements are the exception. Their
  origin is the centre of the body, +Z is forward and +Y is up.

## Construction logic (plausible-physics principle)

The stages follow the usual order of work on a real site:

1. Raft and pile caps, with starter bars in the fresh concrete.
2. The concrete core goes up in self-climbing formwork (yellow box, with a placing boom on top). It stays 1–2 floors
   ahead of the steel frame.
3. Decks follow the frame.
4. Cladding follows the decks, from the bottom up.

On a working site:

- The top decked floors carry climbing screens (yellow).
- Below them hang catch fans (orange nets). Edge rails run along the bare deck edges.
- Temporary X bracing stands in the corner bays of the bare floors.
- A rack-and-pinion hoist stands on the +X face (from stage 3 on). A tube scaffold with blue sheeting covers one face
  at stage 4 (hab: +Z, slab: −Z).

Lights:

- There are red aviation beacons on corners, cores, crane tips and masts, with obstruction beacons every 45 m.
- Work floods light the top floors.
- Clad faces use the facade material (`eq_hub_facade*`), so the shader draws their windows. A bare frame therefore has
  no lit windows.

Materials:

- New materials use the prefix `eq_hubc_*`: concrete, rebar, yellow, net, tarp blue / grey, primer, barrack, barrack_b,
  plywood, container grey / green, foam, nav green.
- `eq_hubc_nav_green` is emissive: it is added to `EMISSIVE` in `tools/materials/seed_material_library.py`.

## Stage kits

The stage heights are the frame tops. A floor is 4 m, the same as the facade window grid.

| Kit | Footprint X × Z (m) | Building envelope | s1 | s2 | s3 | s4 | Final building |
|---|---|---|---|---|---|---|---|
| `hubc_tower_s1..s4` | 100 × 100 | 80 × 80, core 28 × 28 | raft, pit with rebar mat, column stubs, core walls 6 m in formwork | frame 112 m (28 floors), decks to 60 m, core 120 m | frame 208 m, clad to 104 m, decks to 168 m, belt trusses every 100 m, hoist | 288 m, cladding gaps near the top and in a strip on +X (the hoist) | ~300 m |
| `hubc_hab_s1..s4` | 40 × 30 | 32 × 24, core 8 × 7 | as above | 32 m | 60 m, clad to 32 m | 84 m, scaffold + tarp on +Z | ~90 m |
| `hubc_slab_s1..s4` | 220 × 70 | 204 × 56, 2 cores at x ±60 | as above (pit in the middle) | 40 m | 72 m, clad to 36 m | 100 m, scaffold on −Z (x 20..102) | ~105 m |
| `hubc_hall_s1..s4` | 240 × 240 | 224 × 224: 4 spans of 55 m (x), 11 bays of 20 m (z) | pile caps + stubs, half the floor slab poured, a truss lying ready | frames 0–6 with trusses (eaves 72–80, ridge 88), frames 7–8 columns only, bracing in bay 0 | all frames, roof sheeting on bays 0–4, −Z wall and side walls on bays 0–5, crane runways | roof on bays 0–9, walls clad except the door openings on +Z and the top of bay 10 | ~90 m |
| `hubc_dock_s1..s4` | 130 × 300 (pier along +Z) | deck 60 wide, top at y = 42, abutment at z −150..−140 | abutment to 30 m, pile caps, first bent (z −115) and girder stubs | all 6 bents, girders to z +45, launching nose to z +72 | girders to the tip, deck plating to z 0, portal gantry at z −40 (girder at 58..62) | plating complete except one tip panel, rails, bollards, fenders, lamps, gantry trolley, no berth markings | ~60 m |
| `hubc_yard_s1..s4` | 240 × 190 | plating top at y = 2.0 on pedestals (10 m grid) | pedestals (x < 40), main beams (x < −40), survey stakes | full beam grid, two plated patches | plated except x > 55 and a hole, Jersey barriers on 2 sides, 1 light mast up, 2 lying | plated except one strip, barriers, 6 light masts of 25 m, containers arriving | ~25 m |
| `hubc_mast_s1..s3` | 30 × 30 | 3 m lattice, 3 guy anchors at r 13 | base + 20 m lattice, gin pole with a section on the hook, temporary guys | 60 m, guys at 40, temporary at 58, gin pole | 120 m, guys at 40 / 80 / 115, top platform, no antennas or dishes | — | 120 m |
| `hubc_minehead_s1..s3` | 60 × 60 | shaft at (0, z −12), winder house at z 16..29 | collar, leg + back-leg footings, winder slab, sinking headframe 20 m with kibble | headframe legs to 30 m, back legs half-length on props | headframe to 52 m (sheave deck, sheaves, crane beam to 60), back legs, winder-house frame unclad, drum | — | ~60 m |

The stacks, site cabins, floodlight masts and the hazard-dash site boundary are part of every stage. The stacks stand on
the raft or slab, or on the top deck as a laydown.

## Props

| Module | What | Key dimensions |
|---|---|---|
| `hubc_crane_mast` | Tower-crane mast, lattice 6 × 6 m, cross base with ballast, rest platforms, obstruction beacons | **slewing pivot (top of the ring) at y = 330.0** |
| `hubc_crane_mast_s` | The same mast, 120 m tall | **pivot at y = 120.0** |
| `hubc_crane_jib` | The rotating upper part only; origin at the pivot (y = 0 = ring top), rotate it about Y | jib 3.5..80 along +Z (triangular lattice); counter-jib to −25 with 5 counterweight slabs; A-frame apex at y 28; cab at +X; trolley at z 50; hook block at y −32; beacons at both tips and the apex |
| `hubc_gantry` | Goliath gantry crane | span 260 along X, legs on rail bogies at x ±130 (on y = 0), box girders at y 112..120 (280 long), trolley at x +30, hook at y ~42 |
| `hubc_stack_beams` | 2 piles of I-beams on timber dunnage, with straps | 28 × 6.6 × 11.5 |
| `hubc_stack_panels` | 3 A-frame racks of facade panels + pallets | 24 × 7.7 × 7 |
| `hubc_containers` | 3 × 5 container stacks (40-ft and 20-ft pairs), 1–3 high, dull colours | 39.6 × 7.8 × 13.8 |
| `hubc_platform` | Temporary loading platform: grating deck on truss legs, railings, bollards (+X), 4 lamp masts, landing box with H, containers; ramp along the −Z side from (x −30, y 0) to the deck at x +26 | **deck top at y = 15.0**, deck 60 × 40 (x ±30, z ±20); with the ramp z −27.5..+20 |
| `hubc_barracks` | One barrack row: 2 levels × 12 container huts (6.1 m pitch), corrugated walls, doors and small windows (some lit), wall and roof AC boxes, pipes on the back, gallery on +Z with rails, stairs at both ends, laundry lines (LOD0) | 73.2 × 7 × 9 (huts z −4..1.2, gallery to 2.8, stairs to 4) |
| `hubc_barracks_camp` | Worker camp: 4 barrack rows at x −18, z −37 / −22 / −7 / 8; mess hut; washroom block; water tank on stilts; generator container + fuel tank; fence with a gate on −Z; lamp posts | 118 × 16 × 88 |
| `hubc_hauler` | Flying construction transport: truss cargo frame with 2 containers + beam bundle, cab at +Z, 4 thruster pods | 19.6 × 7 × 44.7 |

## Free-space construction flyers and elements

| Module | What | Size (m) |
|---|---|---|
| `hubc_tug` | Boxy pusher, foam bumper frame at +Z, 8 RCS quads, 2 main thrusters, tether reel and cable stub on the roof, floods, hazard stripes | 11.6 × 9.6 × 14.4 |
| `hubc_clawship` | Hull z −14..+12 (cab on top, 3 engines aft, RCS), two articulated claws at x ±8 (shoulder z 12, elbow (±11, 9, 21), wrist above the grip point) | 24 × 14 × 50.6 |
| `hubc_element_beam` | Truss segment 30 m along X, 3 × 3 section, primer, end plates, lifting lugs | 30.4 × 3.9 × 3.6 |
| `hubc_element_panel` | Facade / hull panel 20 (X) × 12 (Y) × 1 (Z), steel edge frame, lugs | 20 × 12.6 × 1 |
| `hubc_pod` | Tethered worker pod: sphere r 1.4, lit window at +Z, manipulator arm, 4 RCS blocks, tether stub at −Z | 3 × 2.8 × 7.6 with arm and tether |
| `hubc_jig` | Static strongback jig on y = 0: 4 corner lattice towers 80 m at (±28, ±28), perimeter trusses at 20 / 50 / 78 m. A beam element is clamped on the +Z truss at y 54 and a panel element hangs at (0, 40, −24). Work lights and beacons | 62.8 × 81 × 62.8 |

**Clawship grip point: (0, 0, +32) in the clawship frame.** Place a `hubc_element_beam` or `hubc_element_panel` with
its origin there and no rotation:

- the beam runs along X through both claws;
- the panel's thin axis is Z.

The jaw pads stand fore and aft of the element at z ≈ 29.7–30.2 and 33.8–34.3, at x ±8, for y −2.2..+2.2. The fingers
come down from the wrists at y 8.5, above the 12 m panel's top edge at y 6. To release, draw the element from the same
point.

## Triangles (LOD0 / LOD1 / LOD2 / LOD3)

These are the counts from the build of 2026-11-19.

- **tower s1–s4:** 17.1k/6.4k/3.9k/600 · 12.3k/11.8k/7.5k/1.0k · 12.5k/12.0k/8.9k/1.2k · 8.8k/8.4k/6.7k/1.3k
- **hab s1–s4:** 2.0k/1.6k/1.2k/168 · 2.2k/2.2k/1.6k/456 · 2.8k/2.8k/2.2k/574 · 5.1k/4.0k/3.1k/730
- **slab s1–s4:** 19.7k/7.9k/5.1k/804 · 9.3k/8.7k/5.1k/972 · 8.4k/7.9k/5.3k/1.2k · 10.6k/8.5k/6.5k/1.6k
- **hall s1–s4:** 23.8k/11.0k/6.5k/804 · 25.5k/16.2k/9.2k/1.1k · 20.8k/20.3k/12.2k/2.0k · 30.2k/20.3k/12.0k/2.0k
- **dock s1–s4:** 17.0k/6.1k/3.1k/696 · 7.6k/7.2k/3.7k/792 · 6.6k/6.3k/3.7k/720 · 12.1k/10.6k/5.8k/840
- **yard s1–s4:** 19.9k/11.2k/4.7k/300 · 9.6k/9.0k/3.6k/180 · 11.6k/9.7k/4.5k/346 · 21.1k/10.7k/4.7k/696
- **mast s1–s3:** 3.5k/3.4k/1.7k/80 · 5.6k/5.4k/2.4k/80 · 7.6k/7.5k/2.8k/82
- **minehead s1–s3:** 6.6k/4.6k/2.4k/484 · 6.2k/6.0k/3.1k/268 · 11.1k/11.0k/6.0k/796
- **cranes:**
  - crane_mast 8.8k/8.8k/3.4k/214
  - crane_mast_s 3.5k/3.5k/1.5k/214
  - crane_jib 1.5k/1.5k/888/372
  - gantry 13.5k/13.5k/6.8k/384
- **stacks:** stack_beams 2.7k/2.6k/1.2k/24 · stack_panels 600/600/456/168 · containers 9.1k/864/432/180
- **site:** platform 8.3k/7.3k/3.5k/564 · barracks 6.9k/2.8k/2.0k/84 · barracks_camp 14.0k/13.7k/9.4k/516
- **flyers:** hauler 2.2k/1.6k/1.1k/528 · tug 1.6k/1.6k/1.5k/456 · clawship 1.5k/1.5k/1.2k/588 · pod 560/560/320/200
- **elements:** element_beam 1.1k/1.1k/624/36 · element_panel 96/96/96/60 · jig 17.6k/17.6k/16.9k/472

## How to regenerate

```sh
python tools/models/build_equipment_models.py --only hubc_ </dev/null          # sources
python tools/materials/seed_material_library.py </dev/null                      # new eq_hubc_* materials (once)
python tools/models/build_equipment_models.py --check --only hubc_ </dev/null   # must say "up to date"
"/c/Program Files/Blender Foundation/Blender 5.0/blender.exe" -b --factory-startup -P tools/models/render_preview.py \
    -- D:/__elite/assets_source/models/hub/hubc_tower_s3 1 D:/tmp/constr_previews/tower_s3.png pad   # a preview
```

To package, use the mesh editor:

1. Start `build/tools/mesh_editor/bin/EliteMeshEditor.exe --serve <seconds>`. Restart it after adding new modules,
   because it reads the catalogue at start.
2. For each module, send `{"cmd": "shell", "category": "hub_modules", "id": "<module>", "variant": "intact"}` over the
   page WebSocket (`ws://127.0.0.1:<port>`; the port is printed on stderr) and wait for `shellDone`. Each module takes
   about 15 s.

## Status (2026-11-19)

- 47 modules are generated.
- `--check --only hubc_` passes.
- The modules are packaged into `assets/compiled/hub_modules/hubc_*`.
- Previews are in `D:/tmp/constr_previews/` (temporary).
- The modules are not yet placed in any hub. City assembly and traffic belong to another task.
