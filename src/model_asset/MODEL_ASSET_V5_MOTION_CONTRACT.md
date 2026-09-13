# Model Asset v5 motion package — DRAFT CONTRACT

> **Status: PROVISIONAL / DESIGN DRAFT.** The current production serializer remains **ModelAsset v4**. This document fixes the first concrete shape of the motion-capable package so editor/runtime work has a common target. It is expected to change while the first articulated, skinned and animated production assets are integrated.

## 1. Non-negotiable ownership

The v5 model keeps four independent hierarchies. They may reference one another, but they are not the same object:

```text
SemanticNode != RigNode != Bone != RenderNode
```

- **SemanticNode** — gameplay/logical part and damage/physics ownership.
- **RigNode** — rigid articulated transform hierarchy used by mechanisms.
- **Bone** — deformation hierarchy used by skinned geometry.
- **RenderNode** — visual hierarchy local to one render LOD.

Motion is split into capability and control:

```text
RIG    = what motion the asset permits
DRIVER = who supplies the motion at runtime
```

The asset serializes permitted motion, channels, clips, events and bindings. AI/gameplay driver logic is not serialized into the model package.

## 2. Package layout

```text
<asset>.elmodel          v5 manifest / lightweight shared asset data
<asset>.lod0.elmesh      independent render LOD0 + optional skin binding
<asset>.lod1.elmesh      independent render LOD1 + optional skin binding
...
<asset>.elanim           optional heavy animation bank
```

Static assets remain valid: they simply have empty motion/skeleton/animation catalogs and no `.elanim` payload.

### Manifest v5

The v4 chunks remain conceptually intact. v5 adds these motion-domain chunks:

| Chunk | Owner | Purpose |
| --- | --- | --- |
| `RIGS` | shared manifest | rigid articulated rigs: nodes, joints, limits, rest values |
| `MCTL` | shared manifest | stable named motion/control channels |
| `SKEL` | shared manifest | skeleton catalog: bones, parent links, rest TRS |
| `ACAT` | shared manifest | animation clip catalog: duration, flags, root motion, track/event ranges |
| `ATCH` | shared manifest | attachment/socket target bindings to SemanticNode, RigNode or Bone |

The whole manifest version is v5; these are not silently appended to writable v4 assets.

### Per-LOD `.elmesh`

Geometry and RenderNode ownership stays LOD-local. A LOD may additionally carry a `SKIN` payload:

- referenced shared skeleton index;
- LOD-local bone palette/remap;
- inverse-bind matrices;
- per-vertex influences;
- optional LOD-specific remap/reweight metadata.

A far LOD may omit skinning entirely and render as a rigid/coarse representation. No independent skeleton is authored per LOD.

### `.elanim`

Proposed magic: `ELANM005`.

The animation bank stores heavy key data separately from the manifest catalog:

- **rigid scalar tracks** → `MotionChannel` (`angle`, `distance`, or other typed scalar);
- **skeletal tracks** → bone local TRS;
- clip events/markers;
- root-motion metadata;
- future compression metadata/version without changing semantic ownership.

The first implementation may store uncompressed keys. Compression is a compiler concern and must not alter the logical clip/channel contract.

## 3. Rigid mechanisms

Minimum joint set:

```text
Fixed
Revolute
Prismatic
```

Detachability is orthogonal to joint type. Do not create combinatorial types such as `ROTATE+DETACH` in the motion schema.

A joint owns:
- parent and child rig nodes;
- local pivot/axis through node rest frames;
- minimum/maximum value;
- neutral/rest value;
- detachable flag.

Named control channels expose the joint to drivers. Examples:

```text
gear.arm.angle
turret.yaw
turret.pitch
gear.slider.extension
```

A deployment clip can drive `gear.*`; an `AimDriver` can drive `turret.*`. The geometry/rig does not care which driver is currently active.

## 4. Skeletons and skinning

A skeleton is asset-wide shared data:
- stable skeleton id;
- stable bone ids;
- parent index;
- rest local TRS.

Higher LODs may use the full skeleton. Lower LODs may use a palette/subset and remapped weights. Very distant LODs may be unskinned.

The authoring/editor representation may keep stable string ids for diagnostics and merge/reimport. The compiled runtime can additionally resolve verified dense indices for fast lookup.

The initial skin payload assumes up to four influences per compiled vertex (`SkinInfluence4`). This is an implementation limit, not a semantic rule; a later payload revision may increase it.

## 5. Clips, events and root motion

`AnimationClipDescriptor` lives in the manifest catalog. Heavy keys live in `.elanim`.

A clip descriptor defines:
- stable clip id;
- duration;
- loop flag;
- optional root-motion extraction target;
- scalar and bone-track ranges;
- event range.

Events are explicit. Runtime must not infer events by checking arbitrary animation time windows. Examples:

```text
footstep_L
footstep_R
gear_start
gear_locked
hatch_opened
```

Root motion is metadata, separate from world/AI trajectory. A walking actor can consume root motion; a bird/dolphin/manta can use AI trajectory while the local clip drives wing/body deformation.

## 6. Attachments and sockets

An attachment target is typed:

```text
SemanticNode
RigNode
Bone
```

The binding also stores a local TRS offset. This permits hand-carried props, cameras, weapons, mechanical equipment mounts and future bone-bound hit volumes without conflating hierarchy types.

## 7. Drivers are runtime state

The package does **not** serialize game logic such as target acquisition, AI state or physics solver state. Runtime drivers may include:

```text
ClipDriver
DirectControlDriver
AimDriver
IKDriver        (later)
PhysicsDriver   (later)
ScriptDriver    (later)
```

Initial production scope needs `ClipDriver`, `DirectControlDriver` and `AimDriver` only.

## 8. Source/import contract

GLB/glTF 2.0 is the intended primary production source for new animated/skinned assets. OBJ remains supported for legacy/simple static assets.

Blender owns DCC authoring: geometry, hierarchy, pivots/rest transforms, armatures, weights, clips and materials. The Model Asset Editor imports, validates and maps those authored structures into the typed Elite asset contract.

Blender/glTF `extras` may carry import hints, but runtime never parses arbitrary `extras`. Import converts them into typed canonical data.

## 9. Coordinate/basis invariant

Any source-to-game basis/scale conversion must be applied consistently to:
- vertices and normals;
- rig rest transforms;
- skeleton bind/rest transforms;
- inverse-bind matrices;
- animation translation channels;
- sockets/attachments;
- collision/hit volumes.

A package where the mesh is correct but the skeleton/animation uses another basis is invalid.

## 10. Version/downlevel safety

**v4 remains production until the v5 serializer/reader is implemented and accepted.**

Rules:
1. A v4 editor must never open a v5 asset as writable v4 and silently discard motion chunks.
2. v5 uses a distinct manifest magic/version and distinct animation payload magic.
3. Static v4 → v5 migration creates empty motion catalogs and preserves existing semantics/render LOD data.
4. Unknown future v5 subchunks may be skipped only when the enclosing schema explicitly guarantees round-trip safety; editor SAVE must never silently drop authoritative data it does not own.

## 11. First production acceptance

Before v5 becomes production authority, the same pipeline must pass at least:
1. articulated ship hatch + folding/telescopic landing support (`Revolute` + `Prismatic`, clip-driven);
2. turret (`Revolute`, `AimDriver`/direct channel driven, no required baked clip);
3. skinned humanoid (shared skeleton + skin + idle/walk + events/root-motion metadata);
4. Zenith/static multi-LOD regression.

The draft is intentionally concrete enough to implement against, but not frozen against corrections discovered by these assets.
