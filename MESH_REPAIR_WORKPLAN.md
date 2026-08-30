# Elite Model Asset Editor — Mesh Repair Workplan

**Baseline:** Model Asset Editor 0.10.14 / asset format v4  
**Purpose:** internal engineering plan for finishing canonical mesh preparation.  
**Primary goal:** produce a usable working mesh. Diagnostics are only sensors for the repair loop.

## 1. Definition of success

`PREPARE MESHES` must transform each resident authored `MeshLod` into a stable working mesh suitable for subsequent authoring.

A mesh is **GOOD ENOUGH** when all of the following are true:

1. all referenced positions are finite and all triangle indices are valid;
2. no collapsed/zero-area triangles remain;
3. no exact duplicate triangles remain;
4. geometric connectivity is stable and does not merge independent coincident sheets;
5. no repairable non-manifold edge or non-manifold vertex fan remains;
6. each orientable surface patch has consistent winding;
7. closed shells are outward-oriented;
8. open / nearly-closed patches have a stable exterior-side orientation when an exterior side is meaningful; true two-sided sheets need only consistent winding;
9. normals are regenerated after final face orientation;
10. UV seams, material boundaries and legitimate hard-normal islands survive render-vertex rebuild;
11. edge topology is rebuilt from the final mesh and marked authoritative;
12. no geometrically coincident boundary crack/T-junction remains if it can be stitched without creating a new face across empty space;
13. a second repair run makes no payload change (idempotence).

Boundary loops by themselves are **not errors**. A real hole may be intentional (`BreachedVolume` / open plate) and must not be filled merely to make a mesh closed.

## 2. Current 0.10.14 behavior

The existing `CanonicalMeshBuilder v6` already does:

- finite-position / index checks;
- collapsed and zero-area triangle removal;
- exact duplicate triangle removal;
- a topology-aware positional weld heuristic;
- two-face adjacency construction;
- BFS parity winding consistency;
- signed-volume outward orientation for closed components;
- normal-island reconstruction after winding;
- render-vertex rebuild preserving UV/material/hard-normal splits;
- edge rebuild with `EdgeCanonicalTopology`.

This is useful code and should be retained.

## 3. Current architectural defects

### 3.1 PREPARE is still a one-shot builder

The current contract explicitly performs one pass and returns. It does not use detected defects as input to a subsequent repair pass.

Required change: introduce a repair controller:

```text
for pass in 0..MAX:
    issues_before = inspect(mesh)
    if good_enough(issues_before): SUCCESS

    fingerprint_before = topology/geometry fingerprint
    apply_repair_pass(mesh, issues_before)
    fingerprint_after = fingerprint(mesh)

    issues_after = inspect(mesh)
    if good_enough(issues_after): SUCCESS
    if fingerprint_after == fingerprint_before: NO_PROGRESS
```

`MAX` is only a safety guard. Termination is based primarily on issue reduction / unchanged payload.

### 3.2 Non-manifold edges are detected but not repaired

Current code stops when a canonical edge has more than two incident faces.

That is too early. Standard polygon-soup repair practice is to split/duplicate points or edges so independent manifold/orientable patches can be recovered.

Required repair:

- partition incident faces into compatible fans/patches;
- duplicate geometric endpoints for separate fans;
- rebuild adjacency;
- rerun orientation;
- only leave unresolved when no unambiguous partition is possible.

### 3.3 Non-manifold vertices are not detected

A vertex can be non-manifold even if every edge has at most two incident faces (bow-tie / multiple disconnected face fans meeting only at one point).

Required repair:

- build face fan(s) around every geometric point;
- if more than one disconnected fan exists, duplicate the point per fan;
- rebuild topology.

### 3.4 Two-use positional weld is still unsafe

For RAW payloads `buildTopologicalPointMap()` currently treats a positional edge with exactly two uses as sufficient evidence to reconnect render splits.

That can still connect two unrelated coincident sheets when exactly one face from each sheet shares the same geometric segment.

Required change:

- source adjacency is strongest evidence;
- positional coincidence alone must not establish connectivity;
- fallback stitching requires compatibility evidence (coincident endpoints + compatible local continuation/face fan and no authored evidence that they are separate);
- after first authoritative rebuild, only `EdgeCanonicalTopology` connectivity is used.

