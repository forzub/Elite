# Model Asset Pipeline

## GEOMETRY authoring boundary (editor 0.10.20)

GEOMETRY edits one `RenderLod` at a time. The active LOD owns its `RenderNode` graph, geometry pool, instance sharing and replacement compatibility authoring; no G-index or RenderNode identity is propagated to another LOD. The editor may preview one main geometry, one standalone additional geometry, or a temporary replacement, but those are viewport-only states.

Instance consolidation and radial duplication reuse LOD-local geometry definitions. Additional/replacement meshes remain independent geometry definitions and record only which stable base visual families they may replace; later DAMAGE/state authoring decides when a compatible replacement is selected. Completing GEOMETRY checkpoints the full authored RenderLod set.


## Purpose

`EliteAssetEditor` is the offline authority for model preparation. The game
runtime must eventually consume compiled `.elmodel` assets and must not rebuild
normals, topology, collision, mass properties, joints or semantic anchors from
OBJ/render geometry.

The migration remains editor-first. `EliteGame` keeps its current renderer/model
loading path until the production catalog has been converted and validated.

## One asset = semantic assembly + independent render documents

A ship/station has one gameplay/semantic identity, but its visual representation
is not required to have the same assembly at every LOD. Asset format v4 therefore
separates two layers:

- `Node` is a semantic/gameplay part: hierarchy, base transform/pivot, joint,
  rigid-body metadata and default gameplay state.
- each `RenderLod` is an independent visual document with its own `RenderNode`
  hierarchy, geometry pool, transforms and instancing.

This means a detailed ship can legitimately be represented as:

```text
LOD0: hull + wings + engines + repeated modules + detachable visual parts
LOD1: one welded outer shell
LOD2: a few coarse proxy meshes
LOD3: one or two cylinders/boxes
```

No geometry id, G-index, topology, node count or instance relationship is required
to exist in another LOD. Instancing is local to one render document. For example,
three station habitat sectors may share one geometry in LOD0 while LOD1 contains
only a single welded station shell.

Gameplay semantics do not disappear when a coarse LOD is active. Collision, damage,
repair, sockets, joints and physical state remain attached to the semantic asset,
not to the currently visible render proxy.

## Semantic damage states and live structural substitution

A semantic `Node` has an implicit `intact` state plus optional `StateVariant` records
such as `damaged`, `breached`, `destroyed` or project-specific states. A variant may
override:

- local position, rotation and pivot (for a bent/partly detached section);
- rigid-body mass/centre-of-mass/inertia;
- detached state.

State-scoped records provide the rest of the live substitution contract:

- `RenderNode::activeStates` selects which visual nodes exist for a state in each LOD;
- `CollisionVolume::activeStates` swaps physical hit/navigation volumes;
- `Socket::activeStates` enables state-specific VFX/lights/attachments;
- `HitRegion` exposes damageable internal regions;
- `Opening` describes a breach that may be traversable and/or permit line of fire;
- `RepairTarget` exposes drone/repair work and the semantic state reached after repair.

A hit can therefore switch one habitat sector from `intact` to `breached` and, on
the same simulation tick, select torn render geometry, remove the old solid collision,
activate collision around the new hole, expose interior hit regions/VFX and register a
traversable/line-of-fire opening. Two still-intact sectors may continue sharing their
LOD0 geometry while only the damaged sector uses a replacement mesh.

State is independent of render LOD. `breached/LOD0` may show torn rooms, bulkheads
and consoles; `breached/LOD1` may use a simplified torn shell; a very distant LOD may
keep the same coarse proxy and communicate damage only through other presentation.

The runtime must apply a semantic state transition atomically: render selection,
collision/hit regions, openings, sockets/VFX, repair targets and any transform/physics
overrides become active together. Render LOD switching must never own gameplay state.

## Stable identity invariants

Stable IDs are authored identity, not display labels. Invalid identity must be rejected or repaired at the producer boundary, not discovered only when a checkpoint is serialized.

Hard rules:

- every semantic `Node::id` is non-empty and globally unique inside the asset;
- every `RenderNode::id` is non-empty and unique inside its own `RenderLod`;
- every render-geometry ID is non-empty and unique inside its own `RenderLod`;
- the same render ID may exist in different LOD documents because those documents are independent.

Source registries are allowed to reuse a human-facing token for different roles. In particular, a module and its child mesh may both be named `station_solar_panels`. The importer must qualify the child deterministically (for example `station_solar_panels.mesh`) instead of creating duplicate semantic Nodes. Legacy v2/v3 migration also normalizes old empty/duplicate IDs before copying semantic identity into v4 RenderNodes.

