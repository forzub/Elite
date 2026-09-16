function createViewportAttachmentEffects({
 state,THREE,clearGroup,activeRenderLod,composeMatrix,semanticDisplayWorldMatrix,semanticWorldMatrix,
 stateApplies,hitVolumeRenderPlan,isCollisionVisible,isSocketVisible,renderCollisionList,renderSocketList,localStatus
}){
 const semanticTransformInput=()=>({
  nodes:state.asset?.nodes||[],stateVariants:state.asset?.stateVariants||[],previewStates:state.previewStates,
  selectedNode:state.selectedNode,wizardStage:state.wizardStage,previewAngleDeg:state.semanticPreviewAngleDeg,
  previewDetached:state.semanticPreviewDetached,minBounds:state.asset?.minBounds||[0,0,0],
  maxBounds:state.asset?.maxBounds||[1,1,1],graphOffsets:state.semanticGraphOffsets
 });

 function collisionPrimitive(collision){
  if(collision.shape==='sphere')return new THREE.SphereGeometry(collision.radius,18,12);
  if(collision.shape==='capsule'){
   const group=new THREE.Group(),radius=collision.radius,halfHeight=collision.halfHeight;
   const cylinder=new THREE.Mesh(
    new THREE.CylinderGeometry(radius,radius,halfHeight*2,16,1,true),
    new THREE.MeshBasicMaterial({wireframe:true})
   );
   const first=new THREE.Mesh(new THREE.SphereGeometry(radius,12,8),new THREE.MeshBasicMaterial({wireframe:true}));
   const second=first.clone();
   first.position.y=halfHeight;
   second.position.y=-halfHeight;
   group.add(cylinder,first,second);
   return group;
  }
  return new THREE.BoxGeometry(collision.halfSize[0]*2,collision.halfSize[1]*2,collision.halfSize[2]*2);
 }

 function makeWirePrimitive(primitive,material,index=null){
  let object;
  if(primitive.isGroup){
   object=primitive;
   object.traverse(child=>{
    if(!child.isMesh)return;
    child.material.dispose?.();
    const edges=new THREE.EdgesGeometry(child.geometry),lines=new THREE.LineSegments(edges,material);
    if(index!==null){lines.userData.collisionIndex=index;lines.userData.pickCollision=true;}
    child.parent.add(lines);
    child.visible=false;
   });
  }else{
   const edges=new THREE.EdgesGeometry(primitive);
   object=new THREE.LineSegments(edges,material);
   if(index!==null){object.userData.collisionIndex=index;object.userData.pickCollision=true;}
   primitive.dispose();
  }
  return object;
 }

 function rebuildStructuralProxies(){
  clearGroup(state.structuralProxyGroup);
  if(state.wizardStage!=='semantics'||state.semanticStructureMode!=='graph'||!state.asset)return;
  for(const link of state.asset.structuralLinks||[]){
   if(!link.enabled)continue;
   for(const proxy of link.damageProxies||[]){
    if(!proxy.enabled)continue;
    const primitive=collisionPrimitive({shape:proxy.shape,radius:proxy.radius,halfHeight:proxy.halfHeight,halfSize:proxy.halfSize});
    const material=new THREE.LineBasicMaterial({
     color:link.kind==='weld_seam'?0xffb84d:link.kind==='controlled_lock'?0x5dff9a:0xff7b54,
     transparent:true,opacity:.9,depthTest:false
    });
    const object=makeWirePrimitive(primitive,material);
    const local=composeMatrix({localPosition:proxy.localPosition,localRotationDeg:proxy.localRotationDeg,pivot:[0,0,0]});
    const world=proxy.parentNodeIndex>=0?semanticDisplayWorldMatrix(proxy.parentNodeIndex,semanticTransformInput()).multiply(local):local;
    object.matrixAutoUpdate=false;
    object.matrix.copy(world);
    object.renderOrder=1090;
    state.structuralProxyGroup.add(object);
   }
  }
 }

 function rebuildCollisions(){
  clearGroup(state.collisionGroup);
  if(!state.asset||!activeRenderLod(state.asset?.renderLods,state.activeLod)?.loaded||!isCollisionVisible())return;
  const plan=hitVolumeRenderPlan({
   collisionVolumes:state.asset.collisionVolumes||[],selectedCollision:state.selectedCollision,
   previewStates:state.previewStates,nodes:state.asset?.nodes||[],transformInput:semanticTransformInput()
  });
  for(const row of plan){
   const primitive=collisionPrimitive(row);
   const material=new THREE.LineBasicMaterial({color:row.selected?0xffdd66:0xff835f,transparent:true,opacity:.75});
   const object=makeWirePrimitive(primitive,material,row.index);
   object.matrixAutoUpdate=false;
   object.matrix.copy(row.worldMatrix);
   object.userData.collisionIndex=row.index;
   state.collisionGroup.add(object);
  }
  renderCollisionList();
 }

 function updateSemanticCollisionTransforms(){
  if(!state.asset||!isCollisionVisible())return;
  for(const object of state.collisionGroup.children){
   const index=Number(object?.userData?.collisionIndex),collision=state.asset.collisionVolumes?.[index];
   if(!Number.isInteger(index)||!collision)continue;
   const local=composeMatrix({localPosition:collision.localPosition,localRotationDeg:collision.localRotationDeg,pivot:[0,0,0]});
   const world=collision.parentNodeIndex>=0?semanticDisplayWorldMatrix(collision.parentNodeIndex,semanticTransformInput()).multiply(local):local;
   object.matrixAutoUpdate=false;
   object.matrix.copy(world);
   object.updateMatrixWorld(true);
  }
 }

 function socketKindColor(kind){
  const value=String(kind||'').toLowerCase();
  if(value.includes('camera'))return 0x55d9ff;
  if(value.includes('weapon')||value.includes('muzzle')||value.includes('missile'))return 0xff665e;
  if(value.includes('drone')&&value.includes('launch'))return 0x66e28a;
  if(value.includes('drone')&&value.includes('recover'))return 0xffd166;
  if(value.includes('drone')||value.includes('dock'))return 0xffa24c;
  if(value.includes('light'))return 0xfff08a;
  if(value.includes('equipment')||value.includes('mount'))return 0xb886ff;
  return 0x66c6ff;
 }

 function socketLocalMatrix(socket){
  return composeMatrix({localPosition:socket?.localPosition||[0,0,0],localRotationDeg:socket?.localRotationDeg||[0,0,0],pivot:[0,0,0]});
 }
 function socketWorldMatrix(socket){
  const local=socketLocalMatrix(socket);
  return Number(socket?.parentNodeIndex)>=0?semanticDisplayWorldMatrix(Number(socket.parentNodeIndex),semanticTransformInput()).multiply(local):local;
 }
 function socketCanonicalWorldMatrix(socket){
  const local=socketLocalMatrix(socket);
  return Number(socket?.parentNodeIndex)>=0?
   semanticWorldMatrix(Number(socket.parentNodeIndex),state.asset?.nodes||[],state.asset?.stateVariants||[],state.previewStates).multiply(local):local;
 }

 function viewFromSocket(socket){
  if(!socket||!state.camera||!state.controls)return;
  const matrix=socketCanonicalWorldMatrix(socket),position=new THREE.Vector3(),quaternion=new THREE.Quaternion(),scale=new THREE.Vector3();
  matrix.decompose(position,quaternion,scale);
  state.camera.position.copy(position);
  state.camera.quaternion.copy(quaternion);
  state.camera.fov=Math.max(5,Math.min(160,Number(socket.previewFovDeg||75)));
  state.camera.updateProjectionMatrix();
  const diagonal=Math.max(...state.asset.maxBounds.map((value,index)=>Math.abs(value-state.asset.minBounds[index])),1);
  const forward=new THREE.Vector3(0,0,-1).applyQuaternion(quaternion).normalize();
  const right=new THREE.Vector3(1,0,0).applyQuaternion(quaternion).normalize();
  const up=new THREE.Vector3(0,1,0).applyQuaternion(quaternion).normalize();
  const focus=Math.max(diagonal*.8,10);
  state.controls.target.copy(position).addScaledVector(forward,focus);
  state.controls.update();
  clearGroup(state.cameraReferenceGroup);
  const material=new THREE.MeshBasicMaterial({wireframe:true,transparent:true,opacity:.55,depthTest:true});
  const add=(geometry,center)=>{
   const object=new THREE.Mesh(geometry,material.clone());
   object.position.copy(center);
   object.userData.editorReference=true;
   state.cameraReferenceGroup.add(object);
  };
  add(new THREE.SphereGeometry(Math.max(diagonal*.035,.5),12,8),position.clone().addScaledVector(forward,focus));
  add(new THREE.BoxGeometry(Math.max(diagonal*.08,1),Math.max(diagonal*.04,.5),Math.max(diagonal*.04,.5)),position.clone().addScaledVector(forward,focus*1.5).addScaledVector(right,focus*.28));
  add(new THREE.BoxGeometry(Math.max(diagonal*.05,.8),Math.max(diagonal*.1,1),Math.max(diagonal*.05,.8)),position.clone().addScaledVector(forward,focus*2).addScaledVector(right,-focus*.25).addScaledVector(up,focus*.08));
  localStatus(tr('model_editor.status.camera_preview','Camera preview: {id} · FOV {fov}°',{id:socket.id,fov:state.camera.fov.toFixed(1)}));
 }

 function updateSemanticSocketTransforms(){
  if(!state.asset)return;
  for(const marker of state.socketGroup.children){
   const index=Number(marker.userData?.socketIndex),socket=state.asset.sockets?.[index];
   if(!socket)continue;
   marker.matrixAutoUpdate=false;
   marker.matrix.copy(socketWorldMatrix(socket));
  }
  state.socketGroup.updateMatrixWorld(true);
 }

 function socketIndexFromObject(object){
  let current=object;
  while(current){
   if(current.userData?.socketIndex!==undefined)return Number(current.userData.socketIndex);
   current=current.parent;
  }
  return null;
 }

 function rebuildSockets(){
  clearGroup(state.socketGroup);
  renderSocketList();
  if(!state.asset||!activeRenderLod(state.asset?.renderLods,state.activeLod)?.loaded)return;
  if(state.wizardStage!=='semantics'||!isSocketVisible())return;
  const diagonal=Math.max(...state.asset.maxBounds.map((value,index)=>Math.abs(value-state.asset.minBounds[index])),1),markerScale=diagonal*.014;
  for(const socket of state.asset.sockets||[]){
   if(!socket.enabled||!stateApplies(state.previewStates,state.asset?.nodes||[],socket))continue;
   const group=new THREE.Group();
   group.matrixAutoUpdate=false;
   group.matrix.copy(socketWorldMatrix(socket));
   group.userData.socketIndex=socket.index;
   const selected=socket.index===state.selectedSocket,color=selected?0xffff66:socketKindColor(socket.kind);
   const material=new THREE.MeshBasicMaterial({color,depthTest:false,depthWrite:false,transparent:true,opacity:selected?1:.92});
   const box=new THREE.Mesh(new THREE.BoxGeometry(markerScale,markerScale,markerScale),material);
   box.userData.socketIndex=socket.index;
   box.renderOrder=1200;
   group.add(box);
   const direction=new THREE.ArrowHelper(new THREE.Vector3(0,0,-1),new THREE.Vector3(0,0,0),diagonal*.055,color,diagonal*.014,diagonal*.008);
   direction.userData.socketIndex=socket.index;
   direction.line.userData.socketIndex=socket.index;
   direction.cone.userData.socketIndex=socket.index;
   direction.line.material.depthTest=false;
   direction.cone.material.depthTest=false;
   direction.renderOrder=1201;
   group.add(direction);
   state.socketGroup.add(group);
  }
  state.socketGroup.updateMatrixWorld(true);
 }

 return {
  rebuildStructuralProxies,rebuildCollisions,updateSemanticCollisionTransforms,
  viewFromSocket,updateSemanticSocketTransforms,socketIndexFromObject,rebuildSockets
 };
}

export {createViewportAttachmentEffects};
