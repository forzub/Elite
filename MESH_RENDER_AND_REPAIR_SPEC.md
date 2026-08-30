# Elite Model Asset Editor — Render Mesh Contract & Repair Engine Specification

**Baseline:** Model Asset Editor 0.10.14 / asset format v4  
**Purpose:** engineering source of truth for finishing mesh preparation.  
**Priority:** produce a render-ready and authoring-ready working mesh. Diagnostics are internal sensors of the repair engine, not the product.

---

## 1. Product target

The game is expected to support at least three visual profiles:

1. **Elite Classic** — black polygon fill + white structural edges.
2. **Experimental distance style** — current distance-dependent fill/edge treatment.
3. **Anime / cartoon style** — actual toon/cel style, potentially using screen-space post-processing rather than a generic 3D-editor look.

All three profiles may use frame-level filters/post-processing. The mesh asset therefore must provide stable geometric inputs; style-specific visual tricks must not be baked into canonical topology unless strictly necessary.

### Non-goal

Canonical mesh preparation does **not** attempt to make every source into CAD-perfect watertight geometry. It makes geometry reliable enough for:

- depth rendering;
- front/back-face semantics;
- lighting and normal-buffer use;
- silhouette/edge extraction;
- LOD processing;
- later surface/semantic/physics/damage authoring.

---

## 2. Minimal canonical mesh contract required by all render profiles

A prepared `MeshLod` is `GOOD_ENOUGH` when the following render-relevant invariants hold.

### 2.1 Payload integrity

Required:

- all referenced positions are finite;
- all triangle indices are valid;
- no zero-area / collapsed triangles remain;
- no exact duplicate triangle remains;
- unreferenced render vertices are removed during final rebuild;
- bounds are recomputed from final positions.

Repair policy:

- malformed primitives that cannot be interpreted may be dropped if the remaining mesh is usable;
- the whole mesh fails only when no usable triangle surface can remain.

### 2.2 Geometric topology is distinct from render topology

A **geometric point** is topological identity. A **render vertex** is a GPU corner representation.

One geometric point may emit multiple render vertices because of:

- UV seams;
- material boundaries;
- hard-normal / smoothing islands.

Authored OBJ normal indices are **not** geometric identity and are **not** authoritative normals.

Positional equality within `1e-4` is only evidence/candidate matching. It is never sufficient by itself to prove that independent sheets are connected.

### 2.3 Orientable patches

Every connected orientable patch must have locally consistent face winding:

- two faces sharing a manifold edge traverse that edge in opposite directions;
- local reversed triangles are flipped;
- a parity/orientation conflict is a topology repair problem, not a normal-generation problem.

### 2.4 Absolute exterior orientation

This is mandatory wherever the concept of an exterior side exists.

#### Closed volume

Use signed volume after local winding consistency. The shell must be oriented outward.

#### Open / breached solid-like shell

A boundary loop does not remove the concept of exterior. Signed volume alone cannot solve this because the component is open.

Use an exterior-side test on the already-consistent patch:

- sample representative face points;
- cast rays into both normal hemispheres (or use an equivalent AO/visibility test);
- the side seeing more free space / lower occlusion is exterior;
- flip the whole patch when the current front side points toward the more occluded/interior hemisphere;
- record confidence; if confidence is too low, do not destructively guess.

This follows the same principle as libigl `reorient_facets_raycast()`.

#### Thin two-sided sheet

There is no physically unique outside. Requirement:

- winding is locally consistent;
- canonical front side is stable but may be arbitrary when exterior confidence is low;
- runtime two-sided shading must flip the visible normal for back-facing fragments (`gl_FrontFacing` or equivalent) rather than requiring duplicated back faces.

### 2.5 Normals

Normals are generated **only after final face orientation**.

Required:

- finite, normalized normals;
- hard/smooth islands derived from smoothing metadata and/or crease angle;
- no authored-normal-only split survives unless it corresponds to a reconstructed hard-normal island;
- a closed/breached exterior surface must not retain inward normals after its patch orientation has been resolved.

### 2.6 Edge graph

After topology and winding are final, rebuild the authoritative edge graph once.

Required edge information:

