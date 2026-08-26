# Model Asset Pipeline

## Purpose

`EliteAssetEditor` is the offline authority for render/collision/socket model data.
The game runtime must eventually consume the compiled `.elmodel` file and stop
reconstructing topology, normals, hit volumes and attachment metadata from OBJ
files during startup or rendering.

This migration is intentionally staged: the editor and shared binary contract
are introduced first. `EliteGame` keeps its current OBJ/assembly path until the
existing model catalog has been reviewed and compiled.

## One asset, definitions + instances

A complete ship/station is an assembly, not one mesh.

- `GeometryDefinition` stores reusable mesh/topology data.
- `Node` stores an assembly transform and optionally references a geometry.
- many nodes may reference the same `geometryIndex`.

The last rule is the instancing contract. A station with three identical
habitat sectors should contain one habitat geometry definition and three nodes
with different transforms, not three copies of the vertex buffer.

Module nodes may have no geometry. They exist to preserve hierarchy, pivots,
rotating sections and stable parents for hit volumes/sockets.

## Binary format

Extension: `.elmodel`

The format is versioned and chunked. Version 1 writes:

- `META` — identity, source object type, bounds, LOD switch distance;
- `GEOM` — geometry definitions, LOD vertices/triangles/topology edges;
- `NODE` — assembly hierarchy and geometry instances;
- `COLL` — authored collision/hit volumes;
- `SOCK` — semantic attachment points.

Unknown chunks are skipped by the reader so later editor metadata can be added
without forcing the runtime to understand it immediately.

The single C++ data contract is `src/model_asset/ModelAsset.h`; binary I/O is
`ModelAssetBinary`. Editor-only classes may manipulate it, but must not create a
second runtime representation of the compiled format.

## Geometry/topology contract

Each LOD stores indexed vertices and triangles. Topological edges retain the
adjacent triangle ids and flags including boundary/polygon-boundary/internal
triangulation state. `renderMask` is authored independently for Technical and
Elite render styles.

The first editor slice imports the current runtime `MeshData`, so it inherits
its currently reconstructed normals/edge defaults. That is transitional only.
The next importer stage must read source normals/smoothing groups/polygon
boundaries directly and make the editor, not `ObjLoader`, authoritative.

## Thin surfaces

`SurfaceMode` is per geometry definition:

- `Closed` — normal closed-volume mesh;
- `ThinOneSided` — zero-thickness sheet rendered from its authored side;
- `ThinTwoSided` — zero-thickness sheet visible from both sides.

Visual thickness and collision thickness are independent. Hull panels therefore
do not need duplicated inner/outer visual shells merely to exist physically.

## Collision

Version 1 supports a general shape enum and edits box volumes in the UI. The
compiled representation already reserves sphere/capsule/convex-hull shapes.
Collision volumes have their own parent node and transform; they are not derived
from the render mesh at game runtime.

## Sockets

A `Socket` is the common attachment concept for camera, weapon, equipment,
dock, RCS, VFX and future semantic points. The `kind` is a stable string rather
than a closed enum so adding gameplay socket types does not require a binary
format revision.

Existing `ShipAttachmentPoint` data is imported into sockets. Other current
semantic catalogs (station/docking anchors, etc.) remain untouched until their
explicit migration pass, so introducing the editor cannot silently delete them.

## Editor v1 workflow

1. Build/run `EliteAssetEditor`.
2. Select a current assembly type.
3. If a compiled file exists it is loaded; otherwise the current runtime
   assembly is imported into the shared asset model.
4. Click a module/submodel in the large viewport or assembly tree.
5. Hide/isolate nodes for inspection.
6. Change a node's geometry definition to author an instance and edit its
   transform.
7. Edit `Closed` / thin-surface semantics.
8. Enter edge-edit mode and toggle candidate edges for Technical or Elite.
9. Inspect/edit/add/delete hit boxes.
10. Inspect/edit/add/delete sockets/attachment points.
11. Save `src/assets/compiled/models/<asset-id>.elmodel`.

`Reimport source` deliberately discards the in-memory/compiled edits for that
asset and rebuilds from the current legacy assembly.

## Next editor slices before runtime migration

- direct source-OBJ importer preserving authored normals, smoothing groups,
  polygon boundaries, UVs/material slots and topology without legacy welding;
- normal/smoothing repair tools and explicit hard-normal editing;
- instance-detection/fit helper for repeated station components;
- LOD/proxy editing and generated LOD validation;
- sphere/capsule/convex collision tools;
- migrate station semantic/docking anchors into sockets;
- binary validation report (degenerate triangles, invalid hierarchy, duplicate
  ids, dangling parents, non-finite data);
- batch compiler so every model is converted before `EliteGame` switches from
  OBJ to `.elmodel`.

Only after that batch conversion is accepted should the game-side model loader
be replaced.
