## Model Asset Editor v0.10.66 — wizard decomposition wave 5N / SEMANTICS command decisions

- Extracted seven deterministic command/selection boundaries from the remaining SEMANTICS effect shells: `wizardSemanticsSelectionTransition`, `wizardSemanticsReparentCommand`, `wizardSemanticsRelationCommand`, `wizardSemanticsMoveToAssetSpaceCommand`, `wizardSemanticsFlattenStaticTreeCommand`, `wizardSemanticsDeletePlan` and `wizardSemanticsDeleteConfirmText`.
- `semanticSelectNode`, reparent/relation actions, MODEL ROOT move/flatten actions and semantic deletion remain effect adapters that own state mutation, confirmation/status UI, preview reset and backend dispatch only. Ctrl/Shift selection ordering, DnD cycle validation, joint payload coercion and delete confirmation content are now explicit-input PURE calculations.
- Frozen behavioural oracles cover all seven new PURE boundaries. Legacy wave5M versus wave5N differential validation passes 8,400 deterministic comparisons (1,200 per boundary), including selection Set insertion order, collapsed/tree-order range selection, DnD cycle rejection, static-flatten candidates and owned-payload deletion.
- Dynamic certification rises from 189 to 196 PURE functions; static census moves from `192 PURE / 5 EASY / 15 TRANSITIVE / 257 deferred-hard` to `199 PURE / 5 EASY / 15 TRANSITIVE / 257 deferred-hard`. Hard count is intentionally unchanged because the seven original effect shells still perform real state/DOM/status/backend effects; their decision logic is no longer embedded in those shells.
- SOURCE / LODS / GEOMETRY / SURFACES remain untouched; backend command names/payload semantics, authored data, persistence, control order and intended visible behaviour are unchanged.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5M / SEMANTICS residual derivation purity

- Purified the last two SEMANTICS helpers that were hard only because of hidden editor-state mutation: `semanticCanonicalNodeAnchorMap` and `semanticSelectedPanels`.
- `semanticCanonicalNodeAnchorMap(nodes, lod, worlds, stateVariants, previewStates)` now derives canonical per-node anchors solely from explicit geometry/world inputs. Existing effectful callers preserve the historical root-matrix refresh ordering by computing canonical render worlds first, calling `state.root.updateMatrixWorld(true)`, then invoking the pure anchor reduction.
- `semanticSelectedPanels(selected, model, text, relationText)` is now deterministic presentation only. The existing `semanticRefreshSelectionUi()` effect shell owns the legacy parent-only preview-angle normalization and localization projection before calling the pure helper.
- Frozen behavioural oracles cover both promoted helpers. Legacy wave5L versus wave5M differential validation passes 4,800 deterministic cases: 2,400 canonical-anchor cases (including a deliberately changing root matrix on refresh) and 2,400 selected-panel/output+preview-angle cases.
- Dynamic certification rises from 187 to 189 PURE functions; static census moves from `190 PURE / 5 EASY / 15 TRANSITIVE / 259 deferred-hard` to `192 PURE / 5 EASY / 15 TRANSITIVE / 257 deferred-hard`. The remaining hard SEMANTICS functions are genuine DOM/backend/state effect shells; SOURCE / LODS / GEOMETRY / SURFACES remain untouched.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5L / SEMANTICS world + graph transform purity

- Converted the remaining SEMANTICS canonical/display transform chain to explicit-input PURE functions: `semanticWorldMatrix`, `semanticJointLocalPointFromWorld`, `semanticPreviewDeltaWorld`, `semanticDisplayWorldMatrix`, `semanticGraphRootCenter`, `semanticGraphRadialMetrics`, `semanticGraphLayoutOffsets` and `semanticGraphDisplayAnchorMap`.
- Purified the semantic socket transform projections `socketWorldMatrix` and `socketCanonicalWorldMatrix`; scene/root/state projection remains in existing apply/rebuild/view effect adapters.
- Preserved the established composition order: canonical semantic world → editor graph explode → joint preview delta. The general editor architecture contract was updated only to recognize the explicit-input call form.
- Frozen behavioural oracles cover all ten new PURE boundaries. Legacy wave5K versus wave5L differential validation passes 13,108 deterministic comparisons across semantic world matrices, joint-point conversion, preview/display matrices, graph center/radial/layout/display anchors and socket transforms.
- Dynamic certification rises from 177 to 187 PURE functions; static census moves from `180 PURE / 5 EASY / 15 TRANSITIVE / 269 deferred-hard` to `190 PURE / 5 EASY / 15 TRANSITIVE / 259 deferred-hard`. SOURCE / LODS / GEOMETRY / SURFACES remain untouched.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5K / SEMANTICS transform-math purity

