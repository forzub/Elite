# Equipment units (SOURCE folders)

One folder per equipment unit, in the editor source-folder layout
(`<unit>/LOD<N>/*.obj`, see `src/assets/models/SOURCE_LAYOUT.md`). The Model
Asset Editor lists every folder here in the "Equipment" catalog group
(catalog id `equipment_<folder>`). This folder is tracked in git; the rest of
`assets/` is not, except `hub/`.

All files are generated from the equipment passports by
`tools/models/build_equipment_models.py` (`--check` verifies them). Do not edit
them by hand; change the generator and rebuild. Preview renders:
`tools/models/render_preview.py` (Blender, headless).

- `cobretty_cr1_vector/` - Cobretty CR-1 Vector field-coupling main drive.
  Unit axis OBJ +Z; reactor flange at z = 0, cooling pad at z = 6.5
  (part `engine_hot_emitter_annulus` = the passport's thermal emitter mesh id).
- `cobretty_er5_reactor/` - Cobretty ER-5 propulsion reactor. Torus axis OBJ +Y;
  flanges to the fore / aft drives with mating faces at z = -2.5 / +2.5
  (they mate the CR-1 flange); cooling pad PT1 (`reactor_cooling_pad`,
  11 x 11 x 0.25 m) at y = 2.3 on the heat-pipe column.

No textures: every detail is geometry, colours are MTL materials (`eq_*`
names are shared asset-level material ids). LOD0 is meant for <= ~30 m,
LOD1 for ~30-150 m, LOD2 beyond; a part missing in a far LOD is a detail
dropped at that distance.