Wizard checkpoints run the same reusable `ModelAssetBinary::validate` preflight as production serialization. Diagnostics must identify the offending ID and indices, e.g. `LOD0 duplicate RenderNode id 'x': node[2] and node[7]`. The serializer remains the final safety net, not the first user-visible validator.

## Canonical coordinates / source basis

Compiled runtime data is canonical:

- +X = right
- +Y = up
- -Z = forward/nose

`SourceBasis` records the source convention. Existing runtime assemblies import
as `game_current`. The editor also provides an explicit Blender-model conversion
(+X right, +Z up, -Y forward) that converts geometry, normals, winding, semantic
transforms/pivots, render transforms, joints, collision, sockets and inertia to
canonical game coordinates. Basis conversion is a deliberate one-time authoring
operation, not runtime correction.

## Runtime normalization vs offline canonical preparation

Source loading and mesh preparation are separate operations. LOAD/restore/reimport preserve and publish the resident payload as read. In the Model Asset Editor the author explicitly runs `ПОДГОТОВИТЬ МЕШИ` from the LODS stage.

The game runtime keeps the tolerant `src/model_asset/RuntimeMeshNormalizer.*` contract for render ingestion: positional weld/cleanup may make a mesh renderable without asserting authoring topology semantics.

The **offline editor** has a stronger canonical authoring boundary. `CanonicalMeshBuilder` uses topology-aware point identity, collapsed/duplicate face cleanup, libigl `split_nonmanifold`, Embree raycast orientation, then editor-owned normal/render-vertex/edge rebuild while preserving UV/material/hard-normal seams. Real authored holes are never capped. libigl/Embree are editor-only and are not runtime dependencies.

Preparation records store the canonical algorithm id plus an output fingerprint. A resident mesh is technically ready for downstream LOD analysis only when the record matches the exact payload and post-inspection reports no structural invalidity, degenerate/duplicate triangles, winding conflicts or inward closed components.

`АНАЛИЗИРОВАТЬ` remains a separate read-only report. It may recommend/record `ClosedVolume`, `ThinTwoSided` or `BreachedVolume`, but these are **SURFACES authoring decisions**. They do not gate read-only LOD0 analysis and do not block completing the LODS checkpoint.

Render LODs remain independent documents; generated LOD1/LOD2/... are derived independently from canonical LOD0 rather than chained from one simplified LOD to the next.

## Binary format v4: semantic manifest + independent render LOD payloads

A compiled asset is a small semantic manifest plus one independent render document
per LOD. For asset `station` the package is:

```text
src/assets/compiled/models/station/
    station.elmodel
    station.lod0.elmesh
    station.lod1.elmesh
    station.lod2.elmesh   # when authored/generated
    ...
```

`station.elmodel` contains gameplay/semantic data and lightweight LOD descriptors:

- `META` — identity, source basis, bounds and LOD switch distance;
- `MATL` — stable style-agnostic material definitions;
- `SEMN` — semantic Node hierarchy, transforms, joints and base rigid-body data;
- `STAT` — state variants including transform/pivot/physics/detach overrides;
- `COLL` — state-scoped compound collision primitives;
- `SOCK` — state-scoped typed anchors/lights/VFX attachment points;
- `HITR` — state-scoped damage regions;
- `OPEN` — breach/opening semantics;
- `REPR` — repair targets and repaired state;
- `LODS` — available render-document descriptors only.

Each `<asset>.lodN.elmesh` owns an entire independent visual graph:

- its own `RenderNode` hierarchy and transforms;
- its own geometry definitions and LOD-local instances;
- vertices, indices, normals and UVs;
- polygon/material/smoothing metadata;
- topology and authored Technical/Elite edge masks;
- optional semantic-node/state bindings used for damage-state visualization.

No cross-LOD `GeometryDefinition` identity is required. `G0/G1/...` labels are local
to the currently active `.elmesh` and have no persistent meaning in another LOD.

Existing v2/v3 assets remain readable only as migration input. The migration creates
one initial independent render graph per legacy LOD, preserving old per-LOD mesh data
and old instance relations *inside that LOD*. Semantic Nodes then drop the legacy
`geometryIndex`. Source OBJ/assembly files are read-only and are never rewritten.

### Independent LOD editor documents