- Removed the remaining low-risk SEMANTICS hidden reads from `semanticRelationLabel`, `semanticRenderBaseMatrix`, `semanticCanonicalRenderWorldMatrices` and `semanticUnboundRenderClusterOffsets`; localization, asset semantic state, preview state, root world matrix and asset bounds are now explicit inputs projected by their effectful callers.
- Dynamically certified the existing pure THREE/math helpers `deg`, `composeMatrix`, `semanticGraphDirection` and `socketLocalMatrix`; the purity harness now executes the vendored Three.js module and canonicalizes `Matrix4` / `Vector3` results for immutable behavioural fixtures.
- Frozen pre-refactor v0.10.66 oracles pass after the explicit-input rewrite, and legacy wave5J versus wave5K differential validation passes 7,674 deterministic comparisons (2,560 relation labels, 3,834 render-base matrices, 640 canonical world-matrix sets and 640 unbound-cluster offset sets).
- Dynamic certification rises from 169 to 177 PURE functions; static census moves from `176 PURE / 6 EASY / 18 TRANSITIVE / 269 deferred-hard` to `180 PURE / 5 EASY / 15 TRANSITIVE / 269 deferred-hard`.
- SOURCE / LODS / GEOMETRY / SURFACES remain untouched by this SEMANTICS-only wave. No command protocol, authored-data/persistence semantics, control IDs/order or intended visible/THREE preview behaviour changes.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5I / SEMANTICS selected panels

- Split the legacy `semanticSelectedPanels()` mixed state/model/localization/HTML helper into certified PURE `wizardSemanticsSelectedPanelsModel(input)` and `wizardSemanticsSelectedPanelsHtml(model, text, fragments)` boundaries plus a compatibility effect wrapper.
- Made the legacy `state.semanticPreviewAngleDeg` render-time normalization explicit in the wrapper; the pure model computes the same clamped angle but never mutates external state.
- Preserved selected-node metadata, semantic-frame controls, MODEL ROOT help, revolute preview controls, joint pivot/axis controls, detachable preview, runtime rate/limits and break-force/torque markup byte-for-byte against the legacy implementation across 700 deterministic synthetic states.
- Purity census moves from `170 PURE / 6 EASY / 21 TRANSITIVE / 269 deferred-hard` to `172 PURE / 6 EASY / 21 TRANSITIVE / 269 deferred-hard`; dynamic certification rises from 163 to 165 PURE functions.
- No backend command, authored-data semantics, control IDs/order, localization keys, THREE preview behaviour or intended user-visible behaviour changed.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5H / SEMANTICS selected-node + motion interactions

- Moved APPLY SEMANTIC FRAME / DELETE LOGICAL PART wiring out of `semanticRefreshSelectionUi()` into a dedicated selected-node effect adapter.
- Reduced `bindSemanticMotionControls()` to orchestration and split concrete motion-preview and joint-authoring DOM/state/THREE wiring into separate effect sub-adapters.
- Added five certified PURE boundaries for semantic-frame command construction, motion angle/zero/rate normalization and transactional joint-update payload construction.
- Verified the new pure calculations against the legacy inline formulas across 1,592 deterministic cases with zero mismatches.
- Purity census moves from `165 PURE / 6 EASY / 21 TRANSITIVE / 266 deferred-hard` to `170 PURE / 6 EASY / 21 TRANSITIVE / 269 deferred-hard`; dynamic certification rises from 158 to 163 PURE functions. The hard increase is three newly named effect sub-adapters, not new side-effect behaviour.
- No backend command names, payload semantics/coercions, THREE motion algorithm, persistence, control IDs/order, localization keys or intended user-visible behaviour changed.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5G / SEMANTICS STRUCTURAL interactions

