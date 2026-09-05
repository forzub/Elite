# Model Asset Pipeline

## GEOMETRY authoring boundary (editor 0.10.20)

GEOMETRY edits one `RenderLod` at a time. The active LOD owns its `RenderNode` graph, geometry pool, instance sharing and replacement compatibility authoring; no G-index or RenderNode identity is propagated to another LOD. The editor may preview one main geometry, one standalone additional geometry, or a temporary replacement, but those are viewport-only states.

Instance consolidation and radial duplication reuse LOD-local geometry definitions. Additional/replacement meshes remain independent geometry definitions and record only which stable base visual families they may replace; later DAMAGE/state authoring decides when a compatible replacement is selected. Completing GEOMETRY validates the current authored RenderLod set and persists its validity in WORKING ASSET; rollback snapshots are separate manual actions.


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

### Complete wizard / maintenance chain (editor 0.10.32)

Initial authoring keeps the ordered nine-stage contract: `SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD`. Before the first production BUILD, a stage becomes editable only when its immediate predecessor is `COMPLETE`. After a production package exists, the editor is a maintenance editor: any stage may be opened directly and readiness is computed from the current persistent WORKING ASSET plus per-component maintenance debt, never from checkpoint ancestry.

`SEMANTICS` owns the shared gameplay hierarchy, pivots/joints, sockets and per-LOD render-to-semantic bindings. `PHYSICS` owns base collision geometry and resolved rigid-body mass/inertia. `DAMAGE` owns state variants plus state selectors for render nodes/collisions/sockets, hit regions, openings and repair targets. `VALIDATE` is read-only and reruns every upstream production contract plus `ModelAssetBinary::validate`. `BUILD` is the terminal production commit and the only normal writer of production `.elmodel/.elmesh`; it writes the complete validated WORKING ASSET and does not create or mutate rollback checkpoints.

Collision and socket state scopes are metadata owned by DAMAGE even though their base shape/transform belongs to PHYSICS/SEMANTICS respectively. Editing only `activeStates` therefore invalidates from DAMAGE rather than rolling back the earlier owning stage.

### SEMANTICS authoring boundary (editor 0.10.28)

The SEMANTICS wizard authors one asset-wide `Node` hierarchy and explicit per-LOD `RenderNode::semanticNodeIndex` bindings. LOD switching in this stage is only an inspection boundary: buttons select/load one independent render document, while the semantic tree remains unchanged. The `APPLY TO ALL LODS` binding convenience matches exact stable RenderNode ids only; it never infers identity from LOD-local indices or fuzzy geometry/name similarity. Geometry-bearing enabled RenderNodes must be bound before the SEMANTICS stage validates; geometry-less render grouping nodes may stay unbound.

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

Wizard stage validation and rollback persistence are independent. A stage check updates validity/debt in the persistent WORKING ASSET; it never creates a checkpoint. A manual rollback snapshot may be created whether or not the current stage validates. The v4 serializer remains the final hard safety net for structural/identity/I/O failures; diagnostics must identify the offending ID and indices, e.g. `LOD0 duplicate RenderNode id 'x': node[2] and node[7]`.

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
Each LOD has `LOADED`/`UNLOADED` and `CLEAN`/`DIRTY` state and can be loaded, reloaded or
unloaded independently. Backend-only residency helpers do not publish geometry as a side effect.
Persistence is deliberately coherent in 0.10.32: ordinary SAVE/autosave writes the persistent
WORKING package plus its matching editor_state. Legacy `save_manifest` / `save_lod` commands
are compatibility aliases to that whole working save; partial package states are not resume heads.

This is also the intended runtime streaming boundary: a distant ship can load its
semantic manifest plus only a coarse render LOD without reading LOD0.

`Generate LOD` is an optional offline authoring operation, not a required wizard gate or
a structural contract. The generator uses the existing 2-pixel visibility rule to derive
a **dimensionless** per-level runtime error instead of a Blender/source-unit camera
distance. There is no fixed authoring FOV/resolution in this authority: current viewport
height and FOV are evaluated only when the runtime projects the real game object.

`relativeGeometricError = omittedFeatureCharacteristic / completePlacedLod0Characteristic`

Generated LODs persist that value in the optional v4 `LERR` manifest chunk. LOD0 is
zero; a negative value on LOD1+ means legacy/manual SSE is unknown and is a conservative
runtime boundary. The existing `LODS` and `.elmesh` layouts are unchanged and old v4
readers safely skip `LERR`.

