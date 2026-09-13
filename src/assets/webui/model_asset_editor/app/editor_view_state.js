class ProjectedVisibilitySet extends Set{
 constructor(values,commit){super(values);this._commit=commit;}
 add(value){const before=this.size;super.add(value);if(this.size!==before)this._commit?.(this);return this;}
 delete(value){const changed=super.delete(value);if(changed)this._commit?.(this);return changed;}
 clear(){if(!this.size)return;super.clear();this._commit?.(this);}
}

class EditorVisibilityMapAdapter{
 constructor(view,projection){this.view=view;this.projection=projection;}
 has(lodIndex){return this.view.visibilityByLod.has(Number(lodIndex));}
 get(lodIndex){
  const index=Number(lodIndex);
  if(!this.has(index))return undefined;
  const values=this.view.projectVisible(index,this.projection);
  return new ProjectedVisibilitySet(values,set=>this.view.acceptProjection(index,this.projection,set));
 }
 set(lodIndex,values){this.view.acceptProjection(Number(lodIndex),this.projection,new Set(values||[]));return this;}
 delete(lodIndex){return this.view.visibilityByLod.delete(Number(lodIndex));}
 clear(){this.view.visibilityByLod.clear();}
}

class HiddenRenderNodeAdapter{
 constructor(view){this.view=view;}
 has(key){
  const parsed=this.view.parseRenderNodeVisibilityKey(key);
  if(!parsed)return false;
  const explicit=this.view.visibilityByLod.get(parsed.lod);
  return !!explicit&&!explicit.has(this.view.renderVisibilityToken(parsed.id));
 }
 add(key){
  const parsed=this.view.parseRenderNodeVisibilityKey(key);
  if(!parsed)return this;
  const visible=this.view.visibleTokens(parsed.lod,true);
  visible.delete(this.view.renderVisibilityToken(parsed.id));
  this.view.visibilityByLod.set(parsed.lod,visible);
  return this;
 }
 delete(key){
  const parsed=this.view.parseRenderNodeVisibilityKey(key);
  if(!parsed)return false;
  const visible=this.view.visibilityByLod.get(parsed.lod);
  if(!visible)return false;
  const token=this.view.renderVisibilityToken(parsed.id),changed=!visible.has(token);
  visible.add(token);
  this.view.compactVisibility(parsed.lod);
  return changed;
 }
 clear(){this.view.visibilityByLod.delete(Number(this.view.activeLod));}
}