- Split the monolithic `bindStructuralGraphPanel()` into viewport, endpoint, link and proxy effect sub-adapters; the parent binder is now orchestration-only.
- Added certified PURE structural command/decision boundaries for MAKE ROOT / SET A-B, CREATE LINK, APPLY LINK and APPLY PROXY payload derivation.
- Reused the existing certified `wizardSemanticsPreviewControlModel()` for STRUCTURAL GRAPH explode-range normalization, keeping DOM/state/THREE effects in the viewport adapter.
- Verified the new command/decision helpers against the legacy inline formulas across 776 deterministic cases with zero mismatches.
- Purity census moves from `161 PURE / 6 EASY / 21 TRANSITIVE / 262 deferred-hard` to `165 PURE / 6 EASY / 21 TRANSITIVE / 266 deferred-hard`; dynamic certification rises from 154 to 158 PURE functions. The hard increase is four newly named effect sub-adapters, not new side-effect behaviour.
- No backend command names/payload semantics, persistence, control IDs/order, localization keys or intended user-visible behaviour changed.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5F / SEMANTICS STRUCTURAL GRAPH

- Extracted STRUCTURAL GRAPH state derivation and deterministic markup into certified pure boundaries while preserving the existing GRAPH workspace and command protocol.
- Converted `semanticStructureModeHtml`, `structuralGraphMeshRowsHtml`, and `structuralEndpointCard` from hidden-state/transitive helpers to explicit-input PURE helpers.
- Added `renderWizardStructuralGraphStage()` as the dedicated effect shell for normalization writes, localization/fragments, DOM assignment, existing graph interaction binding and THREE preview application.
- Verified legacy/new STRUCTURAL GRAPH parity over 240 deterministic synthetic states: final HTML and legacy root/A/B/selected-link normalization matched exactly.
- Purity census moves from `156 PURE / 6 EASY / 24 TRANSITIVE / 262 deferred-hard` to `161 PURE / 6 EASY / 21 TRANSITIVE / 262 deferred-hard`; dynamic certification rises from 149 to 154 PURE functions.
- No backend command, persistence, control-order, localization-key, authored-data or intended user-visible behaviour change.

# Changelog

## Model Asset Editor v0.10.66 — wizard decomposition wave 2 / LODS

- Extracted the LODS branch from `renderWizardPanelContents()`; the mega-function now owns dispatch-only wiring for SOURCE and LODS.
- Added certified PURE `wizardLodsStageModel(lods, payloads)` for declared/saved/stale/missing/PREPARE counts and certified PURE `wizardLodsStageHtml(model, text, fragments)` for deterministic LODS workspace markup.
- Added narrow effect adapter `renderWizardLodsStage(root, lods, payloads)` for active-LOD button binding, PREPARE/ANALYZE commands, tooltips and subordinate panel rendering.
- Extended the wizard-decomposition contract to protect both SOURCE and LODS boundaries and require immutable behavioural oracles for both extracted pure builders.
- Re-accepted the LODS freeze fingerprint only for this structural extraction; the new fingerprint includes the extracted LODS model/view/effect functions so the accepted implementation remains fully protected.

## Model Asset Editor v0.10.66 — wizard decomposition wave 1 / SOURCE

- Began structural decomposition of `renderWizardPanelContents()` with SOURCE only; all other wizard stages remain byte-for-byte in the mega-function.
- Added certified PURE `wizardSourceStageModel(asset, settings)` for SOURCE counts/inventory and certified PURE `wizardSourceStageHtml(model, text, fragments)` for deterministic SOURCE HTML assembly.
- Added narrow effect adapter `renderWizardSourceStage(root, asset, settings)` for DOM assignment, event binding, tooltips and backend commands; the SOURCE branch in `renderWizardPanelContents()` is now dispatch-only.
- Added a wizard-decomposition architecture contract that forbids SOURCE implementation from leaking back into the mega-function and requires both pure SOURCE builders to retain frozen behavioural oracles.
- Re-accepted the SOURCE freeze fingerprint only for this structural extraction; accepted controls, ordering, backend commands, save semantics and user-visible behaviour are unchanged.