The editor loads the semantic manifest and selected render documents independently.
Opening an ordinary v4 asset reads the manifest/descriptors only; no `.elmesh` is read
until an explicit LOAD/RELOAD or a backend operation actually needs that LOD geometry.
Each LOD has `LOADED`/`UNLOADED` and `CLEAN`/`DIRTY` state and can be loaded, reloaded,
unloaded or saved without touching siblings. Backend-only residency helpers do not publish
geometry as a side effect. `Save manifest` writes semantic state only; declared geometry/node
counts remain available for unloaded LODs, and `Save all` writes only dirty/missing package members.

This is also the intended runtime streaming boundary: a distant ship can load its
semantic manifest plus only a coarse render LOD without reading LOD0.

`Generate LOD` is an optional offline authoring operation, not a required wizard gate or
a structural contract. The project maximum supported render resolution is
**2560x1440** and desktop window sizing uses the same central policy. LOD analysis
therefore never preserves geometry solely for a target above that ceiling. It currently
assumes a 70 degree vertical FOV and treats a feature below 2 screen pixels as a
candidate for removal. Smaller runtime resolutions may simplify earlier.

The first generator pass is conservative and read-only. It welds coincident positions
for analysis only (so OBJ UV/normal seams do not create false islands), finds connected
components, estimates principal-axis extents, and uses the **middle principal extent**
as a visual thickness proxy: for a long tube it behaves like diameter; for a flat sheet
it behaves like the smaller in-plane width rather than sheet thickness. The largest
structural island and any island carrying at least 25% of one geometry are protected.
`Preview Cull` sends only compressed triangle-removal ranges to the browser and never
changes the asset or checkpoint state.

Later generator passes may remove surface detail embedded in a carrier patch, collapse
bevels, simplify topology or replace assemblies with proxies. Any generated result must
remain a preview until the author explicitly assigns it to replace/add a target LOD;
LOD identity must never be inferred from filenames.

### Web UI synchronization is metadata-first

The browser viewport receives full render-mesh payloads only when geometry is actually
requested/replaced: explicit LOD load/reload, source reimport, checkpoint restore, or an
operation that changes vertex/index payloads. Ordinary v4 asset open is metadata-only.
Ordinary authoring
commands must use metadata-only synchronization. Position/pivot edits, instance
consolidation, geometry bindings, semantic/damage metadata, collision and socket edits
must not retransmit unchanged vertices, normals, indices or edge lists.

The transport itself has two planes. Small commands/state and render-LOD descriptors use
JSON. Bulk viewport geometry never does: positions, normals, triangle indices, triangle
material/smoothing data, authored edges and optional RAW diagnostic arrays travel in the
versioned editor-only `ELWIR001` binary WebSocket frame. The browser transport adapter
reassembles those arrays into the same `asset` / `lod_payload` objects consumed by the
existing viewport handlers; changing transport must not change viewport ownership or scene
semantics. `ELWIR001` is not an asset format and does not change `.elmodel/.elmesh` v4.
Known targeted mesh changes may use a transport delta: only changed resident LOD frames are
sent, unchanged LOD arrays are reused from the browser's already-resident payload, and the
adapter still reconstructs the complete legacy `asset` object before invoking its handler.
Initial load/reconnect/checkpoint restore/full mesh replacement remain self-contained snapshots.

The browser retains source mesh arrays and `THREE.BufferGeometry` objects in a cache
keyed by stable `LOD + render-geometry id`. A metadata refresh may rebuild the light
scene graph around those cached GPU buffers, but it must not recreate or retransmit
unchanged mesh payloads. A newly broken instance may clone an already-resident geometry
locally; an edge-mask edit transmits only the changed mask.

### Wizard checkpoints are durable snapshots, not the current branch

Workspace validity and checkpoint storage are independent. Editing, restoring or recompleting an earlier stage marks that stage and every downstream stage `stale`/`not_started` as appropriate, but never deletes a checkpoint directory. A stale checkpoint is an explicit rollback snapshot from another workspace lineage, not garbage.

Every new checkpoint stores the v4 ModelAsset package plus stage-local `editor_state.json`. The latter is serialized through the same `EditorAuthoringState` contract as production `production_state.json`, so PREPARE evidence, stable authoring identities, replacement compatibility and topology/surface intent are restored together with mesh bytes. Future SEMANTICS/PHYSICS/DAMAGE/VALIDATE/BUILD editor-only state must extend that same authoring-state serializer.

Existing backend mutations reserved for future wizard pages already use the same invalidation order: semantic hierarchy/joint/socket edits start at SEMANTICS, physics/collision edits at PHYSICS, and state/damage/opening/repair edits at DAMAGE. Future UI enablement must reuse those boundaries rather than bypass checkpoint validity.