Longer-term best fix: preserve source OBJ position/edge identity explicitly in importer-side authoring metadata instead of reconstructing identity from coordinates.

### 3.5 Open components have no absolute exterior orientation

Current 0.10.14 deliberately gives open components only BFS-consistent winding. This is mathematically valid for a generic sheet, but insufficient for a `BreachedVolume` or nearly-closed station shell: the whole patch can remain consistently inward.

This is a direct reason why a prepared mesh can still display inward normals / culling holes.

Required repair strategy:

- closed component: signed volume (already implemented);
- open/two-sided sheet: consistency only is acceptable;
- open or nearly-closed solid-like patch: infer exterior by ray casting / ambient-occlusion side test (libigl `reorient_facets_raycast` style); orient the patch toward the less occluded / more empty hemisphere;
- do not fill its boundary loop.

This orientation stage is geometry repair, not UI classification. If a true thin sheet is flipped as a whole, its two-sided behavior is unchanged.

### 3.6 Coincident cracks and T-junctions are not repaired

Current weld only handles matching points/edges. It does not stitch:

- duplicated boundary edges with near-identical geometry but distinct indices;
- a boundary endpoint lying in the interior of another boundary edge (T-junction);
- collinear edge chains representing the same geometric boundary.

Required conservative repair:

- spatially index boundary segments;
- identify overlapping/coincident compatible boundaries within epsilon;
- split longer boundary edges at matching points when necessary;
- stitch only coincident geometry;
- never synthesize a face across a spatial gap.

Thus accidental cracks close, while a real authored hole remains a hole.

### 3.7 Orientation conflicts currently abort instead of becoming patch boundaries

A parity conflict means the current recovered connectivity is non-orientable or incorrectly glued.

Required repair:

- identify conflict edges;
- split connectivity along the minimum/conflicting seam;
- duplicate geometric endpoints as needed;
- orient resulting patches independently;
- rerun.

Only if splitting cannot produce stable orientable patches should this remain unresolved.

### 3.8 Self-intersection is currently invisible

The current audit does not detect triangle-triangle self-intersection.

Policy for first complete repair version:

- detect self-intersections within one recovered topological component;
- do **not** treat intersections between intentionally independent components as automatically invalid;
- initially report unresolved self-intersection as a remaining blocker rather than attempt destructive boolean/remeshing automatically;
- add automatic remeshing only if real production assets prove it necessary.

### 3.9 Normal correctness is downstream, not an independent repair target

Authored normals are not authoritative.

Policy:

- never try to repair normals before topology/winding is stable;
- regenerate them after final face orientation;
- preserve render splits via UV/material/smoothing/hard-edge policy.

## 4. Internal issue set

The repair engine should inspect and track machine-readable issues, not produce prose during each pass.

Suggested structure:

```cpp
enum class MeshIssueKind {
    InvalidIndex,
    NonFinitePosition,
    DegenerateTriangle,
    DuplicateTriangle,
    NonManifoldEdge,
    NonManifoldVertex,
    OrientationConflict,
    InconsistentWinding,
    InwardClosedShell,
    ExteriorAmbiguousOpenPatch,
    StitchableBoundaryCrack,
    TJunction,
    SelfIntersection
};
```

Each issue should carry involved face/edge/point ids so repair code can act on it directly.

## 5. Repair pass order

Order matters because earlier repairs change topology seen by later ones.

```text
A. INPUT SANITIZE
   invalid/non-finite references -> remove only irreparable affected primitives where safe;
   abort mesh only if no usable surface can remain

B. CLEANUP
   collapsed / zero-area / exact duplicate faces
   remove orphan render vertices

C. RECOVER CONNECTIVITY
   source adjacency
   conservative coincident-boundary stitching
   T-junction edge splitting

D. MANIFOLDIZE
   split non-manifold edges into face fans
   split non-manifold vertices into vertex fans

E. ORIENTABLE PATCHES
   BFS parity
   split conflict seams if needed

F. ABSOLUTE ORIENTATION
   closed -> signed volume outward
   open/nearly closed -> raycast/AO exterior test
   generic thin sheet -> consistent orientation is sufficient

G. REBUILD OUTPUT
   normals
   render vertices
   bounds
   authoritative edges

H. RE-INSPECT
   GOOD ENOUGH -> stop
   payload/issue vector improved -> next pass
   unchanged -> NO PROGRESS
```

