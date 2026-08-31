## 0.10.24 — explicit heavy-operation boundary / lazy LOD data-plane

- UI contract follow-up: SOURCE now describes the actual lazy model instead of the obsolete “complete authoring set” behavior. Its inventory shows declared geometry/render-node counts plus checkpoint/production payload provenance, treats `METADATA` as a normal unloaded state, and exposes an explicit per-LOD `OPEN` viewport action. The storage panel now separates production-package file sizes from the workspace's actual per-LOD payload source, so a checkpoint-backed LOD is no longer displayed as if it were simply missing because production has no corresponding `.elmesh`.
- Selecting/restoring an asset document now drops any browser-resident geometry cache before metadata merge, so a same-asset checkpoint restore cannot silently retain the previous `.elmesh` payload. Collision/socket overlays are also suppressed while no LOD payload is resident, preventing a metadata-only open from drawing a misleading “ghost model”.
- Wizard tabs are now passive navigation only. `SOURCE`/`LODS`/`GEOMETRY`/`SURFACES` stage entry does not load an `.elmesh`, run topology analysis, reset viewport state or call `rebuildScene()`. Heavy work belongs to explicit commands.
- Opening a v4 production asset or restoring/resuming a v4 wizard checkpoint loads only the semantic manifest and wizard metadata. Every LOD remains unloaded until an explicit viewport/heavy-tool request needs it.
- Lazy checkpoint resume records per-LOD payload provenance. An unloaded LOD may come from the checkpoint package while another falls back to production; `Save all` can promote an unopened checkpoint `.elmesh` into production by file copy without decoding the mesh.
- Surface intent is metadata-only. `ClosedVolume`, `BreachedVolume`, `ThinOneSided` and `ThinTwoSided` update the whole geometry contract without triangle scans or automatic topology analysis; cross-LOD propagation uses stable authoring identity and does not eagerly load sibling LODs. `Save all` persists pending surface metadata for an unopened LOD with a surface-only v4 cursor rewrite that skips mesh arrays.
- Web synchronization now has a strict control/data split: authored state and diagnostics use JSON, while explicit viewport LOD payloads use binary WebSocket wire format `ELVPD001`. Vertex/normal/index/material/edge arrays are no longer serialized as JSON. Metadata refreshes do not call `rebuildScene()`.
- Added derived-geometry caches for vertex/triangle/edge counts, bounds/storage estimates, material usage/unassigned counts and topology audit. Mesh/material-assignment mutations invalidate only the corresponding cache.
- v4 `.elmesh` loading now reads the file into one memory buffer and decodes it through a bounds-checked memory cursor. The on-disk v4 layout and semantics are unchanged.
- CanonicalMeshBuilder/libigl/Embree, GeometryInstanceFitter, LOD generation algorithms, stable identities and asset format v4 remain unchanged.

## 0.10.23a — editor-only build deployment hotfix

- Keep shared `copy_assets` rules game/server-owned. An editor-only configure uses `build/tools/model_asset_editor` as `CMAKE_BINARY_DIR`, which is also the editor artifact root; defining the generic copy tree there collided with `copy_model_asset_editor_webui` on the same `model_asset_editor.html` output.
- The editor now has exactly one asset deployment authority in editor-only builds: `copy_model_asset_editor_webui` plus `build_model_asset_editor_ui_pack`.

## 0.10.23 — executable-owned UI package isolation

- Split the universal WebUI resource artifact into `elite_game_ui.pak` and `model_asset_editor_ui.pak`. The game pack explicitly excludes the editor document; the editor pack explicitly includes only its document plus the Three.js modules it imports.
- `HtmlUiServer` no longer guesses or auto-loads `assets/ui/elite_ui.pak`. Every executable explicitly supplies the pack it owns; an empty pack path is filesystem-only serving.
- Model Asset Editor now runs from `build/tools/model_asset_editor` as its actual runtime root, with its own `assets/webui` fallback and `assets/ui/model_asset_editor_ui.pak`; it no longer depends on the shared/game `copy_assets` tree.
- Building either executable removes the obsolete universal `elite_ui.pak` from the relevant runtime tree during migration, preventing a stale packed HTML document from shadowing current source/fallback files.
- Added include/exclude filters to `build_ui_pack.py` and regression contracts that prevent game/editor WebUI package ownership from merging again.
- Unified manual tab changes and automatic checkpoint progression through one wizard stage-entry transaction. GEOMETRY no longer unconditionally rebuilds the complete scene on every tab entry; an active LOD-generator preview is cleared without an intermediate rebuild and at most one rebuild is performed when the viewport representation contract actually changes.

## 0.10.22a — SURFACES manual-entry hotfix

- Entering SURFACES no longer rebuilds the full station scene while the surface audit is still gated. Both manual tab entry and the automatic GEOMETRY→SURFACES wizard transition stay lightweight until the author explicitly presses ANALYZE SURFACES.
- The successful explicit surface-analysis response now performs the one required SURFACES scene rebuild, so material groups/sidedness preview are prepared only after the requested audit completes.

## 0.10.22 — explicit SURFACES analysis / cross-LOD surface intent

- SURFACES no longer runs topology analysis merely because the tab was opened. An explicit `ANALYZE SURFACES` action gates the workspace; reopening the tab reuses the existing result.
- Added a default-on `apply to the same geometry in all LODs` authoring option. Matching uses stable base-visual or replacement-variant family identity, never transient per-LOD `G#`/geometry indices.
- Cross-LOD surface propagation batches all matching LOD edits, writes/publishes once and runs the expensive model preflight once after the batch.
- Clarified surface-intent labels around actual render behavior while retaining `ThinOneSided` for cockpit cards, labels and other intentionally one-sided sheets. Geometry surface intent is now the ordinary sidedness authority: Closed/Breached/ThinOneSided => FrontSide/culling ON; ThinTwoSided => DoubleSide/culling OFF. The legacy material `twoSided` field is no longer edited or used by the SURFACES preview.

## 0.10.21 — SURFACES authoring workspace

