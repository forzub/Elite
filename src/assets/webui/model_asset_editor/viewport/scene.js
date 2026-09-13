function createViewportSceneEffects({
 state,editorViewState,THREE,clearGroup,
 activeRenderLod,lodHasGeometryPayload,composeMatrix,semanticNodePose,stateApplies,semanticIsDescendant,
 previewGeometryForRenderNode,lodGeneratorPreviewGeometry,
 threeGeometry,configureSurfacePreviewGroups,makeSurfacePreviewMaterials,defaultPreviewMaterial,
 clearSemanticGizmos,clearEdgeOverlay,clearNormalOverlay,rebuildCollisions,rebuildSockets,
 renderTree,renderStateVariants,renderRenderTree,renderGeometries,renderNodeInspector,renderRenderNodeInspector,
 renderDamageSemantics,renderActiveLodDetails,rebuildEdgeOverlay,rebuildNormals,rebuildLodFaceNormals,
 applySemanticMotionPreview,assertEditorViewInvariant
}){
 function lodGeneratorSelectedGeometry(){
  if(state.wizardStage!=='lods'||!state.lodAnalysis||state.lodGeneratorMeshSelection==='all'||Number(state.activeLod)!==Number(state.lodAnalysis.lodIndex||0))return null;
  return (activeRenderLod(state.asset?.renderLods,state.activeLod)?.geometries||[]).find(geometry=>geometry.id===state.lodGeneratorMeshSelection)||null;
 }

 function lodGeneratorNodePassesMeshFilter(node,selectedGeometry){
  if(!selectedGeometry)return true;
  return !selectedGeometry.isSourceVariant&&Number(node?.geometryIndex)===Number(selectedGeometry.index);
 }

 function addLodGeneratorStandaloneVariant(){
  const selected=lodGeneratorSelectedGeometry();
  if(!selected?.isSourceVariant)return;
  const geometry=lodGeneratorPreviewGeometry(selected),three=threeGeometry(geometry);
  if(!three)return;
  const mesh=new THREE.Mesh(three,defaultPreviewMaterial());
  mesh.userData.geometryIndex=selected.index;
  mesh.userData.previewGeometryId=geometry.id;
  mesh.userData.sharedGeometry=true;
  mesh.userData.lodGeneratorStandalone=true;
  state.root.add(mesh);
  state.meshObjects.push(mesh);
 }

 function geometrySelectionNeedsAncestor(index,lod){
  for(let selected=0;selected<(lod?.nodes||[]).length;selected++){
   if(!editorViewState.renderNodeVisible(state.activeLod,selected,lod.nodes[selected]))continue;
   let parent=Number(lod.nodes[selected]?.parentIndex??-1);
   while(parent>=0){
    if(parent===index)return true;
    parent=Number(lod.nodes[parent]?.parentIndex??-1);
   }
  }
  return false;
 }

 function updateVisibility(){
  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod);
  if(!lod)return;
  const selectedGeometry=lodGeneratorSelectedGeometry();
  for(let index=0;index<state.renderNodeGroups.length;index++){
   const renderNode=lod.nodes[index],semanticIndex=renderNode?.semanticNodeIndex??-1;
   const editorViewVisible=editorViewState.renderNodeVisible(state.activeLod,index,renderNode);
   const hidden=semanticIndex>=0&&state.hidden.has(semanticIndex);
   const isolated=state.isolated!==null&&semanticIndex>=0&&semanticIndex!==state.isolated&&
    !semanticIsDescendant(state.asset?.nodes||[],semanticIndex,state.isolated)&&
    !semanticIsDescendant(state.asset?.nodes||[],state.isolated,semanticIndex);
   const transformCarrier=!editorViewVisible&&geometrySelectionNeedsAncestor(index,lod);
   state.renderNodeGroups[index].visible=stateApplies(state.previewStates,state.asset?.nodes||[],renderNode)&&!hidden&&!isolated&&(editorViewVisible||transformCarrier);
   for(const child of state.renderNodeGroups[index].children){
    if(child?.isMesh&&child.userData?.renderNodeIndex===index){
     child.visible=editorViewVisible&&lodGeneratorNodePassesMeshFilter(renderNode,selectedGeometry);
    }
   }
  }
  rebuildEdgeOverlay();
  renderTree();
  renderRenderTree();
 }

 function rebuildScene(preserveGeometryCache=false){
  clearGroup(state.root);
  clearSemanticGizmos();
  state.lodFaceNormalOverlays=[];
  if(!preserveGeometryCache){
   state.geometryCache.forEach(geometry=>geometry.dispose?.());
   state.geometryCache.clear();
  }
  state.nodeGroups=[];
  state.renderNodeGroups=[];
  state.meshObjects=[];
  clearEdgeOverlay();
  clearNormalOverlay();
  clearGroup(state.collisionGroup);
  clearGroup(state.socketGroup);
  clearGroup(state.structuralProxyGroup);
  clearGroup(state.cameraReferenceGroup);
  if(!state.asset)return;

  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod);
  const sceneResident=!!lod&&lodHasGeometryPayload(lod);
  if(sceneResident){
   const nodes=lod.nodes||[];
   for(let index=0;index<nodes.length;index++){
    const renderNode=nodes[index],group=new THREE.Group();
    group.matrixAutoUpdate=false;
    let matrix=composeMatrix(renderNode);
    if(renderNode.semanticNodeIndex>=0){
     const base=state.asset.nodes[renderNode.semanticNodeIndex];
     const preview=semanticNodePose(state.asset?.nodes||[],state.asset?.stateVariants||[],state.previewStates,renderNode.semanticNodeIndex);
     if(base&&preview&&preview!==base){
      const delta=composeMatrix(preview).multiply(composeMatrix(base).invert());
      matrix=delta.multiply(matrix);
     }
    }
    group.matrix.copy(matrix);
    group.userData.renderNodeIndex=index;
    group.userData.semanticNodeIndex=renderNode.semanticNodeIndex;
    state.renderNodeGroups[index]=group;
   }

   const selectedGeometry=lodGeneratorSelectedGeometry();
   for(let index=0;index<nodes.length;index++){
    const renderNode=nodes[index],group=state.renderNodeGroups[index];
    const parent=renderNode.parentIndex>=0?state.renderNodeGroups[renderNode.parentIndex]:state.root;
    parent.add(group);
    group.visible=stateApplies(state.previewStates,state.asset?.nodes||[],renderNode);
    if(renderNode.geometryIndex<0)continue;
    const geometry=previewGeometryForRenderNode(index,renderNode);
    if(!geometry)continue;
    const three=threeGeometry(geometry);
    if(!three)continue;
    configureSurfacePreviewGroups(three,geometry);
    const material=state.wizardStage==='surfaces'?makeSurfacePreviewMaterials(geometry):defaultPreviewMaterial();
    const mesh=new THREE.Mesh(three,material);
    mesh.userData.renderNodeIndex=index;
    mesh.userData.semanticNodeIndex=renderNode.semanticNodeIndex;
    mesh.userData.geometryIndex=renderNode.geometryIndex;
    mesh.userData.previewGeometryId=geometry.id;
    mesh.userData.sharedGeometry=true;
    mesh.visible=lodGeneratorNodePassesMeshFilter(renderNode,selectedGeometry);
    group.add(mesh);
    state.meshObjects.push(mesh);
   }
   addLodGeneratorStandaloneVariant();
  }

  rebuildCollisions();
  rebuildSockets();
  renderTree();
  renderStateVariants();
  renderRenderTree();
  renderGeometries();
  renderNodeInspector();
  renderRenderNodeInspector();
  renderDamageSemantics();
  renderActiveLodDetails();
  rebuildEdgeOverlay();
  rebuildNormals();
  updateVisibility();
  rebuildLodFaceNormals();
  if(state.wizardStage==='semantics')applySemanticMotionPreview();
  else clearSemanticGizmos();
  editorViewState.commitSceneLod(state.activeLod);
  editorViewState.syncResidency(state.asset);
  assertEditorViewInvariant('rebuildScene');
 }

 return {rebuildScene,updateVisibility};
}

export {createViewportSceneEffects};
