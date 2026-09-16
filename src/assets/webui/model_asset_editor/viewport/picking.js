function createViewportPickingEffects({
 state,$,raycaster,mouse,activeRenderLod,semanticSetJointPivotWorld,tr,localStatus,toggleEdge,
 wizardSemanticsViewportGizmoPickDecision,semanticSelectionModeFromEvent,renderWizardPanel,structuralPickNode,semanticSelectNode,
 socketIndexFromObject,selectSocket,rebuildCollisions,renderCollisionInspector,wizardSemanticsViewportMeshPickDecision,
 requestSemanticBinding,surfaceSelectionSet,instanceAliasRecordForRenderNode,surfaceSetSelectionAnchor,selectRenderNode,
 requestAnimationFrameFn,focusActiveMeshTableSelection,clearViewportSelection
}){
 function pick(event){
  if(!state.asset)return;
  const rect=state.renderer.domElement.getBoundingClientRect();
  mouse.x=((event.clientX-rect.left)/rect.width)*2-1;
  mouse.y=-((event.clientY-rect.top)/rect.height)*2+1;
  raycaster.setFromCamera(mouse,state.camera);

  if(state.wizardStage==='semantics'&&state.semanticJointPivotPickTarget!==null){
   const visible=state.meshObjects.filter(object=>{
    let current=object;
    while(current){if(!current.visible)return false;current=current.parent;}
    return true;
   });
   const hit=raycaster.intersectObjects(visible,false)[0];
   if(hit){
    const target=Number(state.semanticJointPivotPickTarget);
    state.semanticJointPivotPickTarget=null;
    semanticSetJointPivotWorld(target,hit.point.clone(),tr('model_editor.semantics.pivot.source_mesh_point','3D mesh point'));
    return;
   }
   localStatus(tr('model_editor.semantics.status.pick_visible_surface','SEMANTICS: click a visible mesh surface to set the joint pivot'),'danger');
   return;
  }

  if(state.edgeEdit&&state.edgeOverlay){
   const hit=raycaster.intersectObject(state.edgeOverlay,false)[0];
   if(hit){toggleEdge(hit);return;}
  }

  if(state.wizardStage==='semantics'&&state.semanticBindingPickTarget===null){
   const gizmoHits=raycaster.intersectObjects(state.semanticGizmoGroup.children,true);
   const structuralHit=gizmoHits.find(hit=>hit.object?.userData?.structuralLinkIndex!==undefined);
   const linkHit=gizmoHits.find(hit=>hit.object?.userData?.semanticLinkChildIndex!==undefined);
   const nodeHit=gizmoHits.find(hit=>hit.object?.userData?.semanticNodeIndex!==undefined);
   const decision=wizardSemanticsViewportGizmoPickDecision({
    semanticStructureMode:state.semanticStructureMode,selectionMode:semanticSelectionModeFromEvent(event),
    structuralLinkIndex:structuralHit?.object?.userData?.structuralLinkIndex,
    semanticLinkChildIndex:linkHit?.object?.userData?.semanticLinkChildIndex,
    semanticNodeIndex:nodeHit?.object?.userData?.semanticNodeIndex
   });
   if(decision.action==='structural_link'){
    state.selectedStructuralLink=decision.structuralLinkIndex;
    state.selectedStructuralProxy=0;
    renderWizardPanel();
    return;
   }
   if(decision.action==='structural_node'){structuralPickNode(decision.nodeIndex);return;}
   if(decision.action==='semantic_node'){semanticSelectNode(decision.nodeIndex,decision.selectionMode);return;}
  }

  if(state.wizardStage==='semantics'&&$('socketToggle')?.checked){
   const socketHit=raycaster.intersectObjects(state.socketGroup.children,true)[0];
   if(socketHit){
    const socketIndex=socketIndexFromObject(socketHit.object);
    if(socketIndex!==null){selectSocket(socketIndex);return;}
   }
  }

  if((state.wizardStage==='physics'||state.wizardStage==='damage')&&$('hitToggle').checked){
   const hits=raycaster.intersectObjects(state.collisionGroup.children,true);
   const hit=hits.find(row=>row.object.userData.collisionIndex!==undefined||row.object.parent?.userData.collisionIndex!==undefined);
   if(hit){
    const collisionIndex=hit.object.userData.collisionIndex??hit.object.parent.userData.collisionIndex;
    state.selectedCollision=collisionIndex;
    state.selectedSocket=null;
    rebuildCollisions();
    renderCollisionInspector();
    return;
   }
  }

  const worldVisible=object=>{
   let current=object;
   while(current){if(!current.visible)return false;current=current.parent;}
   return true;
  };
  const hits=raycaster.intersectObjects(state.meshObjects.filter(worldVisible),false);
  if(hits.length){
   const renderNodeIndex=Number(hits[0].object.userData.renderNodeIndex);
   const renderNode=activeRenderLod(state.asset?.renderLods,state.activeLod)?.nodes?.[renderNodeIndex];
   const semanticNodeIndex=Number(renderNode?.semanticNodeIndex??-1);
   if(state.wizardStage==='semantics'){
    const decision=wizardSemanticsViewportMeshPickDecision({
     nodes:state.asset?.nodes||[],bindingPickTarget:state.semanticBindingPickTarget,lodIndex:state.activeLod,
     renderNodeIndex,renderNodeId:renderNode?.id,semanticNodeIndex,applyAllLods:state.semanticApplyAllLods,
     semanticStructureMode:state.semanticStructureMode,selectionMode:semanticSelectionModeFromEvent(event)
    });
    if(decision.action==='binding'){
     state.semanticBindingPickTarget=null;
     if(!decision.command.ok)return;
     requestSemanticBinding(decision.command.payload);
     localStatus(tr('model_editor.semantics.status.binding_set','SEMANTICS: binding {render} → {semantic}',{render:decision.command.renderId,semantic:decision.command.targetId}));
     return;
    }
    if(decision.action==='structural_node'){structuralPickNode(decision.nodeIndex,decision.renderNodeIndex);return;}
    if(decision.action==='semantic_node'){
     state.selectedRenderNode=decision.renderNodeIndex;
     semanticSelectNode(decision.nodeIndex,decision.selectionMode);
     return;
    }
   }
   if(state.wizardStage==='surfaces'){
    const geometry=activeRenderLod(state.asset?.renderLods,state.activeLod)?.geometries?.[Number(renderNode?.geometryIndex)];
    if(geometry){
     const selected=surfaceSelectionSet(state.activeLod,true);
     const alias=instanceAliasRecordForRenderNode(state.asset?.meshSourceRecords,renderNode?.id,state.activeLod);
     selected.clear();
     selected.add(String(geometry.id));
     surfaceSetSelectionAnchor(alias?.geometryId||geometry.id);
     state.surfaceGeometrySelection=String(geometry.id);
    }
   }
   selectRenderNode(renderNodeIndex,{
    scrollPreflight:state.wizardStage==='lods',scrollGeometry:state.wizardStage==='geometry',
    focusTable:['source','lods','geometry'].includes(state.wizardStage)
   });
   if(state.wizardStage==='surfaces'){
    renderWizardPanel();
    requestAnimationFrameFn(()=>focusActiveMeshTableSelection());
   }
   return;
  }
  clearViewportSelection();
 }

 return {pick};
}

export {createViewportPickingEffects};