- Implemented SURFACES as the fourth real wizard stage with its own validation, dirty state and checkpoint; the next unimplemented stage is SEMANTICS.
- Moved final render-surface intent to the stage that owns it. Used/replacement open geometry must explicitly resolve to `ClosedVolume`, `ThinOneSided`, `ThinTwoSided` or `BreachedVolume` before the SURFACES checkpoint; these edits invalidate SURFACES+ only and no longer reopen LODS/GEOMETRY.
- Added a per-LOD surface workspace with geometry selection, complete-LOD/single-geometry preview, compact material-slot usage and explicit unassigned-triangle diagnostics.
- Added conservative missing-material repair: a chosen existing material may be assigned only to triangles whose material index is currently unassigned; existing mixed assignments are preserved.
- Added MaterialDefinition authoring for stable id, base RGBA, emissive color/strength, metallic, roughness, two-sided state and base/emissive texture references. Texture import/baking and UV painting remain outside this stage.
- SURFACES viewport now renders the actual triangle material groups with base/emissive/PBR/two-sided properties instead of the generic diagnostic material, including a visible fallback for invalid/unassigned material references.
- SURFACES checkpoint validates surface intent, SurfaceMode consistency, per-triangle material indices and finite/ranged material properties. Binary regression coverage now protects the expanded material fields through `.elmodel` save/load.

## 0.10.20 — restored GEOMETRY authoring workspace

- Restored GEOMETRY as a full authoring workspace instead of presenting rigid duplicate comparison as the entire stage. The active Render LOD selector is now the first control because every LOD owns an independent geometry/render graph.
- Entering GEOMETRY resets LOD-stage diagnostic state: generated-preview isolation, additional-mesh preview, semantic hide/isolate state, diagnostic wireframe/normals and viewport mode. Automatic LODS → GEOMETRY transition uses the same reset and starts on complete authored LOD0.
- Added a first-class per-LOD geometry browser split into MAIN MESHES and ADDITIONAL / REPLACEMENT MESHES. Clicking a main geometry isolates every RenderNode using it while preserving transform hierarchy; clicking an additional mesh shows it standalone for inspection; SHOW COMPLETE LOD restores the complete authored view.
- Restored instance authoring directly inside the GEOMETRY workflow: duplicate instance, break shared instance and radial instance array are visible as a dedicated vertical action block for the selected main render element. Existing backend commands/data contracts are reused.
- Kept rigid-fit comparison/consolidation as one GEOMETRY tool rather than the stage itself. Comparison, consolidation, replacement and cleanup actions are arranged vertically.
- Restored the additional-mesh replacement master/detail table in the same workflow: select an additional mesh, declare compatible base visual families and preview the replacement on a representative instance before DAMAGE semantics decide when to use it.
- LOD switching from GEOMETRY loads/request-publishes the selected LOD payload on demand, resets transient geometry preview state and never propagates G-index/instance edits across LODs.

## 0.10.19 — LOD apply feedback / on-demand viewport payloads

- Fixed individual main-mesh inspection in the LOD generator. Transform parent groups remain alive while the mesh filter controls only mesh visibility, so selecting a child mesh no longer hides it together with its parent transform hierarchy.
- APPLY now enters an explicit working/progress state immediately, disables duplicate execution while generation is in flight, and reports each generated LOD level as it is built.
- Generated LOD APPLY no longer broadcasts every geometry payload for every resident LOD in one giant JSON message. It publishes compact authored metadata immediately and loads one LOD geometry payload on demand only when that LOD is opened in the viewport.
- Metadata invalidates stale browser geometry caches for replaced/generated LOD slots. The Render LOD list therefore updates immediately to show generated provenance/dirty authored state instead of continuing to display the pre-APPLY LOD list.
- The LOD panel keeps explicit current-session `APPLIED` state and a visible `AUTHORED LOD UPDATED ... · LODS checkpoint required` marker. The APPLY button stays disabled until a new analysis or newly selected unapplied LOD makes repetition meaningful.
- LOD generator actions are laid out vertically. ANALYZE LOD0, repeated identical preview actions and APPLY also disable while their result is current/in-flight.
- APPLY invalidates the LODS wizard checkpoint before compact metadata publication, so `COMPLETE STAGE + CHECKPOINT` becomes active immediately after a successful authored LOD change.

## 0.10.18 — full-asset generated LOD authoring

- LOD0 analysis and component-cull preview now include the complete resident geometry pool: ordinary/main meshes and additional/replacement meshes, even when the latter have no RenderNode usage.
- The LOD Generator can isolate any LOD0 mesh for inspection. Additional/replacement meshes are shown as standalone geometry while the same LOD cull preview is applied to them.
- Every generated level is built independently from canonical LOD0 and carries the complete geometry pool plus the copied LOD0 render-node graph. Generated documents are marked `sourceKind=generated`, `generatedFromLod=0`.
- Added explicit per-level `USE` selection and `APPLY SELECTED LODS`. Existing selected LOD slots are replaced; missing contiguous slots are created. LOD0 is never modified. All generated levels are selected by default after analysis.
- APPLY is transactional: every candidate level is built and checked against the canonical geometry contract before any authored RenderLod is replaced.
- Stable base-visual ids, additional/replacement mesh ids, replacement compatibility and explicit topology classes are propagated from LOD0 into generated LOD documents.
- Generated LOD preparation fingerprints are accepted by the LODS technical gate and persisted in wizard authoring state. Completing the LODS stage writes the full selected LOD set into the checkpoint for GEOMETRY.

## 0.10.17 — LOD gate split / stable editor artifact layout

- LOD0 analysis and LODS checkpoint validation now depend only on technical canonical geometry: current PREPARE records, usable topology, no degenerate/duplicate faces, no winding conflicts and no inward closed shells. `ClosedVolume / ThinTwoSided / BreachedVolume` classification is a later SURFACES authoring concern and no longer blocks LOD analysis or saving the LODS checkpoint.
- Model Preflight separates technical LOD blockers from surface-authoring review. Prepared open geometry that still needs an explicit surface class is shown as review/advisory instead of a red LOD blocker.
- `ANALYZE LOD0` is no longer disabled by stale/missing Preflight classification state. The backend remains authoritative and rejects the command only when canonical LOD0 preparation itself is missing or invalid.
- Editor artifacts have one stable root: `build/tools/model_asset_editor/`. Executables go to `bin/`, per-asset checkpoints/state/logs go to `workspaces/<asset>/`, and spike output defaults to `diagnostics/libigl/`.
- Mesh preparation log is now `workspaces/<asset>/logs/mesh_repair.log` and is truncated at the start of every PREPARE operation, so one file always describes one preparation run instead of accumulating historical algorithms. Instance-fit diagnostics are asset-local as `workspaces/<asset>/logs/instance_fit.log`.
- The diagnostic spike accepts MSYS2 UTF-8 argv paths with `std::filesystem::u8path` and no longer defaults to a CWD-relative `build/libigl_spike` directory.

