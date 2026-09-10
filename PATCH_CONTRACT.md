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
