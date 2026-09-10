### v0.10.66 wizard decomposition wave 5N / SEMANTICS command-decision boundary

- SEMANTICS selection, reparent, relation, MODEL ROOT move/flatten and delete operations must derive deterministic decisions through certified PURE helpers before performing effects. `wizardSemanticsSelectionTransition`, `wizardSemanticsReparentCommand`, `wizardSemanticsRelationCommand`, `wizardSemanticsMoveToAssetSpaceCommand`, `wizardSemanticsFlattenStaticTreeCommand`, `wizardSemanticsDeletePlan` and `wizardSemanticsDeleteConfirmText` are protected v0.10.66 behavioural boundaries.
- Pure command/transition helpers may read only explicit arguments and certified pure helpers. They must not read editor `state` / `editorViewState`, touch DOM/THREE scene objects, call localization/status/confirmation APIs, or dispatch backend commands.
- Existing effect shells retain state mutation, preview reset, confirmation/status UI and `send(...)` ownership. They must not re-embed range-selection traversal, DnD cycle validation, relation payload construction, static-flatten candidate calculation or delete-payload/count derivation.
- Selection Set ordering, Ctrl/Shift/range semantics, DnD placement/cycle rejection, `set_node_parents` / `set_joint` / `delete_semantic_node` payloads and two-stage owned-payload deletion confirmation are behaviour-frozen.
- SOURCE / LODS / GEOMETRY / SURFACES remain closed; this wave authorizes structural extraction only, not feature/protocol/persistence/UI changes.

### v0.10.66 wizard decomposition wave 5M / SEMANTICS residual derivation purity boundary

- `semanticCanonicalNodeAnchorMap(nodes, lod, worlds, stateVariants, previewStates)` and `semanticSelectedPanels(selected, model, text, relationText)` are protected certified PURE boundaries with immutable v0.10.66 behavioural oracles. This supersedes the wave5I allowance for `semanticSelectedPanels()` to own localization or preview-angle state writes.
- Canonical anchor derivation must not read editor `state`, resolve active LOD/root transforms implicitly, or call `updateMatrixWorld()`. Effectful callers must compute the same pre-refresh canonical render-world snapshot, perform the legacy root refresh externally, then pass the snapshot into the pure reducer.
- Selected-panel presentation must not call `tr()` or mutate `semanticPreviewAngleDeg`. `semanticRefreshSelectionUi()` remains the effect shell responsible for localization projection and the historical parent-only preview-angle normalization before applying returned markup.
- Geometry bounds, hierarchy fallback anchors, panel HTML/control IDs and preview normalization are behaviour-frozen. Signature/projection movement is authorized; authored semantic data, backend command protocol, persistence, control order and visible THREE behaviour are not.
- SOURCE / LODS / GEOMETRY / SURFACES remain closed and are not re-baselined by this SEMANTICS-only wave.

### v0.10.66 wizard decomposition wave 5L / SEMANTICS world + graph transform purity boundary

- `semanticWorldMatrix`, `semanticJointLocalPointFromWorld`, `semanticPreviewDeltaWorld`, `semanticDisplayWorldMatrix`, `semanticGraphRootCenter`, `semanticGraphRadialMetrics`, `semanticGraphLayoutOffsets`, `semanticGraphDisplayAnchorMap`, `socketWorldMatrix` and `socketCanonicalWorldMatrix` are explicit-input PURE boundaries with immutable v0.10.66 behavioural oracles.
- These helpers must not read editor `state` / `editorViewState`, resolve active LOD implicitly, mutate scene/root objects, or perform DOM/backend/status effects. Effect adapters must project semantic nodes, state variants, preview state, graph offsets, bounds, root matrix and current selection/motion state explicitly.
- The semantic transform composition order is frozen: canonical semantic transform first, graph explode second, joint preview delta last. Purification may change signatures and adapter wiring only; it must not change matrix multiplication order or socket/joint/graph positions.
- Frozen SOURCE / LODS / GEOMETRY / SURFACES stages remain closed. This wave does not authorize command, persistence, authored-data, control-layout or visible preview changes.