## 0.10.16 — production libigl + Embree canonical preparation

- `ПОДГОТОВИТЬ МЕШИ` now uses libigl `split_nonmanifold` plus Embree `reorient_facets_raycast` as the production topology/orientation authority; the v7 radial-envelope/open-component orientation heuristic is removed.
- Positional `1e-4` equality remains only a cleanup/weld candidate. Existing topology-aware source-edge adjacency is preserved so independent coincident/touching sheets are not merged back into false non-manifold geometry.
- Degenerate/collapsed and duplicate triangles are removed before libigl repair. `split_nonmanifold` may duplicate topology vertices but never creates faces, so authored holes/breaches remain open.
- After orientation, editor-owned code rebuilds normals, hard-normal islands, UV/material-aware render vertices, canonical edges and bounds. libigl/Embree never enter runtime asset metadata or game targets.
- Added viewport modes `ИСХОДНИК` (resident pre-PREPARE RAW snapshot, DoubleSide), `БЕЗ ОТСЕЧЕНИЯ` (prepared mesh, DoubleSide) and `РАБОЧИЙ` (prepared mesh, FrontSide). RAW snapshots are session-only and are never serialized into `.elmodel`/`.elmesh`.
- Updated `build/logs/model_asset_mesh_repair.log` and wizard schema 6 with split topology vertex, raycast patch and raycast-flip counters.
- libigl v2.6.0 + Embree are fetched only when the offline Model Asset Editor or diagnostic spike target is built.

## 0.10.15 — minimal macro-patch mesh repair loop

- `ПОДГОТОВИТЬ МЕШИ` ориентирует открытые/пробитые оболочки, а не только closed volumes. Манifold-connected component рассматривается как большой parent patch ("лапоть"): boundary loops/дырки не мешают определить общее направление, локальные faces сначала выравниваются BFS, затем весь parent patch при необходимости переворачивается целиком.
- Для open parent patch используется дешёвый area-weighted radial envelope score относительно его собственного AABB center. Высокоуверенный inward patch переворачивается; почти плоский/двусторонний patch считается ambiguous и не угадывается разрушительно. Никакого raycast/remesh/Blender-подобного repair.
- Подготовка выполняет bounded repair loop до `GOOD_ENOUGH` или `NO_PROGRESS`; after-state измеряется по фактическому candidate, а не заполняется нулями.
- После первого rebuild выполняется только дешёвая стабилизация canonical point/render projection, если rebuilt authoritative edges уточнили connectivity. Это делает повторный PREPARE byte-idempotent на station LOD0/LOD1.
- Добавлен подробный developer log `build/logs/model_asset_mesh_repair.log`: по каждому mesh — input, repair pass, open-component orientation score/confidence/action, output и точная причина отказа. UI получает только короткий итог.
- Новый preparation record id: `canonical_mesh_builder_v7`; records v6 считаются stale и требуют одного явного PREPARE.

## 0.10.14 — topology-aware canonical winding / outward shells

- `ПОДГОТОВИТЬ МЕШИ` снова является именно authoring canonicalization, но остаётся одним проходом без fixed-point/скрытого ANALYZE.
- Новый preparation record id: `canonical_mesh_builder_v6`; `runtime_mesh_normalizer_v1` и старые canonical records считаются stale и требуют одного явного PREPARE.
- Runtime `RuntimeMeshNormalizer` оставлен отдельным tolerant render contract игры; editor больше не выдаёт его глобальный positional weld за canonical topology.
- positional `1e-4` используется как кандидат на geometric weld; независимые coincident/touching sheets не объединяются, а rebuilt edge adjacency помечается `EdgeCanonicalTopology` и становится авторитетной для последующих анализов.
- adjacency-BFS исправляет локально перевёрнутые triangles; замкнутые компоненты ориентируются наружу по signed volume, вычисленному уже на canonical snapped positions.
- normals и render vertices перестраиваются только после финального winding; UV/material/hard-normal islands сохраняются.
- open boundaries не закрываются: ThinTwoSided/Breached geometry сохраняет реальные отверстия.
- station LOD0/LOD1 acceptance проходит: после preparation `windingFlipsRequired=0`, `insideOutClosedComponents=0`, включая `LOD1/station_solar_panels`.

## 0.10.13 — shared runtime mesh normalization / analysis split

- Added `src/model_asset/RuntimeMeshNormalizer.*` as the single positional-weld/triangle-cleanup implementation shared by the game `ObjLoader` and Model Asset Editor. Both now use the same `1e-4` weld, degenerate cleanup, exact duplicate cleanup and triangle remap contract.
- Simplified `ПОДГОТОВИТЬ МЕШИ`: it now performs only runtime-equivalent normalization, normal reconstruction, render-vertex rebuild preserving UV/material splits, bounds and edge rebuild. It does not solve winding, classify topology, run fixed-point passes or reject renderable open/non-manifold meshes.
- Removed the expensive implicit topology audit/classification from the preparation command. `АНАЛИЗИРОВАТЬ` is now the only operation that runs `ClosedVolume / ThinTwoSided / BreachedVolume / Invalid` analysis.
- Removed the multi-pass canonical stabilization loop and the deep copy of the complete input `MeshLod`. Preparation builds one transactional output mesh and swaps it into `geometry.mesh` only on success.
- Genuine non-manifold or inside-out topology is now an analysis result, not a normalization failure. Broken indices, non-finite positions or a mesh with no usable triangles remain normalization failures.
- Bumped preparation records from `canonical_mesh_builder_v5` to shared `runtime_mesh_normalizer_v1`, so old records are intentionally stale and one explicit preparation is required. Asset format remains v4; wizard state remains schema 5.