- boundary;
- triangulation-internal;
- crease;
- material seam;
- normal seam;
- source-authored edge where explicitly preserved;
- triangle adjacency.

`EdgeCanonicalTopology` means this recovered adjacency is authoritative. Later stages must not re-infer connectivity from coordinate coincidence.

### 2.7 Render-profile consequences

#### Elite Classic

Needs:

- correct depth-writing surfaces;
- stable structural edge graph;
- ability to suppress triangulation-internal edges;
- reliable boundary/crease/material edges.

The existing `EdgeRenderElite` mask is sufficient for now. Do **not** add a cartoon-specific edge mask until a renderer actually needs it.

#### Experimental distance style

Current shaders use vertex normals directly for wrap light, hemisphere ambient, rim/facing, normal tint and procedural grid tangent construction. Inward normals therefore directly corrupt color, rim, sky/bottom lighting and grid orientation.

Needs:

- correct visible-side normal;
- correct depth;
- stable positions/topology.

Distance coloring itself is a shader/postprocess concern, not a mesh-repair concern.

#### Anime / cartoon

Likely inputs:

- stable depth;
- reliable normals/normal buffer;
- material/object IDs;
- optional structural edge graph for authored line work;
- screen-space silhouette / depth-normal discontinuity postprocess.

Do not bake toon outlines into geometry now. The canonical mesh only has to make these buffers correct.

---

## 3. Current 0.10.14 code: what is useful

`tools/model_asset_editor/CanonicalMeshBuilder.cpp` already contains useful primitives:

- `buildWorkingSet()` — degenerate/duplicate cleanup;
- `buildTopologicalPointMap()` — attempts topology-aware identity recovery;
- `buildTopology()` — edge uses;
- `solveOrientation()` — BFS parity consistency;
- `applyParity()` — triangle flipping;
- `flipInsideOutClosedComponents()` — signed-volume closed-shell orientation;
- `rebuildRenderVertices()` — UV/material/hard-normal-aware render rebuild;
- `rebuildEdges()` — final edge graph;
- `canonicalMeshFingerprint()` — stable change detection.

These should be reused rather than rewritten blindly.

---

## 4. Current 0.10.14 defects that matter now

### 4.1 Open/breached components can remain globally inside-out — CURRENT VISUAL BUG

Current code in `flipInsideOutClosedComponents()` does:

```cpp
if (components.boundaryEdges[ci] != 0) continue;
```

Therefore:

- BFS may make every triangle in an open station shell mutually consistent;
- the entire shell may still face inward;
- normals are then correctly regenerated **from the wrong global side**;
- editor `THREE.FrontSide` culls those polygons when `surfaceMode == closed`;
- the normal overlay visibly points inward.

This is the primary current defect.

**Fix:** add `orientOpenPatchesExterior()` after BFS and before normal rebuild, using raycast/AO free-space voting. It must flip whole orientable patches, never individual random faces.

### 4.2 Current audit cannot detect the above defect

`analyzeCanonicalMesh()` only counts `insideOutClosedComponents`. Open components have no absolute orientation test.

Thus a breached/open shell can be visibly inverted while audit says winding is clean.

**Fix:** add internal issue `InwardOpenSolidPatch` / `ExteriorOrientationAmbiguous` based on the same exterior-side estimator used by repair.

This information belongs in the developer repair log. The normal UI only needs READY / NO PROGRESS.

### 4.3 Builder claims some postconditions instead of measuring them

At the end of `canonicalizeMesh()` current code assigns:

```cpp
result.after.windingFlipsRequired = 0;
result.after.windingConflicts = 0;
result.after.insideOutClosedComponents = 0;
```

These are declarations, not a post-repair measurement.

**Fix:** remove fabricated `after` state. The repair controller re-inspects the actual mutated internal mesh/payload after every pass.

### 4.4 PREPARE is still one-shot

Current `canonicalizeMesh()` performs exactly one pass. It stops immediately on:

- canonical multi-use edge;
- orientation conflict;
- other topology it does not know how to repair.

**Fix:** repair-loop controller runs concrete repair passes until `GOOD_ENOUGH` or `NO_PROGRESS`.

### 4.5 RAW two-use positional edge fallback remains unsafe

Current `buildTopologicalPointMap()` has a RAW fallback:

