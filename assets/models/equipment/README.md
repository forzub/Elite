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
- `cobretty_er5_reactor/` - Cobretty ER-5 propulsion reactor, 5 x 5 x 5 m: the
  drive's diameter, so drive - reactor - drive form one cylindrical engine bay.
  Torus coaxial with the bay (axis OBJ Z); flanges to the fore / aft drives on its
  end faces, mating faces at z = -2.5 / +2.5 (they mate the CR-1 flange: place the
  aft CR-1 at z = +2.5 as is, the fore one turned 180 deg about Y at z = -2.5);
  cooling pad PT1 (`reactor_cooling_pad`, 11 x 11 x 0.25 m) at y = 3.0 on the
  heat-pipe column.

`<unit>/hit_volumes.json` - hit volumes generated from the same dimensions as
the mesh (boxes and cylinders, one or more per part, the engine CollisionVolume
fields; "cylinder" = CollisionShape::Cylinder). In the Model Asset Editor: PHYSICS
stage -> "SOURCE HITS" imports them onto the parts. Preview: render_preview.py
`... <view> <hidden> hits`.

No textures: every detail is geometry, colours are MTL materials (`eq_*`
names are shared asset-level material ids). LOD0 is meant for <= ~30 m,
LOD1 for ~30-150 m, LOD2 beyond; a part missing in a far LOD is a detail
dropped at that distance.
