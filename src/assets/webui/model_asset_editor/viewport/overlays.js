function createViewportOverlayEffects({
 state,$,THREE,activeRenderLod,geometryInventoryNodeVisible,requestEdgeRenderMask
}){
 function clearEdgeOverlay(){
  if(state.edgeOverlay){
   state.edgeOverlay.parent?.remove(state.edgeOverlay);
   state.edgeOverlay.geometry?.dispose?.();
   state.edgeOverlay.material?.dispose?.();
   state.edgeOverlay=null;
  }
  state.edgeMap=[];
 }

 function clearNormalOverlay(){
  if(state.normalOverlay){
   state.normalOverlay.parent?.remove(state.normalOverlay);
   state.normalOverlay.geometry?.dispose?.();
   state.normalOverlay.material?.dispose?.();
   state.normalOverlay=null;
  }
 }

 function rebuildNormals(){
  clearNormalOverlay();
  if(!$('normalsToggle').checked||state.selectedRenderNode===null||!state.asset)return;
  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod),node=lod?.nodes?.[state.selectedRenderNode];
  if(!node||node.geometryIndex<0||!geometryInventoryNodeVisible(node,lod))return;
  const geometry=lod.geometries?.[node.geometryIndex];
  if(!geometry?.normals?.length)return;
  const count=geometry.positions.length/3,stride=Math.max(1,Math.ceil(count/1800));
  const diagonal=Math.max(...state.asset.maxBounds.map((value,index)=>Math.abs(value-state.asset.minBounds[index])),1);
  const length=diagonal*.018,positions=[];
  for(let index=0;index<count;index+=stride){
   const offset=index*3;
   positions.push(
    geometry.positions[offset],geometry.positions[offset+1],geometry.positions[offset+2],
    geometry.positions[offset]+geometry.normals[offset]*length,
    geometry.positions[offset+1]+geometry.normals[offset+1]*length,
    geometry.positions[offset+2]+geometry.normals[offset+2]*length
   );
  }
  const threeGeometry=new THREE.BufferGeometry();
  threeGeometry.setAttribute('position',new THREE.Float32BufferAttribute(positions,3));
  const line=new THREE.LineSegments(threeGeometry,new THREE.LineBasicMaterial({color:0x69ffc4,transparent:true,opacity:.72}));
  state.renderNodeGroups[state.selectedRenderNode]?.add(line);
  state.normalOverlay=line;
 }

 function rebuildEdgeOverlay(){
  clearEdgeOverlay();
  if(!state.edgeEdit||state.selectedRenderNode===null||!state.asset)return;
  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod),node=lod?.nodes?.[state.selectedRenderNode];
  if(!node||node.geometryIndex<0||!geometryInventoryNodeVisible(node,lod))return;
  const geometry=lod.geometries?.[node.geometryIndex];
  if(!geometry)return;
  const positions=[],colors=[],target=Number($('edgeTarget').value);
  state.edgeMap=[];
  for(const edge of geometry.edges||[]){
   const a=edge.a*3,b=edge.b*3;
   positions.push(
    geometry.positions[a],geometry.positions[a+1],geometry.positions[a+2],
    geometry.positions[b],geometry.positions[b+1],geometry.positions[b+2]
   );
   const enabled=(edge.renderMask&target)!==0,color=enabled?[.46,.84,1.0]:[.55,.20,.26];
   colors.push(...color,...color);
   state.edgeMap.push(edge.index);
  }
  const threeGeometry=new THREE.BufferGeometry();
  threeGeometry.setAttribute('position',new THREE.Float32BufferAttribute(positions,3));
  threeGeometry.setAttribute('color',new THREE.Float32BufferAttribute(colors,3));
  const material=new THREE.LineBasicMaterial({vertexColors:true,transparent:true,opacity:.95});
  const segments=new THREE.LineSegments(threeGeometry,material);
  segments.userData.edgeOverlay=true;
  segments.userData.geometryIndex=node.geometryIndex;
  state.renderNodeGroups[state.selectedRenderNode]?.add(segments);
  state.edgeOverlay=segments;
 }

 function toggleEdge(intersection){
  if(!state.edgeOverlay||state.selectedRenderNode===null)return;
  const segment=Math.floor((intersection.index??0)/2),edgeIndex=state.edgeMap[segment];
  if(edgeIndex===undefined)return;
  const lod=activeRenderLod(state.asset?.renderLods,state.activeLod),node=lod.nodes[state.selectedRenderNode];
  const geometry=lod.geometries[node.geometryIndex],edge=geometry.edges[edgeIndex],bit=Number($('edgeTarget').value);
  const mask=(edge.renderMask&bit)?(edge.renderMask&~bit):(edge.renderMask|bit);
  requestEdgeRenderMask({geometryIndex:node.geometryIndex,lodIndex:state.activeLod,edgeIndex,renderMask:mask});
 }

 return {clearEdgeOverlay,clearNormalOverlay,rebuildNormals,rebuildEdgeOverlay,toggleEdge};
}

export {createViewportOverlayEffects};