```cpp
if (uses.size() == 2) unionUseEndpoints(uses[0], uses[1]);
```

Two unrelated coincident sheets can therefore still be joined when exactly two faces happen to share the same segment in space.

**Fix:** positional equality alone never establishes topology. Prefer, in order:

1. authoritative canonical edge adjacency;
2. importer-preserved source edge / polygon adjacency;
3. conservative stitching only when local face-fan continuation supports it.

Do not globally weld coincident sheets.

### 4.6 Non-manifold topology is detected but not repaired

Current builder stops on `edgeUses.size() > 2`.

For polygon-soup repair, a common safe operation is to split/duplicate topological points/edges into separate manifold/orientable fans rather than force all coincident uses into one edge.

**Fix:** split non-manifold edge/vertex fans when a deterministic partition exists, then rerun orientation.

### 4.7 Orientation conflict is detected but not repaired

Current BFS conflict terminates preparation.

**Fix:** conflict connectivity becomes a seam candidate. Split the conflicting adjacency, duplicate topological point identity as necessary, then orient the resulting patches independently.

### 4.8 Heavy diagnostics are in the wrong place

`analyzeModelPreflight()` builds a large user-facing report with component counts, boundaries, source NM evidence, classifications and readiness gates.

This data may be useful for development, but it is not the goal of PREPARE.

**Fix:** PREPARE uses an internal issue vector and a file log. User-facing status is only:

```text
station_core_front: READY · passes=2 · changed
station_solar_panels: READY · passes=2 · changed
```

or:

```text
station_x: NO PROGRESS · unresolved=OrientationConflict
```

Do not expand Preflight further while mesh preparation is unfinished.

---

## 5. Problems intentionally deferred unless a production mesh proves they matter

To avoid another architecture detour, the first complete repair engine should **not** spend time on these unless encountered as an actual blocker:

- arbitrary triangle-triangle self-intersection repair;
- automatic hole filling;
- general boolean remeshing;
- global remeshing for aesthetic triangle quality;
- cartoon-outline geometry generation;
- renderer-specific duplicated back faces;
- CAD watertightness as a universal requirement.

Self-intersections may be logged later, but they are not currently required for the three render styles and should not block this milestone by default.

T-junction/stitch repair should be implemented only to the extent that real station/ship cracks remain after source-adjacency recovery. Do not invent a full mesh-healing suite before evidence demands it.

---

## 6. Proposed internal representation: `RepairMesh`

Do not repeatedly convert `MeshLod` into multiple full temporary copies.

Convert once:

```text
MeshLod
  ↓ one decode
RepairMesh
  geometric points
  triangle point IDs
  per-corner UV
  material index
  smoothing/source polygon metadata
  adjacency / patch IDs
  ↓ repair loop in place
final RepairMesh
  ↓ one render rebuild
MeshLod
```

Suggested logical data:

```cpp
struct RepairPoint {
    glm::dvec3 position;
};

struct RepairCorner {
    std::uint32_t point;
    glm::vec2 uv;
};

struct RepairFace {
    RepairCorner corner[3];
    std::int32_t materialIndex;
    std::int32_t sourcePolygonId;
    std::uint32_t smoothingGroupId;
};
```

Normals are not stored as authoritative input in `RepairMesh`.

When manifoldization needs to separate two sheets, duplicate the **RepairPoint identity** while keeping the same position. This is exactly the distinction required between geometric/topological identity and spatial coincidence.

---

## 7. Repair loop

### 7.1 Issue set

Only issues that have a repair or directly violate the render contract belong in the mandatory loop:

```cpp
enum class MeshRepairIssueKind {
    InvalidTriangleReference,
    NonFiniteReferencedPosition,
    DegenerateTriangle,
    DuplicateTriangle,
    NonManifoldEdge,
    NonManifoldVertex,
    OrientationConflict,
    InconsistentWinding,
    InwardClosedPatch,
    InwardOpenSolidPatch,
    ExteriorOrientationAmbiguous
};
```

`BoundaryEdge` is **not an issue**.

`ThinTwoSided` is **not an issue**.

### 7.2 Pass order

