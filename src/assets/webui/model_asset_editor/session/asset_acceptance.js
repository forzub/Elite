function retainGeometryPayload(target,source){
 if(!target||!source)return;
 for(const key of ['positions','normals','indices','triangleMaterials','smoothingGroups','edges','rawSource']){
  if(source[key]!==undefined)target[key]=source[key];
 }
}

function cloneGeometryPayload(target,source){
 if(!target||!source)return;
 for(const key of ['positions','normals','indices','triangleMaterials','smoothingGroups'])target[key]=source[key]||[];
 target.edges=Array.isArray(source.edges)?source.edges.map(edge=>({...edge})):[];
}

function createAssetAcceptanceEffects({
 state,editorViewState,$,activeRenderLod,activeLodAxisMapping,tr,
 updateWorkingSaveStamp,updateAxisLegend,rebuildScene,fitView,pruneGeometryCache,
 highlightSelection,renderWizard,renderWizardPanel,applyWizardVisibility,renderLodFiles,
 renderStorage,renderCollisionList,renderCollisionInspector,renderSocketList,renderSocketInspector,
 renderMaterials,updateActionAvailability
}){
 function mergeAssetMetadata(next,msg){
  const previous=state.asset;
  if(!previous||previous.assetId!==next?.assetId)return next;
  const invalidated=new Set((msg.invalidatedLodPayloads||[]).map(Number));
  for(const li of invalidated){
   for(const [key,geo] of [...state.geometryCache.entries()]){
    if(key.startsWith(`${li}:`)){
     geo.dispose?.();
     state.geometryCache.delete(key);
    }
   }
  }
  const previousLods=previous.renderLods||[];
  for(let li=0;li<(next.renderLods||[]).length;li++){
   const nextLod=next.renderLods[li],previousLod=previousLods[li];
   if(!nextLod||!previousLod||invalidated.has(li))continue;
   const byId=new Map((previousLod.geometries||[]).map(g=>[g.id,g]));
   for(const geometry of nextLod.geometries||[]){
    const old=byId.get(geometry.id);
    if(old)retainGeometryPayload(geometry,old);
   }
  }
  for(const clone of msg.geometryClones||[]){
   const li=Number(clone.lodIndex),sourceIndex=Number(clone.sourceGeometryIndex),newIndex=Number(clone.newGeometryIndex);
   const target=next.renderLods?.[li]?.geometries?.[newIndex],source=previous.renderLods?.[li]?.geometries?.[sourceIndex];
   if(target&&source)cloneGeometryPayload(target,source);
  }
  for(const patch of msg.edgePatches||[]){
   const li=Number(patch.lodIndex),gi=Number(patch.geometryIndex),ei=Number(patch.edgeIndex);
   const edge=next.renderLods?.[li]?.geometries?.[gi]?.edges?.[ei];
   if(edge)edge.renderMask=Number(patch.renderMask)||0;
  }
  return next;
 }

 function resetAssetScopedSession(){
  editorViewState.resetForAsset();
  state.sourceChangeScan=null;
  state.geometryPartPreflight=null;
  state.semanticSelectedNodes.clear();
  state.semanticSelectionAnchor=null;
  state.semanticBindingPickTarget=null;
  state.semanticJointPivotPickTarget=null;
  state.semanticCollapsed.clear();
  state.semanticGraphOffsets.clear();
  state.selectedRenderNode=null;
  state.selectedCollision=null;
  state.selectedSocket=null;
  state.previewStates.clear();
  state.geometryReference=null;
  state.geometryCompareChecked.clear();
  state.geometryVariantSelected=null;
  state.variantPreviewByNode.clear();
  state.modelPreflight=null;
  state.surfaceAnalysisReady=false;
  state.surfaceAnalysisRequested=false;
  state.lodAnalysis=null;
  state.lodGeneratorPreview=null;
  state.lodGeneratorApplyLevels.clear();
  state.lodGeneratorAppliedLevels.clear();
  state.lodGeneratorPendingApplyLevels.clear();
  state.lodGeneratorApplying=false;
  state.lodGeneratorMeshSelection='all';
  state.surfaceGeometrySelection=null;
  state.surfaceMaterialSelection=null;
  state.lodPreviewGeometryCache.clear();
 }

 function resetFullPayloadSession(preserveUiSelection){
  state.sourceChangeScan=null;
  state.geometryPartPreflight=null;
  state.variantPreviewByNode.clear();
  state.surfaceAnalysisReady=false;
  state.surfaceAnalysisRequested=false;
  state.lodAnalysis=null;
  state.lodGeneratorPreview=null;
  state.lodGeneratorApplyLevels.clear();
  state.lodGeneratorAppliedLevels.clear();
  state.lodGeneratorPendingApplyLevels.clear();
  state.lodGeneratorApplying=false;
  state.lodGeneratorMeshSelection='all';
  if(!preserveUiSelection)state.surfaceMaterialSelection=null;
  state.lodPreviewGeometryCache.clear();
 }

 function acceptAssetState(msg,fullPayload){
  const incoming=fullPayload?msg.asset:mergeAssetMetadata(msg.asset,msg);
  const sameAsset=state.asset?.assetId===incoming?.assetId;
  const preserveUiSelection=!!msg.preserveUiSelection;
  if(!incoming)return;
  if(fullPayload&&sameAsset&&state.modelPreflight)state.modelPreflight={...state.modelPreflight,cachedAfterGeometryChange:true};
  if(fullPayload||(!fullPayload&&['source','lods','geometry'].includes(state.wizardStage)))state.surfaceAnalysisReady=false;
  if(!sameAsset)resetAssetScopedSession();
  if(fullPayload)resetFullPayloadSession(preserveUiSelection);

  state.asset=incoming;
  state.dirty=!!msg.dirty;
  updateWorkingSaveStamp();
  if(state.dirty)state.wizardValidationReport=null;

  const invalidatedLods=new Set((msg.invalidatedLodPayloads||[]).map(Number));
  for(const li of invalidatedLods)editorViewState.residentLods.delete(li);
  editorViewState.syncResidency(state.asset);
  if(state.pendingActiveLod!==null&&editorViewState.residentLods.has(Number(state.pendingActiveLod))){
   state.activeLod=state.pendingActiveLod;
   state.pendingActiveLod=null;
  }
  if(!editorViewState.residentLods.has(Number(state.activeLod))){
   const first=[...editorViewState.residentLods].sort((a,b)=>a-b)[0];
   if(first!==undefined)state.activeLod=first;
  }

  if(state.selectedNode!==null&&state.selectedNode>=state.asset.nodes.length)state.selectedNode=null;
  for(const i of [...state.semanticSelectedNodes])if(i<0||i>=state.asset.nodes.length)state.semanticSelectedNodes.delete(i);
  if(state.wizardStage==='semantics'&&state.selectedNode!==null&&state.semanticSelectedNodes.size===0)state.semanticSelectedNodes.add(Number(state.selectedNode));

  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod);
  if(state.selectedRenderNode!==null&&(!lod||state.selectedRenderNode>=lod.nodes.length))state.selectedRenderNode=null;
  if(state.selectedCollision!==null&&state.selectedCollision>=state.asset.collisionVolumes.length)state.selectedCollision=null;
  if(state.selectedSocket!==null&&state.selectedSocket>=state.asset.sockets.length)state.selectedSocket=null;

  $('assetSelect').value=state.asset.assetId;
  $('assetTitle').textContent=state.asset.displayName;
  const lodGeo=lod?.loaded?(lod?.geometries?.length||0):Number(lod?.declaredGeometryCount||0);
  const lodNodes=lod?.loaded?(lod?.nodes?.length||0):Number(lod?.declaredNodeCount||0);
  const axisMap=activeLodAxisMapping(activeRenderLod(state.asset?.renderLods,state.activeLod));
  $('assetMeta').textContent=`${state.asset.assetId} · v${state.asset.formatVersion} · ${state.asset.nodes.length} ${tr('model_editor.meta.semantic_parts','semantic parts')} · LOD${state.activeLod}: ${lodGeo} ${tr('model_editor.meta.geometries','geometries')} / ${lodNodes} ${tr('model_editor.meta.render_nodes','render nodes')} · GAME R+X/U+Y/N-Z · SOURCE R${axisMap.right}/U${axisMap.up}/N${axisMap.forward}`;
  updateAxisLegend();
  state.geometryScan=null;

  if(fullPayload){
   rebuildScene(false);
   if(!sameAsset)fitView();
  }else{
   pruneGeometryCache();
   rebuildScene(true);
  }
  highlightSelection();
  renderWizard();
  renderWizardPanel();
  applyWizardVisibility();
  renderLodFiles();
  renderStorage();
  renderCollisionList();
  renderCollisionInspector();
  renderSocketList();
  renderSocketInspector();
  renderMaterials();
  updateActionAvailability();
 }

 return {acceptAssetState,mergeAssetMetadata};
}

export {retainGeometryPayload,createAssetAcceptanceEffects};