## 6. Good-enough vs unresolved

The repair loop must not demand CAD perfection where the game/editor contract does not need it.

### Allowed after repair

- boundary loops;
- multiple disconnected components;
- thin two-sided sheets;
- intentional breached openings;
- intersections between clearly independent components, unless a later PHYSICS contract forbids them.

### Must be gone or explicitly unresolved

- bad indices / non-finite referenced positions;
- degenerate/duplicate faces;
- accidental stitchable cracks/T-junctions;
- non-manifold edge/vertex connectivity after all safe splitting attempts;
- winding conflicts;
- inward orientation where an exterior side can be determined;
- self-intersection inside a component if it makes a solid contract unusable.

## 7. UI contract

`PREPARE MESHES` should show only final operational status, e.g.:

```text
station_core_front: READY (3 repair passes)
station_habitat_s1: READY (2 repair passes)
station_solar_panels: READY (2 repair passes)
```

or:

```text
station_x: NO PROGRESS — 1 unresolved self-intersection
```

Detailed per-pass issue data belongs in an optional developer log / this engineering report, not in the normal author workflow.

`ANALYZE` remains useful later for assigning `ClosedVolume / ThinTwoSided / BreachedVolume`, but it must not be the mechanism that makes the mesh usable.

## 8. Acceptance tests before declaring canonical preparation finished

Synthetic regression meshes:

1. clean closed cube -> unchanged and outward;
2. cube with random reversed faces -> repaired;
3. fully inside-out cube -> flipped outward;
4. cube with one deleted face -> opening preserved, remaining patch consistently outward;
5. thin plate -> consistent, two-sided-compatible;
6. duplicate faces (same/opposite winding) -> one face retained;
7. UV seam/hard-normal/material seam -> geometric point shared, render vertices preserved as required;
8. two independent coincident sheets -> remain independent;
9. non-manifold edge with separable face fans -> split into manifold patches;
10. bow-tie non-manifold vertex -> split per fan;
11. T-junction -> edge split/stitch, no crack;
12. parity-conflict connectivity -> conflict seam split and patches oriented;
13. true hole -> not capped;
14. self-intersecting component -> detected and terminates NO PROGRESS until a dedicated repair exists;
15. second PREPARE -> byte/fingerprint-stable.

Production acceptance:

- station LOD0;
- station LOD1, specifically `station_solar_panels`;
- detached plate test: visible from both sides;
- breached module: actual opening remains, internal equipment visible;
- no culling holes caused by reversed winding on surfaces that actually exist.

## 9. Immediate implementation work

The next patch should **not extend the Preflight report**. It should:

1. add `MeshRepairEngine` / repair-loop controller around the useful primitives already in `CanonicalMeshBuilder`;
2. convert non-manifold edge and orientation-conflict conditions from immediate failure into repairable issue types;
3. add non-manifold vertex fan splitting;
4. replace unsafe two-use positional auto-weld with conservative connectivity recovery;
5. add coincident boundary/T-junction stitching;
6. add exterior orientation for open/nearly-closed patches using a raycast/AO-style heuristic;
7. rerun inspection and repair until GOOD ENOUGH or NO PROGRESS;
8. rebuild normals/render topology once final topology/orientation is stable;
9. keep user-facing output minimal;
10. update `MODEL_ASSET_EDITOR_GUIDE.md` to make repair-loop, not Preflight, the canonical preparation contract.

## 10. External reference model

The plan follows established geometry-processing patterns rather than inventing a special station-only rule:

- **CGAL Polygon Mesh Repair / Polygon Mesh Processing:** repair polygon soup, merge duplicate points/polygons, orient polygon soup, split/duplicate non-manifold points/edges, stitch boundaries, detect/repair degeneracies.
- **libigl:** `bfs_orient()` for consistent orientable patches; `reorient_facets_raycast()` for outward orientation of closed or nearly-closed surfaces based on ray casting / ambient occlusion.
- **Assimp:** import post-processing separates invalid-data cleanup, identical-vertex joining, normal generation and heuristic infacing-normal correction rather than treating diagnostics as a final product.

These references reinforce the same architecture: detect a concrete defect, apply the matching repair, rebuild/reinspect, and stop only at a stable usable mesh or an explicitly unresolved defect.