class EditorViewState{
 constructor({lodHasGeometryPayload}){
  this.lodHasGeometryPayload=lodHasGeometryPayload;
  this.assetProvider=()=>null;
  this.invariantScheduler=()=>{};
  this.activeLod=0;
  this.sceneLod=null;
  this.pendingActiveLod=null;
  this.selectedRenderNode=null;
  this.selectedRenderNodeId=null;
  this.selectedMeshId=null;
  this.selectedSemanticNode=null;
  this.visibilityByLod=new Map();
  this.hiddenSemanticByLod=new Map();
  this.isolationByLod=new Map();
  this.loadedLods=new Set();
  this.residentLods=new Set();
  this.geometryInventoryVisibility=new EditorVisibilityMapAdapter(this,'geometry');
  this.lodPreflightVisibility=new EditorVisibilityMapAdapter(this,'geometry');
  this.geometryStageVisibility=new EditorVisibilityMapAdapter(this,'renderNode');
  this.hiddenRenderNodes=new HiddenRenderNodeAdapter(this);
 }
 bindAssetProvider(provider){this.assetProvider=typeof provider==='function'?provider:()=>null;}
 setInvariantScheduler(scheduler){this.invariantScheduler=typeof scheduler==='function'?scheduler:()=>{};}
 asset(){return this.assetProvider?.()||null;}
 resetForAsset(){
  this.activeLod=0;this.sceneLod=null;this.pendingActiveLod=null;
  this.selectedRenderNode=null;this.selectedRenderNodeId=null;this.selectedMeshId=null;this.selectedSemanticNode=null;
  this.visibilityByLod.clear();this.hiddenSemanticByLod.clear();this.isolationByLod.clear();
  this.loadedLods.clear();this.residentLods.clear();
 }
 syncResidency(asset){
  this.loadedLods.clear();this.residentLods.clear();
  (asset?.renderLods||[]).forEach((lod,index)=>{
   if(lod?.loaded)this.loadedLods.add(index);
   if(lod?.loaded&&this.lodHasGeometryPayload(lod))this.residentLods.add(index);
  });
 }
 renderVisibilityToken(id){return `rn:${String(id||'')}`;}
 geometryVisibilityToken(id){return `geo:${String(id||'')}`;}
 allVisibilityTokens(lodIndex){
  const lod=this.asset()?.renderLods?.[Number(lodIndex)],result=new Set();
  for(const geometry of lod?.geometries||[])if(geometry?.id)result.add(this.geometryVisibilityToken(geometry.id));
  for(const node of lod?.nodes||[])if(node?.id&&Number(node?.geometryIndex)>=0)result.add(this.renderVisibilityToken(node.id));
  return result;
 }
 visibleTokens(lodIndex,materialize=false){
  const index=Number(lodIndex),existing=this.visibilityByLod.get(index);
  if(existing)return existing;
  if(!materialize)return null;
  const all=this.allVisibilityTokens(index);
  this.visibilityByLod.set(index,all);
  return all;
 }
 compactVisibility(lodIndex){
  const index=Number(lodIndex),set=this.visibilityByLod.get(index);
  if(!set)return;
  const all=this.allVisibilityTokens(index);
  if(set.size===all.size&&[...all].every(token=>set.has(token)))this.visibilityByLod.delete(index);
 }
 projectVisible(lodIndex,projection){
  const index=Number(lodIndex),lod=this.asset()?.renderLods?.[index],visible=this.visibilityByLod.get(index);
  if(!lod)return new Set();
  if(projection==='renderNode'){
   const result=new Set();
   (lod.nodes||[]).forEach((node,nodeIndex)=>{if(!visible||visible.has(this.renderVisibilityToken(node?.id)))result.add(nodeIndex);});
   return result;
  }
  const result=new Set();
  (lod.geometries||[]).forEach(geometry=>{
   const used=(lod.nodes||[]).filter(node=>Number(node?.geometryIndex)===Number(geometry.index));
   if(!visible||visible.has(this.geometryVisibilityToken(geometry?.id))||used.some(node=>visible.has(this.renderVisibilityToken(node?.id))))result.add(String(geometry.id));
  });
  return result;
 }
 acceptProjection(lodIndex,projection,values){
  const index=Number(lodIndex),lod=this.asset()?.renderLods?.[index];
  if(!lod){this.visibilityByLod.set(index,new Set());return;}
  const visible=new Set();
  if(projection==='renderNode'){
   for(const value of values){const node=lod.nodes?.[Number(value)];if(node?.id)visible.add(this.renderVisibilityToken(node.id));}
  }else{
   const geometryIds=new Set([...values].map(String));
   for(const geometry of lod.geometries||[])if(geometryIds.has(String(geometry?.id||'')))visible.add(this.geometryVisibilityToken(geometry.id));
   for(const node of lod.nodes||[]){
    const geometry=lod.geometries?.[Number(node?.geometryIndex)];
    if(geometry&&geometryIds.has(String(geometry.id))&&node?.id)visible.add(this.renderVisibilityToken(node.id));
   }
  }
  this.visibilityByLod.set(index,visible);
  this.compactVisibility(index);
 }
 setRenderNodeVisible(lodIndex,renderNodeIndex,checked){
  const index=Number(lodIndex),lod=this.asset()?.renderLods?.[index],node=lod?.nodes?.[Number(renderNodeIndex)];
  if(!node?.id)return;
  const existing=this.visibilityByLod.get(index),token=this.renderVisibilityToken(node.id);
  let visible;
  if(!existing)visible=new Set([token]);
  else{
   visible=new Set(existing);
   if(checked)visible.add(token);else visible.delete(token);
  }
  this.visibilityByLod.set(index,visible);
  this.compactVisibility(index);
 }
 renderNodeVisible(lodIndex,renderNodeIndex,node){
  const index=Number(lodIndex),set=this.visibilityByLod.get(index);
  const id=node?.id||this.asset()?.renderLods?.[index]?.nodes?.[Number(renderNodeIndex)]?.id;
  return !set||set.has(this.renderVisibilityToken(id));
 }
 semanticHiddenSet(lodIndex=this.activeLod){
  const index=Number(lodIndex);
  if(!this.hiddenSemanticByLod.has(index))this.hiddenSemanticByLod.set(index,new Set());
  return this.hiddenSemanticByLod.get(index);
 }
 isolatedSemantic(lodIndex=this.activeLod){
  const index=Number(lodIndex);
  return this.isolationByLod.has(index)?this.isolationByLod.get(index):null;
 }
 setIsolatedSemantic(value,lodIndex=this.activeLod){
  const index=Number(lodIndex);
  if(value===null||value===undefined)this.isolationByLod.delete(index);else this.isolationByLod.set(index,Number(value));
 }
 parseRenderNodeVisibilityKey(key){
  const text=String(key||''),split=text.indexOf(':');
  if(split<0)return null;
  const lod=Number(text.slice(0,split)),id=text.slice(split+1);
  return Number.isInteger(lod)&&id?{lod,id}:null;
 }
 clearSelection(){
  this.selectedRenderNode=null;this.selectedRenderNodeId=null;this.selectedMeshId=null;this.selectedSemanticNode=null;
 }
 setActiveLod(value){
  const next=Number(value);
  if(!Number.isInteger(next)||next<0)return;
  const stableId=this.selectedRenderNodeId,stableMesh=this.selectedMeshId;
  this.activeLod=next;
  const lod=this.asset()?.renderLods?.[next];
  let match=stableId?(lod?.nodes||[]).findIndex(node=>String(node?.id||'')===stableId):-1;
  if(match<0&&stableMesh){
   const geometryIndex=(lod?.geometries||[]).findIndex(geometry=>String(geometry?.id||'')===stableMesh);
   if(geometryIndex>=0)match=(lod?.nodes||[]).findIndex(node=>Number(node?.geometryIndex)===geometryIndex);
  }
  if(match>=0)this.setSelectedRenderNode(match);else this.clearSelection();
  this.invariantScheduler('activeLod');
 }
 setSelectedRenderNode(value){
  if(value===null||value===undefined||Number(value)<0){this.clearSelection();return;}
  const index=Number(value),lod=this.asset()?.renderLods?.[this.activeLod],node=lod?.nodes?.[index];
  if(!node){this.clearSelection();return;}
  this.selectedRenderNode=index;
  this.selectedRenderNodeId=String(node.id||'')||null;
  const geometry=lod.geometries?.[Number(node.geometryIndex)];
  this.selectedMeshId=geometry?String(geometry.id):null;
  this.selectedSemanticNode=Number(node.semanticNodeIndex)>=0?Number(node.semanticNodeIndex):null;
 }
 commitSceneLod(lodIndex){this.sceneLod=lodIndex===null?null:Number(lodIndex);}
}