## 0.10.12 — explicit mesh preparation / non-blocking load

- Reverted the over-eager 0.10.9–0.10.11 SOURCE-boundary policy: asset load, source reimport, manual LOD load/reload and checkpoint restore now publish exactly the geometry that was read. They do not canonicalize, validate or hide meshes during I/O.
- Restored one explicit `PREPARE MESHES` action in the LOD Preflight block. Only this command runs `CanonicalMeshBuilder` and replaces resident working `MeshLod` payloads. `ANALYZE` remains read-only.
- `sendAsset()` no longer refuses RAW/stale geometry. A failed canonical build leaves that mesh unchanged and visible, reports `MESH PREPARATION INCOMPLETE`, and allows inspection instead of turning a geometry problem into a load/server failure.
- SOURCE stage completion validates only source availability/basic asset serialization. Canonical fingerprint/classification gates move to LODS completion and LOD Generator, eliminating the SOURCE→LODS deadlock for intentionally raw restored/reimported meshes.
- Additional OBJ refresh is again a literal source reload: fresh OBJ data becomes resident RAW geometry and invalidates previous preparation/classification evidence for that variant.
- Preflight can audit mixed RAW/canonical working sets. RAW rows are reported as `PREPARE` rather than prematurely labelled `Invalid`; structural `Invalid` becomes authoritative only after a current canonical preparation record exists.

## 0.10.11 — topology-aware canonical weld fixes false station non-manifold

- Fixed the station blocker exposed by real SOURCE acceptance: `1e-4` positional coincidence is now a weld candidate, not unconditional topology identity. Independent panels/shells that touch on the same coordinates stay separate geometric sheets instead of becoming artificial 3/4-face non-manifold edges.
- Canonical cleanup now removes collapsed/duplicate triangles first, then builds topological geometric points from ordinary two-face positional adjacency plus explicit source-edge adjacency preserved by the importer. A real canonical edge that still has more than two faces remains `Invalid`.
- Edge metadata transfer is keyed by canonical topology edge rather than raw position pair, so one authored edge can no longer contaminate another coincident-but-independent edge with flags/render masks.
- `NativeObjImporter` now computes `EdgeNonManifold` only from real OBJ polygon perimeter edges. Fan-triangulation diagonals no longer manufacture source non-manifold evidence.
- Legacy `.elmesh`/checkpoint payloads with incomplete old edge adjacency are driven to a stable canonical fixed point before their fingerprint is recorded, so the first published payload is already idempotent.
- Bumped the builder record to `canonical_mesh_builder_v5` and added regressions for coincident independent sheets, fan-diagonal false positives, genuine shared-edge non-manifold geometry, and canonical idempotency.

## 0.10.10 — RAW topology can no longer bypass canonicalization

- Fixed the SOURCE-boundary bug in 0.10.9: decoded `EdgeNonManifold` evidence is no longer treated as a blocker before `CanonicalMeshBuilder` runs. RAW authored flags, winding, normals, duplicate faces and collapsed faces are repair inputs, not preflight gates.
- Bumped the builder contract to `canonical_mesh_builder_v4`. A source non-manifold marker is now rejected only when the same positional edge is still used by more than two cleaned triangles after degenerate/duplicate cleanup. Stale/source flags that disappear after cleanup are accepted.
- `MeshLod.edges` rebuild no longer inherits `EdgeNonManifold` from authored edges. The final canonical edge topology is authoritative; old authored render masks and `EdgeAuthored` metadata may still be preserved where endpoints match.
- `canonicalizeLoadedWorkingSet()` now hard-fails the SOURCE boundary when any resident geometry cannot be canonicalized. It never keeps a failed RAW mesh as a usable editor payload. Additional OBJ files are canonicalized while still temporary and are rejected before insertion when the builder fails.
- `verifyLoadedWorkingSetCanonical()` no longer exempts structural failures from fingerprint records. Every resident geometry must carry a current canonical record and satisfy post-build cleanup/orientation invariants.
- `sendAsset()` now has a final invariant guard: reconnects, catalog requests or future call-site mistakes cannot serialize RAW/stale geometry into the browser viewport. Basis conversion and interactive source refresh explicitly re-cross the SOURCE boundary before full geometry publication.
- Preparation records retain pre-cleanup source non-manifold evidence only as diagnostics. Preflight reports it separately from the canonical topology; it is not an `Invalid` reason by itself.
- Added behavioral regressions proving that a stale `EdgeNonManifold` flag is cleaned rather than inherited, while a real three-face source edge that survives cleanup is still rejected. Asset format remains v4; wizard state remains schema 5.

## 0.10.9 — canonical SOURCE boundary / classification-only Preflight

- Canonicalization is now an automatic invariant of SOURCE load/import. Before any asset payload is sent to the browser or consumed by later wizard stages, every resident mesh is migrated through `CanonicalMeshBuilder` unless its current fingerprint already proves the same canonical payload.
- Bumped the builder contract to `canonical_mesh_builder_v3`: the working render mesh now snaps every welded geometric point to one representative position and rebuilds GPU vertices from `geometric point + UV + material + reconstructed normal island`. Source OBJ vertex/normal identity is no longer preserved accidentally, so authored-normal-only duplicates collapse while UV/material/hard-normal splits remain. Existing v2 records are deliberately stale and migrate once on 0.10.9 load.
- Existing `.elmesh` packages and wizard checkpoints from older editor versions cross the same one-time migration boundary on load; structural changes mark the affected LOD dirty and invalidate stale wizard work instead of exposing authored/raw topology.
- Additional OBJ meshes discovered by SOURCE/GEOMETRY refresh are canonicalized before the refreshed geometry payload is broadcast. Manual LOD load/reload and checkpoint restore use the same boundary.
- Removed the user-facing `ПОДГОТОВИТЬ МЕШИ` command and the browser-side `КАК В ИГРЕ` fake normalization preview. The viewport now always renders the actual canonical working mesh; Preflight only audits/classifies `ClosedVolume`, `ThinTwoSided`, `BreachedVolume` or genuine `Invalid`.
- Preflight and LOD generation no longer invoke canonicalization at all. They only verify the SOURCE fingerprint invariant and block if an impossible stale working mesh is detected. Ambiguous open geometry remains the only normal user decision; canonical cleanup itself is no longer a user action.
- Asset format remains v4. `wizard_state.json` schema 5 preparation records are retained as migration/fingerprint evidence, not as an opt-in preparation workflow.