### v0.10.66 wizard decomposition wave 5K / SEMANTICS transform-math purity boundary

- `semanticRelationLabel(node, text)`, `semanticRenderBaseMatrix(index, lod, semanticNodes, stateVariants, previewStates)`, `semanticCanonicalRenderWorldMatrices(input)` and `semanticUnboundRenderClusterOffsets(input)` are protected explicit-input PURE boundaries. They must not regain hidden reads from `state` / `editorViewState` or direct localization through `tr()`.
- Effectful callers remain responsible for projecting the current semantic nodes/state variants/preview states, root `matrixWorld`, asset bounds and localized relation labels into those pure helpers. Moving those reads back into transform helpers violates this boundary.
- `deg`, `composeMatrix`, `semanticGraphDirection` and `socketLocalMatrix` remain transitively PURE and dynamically oracle-certified. The purity harness may import the editor-owned vendored Three.js module solely to execute deterministic `Matrix4` / `Vector3` behavioural fixtures; canonical matrix/vector output is part of the frozen oracle.
- Frozen fixtures were recorded from the pre-refactor wave5J implementation. Signature/invoke adapters may project the same legacy inputs explicitly, but fixture inputs, expected outputs and oracle hashes may not be rebased to make a changed algorithm pass.
- This wave is SEMANTICS-only and does not reopen SOURCE / LODS / GEOMETRY / SURFACES or authorize command, persistence, authored-data, control-layout or visible preview changes.

### v0.10.66 wizard decomposition wave 5I / SEMANTICS selected-panels boundary

- `semanticSelectedPanels()` may own localization lookup and the legacy preview-angle normalization write, but selected-panel derivation and deterministic markup must remain behind certified PURE `wizardSemanticsSelectedPanelsModel(input)` and `wizardSemanticsSelectedPanelsHtml(model, text, fragments)`.
- The pure selected-panels model/view must not read `state` / `editorViewState`, call `tr()` / `send()`, mutate DOM, or bind events.
- The compatibility wrapper must preserve the historical render-time assignment `state.semanticPreviewAngleDeg = model.previewAngleDeg` only when the selected semantic node has a real parent; MODEL ROOT children must not gain that write.
- Selected-node metadata, semantic-frame fields, joint pivot/axis/runtime controls, preview range/play/detach controls and all existing control IDs/order are behavioural-frozen by the v0.10.66 purity oracle plus differential legacy/new parity.
- SOURCE / LODS / GEOMETRY / SURFACES accepted fingerprints are not re-baselined by this SEMANTICS-only decomposition.

# Model Asset Editor patch contract

## v0.10.66 behaviour-preserving function-purity migration

The legacy WebUI may be purified incrementally before the physical module split, but only under the function-purity behavioural-equivalence harness. A promoted function must replace hidden mutable-global reads with explicit arguments, must remain transitively side-effect free, and must produce the frozen v0.10.66 result for the same reviewed fixture inputs. Signature/invoke adapters may change; frozen fixture inputs and expected outputs may not be rewritten as part of the same refactor.

This migration does not reopen accepted SOURCE / LODS / GEOMETRY / SURFACES behaviour. Their existing fingerprint contracts remain authoritative. Complex DOM/state/backend orchestration remains deferred until a separate migration contract exists.

## v0.10.66 wizard-stage decomposition

`renderWizardPanelContents()` may be decomposed one stage at a time only under a stage-level parity contract. The preferred boundary is: (1) a PURE stage model derived only from explicit inputs, (2) a PURE deterministic HTML/view builder derived only from explicit model/text/fragments, and (3) a narrow effect adapter that owns DOM assignment, event binding and backend dispatch. The mega-function branch must collapse to dispatch-only wiring before that stage is considered extracted.

For frozen SOURCE / LODS / GEOMETRY / SURFACES stages, extraction is a structural-only exception: the accepted control IDs/order, authored-data semantics, command protocol and save/reload behaviour remain frozen. Any extracted pure builder must be added to `FUNCTION_PURITY_CONTRACT.json` with immutable behavioural fixtures/oracle hash before the stage branch is replaced. Effect adapters are explicitly not claimed to be pure.