At runtime the final gameplay/world-scaled object bounds are projected through the current
camera/FOV/viewport. The renderer evaluates
`projectedErrorPx = relativeGeometricError * projectedCharacteristicPixels` and selects
the coarsest safe LOD. The shared policy uses 1.8 px to coarsen and 2.2 px to refine around
the 2 px target so an object does not flicker between levels at the boundary. Source mesh
units never enter this runtime decision. A distance can be derived from real world size as
a diagnostic, but is not authored authority. The legacy OBJ `lodSwitchDistance` remains
compatibility-only until EliteGame consumes compiled `.elmodel` LODs.

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

### Persistent WORKING ASSET and rollback-only checkpoints (editor 0.10.32)

The editor resume head is `workspaces/<asset>/working/<asset>.elmodel` plus its `.elmesh` payloads and matching `working/editor_state.json`. OPEN always prefers this package. If it does not exist, production is adopted as the initial working state; if production does not exist either, SOURCE import creates it. Checkpoint timestamps and `checkpointSequence` never select the resume head.

Ordinary SAVE and autosave persist the coherent WORKING package. The editor-only sidecar carries PREPARE evidence, stable authoring identities, SOURCE fingerprints, replacement compatibility, per-component maintenance debt and the current stage-validity map, bound to the exact package bytes by a package stamp. SAVE does not invalidate anything, write production or mutate checkpoint history.

A checkpoint is now only an explicit manual rollback snapshot. `CREATE ROLLBACK SNAPSHOT` stores the exact current package plus editor state without validating the stage, changing stage status, unlocking anything or pruning another snapshot. `checkpointSequence` remains monotonic metadata for snapshot chronology/legacy compatibility and advances only after a manual snapshot is successfully persisted.

RESTORE loads that literal snapshot and immediately persists it as the new WORKING head so restart returns to the restored state. Other snapshots and production are untouched. Legacy snapshots remain readable; missing legacy editor-only evidence is conservatively marked for review.

BUILD is the sole normal production-write boundary. It writes the complete current WORKING asset regardless of working dirty flags and refreshes `production_state.json`. Rollback checkpoints are untouched. `wizard_state.json` is only a session/checkpoint index; current stage validity is owned by the WORKING sidecar.

For first-time authoring, stage unlock still follows the ordered wizard. For an asset with an existing production package, maintenance-mode stage access is direct and blockers come from current validity and local component debt rather than saved checkpoint lineage.

Wizard mutations use the same invalidation order: semantic hierarchy/joint/socket base edits start at SEMANTICS, base physics/collision edits at PHYSICS, and state selectors/damage/opening/repair edits at DAMAGE.

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

Materials use stable semantic ids rather than transient OBJ material numbers, but they
are **sparse visual-role overrides**, not a requirement that every triangle carry a PBR
material. `Triangle::materialIndex == NoIndex` is the valid implicit `DEFAULT` surface.
Technical/Anime/Elite renderers may interpret DEFAULT and the same explicit material
roles differently. One RenderGeometryDefinition may therefore contain mostly DEFAULT
triangles plus a few explicit slots without being split into separate geometry definitions.

Blender/OBJ is the preferred place to partition faces into explicit material groups because
that is a visual face-selection task; the editor imports those groups and owns the stable
runtime ids/properties. A conservative editor command may replace implicit DEFAULT on
selected geometry with one existing explicit material, but validation never mass-creates a
dummy material. Base/emissive/PBR-compatible fields remain optional appearance data for
future styles; texture import/baking and arbitrary face painting are separate tools.

A visibly self-lit surface is an emissive material, but emission does **not** imply bloom/glow. The default renderer may show it only as a high-contrast colour/value. A real light source is a `Socket` with `LightProperties` (`Point` or `Spot`); use those sparsely for gameplay-relevant lamps/searchlights rather than dense window fields. Global cartoon softening/blur belongs to renderer/post-process policy, while haze/glow is reserved for explicit environmental/VFX states. Engine exhaust and explosions are VFX/gameplay effects, not static surface materials.

## Thin surfaces

`SurfaceMode` is LOD-local, per `RenderGeometryDefinition`:

- `Closed`
- `ThinOneSided`
- `ThinTwoSided`

Explicit SURFACES intent edits are geometry-level metadata operations. `ClosedVolume`, `ThinOneSided`, `ThinTwoSided` and `BreachedVolume` do not traverse triangle arrays, invoke PREPARE/model preflight, regenerate mesh buffers, retransmit geometry payloads or rebuild the complete browser scene. A targeted metadata patch updates the affected GeometryDefinition and resident renderer sidedness; topology analysis remains an explicit ANALYZE operation. `AUTO` is the exception because automatic classification needs topology evidence for the selected geometry.

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
