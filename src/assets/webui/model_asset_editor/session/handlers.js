function createEditorSessionMessageHandlers({
 state,$,editorViewState,
 handleWorkingSaved,handleSettings,handleSettingsSaved,handleSettingsStatusError,
 applySemanticTreePatch,applySemanticBindingPatch,applyLodRuntimeMetadataPatch,applySurfaceMetadataPatch,
 beginEditorAssetBinary,beginEditorLodBinary,acceptAssetState,
 rebuildScene,renderWizard,renderWizardPanel,applyWizardVisibility,renderLodFiles,renderActiveLodDetails,
 renderStorage,updateActionAvailability,localStatus,renderGeometryCandidates,settleSourceChangeScanAfterCheck,
 setWizardStage,wizardStageLabel,notice,tr,renderCatalog,progress,clearProgress,translateServerMessage,status,
 setTimeoutFn,assertEditorViewInvariant
}){
 return Object.freeze({
  source_directory_updated:msg=>{
   state.sourceChangeScan=null;
   if(state.asset){
    state.asset.sourceAssetDirectory=msg.sourceAssetDirectory||'';
    state.asset.sourceAssetRoot=msg.sourceAssetRoot||'';
   }
   if(state.wizardStage==='source')renderWizardPanel();
  },
  working_saved:handleWorkingSaved,
  source_change_scan_result:msg=>{
   state.sourceChangeScan=msg;
   if(state.wizardStage==='source')renderWizardPanel();
  },
  source_mesh_deletion_confirmed:msg=>{
   const li=Number(msg.lodIndex),geometryId=String(msg.geometryId||'');
   if(state.sourceChangeScan&&Array.isArray(state.sourceChangeScan.rows)){
    state.sourceChangeScan={
     ...state.sourceChangeScan,
     rows:state.sourceChangeScan.rows.filter(row=>!(String(row.kind||'')==='missing_source'&&Number(row.lodIndex)===li&&String(row.geometryId||'')===geometryId)),
     missingSource:Math.max(0,Number(state.sourceChangeScan.missingSource||0)-1)
    };
   }
   const visible=state.geometryInventoryVisibleByLod.get(li);
   if(visible instanceof Set)visible.delete(geometryId);
   if(String(state.geometryInventorySelectedId||'')===geometryId)state.geometryInventorySelectedId=null;
   if(state.wizardStage==='source')renderWizardPanel();
  },
  geometry_preflight_result:msg=>{
   state.geometryPartPreflight=msg;
   renderWizardPanel();
  },
  wizard_validation_report:msg=>{
   state.wizardValidationReport=msg;
   renderWizardPanel();
  },
  semantic_tree_patch:applySemanticTreePatch,
  semantic_binding_patch:applySemanticBindingPatch,
  lod_runtime_metadata_patch:applyLodRuntimeMetadataPatch,
  surface_metadata_patch:applySurfaceMetadataPatch,
  asset_binary_begin:beginEditorAssetBinary,
  lod_payload_binary_begin:beginEditorLodBinary,
  asset_metadata:msg=>acceptAssetState(msg,false),
  model_preflight_result:msg=>{
   state.modelPreflight={...msg,cachedAfterGeometryChange:false};
   if(state.surfaceAnalysisRequested){
    state.surfaceAnalysisRequested=false;
    state.surfaceAnalysisReady=true;
    rebuildScene(true);
   }
   renderWizardPanel();
  },
  lod_analysis_result:msg=>{
   state.lodAnalysis=msg;
   state.lodGeneratorLevel=0;
   state.lodGeneratorApplyLevels=new Set((msg.levels||[]).map(item=>Number(item.level)).filter(level=>level>0));
   state.lodGeneratorAppliedLevels.clear();
   state.lodGeneratorPendingApplyLevels.clear();
   state.lodGeneratorApplying=false;
   state.lodGeneratorMeshSelection='all';
   rebuildScene(true);
   renderWizardPanel();
  },
  lod_generator_preview_result:msg=>{
   state.lodGeneratorPreview=msg;
   state.lodPreviewGeometryCache.clear();
   rebuildScene(true);
   renderWizardPanel();
  },
  lod_generator_apply_result:msg=>{
   state.lodGeneratorApplying=false;
   state.lodGeneratorAppliedLevels=new Set((msg.levels||[]).map(item=>Number(item.level)).filter(level=>level>0));
   state.lodGeneratorPendingApplyLevels.clear();
   state.lodGeneratorPreview=null;
   state.lodPreviewGeometryCache.clear();
   rebuildScene(true);
   renderWizard();
   renderWizardPanel();
   renderLodFiles();
   renderActiveLodDetails();
   updateActionAvailability();
   notice(`${tr('model_editor.lod_generator.applied','Generated LODs applied')} · ${tr('model_editor.lod_generator.replaced','replaced')} ${Number(msg.replaced||0)} · ${tr('model_editor.lod_generator.created','created')} ${Number(msg.created||0)}`,false);
  },
  lod_payload:msg=>{
   const li=Number(msg.lodIndex);
   if(!state.asset||!Number.isInteger(li)||li<0||!msg.lod)return;
   if(!Array.isArray(state.asset.renderLods))state.asset.renderLods=[];
   for(const [key,geometry] of [...state.geometryCache.entries()]){
    if(key.startsWith(`${li}:`)){
     geometry.dispose?.();
     state.geometryCache.delete(key);
    }
   }
   state.asset.renderLods[li]=msg.lod;
   state.dirty=!!msg.dirty;
   editorViewState.syncResidency(state.asset);
   const activate=state.pendingActiveLod===li;
   if(activate){
    state.activeLod=li;
    state.pendingActiveLod=null;
    rebuildScene(true);
   }else if(state.sceneLod===state.activeLod){
    assertEditorViewInvariant('lod-payload-cache');
   }
   renderLodFiles();
   renderWizardPanel();
   renderActiveLodDetails();
   updateActionAvailability();
   if(activate)localStatus(`LOD${li}: ${tr('model_editor.action.active','ACTIVE')}`);
  },
  geometry_scan_result:msg=>{
   state.geometryScan=msg;
   renderGeometryCandidates();
  },
  wizard_state_patch:msg=>{
   state.dirty=!!msg.dirty;
   if(state.asset&&msg.wizard)state.asset.wizard=msg.wizard;
   renderWizard();
   renderWizardPanel();
   applyWizardVisibility();
   updateActionAvailability();
  },
  wizard_stage_checked:msg=>{
   settleSourceChangeScanAfterCheck(msg.stage);
   const next=msg.nextStage||null;
   if(next==='surfaces'){
    state.surfaceAnalysisReady=false;
    state.surfaceAnalysisRequested=false;
   }
   if(next)setWizardStage(next,true);
   else{
    renderWizard();
    renderWizardPanel();
    applyWizardVisibility();
   }
   notice(`${tr('model_editor.wizard.checked_notice','Stage check passed')} · ${wizardStageLabel(msg.stage)}`,false);
  },
  settings:handleSettings,
  settings_saved:handleSettingsSaved,
  catalog:msg=>{
   state.catalog=msg.items||[];
   $('versionBadge').textContent=`v${msg.editorVersion||'?'} · asset v${msg.assetFormatVersion||'?'}`;
   renderCatalog();
  },
  progress:msg=>{
   state.dirty=!!msg.dirty;
   progress(msg.activity||'reading',msg.stage||'WORKING',msg.completed||0,msg.total||0,msg.path||'');
  },
  status:msg=>{
   state.dirty=!!msg.dirty;
   const localizedMessage=translateServerMessage(msg.message);
   if(state.asset?.storage){
    if(msg.path)state.asset.storage.binaryPath=msg.path;
    if(Number.isFinite(Number(msg.bytes)))state.asset.storage.manifestBytes=Number(msg.bytes);
    renderStorage();
   }
   status(localizedMessage,msg.error?'danger':state.dirty?'dirty':'ok',msg.activity||'idle',msg.path||'',msg.bytes);
   if(msg.error&&state.lodGeneratorApplying){
    state.lodGeneratorApplying=false;
    state.lodGeneratorPendingApplyLevels.clear();
    renderWizardPanel();
   }
   if(msg.error)handleSettingsStatusError(localizedMessage);
   if(msg.error||(msg.activity||'idle')==='idle'){
    if(state.ignoreNextStatusNotice&&!msg.error)state.ignoreNextStatusNotice=false;
    else notice(localizedMessage,!!msg.error);
   }
  },
  asset:msg=>{
   progress('reading','LOAD VIEW',0,1,'building viewport buffers');
   acceptAssetState(msg,true);
   progress('reading','LOAD VIEW',1,1,'viewport ready');
   setTimeoutFn(clearProgress,80);
  }
 });
}

export {createEditorSessionMessageHandlers};