## v0.10.66 dynamic UI localization authority

Dynamic user-facing UI added outside the frozen SOURCE / LODS / GEOMETRY / SURFACES implementation must resolve through the Model Asset Editor localization table. The active locale set is exactly `en`, `ru`, `zh-Hans`, `es`, `ja`; Russian or English fallback text must not become the effective UI of another locale. Literal `tr()` keys used by the WebUI must exist in the dictionary. The localization regression test scans controlled SEMANTICS/shared dynamic HTML/JS and user-facing prompt/status sinks for text that bypasses `tr()`. The four accepted pre-SEMANTICS stages remain fingerprint-frozen and are not reopened by localization cleanup.

## v0.10.64 pre-SEMANTICS acceptance freeze

SOURCE, LODS, GEOMETRY and SURFACES are accepted and frozen before SEMANTICS. Ordinary blank clicks in any 3D viewport clear all selection projections but must not change LOD, visibility/isolation or camera state. The Model Asset Editor localization table must remain complete for `en`, `ru`, `zh-Hans`, `es` and `ja`; visible pre-SEMANTICS UI must not introduce English-only strings.

## v0.10.64 persistent instance families

This patch is an explicit exception to the accepted SOURCE / LODS / GEOMETRY
freeze. It does **not** reopen their layout. It changes only the data semantics
required after GEOMETRY consolidation:

- a SOURCE mesh consolidated into another geometry remains a persistent logical
  identity with `representation=instance`;
- the logical identity points to one canonical `RenderGeometryDefinition` via
  `instanceOfGeometryId` and owns the exact RenderNode ids that represent it;
- alias rows display canonical mesh properties and are visibly marked INSTANCE;
- geometry-owned edits made through any family member resolve to the canonical
  mesh, so topology/surface/material/orientation properties are shared;
- RenderNode transforms remain per-instance placement data;
- converting an existing canonical family into another instance rebases the
  entire family atomically instead of splitting it;
- SOURCE scan may report that an alias source file changed, but it must never
  resurrect a duplicate geometry automatically.

The updated SOURCE/LODS/GEOMETRY fingerprints lock this approved exception.
### v0.10.66 function-purity refactor exception

SOURCE / LODS / GEOMETRY / SURFACES remain behaviourally frozen. During the staged v0.10.66 purity migration, a protected frontend fingerprint may change only when all of the following are true: (1) the change removes a hidden read from a named helper by passing the same value explicitly through arguments/call wiring; (2) the helper has an immutable pre-change v0.10.66 input/output oracle in `FUNCTION_PURITY_CONTRACT.json`; (3) the purity harness proves identical outputs, determinism and no mutation; (4) no layout, control, authored-data, command-protocol or user-visible behaviour changes; and (5) the specific fingerprint update and rationale are documented in CHANGELOG/PROJECT_STATE. This is an explicit structural exception, not permission to bypass frozen-tab failures.
### v0.10.66 function-purity wave 3 accepted fingerprint movement

Wave 3 is an approved instance of the purity-refactor exception above. Eighteen direct hidden-state helpers were given immutable pre-change behavioural fixtures before their signatures or callers changed. SOURCE / LODS / GEOMETRY / SURFACES fingerprints move only because the same legacy values are now passed explicitly to those helpers. Persistent instance-family tests likewise follow the new explicit `meshSourceRecords` argument. No UI layout, authored data, backend command, save format or stage semantics are intentionally changed.
### v0.10.66 function-purity wave 4 accepted fingerprint movement

Wave 4 is an approved instance of the purity-refactor exception above. Thirteen direct hidden-state helpers were given immutable pre-change behavioural fixtures before their signatures or callers changed. SOURCE moves only because source-maintenance/stage-check helpers now receive the same scan/render-LOD inputs explicitly; SURFACES moves only because surface-intent helpers now receive the same preflight rows/active LOD explicitly. LODS and GEOMETRY protected fingerprints do not move. No UI layout, authored data, backend command, save format or stage semantics are intentionally changed.

