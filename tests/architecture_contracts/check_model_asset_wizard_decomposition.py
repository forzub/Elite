#!/usr/bin/env python3
"""Structural contract for incremental wizard-stage decomposition.

A stage may leave DOM/event/backend work in a narrow adapter, but calculations and
HTML assembly extracted from renderWizardPanelContents must be pure and behaviourally
certified before the branch is reduced to dispatch-only wiring.
"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
WEB = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8")
CONTRACT = json.loads((ROOT / "tools/model_asset_editor/FUNCTION_PURITY_CONTRACT.json").read_text(encoding="utf-8"))


def function_source(name: str) -> str:
    marker = f"function {name}("
    start = WEB.find(marker)
    if start < 0:
        raise AssertionError(f"wizard decomposition: missing {name}")
    brace = WEB.index("{", start)
    depth = 0
    quote = None
    escaped = False
    i = brace
    while i < len(WEB):
        c = WEB[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                quote = None
            i += 1
            continue
        if c in ("'", '"', "`"):
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return WEB[start:i + 1]
        i += 1
    raise AssertionError(f"wizard decomposition: unterminated {name}")


shell = function_source("renderWizardPanelContents")
source_model = function_source("wizardSourceStageModel")
source_html = function_source("wizardSourceStageHtml")
source_adapter = function_source("renderWizardSourceStage")
lods_model = function_source("wizardLodsStageModel")
lods_html = function_source("wizardLodsStageHtml")
lods_adapter = function_source("renderWizardLodsStage")
geometry_model = function_source("wizardGeometryStageModel")
geometry_html = function_source("wizardGeometryStageHtml")
geometry_adapter = function_source("renderWizardGeometryStage")
surfaces_model = function_source("wizardSurfacesStageModel")
surfaces_html = function_source("wizardSurfacesStageHtml")
surfaces_adapter = function_source("renderWizardSurfacesStage")
semantics_model = function_source("wizardSemanticsStageModel")
semantics_adapter = function_source("renderWizardSemanticsStage")
semantics_tree_model = function_source("wizardSemanticsTreeBlockModel")
semantics_tree_html = function_source("wizardSemanticsTreeRowsHtml")
semantics_bindings_model = function_source("wizardSemanticsBindingsBlockModel")
semantics_bindings_html = function_source("wizardSemanticsBindingRowsHtml")
semantics_workspace_model = function_source("wizardSemanticsWorkspaceModel")
semantics_workspace_html = function_source("wizardSemanticsWorkspaceHtml")
semantics_tree_drop_placement = function_source("wizardSemanticsTreeDropPlacement")
semantics_tree_drop_valid = function_source("wizardSemanticsTreeDropValid")
semantics_binding_command_model = function_source("wizardSemanticsBindingCommandModel")
semantics_new_node_suggestion = function_source("wizardSemanticsNewNodeSuggestion")
semantics_tree_interactions = function_source("bindWizardSemanticsTreeInteractions")
semantics_binding_interactions = function_source("bindWizardSemanticsBindingInteractions")
semantics_preview_model = function_source("wizardSemanticsPreviewControlModel")
semantics_preview_interactions = function_source("bindWizardSemanticsPreviewInteractions")
semantics_structure_mode_html = function_source("semanticStructureModeHtml")
semantics_structural_model = function_source("wizardSemanticsStructuralGraphModel")
semantics_structural_html = function_source("structuralGraphPanelHtml")
semantics_structural_mesh_rows = function_source("structuralGraphMeshRowsHtml")
semantics_structural_endpoint_card = function_source("structuralEndpointCard")
semantics_structural_adapter = function_source("renderWizardStructuralGraphStage")
semantics_structural_endpoint_action = function_source("wizardSemanticsStructuralEndpointActionModel")
semantics_structural_create_link_command = function_source("wizardSemanticsStructuralCreateLinkCommand")
semantics_structural_link_update_command = function_source("wizardSemanticsStructuralLinkUpdateCommand")
semantics_structural_proxy_update_command = function_source("wizardSemanticsStructuralProxyUpdateCommand")
semantics_structural_bind_panel = function_source("bindStructuralGraphPanel")
semantics_structural_viewport_interactions = function_source("bindWizardStructuralGraphViewportInteractions")
semantics_structural_endpoint_interactions = function_source("bindWizardStructuralGraphEndpointInteractions")
semantics_structural_link_interactions = function_source("bindWizardStructuralGraphLinkInteractions")
semantics_structural_proxy_interactions = function_source("bindWizardStructuralGraphProxyInteractions")
semantics_selected_node_interactions = function_source("bindWizardSemanticsSelectedNodeInteractions")
semantics_motion_preview_interactions = function_source("bindWizardSemanticsMotionPreviewInteractions")
semantics_joint_interactions = function_source("bindWizardSemanticsJointInteractions")
semantics_motion_orchestrator = function_source("bindSemanticMotionControls")
semantics_selection_refresh = function_source("semanticRefreshSelectionUi")
semantics_node_transform_command = function_source("wizardSemanticsNodeTransformCommand")
semantics_motion_angle_model = function_source("wizardSemanticsMotionAngleModel")
semantics_motion_zero_model = function_source("wizardSemanticsMotionZeroModel")
semantics_preview_rate_model = function_source("wizardSemanticsPreviewRateModel")
semantics_joint_update_command = function_source("wizardSemanticsJointUpdateCommand")
semantics_selected_panels_model = function_source("wizardSemanticsSelectedPanelsModel")
semantics_selected_panels_html = function_source("wizardSemanticsSelectedPanelsHtml")
semantics_selected_panels_wrapper = function_source("semanticSelectedPanels")
semantics_selection_refresh_model = function_source("wizardSemanticsSelectionRefreshModel")
semantics_binding_summary_html = function_source("semanticBindingSummaryHtml")
semantics_binding_health_html = function_source("semanticBindingHealthHtml")
semantics_binding_repair_html = function_source("semanticBindingRepairHtml")
semantics_relation_label = function_source("semanticRelationLabel")
semantics_render_base_matrix = function_source("semanticRenderBaseMatrix")
semantics_canonical_render_world_matrices = function_source("semanticCanonicalRenderWorldMatrices")
semantics_unbound_cluster_offsets = function_source("semanticUnboundRenderClusterOffsets")
semantics_graph_direction = function_source("semanticGraphDirection")
semantics_socket_local_matrix = function_source("socketLocalMatrix")
semantics_compose_matrix = function_source("composeMatrix")
semantics_deg = function_source("deg")
semantics_world_matrix = function_source("semanticWorldMatrix")
semantics_joint_local_point = function_source("semanticJointLocalPointFromWorld")
semantics_preview_delta_world = function_source("semanticPreviewDeltaWorld")
semantics_display_world_matrix = function_source("semanticDisplayWorldMatrix")
semantics_graph_root_center = function_source("semanticGraphRootCenter")
semantics_graph_radial_metrics = function_source("semanticGraphRadialMetrics")
semantics_graph_layout_offsets = function_source("semanticGraphLayoutOffsets")
semantics_graph_display_anchor_map = function_source("semanticGraphDisplayAnchorMap")
semantics_socket_world_matrix = function_source("socketWorldMatrix")
semantics_socket_canonical_world_matrix = function_source("socketCanonicalWorldMatrix")
semantics_canonical_node_anchor_map = function_source("semanticCanonicalNodeAnchorMap")
semantics_selection_transition = function_source("wizardSemanticsSelectionTransition")
semantics_reparent_command = function_source("wizardSemanticsReparentCommand")
semantics_relation_command = function_source("wizardSemanticsRelationCommand")
semantics_move_to_asset_command = function_source("wizardSemanticsMoveToAssetSpaceCommand")
semantics_flatten_static_command = function_source("wizardSemanticsFlattenStaticTreeCommand")
semantics_delete_plan = function_source("wizardSemanticsDeletePlan")
semantics_delete_confirm_text = function_source("wizardSemanticsDeleteConfirmText")
semantics_select_node_adapter = function_source("semanticSelectNode")
semantics_reparent_adapter = function_source("semanticReparentSelection")
semantics_relation_adapter = function_source("semanticSetRelationFor")
semantics_move_to_asset_adapter = function_source("semanticMoveSelectionToAssetSpace")
semantics_flatten_static_adapter = function_source("semanticFlattenStaticTree")
semantics_delete_adapter = function_source("semanticDeleteSelectedNode")
semantics_joint_pivot_command = function_source("wizardSemanticsJointPivotCommand")
semantics_parent_origin_pivot_request = function_source("wizardSemanticsParentOriginPivotRequest")
semantics_visual_center_pivot_eligibility = function_source("wizardSemanticsVisualCenterPivotEligibility")
semantics_visual_center_pivot_request = function_source("wizardSemanticsVisualCenterPivotRequest")
semantics_joint_pivot_pick_transition = function_source("wizardSemanticsJointPivotPickTransition")
semantics_nominal_rate_command = function_source("wizardSemanticsNominalRateCommand")
semantics_binding_pick_transition = function_source("wizardSemanticsBindingPickTransition")
semantics_binding_assignment_command = function_source("wizardSemanticsBindingAssignmentCommand")
semantics_joint_pivot_adapter = function_source("semanticSetJointPivotWorld")
semantics_parent_origin_pivot_adapter = function_source("semanticUseParentOriginPivot")
semantics_visual_center_pivot_adapter = function_source("semanticUseVisualCenterPivot")
semantics_joint_pivot_pick_adapter = function_source("semanticBeginJointPivotPick")
semantics_nominal_rate_adapter = function_source("semanticSetNominalRate")
semantics_pick_adapter = function_source("pick")
semantics_binding_interactions_adapter = function_source("bindWizardSemanticsBindingInteractions")
semantics_motion_animation_step = function_source("wizardSemanticsMotionAnimationStep")
semantics_motion_play_transition = function_source("wizardSemanticsMotionPlayTransition")
semantics_motion_detach_transition = function_source("wizardSemanticsMotionDetachTransition")
semantics_collapsed_transition = function_source("wizardSemanticsCollapsedTransition")
semantics_tree_drag_start_transition = function_source("wizardSemanticsTreeDragStartTransition")
semantics_stage_selection_normalization = function_source("wizardSemanticsStageSelectionNormalization")
semantics_lod_selection_restore_model = function_source("wizardSemanticsLodSelectionRestoreModel")
semantics_motion_animation_adapter = function_source("updateSemanticMotionAnimation")
semantics_tree_interactions_adapter = function_source("bindWizardSemanticsTreeInteractions")
semantics_semantics_stage_adapter = function_source("renderWizardSemanticsStage")
semantics_lod_selection_restore_adapter = function_source("restoreSemanticSelectionAfterLodSwitch")
semantics_graph_explode_application_plan = function_source("wizardSemanticsGraphExplodeApplicationPlan")
semantics_motion_preview_target_model = function_source("wizardSemanticsMotionPreviewTargetModel")
semantics_motion_preview_application_plan = function_source("wizardSemanticsMotionPreviewApplicationPlan")
semantics_graph_explode_adapter = function_source("applySemanticGraphExplode")
semantics_motion_preview_adapter = function_source("applySemanticMotionPreview")
semantics_graph_object_model = function_source("wizardSemanticsGraphObjectModel")
semantics_graph_gizmo_plan = function_source("wizardSemanticsGraphGizmoPlan")
semantics_joint_gizmo_plan = function_source("wizardSemanticsJointGizmoPlan")
semantics_viewport_gizmo_pick_decision = function_source("wizardSemanticsViewportGizmoPickDecision")
semantics_viewport_mesh_pick_decision = function_source("wizardSemanticsViewportMeshPickDecision")
semantics_graph_objects_adapter = function_source("ensureSemanticGraphObjects")
semantics_graph_gizmos_adapter = function_source("rebuildSemanticGraphGizmos")
semantics_joint_gizmos_adapter = function_source("rebuildSemanticGizmos")

certified = {entry["name"]: entry for entry in CONTRACT.get("pure", [])}


def assert_pure_block(name: str, body: str) -> None:
    entry = certified.get(name)
    if not entry or not entry.get("oracle_sha256") or not entry.get("fixtures"):
        raise AssertionError(f"wizard decomposition: {name} must have a frozen behavioural oracle")
    for forbidden in (r"\bstate\b", r"\beditorViewState\b", r"\bdocument\b", r"\bsend\s*\(", r"\.innerHTML\s*=", r"\.onclick\s*="):
        if re.search(forbidden, body):
            raise AssertionError(f"wizard decomposition: pure block {name} regained side effect/global dependency: {forbidden}")


# SOURCE: dispatch-only shell + pure model/view + narrow effect adapter.
if "if(stage==='source'){renderWizardSourceStage(root,state.asset,state.settings);return;}" not in shell:
    raise AssertionError("wizard decomposition: SOURCE branch must remain dispatch-only")
for forbidden in ("wizardSourceRefreshBtn", "wizardSourceReimportBtn", "maintenanceSourceScanHtml()"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: SOURCE implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardSourceStageModel", source_model), ("wizardSourceStageHtml", source_html)):
    assert_pure_block(name, body)
for token in (
    "wizardSourceStageModel(asset,settings)",
    "wizardSourceStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "bindPhysicalSizePanel()",
    "bindMaintenanceSourceScan(root)",
    "send('refresh_source_variants')",
    "send('reimport_asset')",
    "bindWizardStageCheckControls('source')",
):
    if token not in source_adapter:
        raise AssertionError(f"wizard decomposition: SOURCE effect adapter missing {token!r}")
if "if(stage==='" in source_adapter:
    raise AssertionError("wizard decomposition: SOURCE adapter must not own cross-stage dispatch")

# LODS: same boundary. Calculations and deterministic markup are pure; DOM/events/backend
# remain isolated in the adapter until the whole tab is moved into its own module.
if "if(stage==='lods'){renderWizardLodsStage(root,lods,payloads);return;}" not in shell:
    raise AssertionError("wizard decomposition: LODS branch must remain dispatch-only")
for forbidden in ("modelPreflightPrepareBtn", "lodGeneratorAnalyzeBtn", "prepare_model_meshes", "analyze_lod_requirements"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: LODS implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardLodsStageModel", lods_model), ("wizardLodsStageHtml", lods_html)):
    assert_pure_block(name, body)
for token in (
    "wizardLodsStageModel(lods,payloads)",
    "wizardLodsStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "switchEditorLod(Number(btn.dataset.lodsLod),'lods-selector')",
    "send('prepare_model_meshes',{})",
    "send('analyze_model_preflight',{})",
    "send('analyze_lod_requirements',{lodIndex:0})",
    "renderModelPreflightPanel()",
    "renderLodGeneratorPanel()",
    "bindWizardStageCheckControls('lods')",
):
    if token not in lods_adapter:
        raise AssertionError(f"wizard decomposition: LODS effect adapter missing {token!r}")
if "if(stage==='" in lods_adapter:
    raise AssertionError("wizard decomposition: LODS adapter must not own cross-stage dispatch")

# GEOMETRY: calculations/markup are isolated from event/backend wiring. The adapter owns
# visibility synchronization, DOM binding and commands until the tab moves to its own file.
if "if(stage==='geometry'){renderWizardGeometryStage(root,lods);return;}" not in shell:
    raise AssertionError("wizard decomposition: GEOMETRY branch must remain dispatch-only")
for forbidden in ("wizardGeometryScanBtn", "wizardGeometryConsolidateBtn", "geometryCompareAllBtn", "scan_render_duplicates", "consolidate_render_duplicates"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: GEOMETRY implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardGeometryStageModel", geometry_model), ("wizardGeometryStageHtml", geometry_html)):
    assert_pure_block(name, body)
for token in (
    "wizardGeometryStageModel(state.activeLod,unused,rows,geometryStageVisibleCount())",
    "wizardGeometryStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "switchGeometryLod(Number(btn.dataset.geometryLod))",
    "showAllGeometryStageMeshes",
    "hideAllGeometryStageMeshes",
    "send('scan_render_duplicates'",
    "send('consolidate_render_duplicates'",
    "renderGeometryCandidates()",
    "renderVariantAssignment()",
    "renderGeometryEditor()",
    "bindWizardStageCheckControls('geometry')",
):
    if token not in geometry_adapter:
        raise AssertionError(f"wizard decomposition: GEOMETRY effect adapter missing {token!r}")
if "if(stage==='" in geometry_adapter:
    raise AssertionError("wizard decomposition: GEOMETRY adapter must not own cross-stage dispatch")

# SURFACES: the adapter owns selection normalization, visibility/DOM and backend commands.
# View-model calculation and primary markup remain deterministic pure boundaries.
if "if(stage==='surfaces'){renderWizardSurfacesStage(root,lods);return;}" not in shell:
    raise AssertionError("wizard decomposition: SURFACES branch must remain dispatch-only")
for forbidden in ("wizardSurfaceAnalyzeBtn", "wizardSurfaceGeometryTable", "set_geometry_topology_class", "set_material_definition"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: SURFACES implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardSurfacesStageModel", surfaces_model), ("wizardSurfacesStageHtml", surfaces_html)):
    assert_pure_block(name, body)
for token in (
    "wizardSurfacesStageModel({analysisReady:false",
    "wizardSurfacesStageHtml(model,text,{lodSelector,stageCheck:''})",
    "wizardSurfacesStageModel({analysisReady:true",
    "wizardSurfacesStageHtml(model,text,{lodSelector,stageCheck:wizardStageCheckControls('surfaces')})",
    "switchSurfaceLod(Number(btn.dataset.surfaceLod))",
    "surfaceShowCompleteLod",
    "surfaceHideCompleteLod",
    "send('analyze_model_preflight',{})",
    "send('set_geometry_topology_class'",
    "send('assign_unassigned_material'",
    "send('set_material_definition'",
    "bindWizardStageCheckControls('surfaces')",
):
    if token not in surfaces_adapter:
        raise AssertionError(f"wizard decomposition: SURFACES effect adapter missing {token!r}")
if "if(stage==='" in surfaces_adapter:
    raise AssertionError("wizard decomposition: SURFACES adapter must not own cross-stage dispatch")

# SEMANTICS subwave A: the mega-function is dispatch-only and the derived semantic
# state/counts/tree projection and the main workspace composition live behind pure behavioural boundaries. TREE/BINDINGS markup is pure; event wiring, STRUCTURAL GRAPH and preview effects deliberately remain quarantined in the adapter.
if "if(stage==='semantics'){renderWizardSemanticsStage(root,lods);return;}" not in shell:
    raise AssertionError("wizard decomposition: SEMANTICS branch must remain dispatch-only")
for forbidden in ("semanticCleanLegacyTree", "data-semantic-render-row", "set_render_node_semantic", "semanticGraphExplode"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: SEMANTICS implementation leaked back into mega-function: {forbidden}")
assert_pure_block("wizardSemanticsStageModel", semantics_model)
for name, body in (("wizardSemanticsTreeBlockModel", semantics_tree_model), ("wizardSemanticsTreeRowsHtml", semantics_tree_html), ("wizardSemanticsBindingsBlockModel", semantics_bindings_model), ("wizardSemanticsBindingRowsHtml", semantics_bindings_html), ("wizardSemanticsWorkspaceModel", semantics_workspace_model), ("wizardSemanticsWorkspaceHtml", semantics_workspace_html), ("wizardSemanticsTreeDropPlacement", semantics_tree_drop_placement), ("wizardSemanticsTreeDropValid", semantics_tree_drop_valid), ("wizardSemanticsBindingCommandModel", semantics_binding_command_model), ("wizardSemanticsNewNodeSuggestion", semantics_new_node_suggestion), ("wizardSemanticsPreviewControlModel", semantics_preview_model), ("semanticStructureModeHtml", semantics_structure_mode_html), ("wizardSemanticsStructuralGraphModel", semantics_structural_model), ("structuralGraphPanelHtml", semantics_structural_html), ("structuralGraphMeshRowsHtml", semantics_structural_mesh_rows), ("structuralEndpointCard", semantics_structural_endpoint_card), ("wizardSemanticsStructuralEndpointActionModel", semantics_structural_endpoint_action), ("wizardSemanticsStructuralCreateLinkCommand", semantics_structural_create_link_command), ("wizardSemanticsStructuralLinkUpdateCommand", semantics_structural_link_update_command), ("wizardSemanticsStructuralProxyUpdateCommand", semantics_structural_proxy_update_command), ("wizardSemanticsNodeTransformCommand", semantics_node_transform_command), ("wizardSemanticsMotionAngleModel", semantics_motion_angle_model), ("wizardSemanticsMotionZeroModel", semantics_motion_zero_model), ("wizardSemanticsPreviewRateModel", semantics_preview_rate_model), ("wizardSemanticsJointUpdateCommand", semantics_joint_update_command), ("wizardSemanticsSelectedPanelsModel", semantics_selected_panels_model), ("wizardSemanticsSelectedPanelsHtml", semantics_selected_panels_html)):
    assert_pure_block(name, body)
for forbidden in ("model.treeItems.map(", "(lod.nodes||[]).map((rn,ri)=>", "root.innerHTML=`<div class=\"semanticWorkspace\">", "root.querySelectorAll('[data-semantic-node]')", "root.querySelectorAll('[data-semantic-bind]')", "semanticReparentSelection(target,placement)", "state.semanticGraphEnabled=graphEnabled.checked", "scheduleSemanticGraphPreview()"):
    if forbidden in semantics_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS TREE/BINDINGS implementation leaked back into main effect adapter: {forbidden}")
for token in (
    "wizardSemanticsStageModel({nodes,lod,lods",
    "wizardSemanticsTreeBlockModel({nodes,treeItems:model.treeItems",
    "wizardSemanticsTreeRowsHtml(treeBlock",
    "wizardSemanticsBindingsBlockModel({nodes,lod",
    "wizardSemanticsBindingRowsHtml(bindingsBlock",
    "wizardSemanticsWorkspaceModel({activeLod:state.activeLod",
    "wizardSemanticsWorkspaceHtml(workspaceModel,workspaceText,workspaceFragments)",
    "root.innerHTML=wizardSemanticsWorkspaceHtml",
    "renderWizardStructuralGraphStage(root,lods)",
    "bindSemanticStructureMode(root)",
    "switchSemanticLod(Number(btn.dataset.semanticLod))",
    "bindWizardSemanticsTreeInteractions(root,nodes)",
    "bindWizardSemanticsBindingInteractions(root,selected)",
    "bindWizardSemanticsPreviewInteractions(root)",
    "semanticRefreshSelectionUi()",
    "bindWizardStageCheckControls('semantics')",
):
    if token not in semantics_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS effect adapter missing {token!r}")
if "if(stage==='" in semantics_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS adapter must not own cross-stage dispatch")

# SEMANTICS wave5D: TREE and BINDINGS own their event wiring in separate effect adapters.
# Their drag/drop decision, binding payload and new-node suggestion calculations stay pure.
for token in (
    "wizardSemanticsTreeDropPlacement(e.clientY,r.top,r.height)",
    "wizardSemanticsTreeDropValid(nodes,moving,target,placement)",
    "semanticReparentSelection(target,placement)",
    "send('clean_legacy_semantics'",
    "wizardSemanticsNewNodeSuggestion(nodes,parentIndex,state.asset.assetId)",
    "send('add_semantic_node'",
):
    if token not in semantics_tree_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS TREE effect adapter missing {token!r}")
for token in (
    "data-semantic-render-row",
    "data-semantic-bind",
    "wizardSemanticsBindingCommandModel(state.selectedNode,cb.checked,state.activeLod",
    "send('set_render_node_semantic',command)",
):
    if token not in semantics_binding_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS BINDINGS effect adapter missing {token!r}")
for adapter_name, body in (("TREE", semantics_tree_interactions), ("BINDINGS", semantics_binding_interactions)):
    if "if(stage==='" in body:
        raise AssertionError(f"wizard decomposition: SEMANTICS {adapter_name} adapter must not own cross-stage dispatch")

# SEMANTICS wave5E: preview/explode controls own their DOM/state/THREE scheduling in a
# separate effect adapter. Value normalization/label/disabled decisions remain pure.
for token in (
    "wizardSemanticsPreviewControlModel(graphEnabled.checked,state.semanticGraphExplode)",
    "state.semanticGraphEnabled=preview.graphEnabled",
    "graphExplode.disabled=preview.explodeDisabled",
    "wizardSemanticsPreviewControlModel(state.semanticGraphEnabled,graphExplode.value)",
    "state.semanticGraphExplode=preview.graphExplode",
    "graphValue.textContent=preview.graphExplodeLabel",
    "scheduleSemanticGraphPreview()",
    "graphExplode.onchange=()=>applySemanticMotionPreview(true)",
    "wizardSemanticsPreviewControlModel(state.semanticGraphEnabled,0)",
    "applySemanticMotionPreview()",
):
    if token not in semantics_preview_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS PREVIEW effect adapter missing {token!r}")
if "if(stage==='" in semantics_preview_interactions:
    raise AssertionError("wizard decomposition: SEMANTICS PREVIEW adapter must not own cross-stage dispatch")

# SEMANTICS wave5F: STRUCTURAL GRAPH composition is now an explicit pure model/HTML
# boundary. State normalization, DOM assignment and graph event binding stay in one
# dedicated effect adapter; the main SEMANTICS controller only dispatches to it.
for forbidden in ("structuralGraphPanelHtml(lods)", "bindStructuralGraphPanel(root)", "structGraphReset", "structCreateLink"):
    if forbidden in semantics_adapter:
        raise AssertionError(f"wizard decomposition: STRUCTURAL GRAPH implementation leaked into main SEMANTICS adapter: {forbidden}")
for token in (
    "wizardSemanticsStructuralGraphModel({nodes,links,lod",
    "state.structuralGraphRoot=model.structuralGraphRoot",
    "state.structuralNodeA=model.structuralNodeA",
    "state.structuralNodeB=model.structuralNodeB",
    "state.selectedStructuralLink=model.selectedStructuralLink",
    "semanticStructureModeHtml(state.semanticStructureMode",
    "structuralGraphMeshRowsHtml(model.lod,model.nodes,model.selectedRenderNode,model.structuralGraphRoot,model.wizardStage",
    "structuralEndpointCard('A',model.structuralNodeA,model.nodes",
    "structuralEndpointCard('B',model.structuralNodeB,model.nodes",
    "root.innerHTML=structuralGraphPanelHtml(model,text,fragments)",
    "bindStructuralGraphPanel(root)",
    "applySemanticMotionPreview(true)",
):
    if token not in semantics_structural_adapter:
        raise AssertionError(f"wizard decomposition: STRUCTURAL GRAPH effect adapter missing {token!r}")
if "if(stage==='" in semantics_structural_adapter:
    raise AssertionError("wizard decomposition: STRUCTURAL GRAPH adapter must not own cross-stage dispatch")
for forbidden in (r"\bstate\b", r"\beditorViewState\b", r"\btr\s*\(", r"\bsend\s*\(", r"\.innerHTML\s*=", r"\.onclick\s*="):
    for name, body in (("structural graph model", semantics_structural_model), ("structural graph HTML", semantics_structural_html), ("structural mesh rows", semantics_structural_mesh_rows), ("structural endpoint card", semantics_structural_endpoint_card), ("structure mode HTML", semantics_structure_mode_html), ("structural endpoint action", semantics_structural_endpoint_action), ("structural create-link command", semantics_structural_create_link_command), ("structural link-update command", semantics_structural_link_update_command), ("structural proxy-update command", semantics_structural_proxy_update_command)):
        if re.search(forbidden, body):
            raise AssertionError(f"wizard decomposition: pure {name} regained hidden/effect dependency: {forbidden}")

# SEMANTICS wave5G: bindStructuralGraphPanel is now only a graph-interaction orchestrator.
# Endpoint/link/proxy command decisions are pure; DOM/state/send/THREE effects live in
# dedicated sub-adapters instead of one monolithic structural binder.
for forbidden in ("structMakeRoot", "structSetA", "structCreateLink", "structApplyLink", "structApplyProxy", "send('"):
    if forbidden in semantics_structural_bind_panel:
        raise AssertionError(f"wizard decomposition: STRUCTURAL binder regained concrete interaction implementation: {forbidden!r}")
for token in (
    "bindSemanticStructureMode(root)",
    "bindWizardStructuralGraphViewportInteractions(root)",
    "bindWizardStructuralGraphEndpointInteractions(root)",
    "bindWizardStructuralGraphLinkInteractions(root)",
    "bindWizardStructuralGraphProxyInteractions()",
    "bindWizardStageCheckControls('semantics')",
):
    if token not in semantics_structural_bind_panel:
        raise AssertionError(f"wizard decomposition: STRUCTURAL interaction orchestrator missing {token!r}")
for token in (
    "wizardSemanticsPreviewControlModel(true,range.value)",
    "state.semanticGraphExplode=preview.graphExplode",
    "scheduleSemanticGraphPreview()",
    "applySemanticMotionPreview(true)",
    "structuralSelectRenderNode(Number(row.dataset.structMeshRi))",
):
    if token not in semantics_structural_viewport_interactions:
        raise AssertionError(f"wizard decomposition: STRUCTURAL viewport adapter missing {token!r}")
for token in (
    "wizardSemanticsStructuralEndpointActionModel('root',endpoint",
    "wizardSemanticsStructuralEndpointActionModel('a',endpoint",
    "wizardSemanticsStructuralEndpointActionModel('b',endpoint",
    "state.structuralGraphRoot=action.structuralGraphRoot",
    "state.structuralNodeA=action.structuralNodeA",
    "state.structuralNodeB=action.structuralNodeB",
):
    if token not in semantics_structural_endpoint_interactions:
        raise AssertionError(f"wizard decomposition: STRUCTURAL endpoint adapter missing {token!r}")
for token in (
    "wizardSemanticsStructuralCreateLinkCommand(",
    "send('add_structural_link',command)",
    "wizardSemanticsStructuralLinkUpdateCommand(",
    "send('set_structural_link'",
    "send('delete_structural_link'",
):
    if token not in semantics_structural_link_interactions:
        raise AssertionError(f"wizard decomposition: STRUCTURAL link adapter missing {token!r}")
for token in (
    "wizardSemanticsStructuralProxyUpdateCommand(",
    "readVec('strp')",
    "readVec('strr')",
    "send('set_structural_proxy',payload)",
):
    if token not in semantics_structural_proxy_interactions:
        raise AssertionError(f"wizard decomposition: STRUCTURAL proxy adapter missing {token!r}")
for adapter_name, body in (("viewport", semantics_structural_viewport_interactions), ("endpoint", semantics_structural_endpoint_interactions), ("link", semantics_structural_link_interactions), ("proxy", semantics_structural_proxy_interactions)):
    if "if(stage==='" in body:
        raise AssertionError(f"wizard decomposition: STRUCTURAL {adapter_name} adapter must not own cross-stage dispatch")

# SEMANTICS wave5H: selected-node transform and motion/joint authoring interactions are
# split out of semanticRefreshSelectionUi. DOM/state/send stay effectful; command/value
# normalization is owned by certified pure helpers.
for forbidden in ("send('set_node_transform'", "wizardSemanticDeleteNode", "bindSemanticMotionControls(selected)"):
    if forbidden in semantics_selection_refresh:
        raise AssertionError(f"wizard decomposition: SEMANTICS selection refresh regained selected-node authoring wiring: {forbidden!r}")
if "bindWizardSemanticsSelectedNodeInteractions(selected)" not in semantics_selection_refresh:
    raise AssertionError("wizard decomposition: SEMANTICS selection refresh does not delegate selected-node wiring")
for token in (
    "wizardSemanticsNodeTransformCommand(state.selectedNode,readVec('semPos'),readVec('semRot'),readVec('semPivot'))",
    "send('set_node_transform',command)",
    "semanticDeleteSelectedNode()",
    "bindSemanticMotionControls(selected)",
):
    if token not in semantics_selected_node_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS selected-node adapter missing {token!r}")
for forbidden in ("semanticMotionRange", "semanticPreviewRate", "semanticPivotParent", "semanticApplyJoint"):
    if forbidden in semantics_motion_orchestrator:
        raise AssertionError(f"wizard decomposition: SEMANTICS motion orchestrator regained concrete control wiring: {forbidden!r}")
for token in (
    "bindWizardSemanticsMotionPreviewInteractions()",
    "bindWizardSemanticsJointInteractions(selected)",
    "syncSemanticMotionButtons()",
    "applySemanticMotionPreview()",
):
    if token not in semantics_motion_orchestrator:
        raise AssertionError(f"wizard decomposition: SEMANTICS motion orchestrator missing {token!r}")
for token in (
    "wizardSemanticsMotionAngleModel(a)",
    "wizardSemanticsMotionZeroModel(range?.min,range?.max)",
    "wizardSemanticsPreviewRateModel(previewRate.value)",
    "wizardSemanticsMotionDetachTransition(state.semanticPreviewDetached)",
    "resetSemanticMotionPreview(true)",
):
    if token not in semantics_motion_preview_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS motion-preview adapter missing {token!r}")
for token in (
    "semanticUseParentOriginPivot()",
    "semanticUseVisualCenterPivot()",
    "semanticBeginJointPivotPick()",
    "data-sem-axis",
    "wizardSemanticsJointUpdateCommand(state.selectedNode,kind,readVec('sjp'),rot?readVec('sja'):null",
    "semanticSendJoint(command.nodeIndex,command.overrides)",
):
    if token not in semantics_joint_interactions:
        raise AssertionError(f"wizard decomposition: SEMANTICS joint adapter missing {token!r}")
for adapter_name, body in (("selected-node", semantics_selected_node_interactions), ("motion-preview", semantics_motion_preview_interactions), ("joint", semantics_joint_interactions)):
    if "if(stage==='" in body:
        raise AssertionError(f"wizard decomposition: SEMANTICS {adapter_name} adapter must not own cross-stage dispatch")

# SEMANTICS wave5I: selected-node/motion panel derivation is a pure model + pure HTML
# boundary. Wave5M later removes the compatibility wrapper's hidden state/localization reads.
for forbidden in (r"\bstate\b", r"\beditorViewState\b", r"\btr\s*\(", r"\bsend\s*\(", r"\.innerHTML\s*=", r"\.onclick\s*="):
    for name, body in (("selected panels model", semantics_selected_panels_model), ("selected panels HTML", semantics_selected_panels_html)):
        if re.search(forbidden, body):
            raise AssertionError(f"wizard decomposition: pure {name} regained hidden/effect dependency: {forbidden}")
for forbidden in ("state.semanticPreviewAngleDeg=angle", "const min=Number.isFinite(Number(j.minAngleDeg))", "const pivotUi=`", "const motion=`"):
    if forbidden in semantics_selected_panels_wrapper:
        raise AssertionError(f"wizard decomposition: SEMANTICS selected-panels legacy calculation/markup leaked back into wrapper: {forbidden!r}")

# SEMANTICS wave5J: selection/binding refresh derivation is pure and the three
# binding presentation helpers no longer read state/tr transitively. The refresh
# function remains the explicit DOM/effect shell.
for fn_name, body in (
    ("wizardSemanticsSelectionRefreshModel", semantics_selection_refresh_model),
    ("semanticBindingHealthHtml", semantics_binding_health_html),
    ("semanticBindingRepairHtml", semantics_binding_repair_html),
):
    assert_pure_block(fn_name, body)
summary_entry=certified.get("semanticBindingSummaryHtml")
if not summary_entry or not summary_entry.get("oracle_sha256") or not summary_entry.get("fixtures"):
    raise AssertionError("wizard decomposition: semanticBindingSummaryHtml must have a frozen behavioural oracle")
for forbidden in ("bindingCounts=new Map()", "semanticTopLevelSelected(", "semanticCurrentLodUnbound(", ".filter(item=>item.loaded&&!(item.nodes||[]).some"):
    if forbidden in semantics_selection_refresh:
        raise AssertionError(f"wizard decomposition: SEMANTICS selection refresh regained derived binding/selection calculation: {forbidden!r}")
for token in (
    "wizardSemanticsSelectionRefreshModel({nodes,lods,lod,selectedNode:state.selectedNode",
    "semanticBindingRepairHtml(selected,model.selectedVisualCount,model.bindingPickActive",
    "semanticBindingHealthHtml(state.selectedNode,lods,model.unboundCurrent,selected",
    "semanticBindingSummaryHtml(state.selectedNode,lods,state.activeLod",
    "bindingDetails.open=!!model.bindingDetailsOpen",
    "cb.disabled=model.bindingDisabled",
    "cb.checked=!!bindingModel?.checked",
):
    if token not in semantics_selection_refresh:
        raise AssertionError(f"wizard decomposition: SEMANTICS selection refresh effect shell missing {token!r}")
for body_name, body in (("summary", semantics_binding_summary_html), ("health", semantics_binding_health_html), ("repair", semantics_binding_repair_html)):
    for forbidden in ("state.", "tr(", "editorViewState", "send(", ".innerHTML", ".onclick"):
        if forbidden in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS pure binding {body_name} helper regained effect dependency {forbidden!r}")


# SEMANTICS wave5K: the remaining low-risk transform/graph presentation helpers no
# longer read editor state or localization implicitly. The Three.js math functions are
# dynamically oracle-certified through the vendored Three.js module, not only marked
# statically pure.
for fn_name, body in (
    ("semanticRelationLabel", semantics_relation_label),
    ("semanticRenderBaseMatrix", semantics_render_base_matrix),
    ("semanticCanonicalRenderWorldMatrices", semantics_canonical_render_world_matrices),
    ("semanticUnboundRenderClusterOffsets", semantics_unbound_cluster_offsets),
    ("semanticGraphDirection", semantics_graph_direction),
    ("socketLocalMatrix", semantics_socket_local_matrix),
    ("composeMatrix", semantics_compose_matrix),
    ("deg", semantics_deg),
):
    assert_pure_block(fn_name, body)
if "tr(" in semantics_relation_label:
    raise AssertionError("wizard decomposition: semanticRelationLabel regained implicit localization")
for token in (
    "const relationText={root:tr('model_editor.semantics.relation.root'",
    "semanticRenderBaseMatrix(i,lod,semanticNodes,stateVariants,state.previewStates)",
    "semanticCanonicalRenderWorldMatrices({lod,semanticNodes:nodes,stateVariants,previewStates,rootWorld:state.root.matrixWorld})",
    "semanticUnboundRenderClusterOffsets({lod,amount,semanticNodes:state.asset?.nodes||[],stateVariants:state.asset?.stateVariants||[],previewStates:state.previewStates,rootWorld:state.root.matrixWorld,minBounds:state.asset?.minBounds||[0,0,0],maxBounds:state.asset?.maxBounds||[1,1,1]})",
):
    if token not in WEB:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5K explicit-input adapter wiring missing {token!r}")

# SEMANTICS wave5L: canonical semantic world/display transforms, radial graph layout,
# preview delta and socket transforms are value functions. Runtime scene mutation remains
# in the existing apply/rebuild adapters.
for fn_name, body in (
    ("semanticWorldMatrix", semantics_world_matrix),
    ("semanticJointLocalPointFromWorld", semantics_joint_local_point),
    ("semanticPreviewDeltaWorld", semantics_preview_delta_world),
    ("semanticDisplayWorldMatrix", semantics_display_world_matrix),
    ("semanticGraphRootCenter", semantics_graph_root_center),
    ("semanticGraphRadialMetrics", semantics_graph_radial_metrics),
    ("semanticGraphLayoutOffsets", semantics_graph_layout_offsets),
    ("semanticGraphDisplayAnchorMap", semantics_graph_display_anchor_map),
    ("socketWorldMatrix", semantics_socket_world_matrix),
    ("socketCanonicalWorldMatrix", semantics_socket_canonical_world_matrix),
):
    assert_pure_block(fn_name, body)
for forbidden in ("state.", "activeRenderLod(", "updateMatrixWorld("):
    for fn_name, body in (
        ("semanticWorldMatrix", semantics_world_matrix),
        ("semanticPreviewDeltaWorld", semantics_preview_delta_world),
        ("semanticDisplayWorldMatrix", semantics_display_world_matrix),
        ("semanticGraphRootCenter", semantics_graph_root_center),
        ("semanticGraphRadialMetrics", semantics_graph_radial_metrics),
        ("semanticGraphLayoutOffsets", semantics_graph_layout_offsets),
        ("semanticGraphDisplayAnchorMap", semantics_graph_display_anchor_map),
        ("socketWorldMatrix", semantics_socket_world_matrix),
        ("socketCanonicalWorldMatrix", semantics_socket_canonical_world_matrix),
    ):
        if forbidden in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS wave5L pure transform helper {fn_name} regained adapter dependency {forbidden!r}")
for token in (
    "semanticJointLocalPointFromWorld(i,input?.worldPoint,nodes,input?.stateVariants||[],input?.previewStates)",
    "semanticGraphLayoutOffsets(canonicalAnchors,amount,{nodes:state.asset?.nodes||[],lod,stateVariants:",
    "semanticGraphDisplayAnchorMap(canonicalAnchors,{nodes:state.asset?.nodes||[],stateVariants:",
    "socketCanonicalWorldMatrix(s,state.asset?.nodes||[],state.asset?.stateVariants||[],state.previewStates)",
):
    if token not in WEB:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5L explicit-input adapter wiring missing {token!r}")

# SEMANTICS wave5M: the two residual state-mutation-only SEMANTICS helpers become
# certified explicit-input value functions. Root matrix refresh and preview-angle writes
# remain in their already-effectful callers; no new effect adapter is introduced.
for fn_name, body in (
    ("semanticCanonicalNodeAnchorMap", semantics_canonical_node_anchor_map),
    ("semanticSelectedPanels", semantics_selected_panels_wrapper),
):
    assert_pure_block(fn_name, body)
for forbidden in ("state.", "tr(", "updateMatrixWorld("):
    for fn_name, body in (
        ("semanticCanonicalNodeAnchorMap", semantics_canonical_node_anchor_map),
        ("semanticSelectedPanels", semantics_selected_panels_wrapper),
    ):
        if forbidden in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS wave5M pure helper {fn_name} regained adapter dependency {forbidden!r}")
for token in (
    "const panelModel=wizardSemanticsSelectedPanelsModel({selected,nodes",
    "if(panelModel.hasParent)state.semanticPreviewAngleDeg=panelModel.previewAngleDeg",
    "semanticSelectedPanels(selected,panelModel,panelText,relationText)",
    "semanticCanonicalNodeAnchorMap(nodes,anchorLod,anchorWorlds,stateVariants,previewStates)",
):
    if token not in semantics_selection_refresh and token.startswith("const panelModel"):
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5M selected-panel effect wiring missing {token!r}")
    if token not in semantics_selection_refresh and token.startswith("if(panelModel"):
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5M selected-panel preview write missing {token!r}")
    if token not in semantics_selection_refresh and token.startswith("semanticSelectedPanels"):
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5M selected-panel pure call missing {token!r}")
    if token.startswith("semanticCanonicalNodeAnchorMap") and token not in WEB:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5M canonical-anchor adapter wiring missing {token!r}")

# SEMANTICS wave5N: command/selection effect shells retain only state/DOM/status/send
# work. Selection-range calculation, DnD validation, relation payloads, MODEL ROOT
# commands and delete confirmation derivation are frozen pure decision boundaries.
for fn_name, body in (
    ("wizardSemanticsSelectionTransition", semantics_selection_transition),
    ("wizardSemanticsReparentCommand", semantics_reparent_command),
    ("wizardSemanticsRelationCommand", semantics_relation_command),
    ("wizardSemanticsMoveToAssetSpaceCommand", semantics_move_to_asset_command),
    ("wizardSemanticsFlattenStaticTreeCommand", semantics_flatten_static_command),
    ("wizardSemanticsDeletePlan", semantics_delete_plan),
    ("wizardSemanticsDeleteConfirmText", semantics_delete_confirm_text),
):
    assert_pure_block(fn_name, body)

for adapter_name, body, required, forbidden in (
    ("selection", semantics_select_node_adapter, "wizardSemanticsSelectionTransition({", ("if(mode==='range'", "ordered.indexOf(", "semanticTreeRows(")),
    ("reparent", semantics_reparent_adapter, "wizardSemanticsReparentCommand({", ("semanticTopLevelSelected(", "semanticCanUseParent(")),
    ("relation", semantics_relation_adapter, "wizardSemanticsRelationCommand(state.asset?.nodes||[],index,kind)", ("kind==='rotating'", "defaultRateDegPerSec:Number(j.defaultRateDegPerSec")),
    ("move-to-root", semantics_move_to_asset_adapter, "wizardSemanticsMoveToAssetSpaceCommand({", ("semanticTopLevelSelected(", ".filter(i=>Number(nodes[i]?.parentIndex)>=0)")),
    ("flatten-static", semantics_flatten_static_adapter, "wizardSemanticsFlattenStaticTreeCommand(state.asset?.nodes||[],state.asset?.stateVariants||[])", ("semanticStaticFlattenCandidates(",)),
    ("delete", semantics_delete_adapter, "wizardSemanticsDeletePlan(state.asset?.nodes||[],state.selectedNode)", ("semanticUsageForNode(", "semanticOwnedPayloadCount(", "const payload=", "lines=[")),
):
    if required not in body:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5N {adapter_name} adapter missing pure decision boundary {required!r}")
    for token in forbidden:
        if token in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS wave5N {adapter_name} adapter regained decision logic {token!r}")
if "wizardSemanticsDeleteConfirmText(plan,{" not in semantics_delete_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5N delete adapter missing pure confirmation-text builder")

# SEMANTICS wave5O: joint presets/pick mode/runtime-rate and visual binding pick/assignment
# decisions are explicit-input PURE values. Raycasting, root refresh, DOM/status, state
# mutation and backend sends stay in the pre-existing effect shells.
for fn_name, body in (
    ("wizardSemanticsJointPivotCommand", semantics_joint_pivot_command),
    ("wizardSemanticsParentOriginPivotRequest", semantics_parent_origin_pivot_request),
    ("wizardSemanticsVisualCenterPivotEligibility", semantics_visual_center_pivot_eligibility),
    ("wizardSemanticsVisualCenterPivotRequest", semantics_visual_center_pivot_request),
    ("wizardSemanticsJointPivotPickTransition", semantics_joint_pivot_pick_transition),
    ("wizardSemanticsNominalRateCommand", semantics_nominal_rate_command),
    ("wizardSemanticsBindingPickTransition", semantics_binding_pick_transition),
    ("wizardSemanticsBindingAssignmentCommand", semantics_binding_assignment_command),
):
    assert_pure_block(fn_name, body)

for adapter_name, body, required, forbidden in (
    ("joint-pivot", semantics_joint_pivot_adapter, "wizardSemanticsJointPivotCommand({", ("semanticJointLocalPointFromWorld(", "semanticJointPayload(")),
    ("parent-origin", semantics_parent_origin_pivot_adapter, "wizardSemanticsParentOriginPivotRequest({", ("new THREE.Vector3().setFromMatrixPosition", "semanticWorldMatrix(")),
    ("visual-center", semantics_visual_center_pivot_adapter, "wizardSemanticsVisualCenterPivotRequest({", (".get(i)",)),
    ("pivot-pick", semantics_joint_pivot_pick_adapter, "wizardSemanticsJointPivotPickTransition({", ("const i=Number(state.selectedNode)", "const n=state.asset?.nodes?.[i]")),
    ("nominal-rate", semantics_nominal_rate_adapter, "wizardSemanticsNominalRateCommand(state.asset?.nodes||[],state.selectedNode,value)", ("Number.isFinite(rate)", "String(j.type||'fixed')")),
):
    if required not in body:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5O {adapter_name} adapter missing pure boundary {required!r}")
    for token in forbidden:
        if token in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS wave5O {adapter_name} adapter regained derivation logic {token!r}")

if "wizardSemanticsVisualCenterPivotEligibility(nodes,state.selectedNode)" not in semantics_visual_center_pivot_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5O visual-center adapter must preserve pre-refresh eligibility guard")
if "state.root.updateMatrixWorld(true)" not in semantics_visual_center_pivot_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5O visual-center adapter lost root-world refresh effect")
if "wizardSemanticsBindingPickTransition(state.selectedNode,selected.id,state.semanticBindingPickTarget)" not in semantics_selection_refresh:
    raise AssertionError("wizard decomposition: SEMANTICS wave5O selection-refresh binding pick missing PURE transition")
if "wizardSemanticsBindingPickTransition(state.selectedNode,selected.id,state.semanticBindingPickTarget)" not in semantics_binding_interactions_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5O binding-interactions adapter missing PURE transition")
if "wizardSemanticsBindingAssignmentCommand({" not in semantics_viewport_mesh_pick_decision:
    raise AssertionError("wizard decomposition: SEMANTICS wave5O viewport binding assignment missing PURE payload builder in viewport decision boundary")
for forbidden in ("targetNode=state.asset?.nodes?.[target]", "send('set_render_node_semantic',{lodIndex:"):
    if forbidden in semantics_pick_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5O viewport binding adapter regained payload derivation {forbidden!r}")

# SEMANTICS wave5P: runtime motion advancement and TREE/LOD selection state transitions
# are explicit-input PURE values. DOM event wiring, editorViewState synchronization,
# animation preview application and rendering remain effectful adapters.
for fn_name, body in (
    ("wizardSemanticsMotionAnimationStep", semantics_motion_animation_step),
    ("wizardSemanticsMotionPlayTransition", semantics_motion_play_transition),
    ("wizardSemanticsMotionDetachTransition", semantics_motion_detach_transition),
    ("wizardSemanticsCollapsedTransition", semantics_collapsed_transition),
    ("wizardSemanticsTreeDragStartTransition", semantics_tree_drag_start_transition),
    ("wizardSemanticsStageSelectionNormalization", semantics_stage_selection_normalization),
    ("wizardSemanticsLodSelectionRestoreModel", semantics_lod_selection_restore_model),
):
    assert_pure_block(fn_name, body)

for adapter_name, body, required, forbidden in (
    ("motion-animation", semantics_motion_animation_adapter, "wizardSemanticsMotionAnimationStep({", ("const dt=Math.min(.05", "while(next>hi)", "state.semanticMotionDirection=-1")),
    ("tree-drag", semantics_tree_interactions_adapter, "wizardSemanticsTreeDragStartTransition({", ("if(!state.semanticSelectedNodes.has(target))", "setData('text/plain',semanticTopLevelSelected")),
    ("tree-collapse", semantics_tree_interactions_adapter, "wizardSemanticsCollapsedTransition([...state.semanticCollapsed],id)", ("state.semanticCollapsed.delete(String(id))", "state.semanticCollapsed.add(String(id))")),
    ("stage-selection", semantics_semantics_stage_adapter, "wizardSemanticsStageSelectionNormalization({", ("state.selectedNode<0||state.selectedNode>=nodes.length", "if(i<0||i>=nodes.length)state.semanticSelectedNodes.delete(i)")),
    ("lod-selection", semantics_lod_selection_restore_adapter, "wizardSemanticsLodSelectionRestoreModel({", ("Number.isInteger(Number(i))&&Number(i)>=0", ".findIndex(rn=>Number(rn?.semanticNodeIndex)===validPrimary)")),
):
    if required not in body:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5P {adapter_name} adapter missing pure boundary {required!r}")
    for token in forbidden:
        if token in body:
            raise AssertionError(f"wizard decomposition: SEMANTICS wave5P {adapter_name} adapter regained transition derivation {token!r}")
if "wizardSemanticsMotionPlayTransition(state.semanticMotionPlaying)" not in semantics_motion_preview_interactions:
    raise AssertionError("wizard decomposition: SEMANTICS wave5P motion play adapter missing PURE transition")
if "wizardSemanticsMotionDetachTransition(state.semanticPreviewDetached)" not in semantics_motion_preview_interactions:
    raise AssertionError("wizard decomposition: SEMANTICS wave5P detach adapter missing PURE transition")
if "wizardSemanticsCollapsedTransition([...state.semanticCollapsed],'__ASSET_SPACE__')" not in semantics_tree_interactions_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5P MODEL ROOT collapse missing PURE transition")

# SEMANTICS wave5Q: graph explode and motion preview application plans are explicit-input PURE
# matrix plans. THREE matrix copies/updateMatrixWorld and downstream gizmo/collision/socket rebuilds
# remain effectful adapters.
for fn_name, body in (
    ("wizardSemanticsGraphExplodeApplicationPlan", semantics_graph_explode_application_plan),
    ("wizardSemanticsMotionPreviewTargetModel", semantics_motion_preview_target_model),
    ("wizardSemanticsMotionPreviewApplicationPlan", semantics_motion_preview_application_plan),
):
    assert_pure_block(fn_name, body)

if "wizardSemanticsGraphExplodeApplicationPlan({" not in semantics_graph_explode_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5Q graph explode adapter missing PURE application plan")
for forbidden in ("const desired=[]", "const done=new Set(),apply=i=>", "parentWorld.invert().multiply(desired[i])"):
    if forbidden in semantics_graph_explode_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5Q graph explode adapter regained matrix-plan derivation {forbidden!r}")
if "wizardSemanticsMotionPreviewTargetModel(state.asset?.nodes||[],lod,state.selectedNode)" not in semantics_motion_preview_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5Q motion preview adapter missing PURE target model")
if "wizardSemanticsMotionPreviewApplicationPlan({" not in semantics_motion_preview_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5Q motion preview adapter missing PURE application plan")
for forbidden in ("const selectedSet=semanticSelectionSet", "targets=new Set()", "parentIndex>=0&&targets.has(parentIndex)", "parentWorld.invert().multiply(desired)"):
    if forbidden in semantics_motion_preview_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5Q motion preview adapter regained target/matrix planning {forbidden!r}")
for required in ("g.matrix.copy(item.localMatrix)", "g.updateMatrixWorld(true)"):
    if required not in semantics_graph_explode_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5Q graph adapter lost THREE effect {required!r}")
if "g.matrix.copy(item.localMatrix)" not in semantics_motion_preview_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5Q motion adapter lost THREE matrix-copy effect")

# SEMANTICS wave5R: graph object topology, graph/joint gizmo presentation and viewport
# semantic routing are explicit-input PURE plans. THREE object construction/application,
# raycasting, state mutation and backend/status effects remain in the existing adapters.
for fn_name, body in (
    ("wizardSemanticsGraphObjectModel", semantics_graph_object_model),
    ("wizardSemanticsGraphGizmoPlan", semantics_graph_gizmo_plan),
    ("wizardSemanticsJointGizmoPlan", semantics_joint_gizmo_plan),
    ("wizardSemanticsViewportGizmoPickDecision", semantics_viewport_gizmo_pick_decision),
    ("wizardSemanticsViewportMeshPickDecision", semantics_viewport_mesh_pick_decision),
):
    assert_pure_block(fn_name, body)

if "wizardSemanticsGraphObjectModel(nodes,structuralLinks,state.semanticGraphStructureKey,state.semanticGraphNodeObjects.size)" not in semantics_graph_objects_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5R graph-object adapter missing PURE topology model")
for forbidden in ("signature=semanticGraphStructureSignature", "for(let i=0;i<nodes.length;i++){const p=Number(nodes[i].parentIndex)"):
    if forbidden in semantics_graph_objects_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R graph-object adapter regained topology derivation {forbidden!r}")
if "wizardSemanticsGraphGizmoPlan({" not in semantics_graph_gizmos_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5R graph-gizmo adapter missing PURE presentation plan")
for forbidden in ("visualCounts=new Map()", "selectedMarker=selected&&!hasVisual", "kind=semanticRelationKind(nodes[i])"):
    if forbidden in semantics_graph_gizmos_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R graph-gizmo adapter regained presentation derivation {forbidden!r}")
if "wizardSemanticsJointGizmoPlan({" not in semantics_joint_gizmos_adapter:
    raise AssertionError("wizard decomposition: SEMANTICS wave5R joint-gizmo adapter missing PURE presentation plan")
for forbidden in ("canonicalWorld=semanticWorldMatrix", "rawA0=deg(", "new THREE.Vector3(...(j.axis"):
    if forbidden in semantics_joint_gizmos_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R joint-gizmo adapter regained geometry derivation {forbidden!r}")
for required in ("new THREE.Mesh(", "new THREE.Line(", "new THREE.ArrowHelper("):
    if required not in semantics_joint_gizmos_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R joint-gizmo adapter lost THREE effect {required!r}")
for required in ("wizardSemanticsViewportGizmoPickDecision({", "wizardSemanticsViewportMeshPickDecision({"):
    if required not in semantics_pick_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R viewport adapter missing PURE routing boundary {required!r}")
for forbidden in ("if(structHit&&state.semanticStructureMode==='graph')", "const command=wizardSemanticsBindingAssignmentCommand({nodes:state.asset?.nodes"):
    if forbidden in semantics_pick_adapter:
        raise AssertionError(f"wizard decomposition: SEMANTICS wave5R viewport adapter regained routing derivation {forbidden!r}")

print("[PASS] Model Asset Editor wizard decomposition: SOURCE + LODS + GEOMETRY + SURFACES extracted; SEMANTICS core/TREE/BINDINGS/WORKSPACE/PREVIEW/STRUCTURAL/SELECTED-PANELS/SELECTION-REFRESH/TRANSFORM-MATH/WORLD-GRAPH-MATH/RESIDUAL-DERIVATION/COMMAND-DECISIONS/JOINT-PICK-BINDING-DECISIONS/TREE-MOTION-TRANSITIONS/PREVIEW-APPLICATION-PLANS/GRAPH-VIEWPORT-PLANS pure boundaries + isolated TREE/BINDINGS/PREVIEW/STRUCTURAL/SELECTED-MOTION/COMMAND/JOINT-PICK-BINDING/TREE-MOTION/PREVIEW-APPLICATION/GRAPH-VIEWPORT effect shells")