## 0.10.8 — canonical mesh builder / explicit preparation contract

- Replaced the transitional Preflight repair path with a dedicated `CanonicalMeshBuilder`. Positional weld at `1e-4` now defines geometric points while render vertices remain independently split by UV/material identity and by reconstructed hard-normal islands.
- Canonical preparation now removes both collapsed and duplicate geometric triangles, solves orientable winding, turns closed inside-out shells outward, rebuilds normals from polygon/smoothing/25° crease continuity, and completely rebuilds final `MeshLod.edges` against the rewritten triangle indices. Authored breach boundaries are never capped.
- True source non-manifold edges, invalid triangle indices, non-finite positions and non-orientable topology are classified `Invalid`; canonical multi-use caused only by the positional weld remains a warning so touching/coincident authored shells are not rejected by accident.
- Preparation is now explicit editor state (`wizard_state.json` schema 5): every prepared geometry stores the builder algorithm id, before/after counts, cleanup statistics and a fingerprint of its canonical render payload. Any later mesh mutation invalidates the record automatically by fingerprint mismatch.
- Model Preflight now reports source render vertices → geometric points → canonical render vertices/triangles, separates default and additional meshes, and enables `Prepare meshes` whenever a loaded mesh lacks a current preparation record. Closed volumes and high-confidence thin sheets resolve automatically; ambiguous open geometry is classified once by the author.
- Added behavioral model-asset tests for a closed cube, a two-sided plate, an authored breached cube, degenerate/duplicate cleanup with UV seams, hard-normal render splitting and genuine `Invalid` topology. LOD generation remains downstream and deliberately unchanged.

## 0.10.7 — runtime-equivalent canonical mesh preparation

- Model Preflight is now a preparation stage instead of an author-topology purity checker. The canonical geometric identity follows the proven game OBJ-loader policy: positional weld at `1e-4`.
- Preparation preserves UV/material render-vertex splits, but treats coincident positions as one geometric point for topology, winding and normal generation. This gives game-parity geometry without destroying future texture seams.
- Applying preparation removes triangles collapsed by the weld, normalizes winding where adjacency is unambiguous, flips closed inside-out shells, and recomputes normals on welded geometric points before copying them back to render-vertex splits.
- Canonical multi-use edges are diagnostic warnings rather than automatic manual-repair blockers. Open/mixed geometry instead requires an explicit final target class: `ClosedVolume`, `ThinTwoSided` or `BreachedVolume`.
- The LOD Generator remains downstream and unchanged; it is unlocked only after LOD0 has been prepared and its target geometry classes are resolved.

## 0.10.6 — author topology / runtime parity preflight

- Model Preflight no longer treats coincident positions as true author adjacency. It now audits the source-edge topology preserved by `NativeObjImporter`, so UV/normal seams remain connected while separate touching panels are not fused into one non-manifold object.
- `NativeObjImporter` now preserves a real source non-manifold marker when one source edge is used by more than two triangles. This uses a new generic edge flag and does not change the binary layout.
- Preflight separately reports the legacy game-loader normalization footprint: source vertices → vertices after the runtime 1e-4 positional weld. Runtime weld is diagnostic evidence, not asset topology.
- Added a read-only `As in game` viewport mode that emulates the current runtime OBJ path locally: 1e-4 positional weld, recomputed vertex normals and two-sided rendering. `Author view` returns to the asset surface modes without mutating geometry or checkpoints.
- Preflight rows are visually grouped by LOD and split with a double separator into default and additional meshes, so variants are no longer mixed into the base-model list.
- Orientation repair now follows author source-edge adjacency instead of positional-weld adjacency, preventing safe-fix from propagating winding across merely coincident independent pieces.

## 0.10.5 — SOURCE owns complete authoring set / actionable preflight / Russian UI sweep

- SOURCE now materializes the complete authoring working set before later wizard stages: every declared render LOD is loaded into memory and every additional OBJ discovered recursively below those LOD source trees is imported with its existing stable authoring id. Initial discovery belongs to SOURCE; later LOD/GEOMETRY stages consume that snapshot instead of silently discovering missing meshes.
- The SOURCE stage now shows a per-LOD inventory (loaded state, base meshes, additional meshes and source-backed geometry). `Refresh source set` rescans only additional OBJ files without rebuilding the base assembly; full source reimport still deliberately discards in-memory edits.
- Model Preflight still audits every loaded geometry, including additional meshes and pre-existing lower LODs, but its output is now actionable: each row says READY, AUTO FIX, SET TYPE, FIX → TYPE or FIX MESH, and the summary separates automatic repairs, explicit open-surface classification and manual topology repair. LOD0 generator readiness remains a separate gate so an unwanted bad lower LOD may later be replaced instead of blocking LOD0 analysis.
- Expanded Russian localization across SOURCE/Preflight, asset metadata, common geometry labels and frequent editor status messages. Raw English preflight reason text is no longer shown in the Russian UI.

## 0.10.4 — model preflight / topology intent / safe normals repair

- Added a required Model Preflight block before the optional LOD Generator. It audits every loaded render geometry by connected component for open boundaries, non-manifold edges, degenerate triangles, winding conflicts, closed components that are globally inside-out, and authored normals that point against triangle winding.
- Open geometry is no longer treated as one ambiguous category. Preflight distinguishes `ClosedVolume`, `ThinTwoSided`, `BreachedVolume`, `Mixed` and `Invalid`; open meshes receive a conservative suggestion but require an explicit authoring choice before used LOD0 geometry may enter the generator.
- Explicit topology intent is persisted as editor authoring metadata in `wizard_state.json` schema 4 and is independent of OBJ filenames. `ThinTwoSided` maps to the existing two-sided surface render mode; closed and intentionally breached volumes remain front-sided shells.
- `Safe Auto Fix` only touches unambiguous orientation data: it propagates consistent winding across manifold adjacency, flips an entire closed component when signed volume proves it is inside-out, and recomputes normals after an orientation repair. It never fills holes, joins surfaces or guesses whether an open mesh is a plate or a breach.
- The LOD Generator backend is now gated by preflight readiness for used non-variant LOD0 geometry. Existing component-cull and coplanar previews remain otherwise unchanged.

