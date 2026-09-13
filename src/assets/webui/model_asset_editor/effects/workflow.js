// Top-level workflow transition effect adapter.
// State ownership stays in app/controller + reducer/store; this module performs
// the DOM/scene redraw consequences of an accepted state transition.
function createWorkflowEffects({
 state,document,wizardStageIds,wizardStageAllowed,wizardStageLabel,tr,localStatus,
 captureEditorViewTransition,resetSemanticMotionPreview,clearLodGeneratorPreview,
 rebuildScene,renderWizard,renderWizardPanel,applyWizardVisibility,renderSharedStageMeshPanel,
 renderRenderTree,renderGeometries,renderRenderNodeInspector,renderTree,renderNodeInspector,
 renderCollisionList,renderCollisionInspector,renderSocketList,renderSocketInspector,
 renderDamageSemantics,highlightSelection,renderLodFiles,renderActiveLodDetails,
 assertEditorViewTransitionPreserved,assertEditorViewInvariant
}){
 const stageRefresh=Object.freeze({
  geometry:Object.freeze([renderRenderTree,renderGeometries,renderRenderNodeInspector]),
  semantics:Object.freeze([renderTree,renderNodeInspector,renderRenderTree,renderRenderNodeInspector,renderCollisionList,renderCollisionInspector,renderSocketList,renderSocketInspector,renderDamageSemantics,highlightSelection]),
  physics:Object.freeze([renderTree,renderNodeInspector,renderRenderTree,renderRenderNodeInspector,renderCollisionList,renderCollisionInspector,renderSocketList,renderSocketInspector,renderDamageSemantics,highlightSelection]),
  damage:Object.freeze([renderTree,renderNodeInspector,renderRenderTree,renderRenderNodeInspector,renderCollisionList,renderCollisionInspector,renderSocketList,renderSocketInspector,renderDamageSemantics,highlightSelection])
 });
 const lodDetailStages=new Set(['lods','geometry','surfaces','semantics','physics','damage','validate','build']);

 function applyStageChrome(id){
  document.body.classList.toggle('semanticsWorkspaceWide',id==='semantics');
 }

 function renderStageShell(){
  renderWizard();
  renderWizardPanel();
  applyWizardVisibility();
  renderSharedStageMeshPanel();
 }

 function refreshStageOwnedViews(id){
  for(const render of stageRefresh[id]||[])render();
  if(lodDetailStages.has(id)){
   renderLodFiles();
   renderActiveLodDetails();
  }
 }

 function setWizardStage(id,force=false){
  if(!wizardStageIds.includes(id))return;
  if(!force&&!wizardStageAllowed(state.asset?.wizard?.stages,id)){
   localStatus(`${wizardStageLabel(id)}: ${tr('model_editor.wizard.locked','LOCKED')}`,'danger');
   return;
  }
  const previousStage=state.wizardStage;
  if(previousStage===id){
   applyStageChrome(id);
   renderStageShell();
   assertEditorViewInvariant('stage-reselect');
   return;
  }

  const viewBefore=captureEditorViewTransition();
  if(previousStage==='semantics'&&id!=='semantics')resetSemanticMotionPreview(false);
  if(id!=='lods')clearLodGeneratorPreview(false,false);

  // Compatibility assignment dispatches through ApplicationController ->
  // action -> reducer/store; this adapter does not own workflow state.
  state.wizardStage=id;
  applyStageChrome(id);
  state.geometryScan=null;
  if(id!=='validate')state.wizardValidationReport=null;

  rebuildScene(true);
  renderStageShell();
  refreshStageOwnedViews(id);
  assertEditorViewTransitionPreserved(viewBefore,`tab:${previousStage}->${id}`);
  assertEditorViewInvariant(`tab:${previousStage}->${id}`);
 }

 return {setWizardStage};
}

export {createWorkflowEffects};
