function lodHasGeometryPayload(lod){
 return !!lod&&(lod.geometries||[]).every(geometry=>!Number(geometry.vertexCount||0)||!!geometry.positions?.length);
}

function createViewportGeometryRuntime({state,THREE,geometryCacheKey,lodDiagnosticActive}){
 function pruneGeometryCache(){
  if(!state.asset)return;
  const valid=new Set();
  for(let lodIndex=0;lodIndex<(state.asset.renderLods||[]).length;lodIndex++){
   for(const geometry of state.asset.renderLods[lodIndex]?.geometries||[]){
    valid.add(geometryCacheKey(lodIndex,geometry,state.meshViewportMode));
   }
  }
  for(const [key,geometry] of state.geometryCache.entries()){
   if(valid.has(key))continue;
   geometry.dispose?.();
   state.geometryCache.delete(key);
  }
 }

 function activeLodNeedsRawSource(lod){
  return !!lod?.loaded&&(lod.geometries||[]).some(geometry=>Number(geometry?.vertexCount||0)>0&&!geometry?.rawSource);
 }

 function threeGeometry(geometry){
  const key=geometryCacheKey(state.activeLod,geometry,state.meshViewportMode);
  if(state.geometryCache.has(key))return state.geometryCache.get(key);
  const payload=state.meshViewportMode==='source'?(geometry?.rawSource||geometry):geometry;
  if(!payload?.positions?.length)return null;
  const result=new THREE.BufferGeometry();
  result.setAttribute('position',new THREE.Float32BufferAttribute(payload.positions,3));
  if(payload.normals?.length===payload.positions.length)result.setAttribute('normal',new THREE.Float32BufferAttribute(payload.normals,3));
  else result.computeVertexNormals();
  const indices=payload.indices||[];
  result.setIndex(ArrayBuffer.isView(indices)?new THREE.BufferAttribute(indices,1):indices);
  result.computeBoundingBox();
  result.computeBoundingSphere();
  state.geometryCache.set(key,result);
  return result;
 }

 function configureSurfacePreviewGroups(threeGeometryObject,geometry){
  threeGeometryObject.clearGroups();
  if(state.wizardStage!=='surfaces')return;
  const triangleMaterials=geometry?.triangleMaterials||[];
  const count=Math.floor((geometry?.indices||[]).length/3);
  if(!count||triangleMaterials.length<count)return;
  const materialCount=(state.asset?.materials||[]).length,fallback=materialCount;
  const slot=value=>Number.isInteger(value)&&value>=0&&value<materialCount?value:fallback;
  let start=0,current=Number(triangleMaterials[0]);
  for(let triangle=1;triangle<=count;triangle++){
   const next=triangle<count?Number(triangleMaterials[triangle]):Number.NaN;
   if(triangle===count||slot(next)!==slot(current)){
    threeGeometryObject.addGroup(start*3,(triangle-start)*3,slot(current));
    start=triangle;
    current=next;
   }
  }
 }

 function makeSurfacePreviewMaterials(geometry){
  const materials=state.asset?.materials||[],forceDouble=geometry?.surfaceMode==='thin_two_sided';
  const result=materials.map(material=>{
   const base=material.baseColor||[.65,.68,.72,1],emissive=material.emissiveColor||[0,0,0];
   const alpha=Math.max(0,Math.min(1,Number(base[3]??1)));
   return new THREE.MeshStandardMaterial({
    color:new THREE.Color(Number(base[0]??.65),Number(base[1]??.68),Number(base[2]??.72)),
    emissive:new THREE.Color(Number(emissive[0]||0),Number(emissive[1]||0),Number(emissive[2]||0)),
    emissiveIntensity:Math.max(0,Number(material.emissiveStrength||0)),
    metalness:Math.max(0,Math.min(1,Number(material.metallic||0))),
    roughness:Math.max(0,Math.min(1,Number(material.roughness??.65))),
    side:forceDouble?THREE.DoubleSide:THREE.FrontSide,
    transparent:alpha<.999,
    opacity:alpha,
    depthWrite:alpha>=.999
   });
  });
  result.push(new THREE.MeshStandardMaterial({color:0x7d91a7,roughness:.72,metalness:0,side:forceDouble?THREE.DoubleSide:THREE.FrontSide}));
  return result;
 }

 function defaultPreviewMaterial(){
  const diagnostic=lodDiagnosticActive(state.wizardStage,state.lodAnalysis);
  return new THREE.MeshPhongMaterial({
   color:0x7d91a7,
   specular:0x314254,
   shininess:24,
   side:state.meshViewportMode==='working'?THREE.FrontSide:THREE.DoubleSide,
   transparent:!diagnostic,
   opacity:diagnostic?1:.93,
   depthWrite:true,
   wireframe:diagnostic&&state.lodDiagnosticWireframe
  });
 }

 return {
  pruneGeometryCache,activeLodNeedsRawSource,threeGeometry,
  configureSurfacePreviewGroups,makeSurfacePreviewMaterials,defaultPreviewMaterial
 };
}

export {lodHasGeometryPayload,createViewportGeometryRuntime};