## 0.10.3 — diagnostic LOD preview / coplanar region collapse

- LOD comparison switches to an opaque depth-writing diagnostic material after analysis, removing the translucent "x-ray" ambiguity that made cull results hard to judge. Optional wireframe and sampled face-normal overlays are available directly in the LOD Generator block.
- Added a second, independent `Coplanar Preview` pass. It only merges topologically connected triangle regions that share a plane and compatible material/smoothing/UV/normal boundaries; holes, non-manifold boundaries, multiple loops and regions that do not reduce triangle count are skipped.
- Coplanar preview is non-destructive. The backend sends compressed removed-triangle ranges plus only the small replacement boundary triangulations; the browser reuses resident vertex/normal/UV payloads instead of retransmitting meshes.
- LOD rows now show the practical rendered-triangle budget as `LOD0 → remaining`, including the explicit LOD0 baseline. The existing disconnected-component Cull algorithm is unchanged.

## 0.10.2 — explicit LOD0 comparison row

- Added LOD0 as an explicit first row in the optional LOD Generator table so the untouched authored model is available beside every cull-preview level.
- Selecting LOD0 and pressing `Show LOD0` clears any active cull preview and restores the authored LOD0 locally. This is presentation-only and does not change asset data, checkpoints or the analyzer/cull algorithm.
- LOD analysis now selects LOD0 by default after every analysis. This patch is based directly on the retained 0.10.0 disconnected-component cull; the rejected 0.10.1 planar-guard experiment is not included.

## 0.10.0 — optional LOD analysis / disconnected-detail preview

- Added an optional LOD Generator block to the LOD stage. Its first pass is deliberately read-only: `Analyze LOD0` measures used default geometry without dirtying the asset, saving files or replacing any LOD.
- Fixed the project maximum supported render resolution at 2560×1440 through a shared policy used by desktop window sizing and LOD visibility analysis, with a 70° vertical FOV and a default 2 px feature cutoff. The analyzer reports recommended total LOD count, feature-size bands and estimated 2 px disappearance distances.
- The first practical simplification pass analyzes geometrically connected components after an analysis-only positional weld across OBJ UV/normal seams. Principal-axis extents are used so the middle extent behaves as tube diameter for rods and in-plane width for sheets; filename/folder identity is irrelevant.
- `Preview Cull` is non-destructive and transfers only compressed triangle-removal ranges. The browser reuses already resident vertices/normals and constructs temporary index buffers locally, so the preview does not reintroduce full-mesh WebSocket churn.
- Safety for this first experiment is intentionally conservative: the largest connected island of each geometry and any island carrying at least 25% of that geometry's triangles are protected. Only disconnected thin/small components are preview-culled. No generated LOD is committed yet; assignment/replacement of LOD slots remains a later explicit action.

## 0.9.9 — resume latest wizard checkpoint

- Selecting an asset now resumes the highest existing wizard checkpoint before consulting the production `.elmodel` package. A checkpoint is a complete authoring snapshot, so additional meshes and other Geometry-stage edits survive editor restart without requiring `Refresh additional LOD meshes`.
- Resume uses only canonical editor-owned checkpoint paths below the asset workspace; stale persisted checkpoint paths are rebound to the current workspace or cleared when their files were pruned.
- A checkpoint that exists but cannot be loaded is treated as an error. The editor deliberately refuses to silently fall back to an older production package, because doing so would make saved authoring work appear to have vanished.
- Explicit `Reimport Source` still bypasses checkpoint resume and rebuilds from read-only source assembly/OBJ data. Resumed checkpoint data is marked dirty relative to production output until the user explicitly saves the production package.

## 0.9.8 — Geometry finish pass / recursive extras / truthful actions

- Additional OBJ discovery is recursive again across each loaded `LOD<N>` directory tree. Runtime-registered source meshes remain the default set; every other OBJ is an additional mesh. Folder and filename names remain organizational only and do not encode authoring semantics. Refresh now detects byte-for-byte authoring-equivalent mesh content and does not mark GEOMETRY stale merely because an unchanged OBJ was re-read.
- Replacement preview is now explicitly toggleable: clicking the active preview dot again, or `Original`, restores the authored mesh without mutating the asset. `Original` is disabled when no preview is active.
- Geometry now shows the exact ordinary unused-geometry count and list before cleanup. Both cleanup buttons are disabled when the active LOD has nothing deletable; additional replacement meshes remain protected.
- Wizard checkpoint actions now reflect stage state: `Complete stage + checkpoint` is disabled while the current checkpoint is already up to date and re-enables after that stage becomes stale/changed. Restore wording now states precisely that it returns to the exact state recorded at the last checkpoint, discards later edits, and prunes later-stage checkpoints.
- Obvious save actions now expose meaningful disabled states: clean LOD payloads are not offered for redundant save, manifest save is disabled while clean, and Save All is disabled only when the package is complete and nothing is dirty.

## 0.9.7 — stable extra-mesh assignment / world axis labels

- Extra meshes are now discovered only as sibling OBJ files directly inside each loaded `LOD<N>` directory; registered/default OBJ files are excluded. Subfolder and filename naming conventions no longer carry authoring semantics.
- Added persistent opaque authoring ids for additional meshes (`extra.XXXXXX`) and default visual families (`base.XXXXXX`) in `wizard_state.json`. Replacement compatibility is stored `extra -> base visual ids`, never by filename stem or ephemeral `G#`. Existing v0.9.5/v0.9.6 compatibility is migrated when the relevant LOD is loaded.
- Replaced the per-element replacement list with a Geometry-stage master/detail UI: upper table selects one additional mesh, lower table lists each used default geometry once, checkboxes author compatibility, and radio preview temporarily swaps one representative instance before or after compatibility is checked.
- Source reimport no longer auto-pairs additional LOD meshes by matching filenames. Future generated LODs can explicitly reuse the same authoring ids without any naming convention.
- Replaced the corner-only XYZ widget with world-space `+X/-X`, `+Y/-Y`, `+Z/-Z` labels at the ends of the viewport grid/axes.

