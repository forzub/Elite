# Authored replacement meshes

The Model Asset Editor may import alternate meshes for catastrophic damage and
other authored visual substitutions without putting them into the normal render
node graph.

## Discovery rule

For every loaded render LOD, the registered assembly OBJ files are the **default
meshes**. The editor recursively scans the whole `LOD<N>/` directory tree; any
other `.obj` found anywhere below that LOD root is an **additional mesh**.

```text
LOD0/
  station_core.obj               <- registered default source
  station_habitat_s1.obj         <- registered default source
  station_habitat_s2.obj         <- registered default source
  qwe123.obj                     <- additional mesh
  damage/final_final_7.obj       <- additional mesh
  experiments/set_a/test.obj     <- additional mesh

LOD1/
  lod1_station_core.obj          <- registered default source
  generated_x.obj                <- additional mesh
```

The scan is recursive, but subfolder names do not define authoring semantics. A
`variants/`, `damage/`, `experiments/` or any other folder may be used purely for
organization; no folder or filename changes what a mesh means.

Every OBJ already declared by the runtime assembly for that LOD is excluded even
if its GeometryDefinition was later removed by instance consolidation or unused-
geometry cleanup. This prevents old baked duplicates from reappearing as extras.

## Filenames are source pointers, not identities

An additional OBJ filename does not identify a damage state, replacement target,
or LOD family. On first discovery the editor assigns an opaque persistent
authoring id such as `extra.000001` and stores that relation in the workspace
`wizard_state.json`. The source path is retained only so the OBJ can be re-read.

Likewise, each ordinary/default GeometryDefinition receives a persistent
`base.<N>` visual id in the authoring workspace. Ephemeral `G#` indices are UI
labels only and are never persisted as replacement contracts.

Future generated LOD representations can explicitly reuse the same base/extra
visual ids. No LOD association is inferred from matching filenames.

## Replacement compatibility is explicit

The Geometry stage uses a master/detail assignment UI:

1. choose one **additional mesh** in the upper table;
2. the lower table lists each used **default geometry family** once, even when it
   has many render instances;
3. check the default families the selected extra mesh may replace;
4. use Preview to temporarily substitute the extra mesh on one representative
   instance. Preview is allowed before checking compatibility and never mutates
   the asset. Clicking the active preview dot again, or pressing `Original`,
   restores the authored/default mesh.

Compatibility is many-to-many and is stored as:

```text
extra visual id -> [base visual id, ...]
```

This relation answers only **what may replace what**. The later DAMAGE stage
answers **why/when a particular compatible replacement is selected** (for
example a rare catastrophic breaching weapon plus an authored hit region).

## Coordinate contract

An extra mesh must use the same object-local coordinate system as the default
geometry it is intended to replace. The safest Blender workflow is to duplicate
the intact object, edit topology without moving its origin/object transform, and
export the result. Topology may otherwise be completely different.

## Editor behavior

`Refresh additional LOD meshes` recursively scans OBJ files below already-loaded
LOD roots without rebuilding the intact assembly, so duplicate/instance cleanup
is preserved. Extra meshes remain hidden because no RenderNode is created for them
and they are protected from `Clean unused geometry`.
