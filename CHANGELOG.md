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