## 0.9.6 — recursive LOD extra-mesh discovery

- Removed the special meaning of `LOD*/variants/`. Refresh now resolves each real `LOD<N>` source directory and recursively scans all subfolders for OBJ files. Folder names are organizational only.
- Every OBJ declared by the runtime assembly is excluded from the extra-mesh scan even if its GeometryDefinition was later removed by duplicate/instance cleanup, preventing baked source duplicates from being rediscovered as damage variants.
- Extra-mesh identity remains the OBJ filename stem; duplicate extra stems inside one LOD produce an explicit rename diagnostic. Empty scans now report the actual resolved LOD roots that were checked.
- Geometry UI wording now says `Refresh additional LOD meshes`; existing replacement compatibility and hidden variant geometry behavior are unchanged.

## 0.9.5 — flat source variants / explicit replacement compatibility

- Fixed the v0.9.4 MinGW build failure caused by a duplicate `baseGeometryId` declaration in `RuntimeAssemblyImporter.cpp`.
- Source variants now live directly in `LOD*/variants/<variant-id>.obj`; the source directory no longer pretends that one damaged mesh belongs to one precisely named intact module.
- Variant geometry identity is now independent (`source_variant.<variant-id>`). Existing v0.9.4 `<base>.variant.<variant>` IDs remain readable for editor-checkpoint migration.
- Added explicit many-to-many replacement compatibility in the Geometry inspector. Select an intact element and check which source variants may replace its geometry; preview is enabled only for compatible variants.
- Replacement compatibility is authoring metadata persisted in `wizard_state.json`. It intentionally does not decide damage behavior: the later DAMAGE stage will choose among compatible variants from hit region/energy/penetration/accumulated-damage/randomness rules.

## 0.9.4 — source render variants / XYZ orientation

- Source reimport now discovers authored alternate OBJ meshes from `LOD*/variants/<base-mesh-id>/<variant-id>.obj`. The Geometry stage can also refresh those variant files into already-cleaned loaded LODs without rebuilding the intact assembly or losing instance consolidation. Variants enter the LOD-local geometry pool without RenderNodes, so they remain hidden until explicitly previewed or later bound to semantic state.
- Variant geometry identity is deterministic (`<base>.variant.<variant>`), LOD0 is required, LOD1 is optional, and source variants are protected from `Clean unused geometry`.
- The Geometry inspector exposes viewport-only radio preview for variants of the selected intact element. Preview swaps only that element and never changes gameplay state, collision or the asset.
- Added a persistent camera-orientation widget with labelled X/Y/Z axes in the viewport.

## 0.9.3 — metadata-only UI sync / linear checkpoint pruning

- Ordinary editor commands no longer retransmit every render mesh. Full vertex/normal/index/edge payloads are sent only for initial asset load, source reimport, checkpoint restore, LOD load/reload, and other operations that actually replace geometry data.
- Added metadata-only `asset_metadata` synchronization. The browser retains geometry arrays and cached `THREE.BufferGeometry` objects by stable `LOD + geometry id`, then rebuilds only the lightweight scene graph around those cached buffers. `break instance` clones its already-resident mesh locally; edge-mask edits send only the changed edge mask.
- Wizard checkpoints are now linear. Editing/restoring/completing an earlier stage physically removes every later checkpoint directory and resets those stages to `not_started`; only the current stage's previous checkpoint may remain as a rollback point while that stage is stale.

## 0.9.2 — geometry workflow / UI clarity

- Replaced the all-pairs duplicate-card workflow with a reference-based comparison table: choose one reference, check target rows, compare once, and see matches in green and differences in pink. Table selection and viewport selection are synchronized.
- Added one-shot bulk consolidation for checked matching rows. Comparison itself is read-only; only the explicit consolidation command changes geometry bindings. Existing instances of the reference geometry are identified without re-testing.
- Simplified the Geometry stage sidebar: technical render-tree/geometry-pool/storage sections no longer compete with the active task. The selected-element inspector is grouped into Element, Placement, Instance tools, Advanced/manual and Destructive sections. Semantic-state and surface controls no longer appear in the Geometry-stage inspector.
- Inspector actions now have explicit availability rules and contextual hints. Break Instance is disabled for unique geometry, destructive deletion is disabled for parent nodes with children, and manual fit stays collapsed under Advanced.
- Replaced the radial-array prompt chain with a parameter dialog for count, total angle, axis and rotation center.
- Widened the working sidebar and restored the missing dynamic-tooltip binding used by LOD controls.
- Automatic LOD generation remains intentionally deferred; this release is UI/workflow cleanup only.

## 0.9.1 — stable ID preflight / station import repair

- Source import now allocates deterministic semantic Node IDs and qualifies child mesh identity when a module and mesh share the same source name (the Orbital Station `station_solar_panels` case). Legacy v2/v3 migration also repairs empty/duplicate semantic and render-geometry IDs before building v4 RenderNodes.
- Added reusable `ModelAssetBinary::validate` preflight. Wizard checkpoints validate before I/O, while serializer diagnostics now name the duplicate/empty ID and both offending indices.
- The status bar now renders an explicit separator between diagnostic text and the asset path, so copied errors can no longer collapse into strings such as `LOD0D:/...`.

## 0.9.0 — wizard pipeline / capability gate / LOD manager

- Added a top-level asset-processing wizard. Source, LOD review and Geometry are working stages; later stages are visible but locked until their tools are migrated.
- Completing a working stage writes a non-production checkpoint below `build/tools/model_asset_editor/workspaces/<asset>/` and unlocks the next stage.
- Render LOD discovery now uses the union of declared render documents and saved payloads, so an existing LOD can no longer disappear from the UI merely because it is not loaded.
- Geometry duplicate detection is a first-class wizard operation and uses the existing topology-independent `GeometryInstanceFitter`; accepted candidates can be consolidated directly from the stage.
- Added `EDITOR_CAPABILITIES.json` plus architecture checks that require every protected editor capability to have a data-model/backend/UI/test contract. This prevents working tools from silently disappearing during later UI/data-model rewrites.