### v0.10.66 function-purity wave 5 accepted fingerprint movement

Wave 5 is an approved instance of the purity-refactor exception. `activeRenderLod` and twenty-seven additional low-risk helpers were first given immutable pre-change behavioural fixtures, then converted from hidden `state` reads/default-state projections to explicit arguments. Because `activeRenderLod` is shared across the entire accepted frontend, SOURCE / LODS / GEOMETRY / SURFACES protected fingerprints all move even though each call receives the same `state.asset.renderLods` and `state.activeLod` values as before. No UI layout, authored data, backend command, persistence format or stage semantics are intentionally changed. The behavioural oracle and call-graph purity checks are authoritative for this exception.
### v0.10.66 wizard decomposition wave 2 / LODS accepted fingerprint movement

Wave 2 applies the wizard-stage decomposition contract to LODS. The accepted LODS calculations and deterministic markup are moved verbatim in behaviour into certified PURE `wizardLodsStageModel(lods, payloads)` and `wizardLodsStageHtml(model, text, fragments)`; DOM/event/backend work is isolated in `renderWizardLodsStage`. The LODS branch in `renderWizardPanelContents()` is dispatch-only. The protected LODS fingerprint is re-baselined to include all three extracted functions plus the dispatch branch, so extraction does not weaken the freeze. No control IDs/order, PREPARE/ANALYZE command semantics, authored data, save format or user-visible behaviour is intentionally changed.

### v0.10.66 wizard decomposition wave 3 / GEOMETRY accepted fingerprint movement

Wave 3 applies the wizard-stage decomposition contract to GEOMETRY. The accepted stage counts and deterministic workspace markup are isolated in certified PURE `wizardGeometryStageModel(...)` and `wizardGeometryStageHtml(...)`; DOM/event/backend work remains in `renderWizardGeometryStage(root, lods)`. The GEOMETRY branch in `renderWizardPanelContents()` is dispatch-only. The protected GEOMETRY fingerprint is re-baselined to cover that dispatch plus the complete extracted model/view/effect block, so the freeze remains at least as strong after extraction. Existing visibility/isolation, duplicate comparison/consolidation, variant-preview, clean-unused, selection and stage-check behaviour remains unchanged.

### v0.10.66 wizard decomposition wave 4 / SURFACES accepted fingerprint movement

Wave 4 applies the wizard-stage decomposition contract to SURFACES. Deterministic surface/material/selection summary derivation is isolated in certified PURE `wizardSurfacesStageModel(input)` and the main analysis/ready markup in certified PURE `wizardSurfacesStageHtml(model, text, fragments)`. Selection-set normalization, EditorViewState visibility reads, DOM table/event wiring and surface/material backend commands remain in `renderWizardSurfacesStage(root, lods)`. The `renderWizardPanelContents()` SURFACES branch is dispatch-only. The frozen SURFACES fingerprint is re-baselined to include dispatch + model/view/effect functions plus the existing SURFACES helper/CSS contract, so extraction does not weaken the accepted-tab fence. No control IDs/order, authored data, persistence, command protocol or intended user-visible behaviour is changed.

### v0.10.66 wizard decomposition wave 5A / SEMANTICS core

Wave 5A begins SEMANTICS decomposition without treating its large orchestration surface as a single pure rewrite. `renderWizardPanelContents()` may contain only the dispatch call `renderWizardSemanticsStage(root, lods)` for the SEMANTICS stage. Deterministic selection, binding-count, semantic-tree projection and summary derivation is owned by certified PURE `wizardSemanticsStageModel(input)` and must retain its immutable behavioural oracle. Selection normalization, TREE/GRAPH HTML, DOM/event binding, preview mutation and backend commands remain explicitly effectful inside `renderWizardSemanticsStage` until later subwaves split those boundaries. This wave does not reopen the frozen SOURCE / LODS / GEOMETRY / SURFACES stages and does not authorize UI, semantic-data, command-protocol or persistence changes.