## Model Asset Editor v0.10.66 — function-purity migration wave 4

- Froze pre-refactor v0.10.66 behaviour for 13 additional direct hidden-state helpers and converted all 13 to explicit-argument pure functions without changing their frozen outputs.
- Dynamically certified coverage grows from 88 to 101 functions; all 101 are certified pure with immutable per-function oracle hashes.
- Static call-graph purity improves from 95/13/58 to 108/5/53 for `PURE` / `EASY_CANDIDATE` / `TRANSITIVE_CANDIDATE`; the hard/deferred set remains 254.
- The wave removes hidden state reads from language/source maintenance helpers, mesh stage styling, semantic preview/tree/flatten/state predicates, source-scan acceptance and surface-intent classification.
- SOURCE and SURFACES protected fingerprints are re-accepted only for oracle-covered explicit-argument wiring. LODS and GEOMETRY fingerprints do not move; no layout, authored-data semantics, command protocol or intended user-visible behaviour changes.

## Model Asset Editor v0.10.66 — function-purity migration wave 3

- Froze pre-refactor v0.10.66 behaviour for 18 additional direct hidden-state helpers, then converted all 18 to explicit-argument pure functions without changing their frozen outputs.
- Dynamically certified coverage grows from 70 to 88 functions; all 88 are now certified pure with immutable per-function oracle hashes.
- Static call-graph purity improves from 75/26/65 to 95/13/58 for `PURE` / `EASY_CANDIDATE` / `TRANSITIVE_CANDIDATE`; the hard/deferred set remains 254.
- The wave removes hidden state reads from instance-alias lookups, geometry-match selection, mesh stage checks, semantic selection/tree/joint queries, surface material/preflight queries, scroll-context derivation and wizard-stage gating.
- SOURCE / LODS / GEOMETRY / SURFACES protected fingerprints are re-accepted only for this oracle-covered explicit-argument wiring; no layout, authored-data semantics, command protocol or intended user-visible behaviour changes.

## Model Asset Editor v0.10.66 — function-purity migration wave 2

- Dynamically froze the pre-refactor v0.10.66 behaviour of 14 additional direct hidden-state helpers, then promoted those helpers plus the already-certified `surfaceSelectedGeometry` to explicit-argument pure functions.
- Certified purity coverage grows from 56 to 70 functions; all 70 are now certified pure and retain immutable per-function v0.10.66 input/output oracle hashes.
- Static call-graph purity improves from 60/32/74 to 75/26/65 for `PURE` / `EASY_CANDIDATE` / `TRANSITIVE_CANDIDATE`; the hard/deferred set remains 254.
- SOURCE / GEOMETRY / SURFACES freeze fingerprints were intentionally re-accepted only for behaviour-preserving call-wiring/signature changes covered by the frozen purity oracle. LODS remains byte-for-byte on its previous protected fingerprint.
- No UI layout, authored asset data, command protocol or user-visible behaviour is intentionally changed.

## Model Asset Editor v0.10.66 — function-purity migration wave 1

- Expanded frozen behavioural-equivalence coverage from 24 to 56 named WebUI functions: 55 certified pure and one still-certified low-risk candidate.
- Upgraded the dynamic contract to per-function immutable `oracle_sha256` hashes so adding a new certified function cannot re-baseline older frozen fixtures.
- Promoted seven previously certified functions from hidden `state` reads to explicit-argument pure helpers: semantic subtree/root/selection queries, semantic transform-variant lookup, current-LOD unbound count and LOD diagnostic predicate.
- The call graph improves from 53/32/81 to 60/32/74 for `PURE` / `EASY_CANDIDATE` / `TRANSITIVE_CANDIDATE`, with the 254 hard/deferred functions untouched.
- SOURCE / LODS / GEOMETRY / SURFACES frozen acceptance fingerprints remain unchanged; all 24 Model Asset Editor architecture tests pass.

