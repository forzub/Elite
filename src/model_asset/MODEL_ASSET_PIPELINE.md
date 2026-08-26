# Model Asset Pipeline

## Purpose

`EliteAssetEditor` is the offline authority for model preparation. The game
runtime must eventually consume compiled `.elmodel` assets and must not rebuild
normals, topology, collision, mass properties, joints or semantic anchors from
OBJ/render geometry.

The migration remains editor-first. `EliteGame` keeps its current renderer/model
loading path until the production catalog has been converted and validated.

## One asset = geometry definitions + assembly instances

A ship/station is an assembly, not one monolithic mesh.

- `GeometryDefinition` stores reusable indexed mesh/topology data.
- `Node` stores hierarchy, transform, pivot, joint and rigid-body metadata.
- many nodes may reference one `geometryIndex`.

Repeated station sectors therefore use one geometry definition plus several
node transforms. The editor supports re-pointing a node to another geometry,
`Duplicate instance`, `Break instance`, radial instance arrays, node deletion
(with dependency guards) and removal of unused geometry definitions.

## Canonical coordinates / source basis

Compiled runtime data is canonical:

- +X = right
- +Y = up
- -Z = forward/nose

`SourceBasis` records the source convention. Existing runtime assemblies import
as `game_current`. The editor also provides an explicit Blender-model conversion
(+X right, +Z up, -Y forward) that converts geometry, normals, winding, node
transforms/pivots, joints, collision, sockets and inertia to canonical game
coordinates. Basis conversion is a deliberate one-time authoring operation, not
runtime correction.

## Binary format v2

Extension: `.elmodel`

The shared and only compiled C++ data contract is
`src/model_asset/ModelAsset.h`. Version 2 contains:

- `META` — identity, source basis, bounds, LOD switch distance;
- `MATL` — stable semantic material ids and style-agnostic material properties;
- `GEOM` — indexed LOD geometry, source polygon ids, material ids and topology;
- `NODE` — hierarchy, instances, pivots, joints/break metadata and rigid mass properties;
- `COLL` — compound collision primitives;
- `SOCK` — typed semantic anchors, including optional light payloads.

Old v1 editor output is intentionally rejected; reimport the source asset.

## Native OBJ compilation

Stage 2 no longer routes editor mesh import through runtime `ObjLoader`,
`MeshData` or `AssemblyMeshLibrary`. `NativeObjImporter` reads source OBJ/MTL
directly and preserves:

- original polygon identity through triangulation;
- authored corner normals when present;
- UV coordinates;
- material assignment per triangle;
- material seams and normal seams;
- topology adjacency;
- internal triangulation edges separately from authored polygon boundaries.

Technical and Elite edge masks remain separately authorable.

## Materials and lights

Materials use stable semantic ids rather than transient OBJ material numbers.
The asset stores base/emissive semantics, roughness/metallicity, two-sided state
and texture names. Technical/Anime/Elite renderers will interpret the same
material differently.

Visible glowing geometry is an emissive material. A real light source is a
`Socket` with `LightProperties` (`Point` or `Spot`). One fixture may use both.

## Thin surfaces

`SurfaceMode` is per geometry:

- `Closed`
- `ThinOneSided`
- `ThinTwoSided`

Visual sheets do not require duplicated front/back shells. Collision thickness
is authored separately.

## Compound collision

Collision is independent of render geometry. A model may contain any number of:

- oriented Box/OBB primitives;
- Sphere primitives;
- Capsule primitives (local +Y axis).

The editor supports add/delete/duplicate, click selection, position/rotation and
shape parameters. A radial capsule-chain generator is provided for toroidal or
curved structures such as the station ring, avoiding one enormous false-positive
AABB through the empty centre.

Large AABB/sphere bounds remain broad-phase bounds only; authored collision is
the narrow-phase/navigation representation.

## Joints and structural separation

Every node carries an authored `NodeJoint`:

- `Fixed` or `Revolute`;
- local pivot and axis;
- default angular rate and angle limits;
- optional breakability with break force/torque thresholds.

This keeps rotation axes and structural attachment geometry out of later object
descriptors. Runtime state supplies only current angle/rate/damage state.

## Detached rigid-body physics

A detached module must be able to become a rigid body without inspecting its
render mesh. `RigidBodyProperties` therefore stores the complete local mass
contract:

- mode (`Disabled`, `AutoFromCollision`, `Manual`);
- density used for authoring;
- resolved mass in kg;
- local centre of mass;
- full symmetric inertia tensor represented as `(Ixx,Iyy,Izz)` plus
  `(Ixy,Ixz,Iyz)` about the centre of mass.

The editor can estimate these values from the node's local compound collision
using analytic primitive mass/inertia plus the parallel-axis theorem. Manual
override remains available for components whose real internal mass distribution
does not resemble their shell.

This is the minimum data needed for believable impact torque, free rotation and
motion of detached panels/reactors/station sectors later.

## Sockets / anchors

Sockets remain stable semantic attachment points for cameras, weapons,
equipment, docks, main/RCS thrusters, lights, VFX, sensors and future gameplay
anchors. They retain parent node plus local position/orientation. Existing ship
attachment points are imported without deleting the legacy source descriptors.

## Editor I/O status

The editor has a persistent bottom status bar. Asset read/import/write operations
report `READING`, `WRITING`, `IDLE` or `ERROR`, message, compiled path and file
size where available. Long asset operations must not look like a frozen editor.

## Current acceptance target: orbital station

The station is the first stress asset because it exercises almost the complete
format:

- large viewport / many nodes;
- repeated geometry converted to instances/radial arrays;
- rotating sections and explicit joint axes/pivots;
- compound ring collision;
- many sockets/lights/docking anchors;
- material and edge authoring;
- rigid-body metadata for detachable sections.

## Still required before game runtime migration

- direct catalog/file browser for arbitrary new source models (not only current assembly registry ids);
- transform gizmos for nodes/collision/sockets/joints instead of numeric-only editing;
- automatic duplicate-geometry detection by canonical mesh hash;
- normal/smoothing repair tools and explicit hard-normal editing;
- LOD/proxy authoring and validation;
- migrate station docking/semantic catalogs into sockets;
- full validation report (degenerates, non-manifold topology, duplicate ids,
  dangling parents, invalid mass tensors, non-finite data);
- batch compiler and production conversion of every model.

Only after the converted catalog survives editor round-trip and validation does
`EliteGame` switch from OBJ to `.elmodel`.