function createEditorViewState(options){return new EditorViewState(options);}

function installEditorViewProjection(state,view){
 Object.defineProperties(state,{
  activeLod:{get:()=>view.activeLod,set:value=>view.setActiveLod(value)},
  sceneLod:{get:()=>view.sceneLod},
  pendingActiveLod:{get:()=>view.pendingActiveLod,set:value=>view.pendingActiveLod=value===null?null:Number(value)},
  selectedRenderNode:{get:()=>view.selectedRenderNode,set:value=>view.setSelectedRenderNode(value)},
  selectedNode:{get:()=>view.selectedSemanticNode,set:value=>view.selectedSemanticNode=value===null?null:Number(value)},
  geometryInventorySelectedId:{get:()=>view.selectedMeshId,set:value=>view.selectedMeshId=value===null?null:String(value)},
  surfaceGeometrySelection:{get:()=>view.selectedMeshId,set:value=>view.selectedMeshId=value===null?null:String(value)},
  geometryInventoryVisibleByLod:{get:()=>view.geometryInventoryVisibility},
  lodPreflightVisibleByLod:{get:()=>view.lodPreflightVisibility},
  geometryStageVisibleByLod:{get:()=>view.geometryStageVisibility},
  hiddenRenderNodes:{get:()=>view.hiddenRenderNodes},
  hidden:{get:()=>view.semanticHiddenSet()},
  isolated:{get:()=>view.isolatedSemantic(),set:value=>view.setIsolatedSemantic(value)}
 });
 return state;
}

export {EditorViewState,createEditorViewState,installEditorViewProjection};
