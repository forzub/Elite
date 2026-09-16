function createEditorViewInvariants({state,editorViewState,reportDiagnostic,queueMicrotaskFn,logger}){
 function captureEditorViewTransition(){
  return {
   activeLod:state.activeLod,
   sceneLod:state.sceneLod,
   selectedRenderNode:state.selectedRenderNode,
   selectedRenderNodeId:editorViewState.selectedRenderNodeId,
   selectedMeshId:editorViewState.selectedMeshId,
   selectedSemanticNode:editorViewState.selectedSemanticNode,
   semanticSelection:[...state.semanticSelectedNodes].sort((a,b)=>a-b),
   visibility:[...editorViewState.visibilityByLod].map(([lod,set])=>[lod,[...set].sort()]),
   hidden:[...editorViewState.hiddenSemanticByLod].map(([lod,set])=>[lod,[...set].sort((a,b)=>a-b)]),
   isolated:[...editorViewState.isolationByLod],
   camera:state.camera?{
    position:state.camera.position.toArray(),
    quaternion:state.camera.quaternion.toArray(),
    target:state.controls?.target?.toArray?.()||null
   }:null
  };
 }

 function editorViewSnapshotsEqual(a,b){return JSON.stringify(a)===JSON.stringify(b);}

 function assertEditorViewTransitionPreserved(before,context){
  const after=captureEditorViewTransition();
  if(editorViewSnapshotsEqual(before,after))return true;
  const changed=Object.keys(after).filter(key=>JSON.stringify(before?.[key])!==JSON.stringify(after?.[key]));
  const error=new Error(`EditorViewState transition mutated persistent view state: ${context} [${changed.join(',')}]`);
  reportDiagnostic('state_invariant',error,{context,changed,before,after});
  throw error;
 }

 function assertEditorViewInvariant(context='runtime'){
  if(!state.asset)return true;
  if(Number(state.sceneLod)!==Number(state.activeLod)){
   const error=new Error(`EditorViewState invariant failed: sceneLod=${state.sceneLod} activeLod=${state.activeLod}`);
   reportDiagnostic('state_invariant',error,{context});
   throw error;
  }
  if(state.renderNodeGroups.length>0&&!editorViewState.residentLods.has(Number(state.sceneLod))){
   const error=new Error(`EditorViewState invariant failed: rendered scene LOD${state.sceneLod} is not resident`);
   reportDiagnostic('state_invariant',error,{context});
   throw error;
  }
  return true;
 }

 let scheduled=false;
 function scheduleEditorViewInvariantCheck(context){
  if(scheduled)return;
  scheduled=true;
  queueMicrotaskFn(()=>{
   scheduled=false;
   try{assertEditorViewInvariant(`deferred:${context}`);}catch(error){logger.error(error);}
  });
 }

 return {captureEditorViewTransition,assertEditorViewTransitionPreserved,assertEditorViewInvariant,scheduleEditorViewInvariantCheck};
}

export {createEditorViewInvariants};