# Elite Model Asset Editor changelog

## 0.8.2 — reliable settings save / native window close

- Settings Save now enters the visible saving state before any locale/UI refresh and waits for an explicit backend acknowledgement; a timeout reports a missing acknowledgement instead of leaving a dead-looking button.
- The backend acknowledges a successful settings-file write before broadcasting catalog/asset refresh messages, so UI refresh errors cannot hide the save result. Settings requests and successful writes are logged to the editor console with their resolved paths.
- Removed the redundant in-page Quit command/button. The editor closes through the native window close button; the old WebSocket-thread `webview::terminate()` path was unreliable and unnecessary.

## 0.8.1 — source-root defaults / settings acknowledgement

- Source-model root now auto-detects the current project layout (`<project>/assets/models`) instead of defaulting blindly to `<project>/src`; the legacy `src/assets/models` layout remains supported.
- Saving Settings is now an acknowledged operation: the dialog stays open and shows a saving state until C++ confirms the settings file was written, then closes and reports the applied source/compiled paths. Errors keep the dialog open.
- `Defaults` now resolves to the detected source-model root, avoiding a valid-but-wrong `src` directory that causes reimport failures.

## 0.8.0 — semantic states / independent render LOD graphs

- Asset format v4 separates the shared gameplay/semantic assembly from render representation.
- Every render LOD now owns its own RenderNode hierarchy, geometry pool and instancing; no G-index or geometry dependency crosses LOD boundaries. LOD0 may be a detailed assembly while LOD1 is one welded shell and distant LODs may be only a few proxy primitives.
- Added semantic StateVariant records for damaged/breached/destroyed states, including optional transform/pivot, rigid-body and detached overrides. A damaged section may therefore bend or pull away from the intact pose without modifying the base Node.
- Collision volumes and sockets can be state-scoped. Added state-scoped HitRegion, Opening and RepairTarget semantics so a breach can atomically change collision/hit behaviour, expose a traversable/line-of-fire hole and create repair work.
- RenderNodes may bind to a semantic part and a set of active states. Damage-state visualization is independent for every render LOD.
- v2/v3 assets migrate in memory into independent v4 render graphs. Source OBJ/assembly files remain read-only.

## 0.7.0 — shared assembly / active LOD clarity

- Assembly hierarchy and Node -> GeometryDefinition instance links are labelled explicitly as SHARED across every LOD. Creating instances once in the manifest therefore applies automatically to LOD0/LOD1/LOD2/etc.
- GeometryDefinition rows are now semantic/shared IDs only; misleading all-LOD byte estimates were removed from that list.
- Added an Active LOD details section with per-geometry AVAILABLE/MISSING state, instance usage, vertex/triangle/edge counts and per-LOD payload estimate. A missing representation is now visible instead of looking like an uncreated instance.
- Replaced LOD text commands (View/Reload/Save/Unload and Save manifest) with compact icon buttons using the same structured localized tooltips as the main toolbar.
- Narrow sidebar layouts no longer require a horizontal scrollbar for the LOD command row.

## 0.6.0 — localized commands + modal command activity

- Added editor UI localization JSON for the same enabled locales as the game UI: English, Russian, Simplified Chinese, Spanish and Japanese.
- Interface language is selectable in Settings, persists in the editor-local settings JSON, and cycles with `Ctrl+Alt+F12`.
- Toolbar command names/descriptions, LOD I/O controls, settings labels and editor action buttons resolve through localization keys rather than hard-coded language branches.
- Reworked toolbar hints into a structured popup with a prominent uppercase command title and a smaller explanatory description.
- Mutating editor commands now open a true modal busy overlay, not only a status-bar `WORKING` message; the overlay includes a continuously rotating umbrella-style activity indicator and blocks accidental concurrent edits until the command reports completion/error.
- Source OBJ/assembly files remain read-only; reimport only reloads them into editor state.

## 0.5.0 — icon toolbar + persistent editor paths

- Replaced the long top-row text commands with grouped icon buttons and hover hints.
- Normals, hit volumes and sockets are explicit toolbar toggle icons with active-state feedback.
- Added a gear/settings dialog for the source-model root and compiled-model output root.
- Editor path settings persist in `build/tools/model_asset_editor/model_asset_editor.settings.json`; changing them does not move or overwrite existing source/compiled files.
- LOD rows now mark source-backed representations with a `SOURCE` provenance badge and explicitly state that automatic LOD generation is not implemented yet.
- Source-path resolution accepts the project root, `src`, `assets`, or `assets/models` as a practical source-model root.

## 0.4.0 — independently editable LOD package / `.elmodel` v3

- `.elmodel` is a lightweight semantic manifest; heavy geometry lives in one
  `<asset>.lodN.elmesh` file per LOD level.
- LOD files are first-class editor documents with independent `LOADED`,
  `UNLOADED`, `CLEAN` and `DIRTY` state.
- `Load`, `Reload`, `Unload` and `Save` operate on one LOD only. `Save manifest`
  writes semantic metadata only. `Save all` writes only dirty/missing package
  members and does not rewrite clean LOD files.
- v3 opens the manifest plus LOD0 by default; other LOD payloads remain off disk
  until explicitly loaded or an operation genuinely requires all LODs.
- External `.elmesh` entries bind to stable `GeometryDefinition::id`, not the
  transient `G0/G1/...` UI vector index.
- Structural operations that must touch every LOD (basis conversion, breaking
  an instance, deleting geometry definitions) load all LODs first and mark each
  affected payload dirty.
- Existing monolithic `.elmodel` v2 files remain readable for migration. Saving
  writes v3 beside the untouched v2 source binary.
- Editor version and asset-format version are visible in the window/UI.
- Compiled `.elmodel`/`.elmesh` packages are generated artifacts and are kept
  out of Git; editor source, tests and documentation are intended to be tracked.

## 0.2.x — model workbench / native import / instancing

- Native OBJ import retaining topology, materials, normals and UV data.
- Shared GeometryDefinition + Node instancing.
- Collision, sockets, joints and detached rigid-body metadata.
- Baked duplicate -> instance consolidation with LOD0 rigid fitting.
- Operation status/progress feedback and storage diagnostics.