## Model Asset Editor v0.10.66 — dynamic UI localization fence

- Completed dynamic Model Asset Editor localization for SEMANTICS and the shared SEMANTICS/PHYSICS/DAMAGE inspectors, including runtime statuses, confirmations, prompts, socket/collision/damage controls, structural graph authoring and the 3D viewport mode selector.
- Added complete `en` / `ru` / `zh-Hans` / `es` / `ja` entries for every new key; Russian is no longer an implicit fallback path for other locales.
- Strengthened the localization architecture contract: every literal `tr()` key must exist, and uncontrolled hardcoded user-facing text is rejected in unfrozen dynamic UI. SOURCE / LODS / GEOMETRY / SURFACES fingerprints remain unchanged.
- Bumped the visible editor version to 0.10.66.

## Model Asset Editor v0.10.65 — SEMANTICS higher-LOD visual explode fix

- Fixed SEMANTICS explode for higher-LOD RenderNodes that are present/resident but still UNBOUND to the asset-wide semantic tree.
- Such RenderNodes separate as temporary visual clusters around MODEL ROOT for preview/binding work; semantic ownership is never guessed or persisted by the preview.
- Bumped the visible editor version to 0.10.65.

## Model Asset Editor v0.10.64 — pre-SEMANTICS acceptance freeze

- SOURCE, LODS, GEOMETRY and SURFACES are now the frozen accepted authoring baseline before SEMANTICS.
- Blank clicks in any 3D editor viewport clear all selection projections without changing LOD, visibility/isolation or camera state.
- Model Asset Editor localization is complete for English, Russian, Simplified Chinese, Spanish and Japanese, including the active-LOD rotation dialog.

## Model Asset Editor v0.10.64 — persistent instance families

- Added persistent SOURCE-to-canonical instance links after GEOMETRY duplicate
  consolidation.
- Removed consolidated duplicate geometry payloads instead of leaving unused
  copies behind.
- Rebased complete instance families when their former canonical mesh is itself
  consolidated into another geometry.
- SOURCE, LODS, GEOMETRY and SURFACES tables now expose alias rows as INSTANCE
  links and read effective mesh properties from the canonical geometry.
- SOURCE reconciliation preserves instance links across SAVE/RESTORE and source
  scans; changed alias source files do not silently recreate independent mesh
  geometry.
- Updated accepted SOURCE/LODS/GEOMETRY freeze fingerprints only for this
  explicitly approved instance-link semantic change.

## Model Asset Editor v0.10.66 — function-purity wave 5

- Converted the high-fan-out `activeRenderLod` helper from an implicit global-state read to an explicit `(renderLods, activeLod)` projection, with frozen pre-change behavioural fixtures.
- Promoted another 27 low-risk selectors/lookup/calculation helpers to explicit-argument pure functions, including geometry/instance lookup, active-LOD geometry rows, semantic render ownership/profile and LOD-generator selection helpers.
- Dynamic behavioural coverage rises from 101 to 129 certified PURE functions.
- Static call-graph classification improves from 108/5/53 to 136/6/24 for `PURE` / `EASY_CANDIDATE` / `TRANSITIVE_CANDIDATE`; the 254 hard/deferred functions remain untouched.
- SOURCE / LODS / GEOMETRY / SURFACES fingerprints move only because shared hidden-state reads were replaced by explicit call wiring under the documented purity exception.

## Model Asset Editor v0.10.66 — wizard decomposition wave 3 / GEOMETRY