Ordinary OPEN never auto-resumes a checkpoint: production `.elmodel/.elmesh` is the saved authority. A checkpoint enters the current working copy only through explicit RESTORE and remains dirty relative to production until SAVE ALL. Closing without SAVE discards that restored working copy and the next OPEN returns to production.

Editor-only state that survives a session is bound to the same geometry snapshot. `production_state.json` stores production authoring/stage state plus a package-member stamp; each checkpoint stores its own `editor_state.json`. `wizard_state.json` is only a session/checkpoint index and must never be attached as authoritative authoring metadata to freshly opened production geometry.

## Native OBJ compilation

Stage 2 still imports editor OBJ/MTL through `NativeObjImporter` rather than the runtime `ObjLoader`, because authoring must preserve UV/material/polygon metadata. After import, the explicit preparation action passes its positions/triangles through the same shared `RuntimeMeshNormalizer` used by the game. `NativeObjImporter` preserves:

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
material differently. Triangle `materialIndex` values reference this shared asset
material table; one RenderGeometryDefinition may therefore contain several
material slots without being split into separate geometry definitions.

SURFACES authoring edits material definitions and audits those existing per-triangle
assignments. A conservative repair may assign a material only to triangles whose
index is `NoIndex`; arbitrary per-face painting/repartitioning and texture import or
baking are separate future tools. Texture fields remain authored references.

Visible glowing geometry is an emissive material. A real light source is a
`Socket` with `LightProperties` (`Point` or `Spot`). One fixture may use both.

## Thin surfaces

`SurfaceMode` is LOD-local, per `RenderGeometryDefinition`:

- `Closed`
- `ThinOneSided`
- `ThinTwoSided`

The editor keeps a separate authoring surface intent for the SURFACES workflow:
`ClosedVolume`, `ThinOneSided`, `ThinTwoSided`, `BreachedVolume`. The intent answers
what an open/closed shell represents; `SurfaceMode` answers how it is rendered.
`ClosedVolume` and `BreachedVolume` map to front-sided `Closed`, while the two thin
intents map to their corresponding render modes. Selecting an intent never repairs
or fills mesh topology.

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

## Authored alternate render geometry

Additional authored visual meshes are discovered by recursively scanning each real
`LOD<N>` source directory tree. Registered assembly OBJ files are the default set
and are excluded from the scan. Every other OBJ below the LOD root is additional;
directory and filename conventions do not encode damage semantics.

At first discovery the editor creates an opaque authoring identity such as
`extra.000001`. The OBJ path is only a reload pointer. Ordinary geometry receives
a separate stable `base.000001` authoring identity. `G#` is an ephemeral LOD-local
UI index and is never used as a persisted replacement contract.

The workspace persists a many-to-many relation:

```text
extra visual id -> [base visual id, ...]
```

The Geometry stage presents this as a master/detail table: select one extra mesh,
then check the default geometry families it may replace. A default GeometryDefinition
appears once even when many RenderNodes instance it. Preview temporarily swaps the
selected extra onto one representative instance and is allowed before compatibility
is committed; preview does not mutate semantic/gameplay data.

No LOD relationship is inferred from filenames. A future LOD generator may
explicitly copy the same base/extra visual identity into its generated LOD
representation, so generated file names can be arbitrary.

Compatibility answers only **what may replace what**. DAMAGE later answers
**when/why which compatible visual is selected**. For stations the intended first
use is a small authored set of catastrophic structural variants, while ordinary
hits remain VFX/decal/integrity events and detachable equipment remains separate
semantic Nodes.

Unused-geometry cleanup must never delete authored extra meshes merely because
they are not yet referenced by a RenderNode.

## View orientation

The model viewport must label the **ends of the world grid/axes** with `+X/-X`,
`+Y/-Y` and `+Z/-Z`. These labels live in world space with the grid, not beneath a
status overlay or in a corner-only camera widget. Numeric transforms and radial-
array controls must never require the user to infer axis orientation from the
model silhouette.

## Generated render LOD authoring (editor 0.10.18)

LOD0 is the canonical generation source. A generated RenderLod is a complete independent render document, not a delta from another LOD. Generation includes the entire LOD0 geometry pool, including additional/replacement meshes that are not bound to default RenderNodes. The editor may replace an existing LOD slot or create a new contiguous slot, but never mutates LOD0 through the generator. Stable replacement authoring identities are propagated across generated LOD documents.