### v0.10.66 wizard decomposition wave 5B / SEMANTICS TREE + BINDINGS

Inside `renderWizardSemanticsStage`, TREE and active-LOD visual-binding row computation/markup are now separate certified PURE boundaries. `wizardSemanticsTreeBlockModel(input)` and `wizardSemanticsBindingsBlockModel(input)` may derive only from explicit inputs; `wizardSemanticsTreeRowsHtml(model, text)` and `wizardSemanticsBindingRowsHtml(model, text)` may only assemble deterministic markup from their explicit model/text inputs. The SEMANTICS effect adapter must not reintroduce the former inline `model.treeItems.map(...)` or active-LOD `(lod.nodes||[]).map((rn,ri)=>...)` renderers. DOM/event wiring, localization lookup, selection normalization, structural-graph mode, preview mutation and backend commands remain effectful and quarantined in the adapter. All four new pure boundaries retain immutable behavioural oracles; this subwave does not authorize any semantic-data, UI, command-protocol or persistence change.

### SEMANTICS workspace decomposition exception (0.10.66)
`renderWizardSemanticsStage()` may delegate deterministic workspace state/markup to `wizardSemanticsWorkspaceModel()` and `wizardSemanticsWorkspaceHtml()` only when both remain statically PURE and have immutable behavioural oracles. DOM mutation, event binding, preview mutation, and backend commands remain in the SEMANTICS effect adapter. The inline `semanticWorkspace` template must not return to the adapter.

### v0.10.66 wizard decomposition wave 5D / SEMANTICS TREE + BINDINGS effect boundaries

`renderWizardSemanticsStage()` must delegate TREE row interactions/drag-drop to `bindWizardSemanticsTreeInteractions(root, nodes)` and visual binding interactions to `bindWizardSemanticsBindingInteractions(root, selected)`. Those adapters are intentionally effectful and may own DOM events, global editor selection state and backend dispatch, but they must not own cross-stage wizard dispatch. TREE drop-zone classification/validation, binding-command payload derivation and new-node suggestion derivation are separate PURE boundaries (`wizardSemanticsTreeDropPlacement`, `wizardSemanticsTreeDropValid`, `wizardSemanticsBindingCommandModel`, `wizardSemanticsNewNodeSuggestion`) with immutable behavioural oracles. Reintroducing direct TREE/BINDINGS row event wiring into the main SEMANTICS adapter is not permitted by this exception.
### v0.10.66 wizard decomposition wave 5E / SEMANTICS PREVIEW effects

Wave 5E isolates TREE-mode SEMANTICS transform-preview/explode controls from the main stage adapter. `wizardSemanticsPreviewControlModel(graphEnabled, graphExplode)` is a certified PURE normalization boundary for enabled state, explode amount, percent label and slider-disabled state. `bindWizardSemanticsPreviewInteractions(root)` is explicitly effectful and owns DOM event binding, preview state writes, scheduled preview updates and immediate THREE preview rebuilds. `renderWizardSemanticsStage()` may only call this adapter; inline preview-control wiring must not return to the main SEMANTICS controller. No preview geometry/layout algorithm, command protocol, authored semantic data, persistence or user-visible control behaviour is changed.
## Model Asset Editor v0.10.66 — wizard decomposition wave 5F / SEMANTICS STRUCTURAL GRAPH

STRUCTURAL GRAPH may be decomposed structurally without changing its authored-data semantics, control IDs/order, backend command payloads, persistence, explode/selection behaviour or intended user-visible output. The deterministic graph state/markup boundary must remain explicit-input and certified by the function-purity oracle. Legacy normalization of invalid STRUCTURAL GRAPH root/A/B/selected-link state is an effect and remains owned by the dedicated structural adapter before DOM binding.

`renderWizardSemanticsStage()` must not regain STRUCTURAL GRAPH markup/event implementation; GRAPH mode dispatches to `renderWizardStructuralGraphStage(root, lods)`. `wizardSemanticsStructuralGraphModel`, `structuralGraphPanelHtml`, `semanticStructureModeHtml`, `structuralGraphMeshRowsHtml`, and `structuralEndpointCard` are protected PURE boundaries. `renderWizardStructuralGraphStage` and `bindStructuralGraphPanel` are explicitly effectful and are not claimed pure.
### v0.10.66 wizard decomposition wave 5G / SEMANTICS STRUCTURAL interaction boundaries