- Extracted the GEOMETRY branch from `renderWizardPanelContents()` into certified PURE `wizardGeometryStageModel(...)`, certified PURE `wizardGeometryStageHtml(...)`, and narrow effect adapter `renderWizardGeometryStage(root, lods)`.
- The mega-function GEOMETRY branch is now dispatch-only; comparison visibility synchronization, DOM/event binding, duplicate scan/consolidation commands, variant controls and geometry editor refresh remain isolated in the effect adapter.
- Added immutable behavioural fixtures/oracle hashes for both new pure GEOMETRY boundary functions; dynamic certification rises from 133 to 135 PURE functions and static purity from 140 to 142 PURE functions.
- Re-accepted the frozen GEOMETRY fingerprint as the dispatch branch plus the extracted model/view/effect block; SOURCE, LODS and SURFACES fingerprints are unchanged.
- Updated GEOMETRY acceptance tests to protect the logical extracted block rather than require implementation text to remain physically inside the mega-function. No control IDs/order, authored data, command protocol or intended user-visible behaviour changes.

## Model Asset Editor v0.10.66 — wizard decomposition wave 4 / SURFACES

- Extracted the SURFACES branch from `renderWizardPanelContents()` into certified PURE `wizardSurfacesStageModel(input)`, certified PURE `wizardSurfacesStageHtml(model, text, fragments)`, and narrow effect adapter `renderWizardSurfacesStage(root, lods)`.
- The pure stage model owns deterministic surface/material/selection summary derivation from an explicit snapshot; the pure HTML builder owns the analysis-required and ready workspace markup. Selection normalization, visibility reads, DOM row/event binding and backend commands remain quarantined in the effect adapter.
- Added immutable behavioural fixtures/oracle hashes for both pure SURFACES boundary functions; dynamic certification rises from 135 to 137 PURE functions and static purity from 142 to 144 PURE functions.
- Re-accepted the frozen SURFACES fingerprint as dispatch + extracted model/view/effect block + existing SURFACES helpers/CSS. SOURCE, LODS and GEOMETRY frozen fingerprints do not move.
- The first four accepted stages are now all dispatch-only inside the mega-function: SOURCE / LODS / GEOMETRY / SURFACES. No control IDs/order, authored-data semantics, command protocol or intended user-visible behaviour changes.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5A / SEMANTICS core

- Reduced the SEMANTICS branch in `renderWizardPanelContents()` to dispatch-only wiring through `renderWizardSemanticsStage(root, lods)`.
- Added certified PURE `wizardSemanticsStageModel(input)` for deterministic semantic selection, tree projection, binding counts and stage summary derivation using explicit inputs only.
- Added immutable behavioural fixtures/oracle hash for the new SEMANTICS model boundary; dynamic certification rises from 137 to 138 PURE functions and static purity from 144 to 145 PURE functions.
- Kept TREE/GRAPH markup, DOM/event wiring, selection normalization, preview effects and backend commands quarantined in the SEMANTICS effect adapter for a later internal split instead of pretending the whole stage is pure.
- Updated the wizard decomposition and semantic-tree acceptance checks to follow the extracted logical block. SOURCE / LODS / GEOMETRY / SURFACES frozen fingerprints are unchanged; no intended UI, authored-data, command-protocol or persistence behaviour changes.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5B / SEMANTICS TREE + BINDINGS

- Split the quarantined SEMANTICS adapter internally without changing the wizard-stage API: TREE row derivation/markup and visual-binding row derivation/markup now live behind four certified PURE functions.
- Added `wizardSemanticsTreeBlockModel(input)` + `wizardSemanticsTreeRowsHtml(model, text)` and `wizardSemanticsBindingsBlockModel(input)` + `wizardSemanticsBindingRowsHtml(model, text)`; all stage/global reads remain in the effect adapter and are passed explicitly.
- Recorded immutable behavioural fixtures/oracle hashes for all four boundaries and separately verified the extracted TREE/BINDINGS output against the pre-extraction inline algorithms over 250 deterministic synthetic cases each.
- Dynamic behavioural certification rises from 138 to 142 PURE functions; static purity rises from 145 to 149 PURE functions. The existing 6 EASY / 24 TRANSITIVE / 259 deferred-hard functions are unchanged.
- SOURCE / LODS / GEOMETRY / SURFACES frozen fingerprints are unchanged. No semantic data, control IDs/order, backend command protocol, persistence or intended user-visible behaviour changes.