```text
PASS N

1. SANITIZE
   remove uninterpretable primitives where safe

2. CLEAN
   degenerate / duplicate cleanup

3. CONNECTIVITY
   recover source/canonical adjacency
   split deterministic non-manifold edge/vertex fans

4. LOCAL ORIENTATION
   BFS parity on each orientable patch
   split conflict seam if safely identifiable

5. EXTERIOR ORIENTATION
   closed patch -> signed volume
   open patch -> raycast/AO free-space vote
   ambiguous free sheet -> leave stable BFS side

6. RE-INSPECT GEOMETRIC REPAIR STATE

if GOOD_ENOUGH:
    final normal islands
    render vertex rebuild
    authoritative edge rebuild
    bounds
    publish MeshLod
    STOP READY

if payload/topology changed and issue score improved:
    NEXT PASS

if no topology/orientation change OR issue vector unchanged:
    STOP NO_PROGRESS
```

Use a small hard cap (for example 8) only as a corruption guard. Normal production meshes should converge in 1–3 passes.

### 7.3 Progress metric

Do not use prose. Track a deterministic tuple, for example:

```text
fatal_input
non_manifold_edges
non_manifold_vertices
orientation_conflicts
inconsistent_faces
inward_resolvable_patches
degenerate_faces
duplicate_faces
```

A pass has progress when this tuple decreases lexicographically/weighted **or** topology fingerprint changes toward a lower issue count.

---

## 8. Exterior orientation implementation

This is the immediate missing algorithm.

### 8.1 Closed patches

Retain current signed-volume method.

### 8.2 Open patches

Implement an editor-only CPU ray intersector. It does not need to enter runtime.

For each sufficiently large orientable patch:

1. choose deterministic area-weighted face samples;
2. offset ray origin by a small epsilon from the surface;
3. cast a fixed deterministic set of rays over the `+N` and `-N` hemispheres;
4. ignore the source face / immediate epsilon self-hit;
5. measure hit ratio and optionally distance-weighted occlusion;
6. compare both hemispheres;
7. the less occluded hemisphere is exterior;
8. flip the entire patch if current `+N` is more occluded;
9. if the score difference is below a confidence threshold, mark `ExteriorOrientationAmbiguous` and do not guess.

Acceleration:

- build one BVH/AABB tree per geometry per repair pass;
- reuse it for all patch samples;
- deterministic sample count, e.g. 32–128 rays per patch depending on area/triangle count;
- this is offline authoring, but should still be bounded.

For a free thin plate both hemispheres will tend to be similarly unoccluded; ambiguity is acceptable because runtime two-sided shading handles both sides.

For a breached station shell, the interior hemisphere should be substantially more occluded, yielding a confident outward decision.

Reference behavior: libigl `bfs_orient()` + `reorient_facets_raycast()` uses the same conceptual split: first make orientable patches consistent, then orient solid-like patches so the front side faces the less occluded/more empty space.

---

## 9. Detailed mesh repair log

The log is for development and debugging. It must not inflate normal UI.

### 9.1 Location

```text
build/tools/model_asset_editor/logs/
    mesh_repair_<asset>_<timestamp>.log
```

One log per explicit `PREPARE MESHES` operation.

### 9.2 Header

Example:

```text
[repair.begin]
editor=0.10.xx
asset=station
algorithm=mesh_repair_engine_v1
weld_epsilon=0.0001
loaded_lods=0,1
```

### 9.3 Per-mesh input

```text
[mesh.begin] lod=0 id=station_core_front
input V=219128 T=201450 E=...
fingerprint=...
surface_mode=closed
```

### 9.4 Per-pass inspection

```text
[pass.inspect] n=0
invalid_ref=0 nonfinite=0 degenerate=118 duplicate=0
points=... patches=...
nonmanifold_edge=0 nonmanifold_vertex=0 orient_conflict=0
winding_flips=1636
closed_patches=... inward_closed=20
open_patches=... open_exterior_candidates=... inward_open=...
```

### 9.5 Per-pass actions with timings

```text
[pass.repair] n=0
cleanup removed_deg=118 removed_dup=0 ms=...
connectivity split_nm_edges=0 split_nm_vertices=0 stitched=0 ms=...
orientation local_face_flips=1636 conflict_splits=0 ms=...
exterior closed_patch_flips=20 open_patch_flips=7 ambiguous_open=3 rays=... ms=...
fingerprint_before=...
fingerprint_after=...
```

