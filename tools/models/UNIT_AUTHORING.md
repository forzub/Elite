# Authoring an equipment unit (tools/models/units/<maker>.py)

Procedural, geometry-only models built from the equipment passports. No textures: every detail is geometry, colours
are MTL materials. Reference implementation: `units/cobretty.py` (CR-1 drive, ER-5 reactor), `units/sol_reactor.py`,
`units/tianhe.py`. Kernel API: `eqlib.py` (read its docstrings). Design language per maker: `DESIGN_SCHOOLS.md`.
Passport data per maker: `briefs/<maker>.json` (envelope, mass, ratings, notes, fault class + serviceable units).

## Module contract
- `from eqlib import *`; register the maker palette once: `register_materials({"eq_<maker>_...": (Kd, Ks, Ns), ...})`.
  Shared ids (`eq_metal_graphite`, `eq_metal_band`, `eq_metal_panel`, `eq_steel`, `eq_pad_ceramic`, `eq_field_coil`,
  `eq_copper`, `eq_insulator`, `eq_status_amber`) may be used by anyone; never redefine them.
- Export `ASSETS = {folder: parts_fn}`, `HITS = {folder: hits_fn}`, `MODEL_ASSETS = {part_id: folder}`.
  Folder = `<maker-short>_<model>_<kind>` in lowercase ASCII, e.g. `ofu_u33_drive`, `xinzhi_n10_radar`.
- `parts_fn(lod)` returns `{part_name: Part}`. `lod` is the mid level 0 / 1 / 2 (= asset LOD1 / 2 / 3); the close
  asset LOD0 calls `parts_fn(0)` with `close()` True - add close-up extras under `if close():` (harnesses, pipes,
  clamps, small fasteners, labels-as-relief, hinges). Use `seg_for(lod)` / `small_for(lod)` for round segment counts.
- Part names = the serviceable units (SRUs) of the unit's fault class where they are physically separate, plus
  structure (housing, mounts, flange). Prefix with the unit prefix (`u33_field_ctl`). Every main drive names its
  cooling pad `engine_hot_emitter_annulus` (the runtime thermal-emitter mesh id); every propulsion / service reactor
  pad is `reactor_cooling_pad`. One OBJ per part; empty parts at far LODs are dropped automatically.
- `hits_fn()` returns a list of `hbox(part, center, size, rot_deg)` / `hcyl(part, p0, p1, r)` volumes that follow
  the shape closely (boxes and cylinders only; cylinder axis along Z or in the XY plane). Every part with gameplay
  meaning gets at least one volume.

## Frame and fit
- Metres, OBJ +Y up. Units mounted along the ship axis (drives, reactors, jump cores, tanks, train modules,
  passenger modules...) have their long axis on OBJ Z; a drive's pad is at its +Z end, its reactor flange at z = 0.
  Units that sit on the hull skin (radars, antennas, radiators, weapons, sensor masts, docking ports, landing gear,
  heat shields) have their outward / firing direction along +Y and their mounting face at y = 0.
- The model must fit the passport `geometry.envelope_m` [W, H, L] = [X, Y, Z] within ~3 %; the pad of a drive is
  `nozzle_or_emitter_diameter_m`, a reactor pad is `cooling_pad_side_m` (pads may exceed the envelope - they are hull
  skin). Do not change passports; if a shape needs another envelope, say so in your report.
- "Similar modules must not be identical": two makers' units of the same category must differ in silhouette, layout
  of serviceable units, mounting pattern and flange / interface standard (each maker its own), so a foreign unit is
  visibly an adapter job.

## LOD budgets (triangles per unit) and visibility
- LOD0 (close, a few metres) <= ~40k; LOD1 (20-30 m) ~8-14k - fasteners < 6 cm and chamfers < 2.5 cm are culled by
  the kernel; LOD2 (30-150 m) ~2-4k, no fasteners / panels; LOD3 (> 150 m) < ~1.5k, plain volumes.
- Big round shapes: `seg_for(lod)`; small round details: `small_for(lod)` or fewer. Do not build hidden faces.

## Check and render
- `python tools/models/build_equipment_models.py --only <folder>` writes the files and prints triangles per LOD.
- `"/c/Program Files/Blender Foundation/Blender 5.0/blender.exe" -b --factory-startup -P tools/models/render_preview.py
  -- assets/models/equipment/<folder> <LOD> <ABSOLUTE out.png> [pad|flange|side|top|under] [hidden,parts] [mesh|hits]`
  (absolute output path; a 1.8 m figure for scale). Look at LOD1 and LOD0 and at the hit volumes before finishing.
- `python tools/models/build_equipment_models.py --check` must pass at the end.