## 0.10.66 purity/decomposition follow-up — SEMANTICS workspace boundary
- Extracted the main SEMANTICS workspace composition from `renderWizardSemanticsStage()` into certified pure `wizardSemanticsWorkspaceModel()` + `wizardSemanticsWorkspaceHtml()` boundaries.
- The effect adapter now prepares localization/fragments, assigns the returned HTML, and retains DOM/events/backend/preview side effects.
- Legacy inline workspace output was differentially checked against the new pure composition before wiring; markup is unchanged.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5D / SEMANTICS TREE + BINDINGS effects

- Moved SEMANTICS TREE row interaction/drag-drop wiring out of `renderWizardSemanticsStage()` into dedicated effect adapter `bindWizardSemanticsTreeInteractions(root, nodes)`.
- Moved active-LOD visual binding row selection/checkbox wiring into dedicated effect adapter `bindWizardSemanticsBindingInteractions(root, selected)`.
- Extracted four additional certified PURE decision/model helpers: `wizardSemanticsTreeDropPlacement`, `wizardSemanticsTreeDropValid`, `wizardSemanticsBindingCommandModel`, and `wizardSemanticsNewNodeSuggestion`.
- Verified the new pure decision helpers against the pre-extraction inline logic over 4,152 deterministic legacy/new parity cases; dynamic certification rises from 144 to 148 PURE functions and static purity from 151 to 155 PURE functions.
- The two new event adapters are explicitly effectful/deferred; no event semantics, backend command payloads, control IDs/order, semantic data, persistence or intended user-visible behaviour changes.
## Model Asset Editor v0.10.66 — wizard decomposition wave 5E / SEMANTICS PREVIEW

- Moved TREE-mode SEMANTICS graph-enabled/explode slider/reset event wiring out of `renderWizardSemanticsStage()` into dedicated effect adapter `bindWizardSemanticsPreviewInteractions(root)`.
- Added certified PURE `wizardSemanticsPreviewControlModel(graphEnabled, graphExplode)` for the exact legacy normalization of graph-enabled state, explode amount, percent display and slider-disabled state; `wizardSemanticsWorkspaceModel()` now reuses the same pure calculation.
- Recorded immutable behavioural fixtures/oracle hash for the preview-control model and verified the extracted calculation against legacy formulas over 133 scalar edge cases.
- Dynamic certification rises from 148 to 149 PURE functions; static purity rises from 155 to 156 PURE functions. The new PREVIEW adapter is deliberately effectful/deferred, so deferred-hard rises from 261 to 262.
- No THREE explode/layout algorithm, control IDs/order, semantic authored data, backend protocol, persistence or intended user-visible behaviour changes. SOURCE / LODS / GEOMETRY / SURFACES frozen fingerprints remain unchanged.

## Model Asset Editor v0.10.66 — wizard decomposition wave 5J / SEMANTICS selection refresh + binding presentation

- Extracted deterministic SEMANTICS selection-refresh derivation into certified PURE `wizardSemanticsSelectionRefreshModel(input)`; row selection state, selected counts, MODEL ROOT move count, binding checkbox state, active-LOD unbound count and repair-open decision now use explicit inputs only.
- Purified the existing `semanticBindingSummaryHtml`, `semanticBindingHealthHtml` and `semanticBindingRepairHtml` helpers by replacing hidden `state` / `tr()` reads with explicit data and localized text arguments.
- `semanticRefreshSelectionUi()` remains the explicit DOM/effect shell: it applies the pure model, updates hosts/classes/controls, owns binding-pick state mutation and delegates selected-node authoring/preview effects.
- Added immutable behavioural fixtures for all four pure boundaries and differentially verified legacy/new output over 1,600 deterministic cases (summary 320, health 320, repair 320, refresh model 640).
- Dynamic certification rises from 165 to 169 PURE functions; static purity rises from 172 to 176; TRANSITIVE candidates fall from 21 to 18; deferred-hard remains 269. No UI/control order, localization keys, semantic authored data, backend protocol or persistence behaviour changes.