### 9.6 Exterior decisions

For each flipped/ambiguous open patch:

```text
[patch.exterior] patch=17 faces=842 boundary_edges=23
method=raycast
plus_occlusion=0.71 minus_occlusion=0.18
confidence=0.53 action=flip
```

This is the information needed to debug inward normals. No guess remains invisible.

### 9.7 Final state

```text
[mesh.done] lod=0 id=station_core_front status=READY passes=2
output V=... T=... E=...
remaining_issues=0
ms_total=...
```

or:

```text
[mesh.done] ... status=NO_PROGRESS passes=3
remaining nonmanifold_edge=1 exterior_ambiguous=2
```

At operation end:

```text
[repair.done] ready=8 no_progress=0 failed=0 ms_total=...
```

---

## 10. User-facing PREPARE UI

Keep it brutally small.

During work:

```text
ПОДГОТОВКА МЕШЕЙ… LOD0/station_core_front · pass 2
```

Finished:

```text
ПОДГОТОВКА ЗАВЕРШЕНА · 8/8 READY · 2.8 s
```

If unresolved:

```text
ПОДГОТОВКА НЕ ЗАВЕРШЕНА · 7 READY · 1 NO PROGRESS
Подробнее: <log path>
```

No giant topology report is required for normal operation.

`АНАЛИЗИРОВАТЬ` may remain later for semantic type selection (`ClosedVolume / ThinTwoSided / BreachedVolume`) and LOD readiness, but it is not part of making the mesh usable.

---

## 11. Immediate implementation sequence

### Step 1 — fix the known visual bug first

Add open-patch exterior orientation after BFS and before normal rebuild.

Acceptance:

- station surfaces currently showing inward normal arrows become outward where exterior confidence is high;
- existing boundary holes remain holes;
- thin free plates remain renderable from both sides.

### Step 2 — add real post-pass inspection and repair log

Remove hardcoded `result.after.* = 0` claims. Measure actual state and write the structured log above.

Acceptance:

- every flipped patch is visible in the log with method and confidence;
- no report field claims zero without calculation.

### Step 3 — wrap current primitives in `MeshRepairEngine`

Convert once to `RepairMesh`, iterate repairs, rebuild `MeshLod` once at success/no-progress boundary.

Acceptance:

- second PREPARE is idempotent;
- no repeated full browser payload inside the loop;
- no unnecessary full `MeshLod` deep-copy per pass.

### Step 4 — repair non-manifold instead of aborting

Only after Steps 1–3 are stable:

- split deterministic non-manifold edge fans;
- split bow-tie/non-manifold vertex fans;
- split orientation-conflict seams where deterministic.

Acceptance based on real/synthetic meshes.

### Step 5 — only add stitching/T-junction work if station/ship still exhibit actual cracks

Do not implement speculative general mesh surgery before evidence.

---

## 12. Acceptance for this milestone

Preparation is finished only when all of these pass:

1. station LOD0 — existing surfaces have correct outward normals where exterior is meaningful;
2. station LOD1 — same, including `station_solar_panels`;
3. no visible culling holes caused by reversed existing faces;
4. actual authored openings remain open;
5. detached ThinTwoSided plate is visible from both sides;
6. UV/material/hard-normal seams survive;
7. Elite structural edges remain correct and triangulation internals can remain hidden;
8. second PREPARE makes no change;
9. detailed log explains every repair pass and every unresolved issue;
10. user-facing UI remains concise.

Only after this milestone is accepted should LOD Generator work resume.

---

## 13. External engineering references

- CGAL Polygon Mesh Repair: consistent polygon-soup orientation, duplicate cleanup, non-manifold point/edge duplication, boundary stitching. The key architectural lesson is to repair combinatorial topology before treating it as a usable polygon mesh.
- libigl: `bfs_orient()` for local consistent patches, then `reorient_facets_raycast()` for outward orientation of closed/nearly-closed surfaces using free-space/occlusion ray casting.

These references support the minimal architecture above; they do not require importing CGAL/libigl as project dependencies. We can implement only the small subset needed by our assets.