`bindStructuralGraphPanel(root)` is an orchestration shell only. Concrete STRUCTURAL GRAPH interaction wiring must remain delegated to `bindWizardStructuralGraphViewportInteractions(root)`, `bindWizardStructuralGraphEndpointInteractions(root)`, `bindWizardStructuralGraphLinkInteractions(root)`, and `bindWizardStructuralGraphProxyInteractions()`. These adapters are intentionally effectful and may own DOM reads/events, editor-state writes, status/confirm calls, THREE preview application and backend dispatch, but they must not own wizard-stage dispatch.

Endpoint/root/A-B assignment decisions and structural command payload construction are protected PURE boundaries with immutable v0.10.66 behavioural oracles: `wizardSemanticsStructuralEndpointActionModel`, `wizardSemanticsStructuralCreateLinkCommand`, `wizardSemanticsStructuralLinkUpdateCommand`, and `wizardSemanticsStructuralProxyUpdateCommand`. Their outputs must remain equivalent to the pre-extraction inline formulas. This exception does not authorize changes to structural-link ids, command names, payload fields/coercions, proxy shape handling, control order, persistence or intended UI behaviour.

### v0.10.66 wizard decomposition wave 5H / SEMANTICS selected-node + motion interaction boundaries

`semanticRefreshSelectionUi()` may refresh selection-dependent markup/state, but concrete semantic-frame authoring and motion-control event wiring must remain delegated to dedicated adapters. `bindWizardSemanticsSelectedNodeInteractions(selected)` owns APPLY SEMANTIC FRAME / DELETE LOGICAL PART dispatch and delegates motion authoring to `bindSemanticMotionControls(selected)`. `bindSemanticMotionControls(selected)` is orchestration-only and delegates concrete preview-range/play/rate/detach/reset wiring to `bindWizardSemanticsMotionPreviewInteractions()` and joint pivot/axis/runtime-parameter wiring to `bindWizardSemanticsJointInteractions(selected)`. These adapters are explicitly effectful.

Semantic-frame payload construction, motion angle/zero/rate normalization and joint runtime payload construction are protected certified PURE boundaries with immutable v0.10.66 behavioural oracles: `wizardSemanticsNodeTransformCommand`, `wizardSemanticsMotionAngleModel`, `wizardSemanticsMotionZeroModel`, `wizardSemanticsPreviewRateModel`, and `wizardSemanticsJointUpdateCommand`. Their outputs must remain equivalent to the pre-extraction inline formulas. This exception does not authorize changes to `set_node_transform` / `set_joint` command names or payload fields/coercions, motion preview algorithms, control IDs/order, persistence or intended UI behaviour.

### v0.10.66 wizard decomposition wave 5J / SEMANTICS selection-refresh boundary

Accepted structural movement only:
- `wizardSemanticsSelectionRefreshModel(input)` is the authoritative pure derivation boundary for selection/binding refresh state.
- `semanticBindingSummaryHtml(selectedIndex, lods, activeLod, text)`, `semanticBindingHealthHtml(selectedIndex, lods, unboundCurrent, node, text)` and `semanticBindingRepairHtml(selected, selectedVisualCount, bindingPickActive, text)` are certified PURE presentation helpers with explicit inputs.
- `semanticRefreshSelectionUi()` remains effectful and must own DOM writes, binding-pick mutation, selected-node interaction binding and preview reapplication; it must not regain binding-count / missing-LOD / top-level-selection derivation.
- Existing SEMANTICS UI ids/order, localization keys, active-LOD binding semantics, backend commands and authored/persistent data are frozen by behavioural parity; no feature redesign is authorized by this wave.
- SOURCE / LODS / GEOMETRY / SURFACES frozen fingerprints remain unchanged.
