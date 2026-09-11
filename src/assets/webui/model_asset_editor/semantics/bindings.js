// Model Asset Editor portable core. Physical extraction wave7C: semantics_bindings.
import {escapeMarkup} from '../core/shared.js';

function semanticCurrentLodUnbound(lod){if(!lod?.loaded)return 0;return (lod.nodes||[]).filter(n=>n.enabled&&Number(n.geometryIndex)>=0&&Number(n.semanticNodeIndex)<0).length;}
function semanticOwnedPayloadCount(node){const u=semanticUsageForNode(node);return Math.max(0,u.collisions-u.legacySourceBootstrapCollisions)+u.sockets+u.stateVariants+u.hitRegions+u.openings+u.repairTargets+(u.physicsEnabled?1:0);}
function semanticRenderOwner(index,lod){let i=Number(index),guard=0;while(i>=0&&guard++<256){const rn=lod?.nodes?.[i];if(!rn)return-1;const si=Number(rn.semanticNodeIndex??-1);if(si>=0)return si;i=Number(rn.parentIndex??-1);}return-1;}
function semanticUnboundRenderClusterRoot(index,lod){let i=Number(index),root=i,guard=0;while(i>=0&&guard++<256){const rn=lod?.nodes?.[i];if(!rn)return root;if(Number(rn.semanticNodeIndex??-1)>=0)return-1;root=i;const parent=Number(rn.parentIndex??-1);if(parent<0)break;i=parent;}return root;}
function semanticUsageForNode(node){const u=node?.semanticUsage||{};return{renderBindings:Number(u.renderBindings||0),children:Number(u.children||0),collisions:Number(u.collisions||0),legacySourceBootstrapCollisions:Number(u.legacySourceBootstrapCollisions||0),sockets:Number(u.sockets||0),stateVariants:Number(u.stateVariants||0),hitRegions:Number(u.hitRegions||0),openings:Number(u.openings||0),repairTargets:Number(u.repairTargets||0),physicsEnabled:!!u.physicsEnabled,orphanCandidate:!!u.orphanCandidate};}
function semanticVisualLodProfile(lod){const visual=[],semanticOwners=new Set(),unboundClusters=new Set();for(let i=0;i<(lod?.nodes||[]).length;i++){const rn=lod.nodes[i];if(rn?.enabled===false||Number(rn?.geometryIndex)<0)continue;visual.push(i);const owner=semanticRenderOwner(i,lod);if(owner>=0)semanticOwners.add(owner);else unboundClusters.add(semanticUnboundRenderClusterRoot(i,lod));}const clusterCount=semanticOwners.size+unboundClusters.size;return{visualCount:visual.length,ownerCount:semanticOwners.size,unboundVisualCount:visual.filter(i=>semanticRenderOwner(i,lod)<0).length,unboundClusterCount:unboundClusters.size,clusterCount,monolithic:visual.length===1,separable:visual.length>1&&clusterCount>1};}
function wizardSemanticsBindingsBlockModel(input){
 const nodes=input?.nodes||[],lod=input?.lod,selectedNode=input?.selectedNode??null,selectedRenderNode=input?.selectedRenderNode??null;
 if(!lod?.loaded)return{loaded:false,rows:[]};
 return{loaded:true,rows:(lod.nodes||[]).map((rn,ri)=>{const semanticIndex=Number(rn.semanticNodeIndex),geometryIndex=Number(rn.geometryIndex);return{renderNodeIndex:ri,id:String(rn.id??''),selected:selectedRenderNode!==null&&ri===Number(selectedRenderNode),unbound:geometryIndex>=0&&semanticIndex<0,checked:selectedNode!==null&&semanticIndex===Number(selectedNode),disabled:selectedNode===null,geometryLabel:geometryIndex>=0?`G${geometryIndex}`:'—',binding:semanticIndex>=0?String(nodes[semanticIndex]?.id||`N${semanticIndex}`):null};})};
}
function wizardSemanticsBindingRowsHtml(model,text){
 if(!model?.loaded)return `<div class="variantEmpty">${text.chooseLoadedLod}</div>`;
 return (model.rows||[]).map(row=>`<div class="semanticBindingRow ${row.selected?'selected ':''}${row.unbound?'unbound':''}" data-semantic-render-row="${row.renderNodeIndex}"><input type="checkbox" data-semantic-bind="${row.renderNodeIndex}" ${row.checked?'checked':''} ${row.disabled?'disabled':''}><span class="metric">RN${row.renderNodeIndex}</span><span class="name">${escapeMarkup(row.id)}</span><span class="metric">${row.geometryLabel}</span><span class="binding">${escapeMarkup(row.binding===null?text.unbound:row.binding)}</span></div>`).join('');
}
function wizardSemanticsBindingCommandModel(selectedNode,checked,activeLod,renderNodeIndex,applyAllLods){
 if(selectedNode===null||selectedNode===undefined)return null;
 return {lodIndex:activeLod,renderNodeIndex,semanticNodeIndex:checked?selectedNode:-1,applyAllLods:applyAllLods!==false};
}
function semanticBindingSummaryHtml(selectedIndex,lods,activeLod,text){
 if(selectedIndex===null||selectedIndex===undefined)return`<div class="variantEmpty">${text.selectNodeHelp}</div>`;
 return (lods||[]).map((lod,li)=>{const bound=(lod.nodes||[]).filter(rn=>Number(rn.semanticNodeIndex)===Number(selectedIndex)),names=bound.length?bound.map(rn=>escapeMarkup(rn.id)).join('<br>'):'—',stateLabel=bound.length?`${bound.length} RN`:(lod.loaded?text.notBound:text.notLoaded),active=li===activeLod;return `<div class="semanticBindingSummaryRow ${active?'active':''}"><b>LOD${li}${active?' · '+text.active:''}</b><div class="names">${names}</div><span class="state">${stateLabel}</span></div>`;}).join('');
}
function semanticBindingHealthHtml(selectedIndex,lods,unboundCurrent,node,text){
 if(selectedIndex===null||selectedIndex===undefined)return'';
 const fill=(value,vars)=>String(value).replace(/\{([A-Za-z0-9_]+)\}/g,(m,k)=>vars[k]??m),missing=(lods||[]).filter(lod=>lod.loaded&&!(lod.nodes||[]).some(rn=>Number(rn.semanticNodeIndex)===Number(selectedIndex))).length,orphan=!!semanticUsageForNode(node).orphanCandidate,needs=Number(unboundCurrent||0)>0||missing>0||orphan;
 return needs?`<div class="semanticBindingWarn">${fill(text.warn,{missing,unbound:Number(unboundCurrent||0),orphan:orphan?' · ORPHAN':''})}</div>`:`<div class="semanticBindingReady">${text.ok}</div>`;
}
function semanticBindingRepairHtml(selected,selectedVisualCount,bindingPickActive,text){
 if(!selected)return'';
 const fill=(value,vars)=>String(value).replace(/\{([A-Za-z0-9_]+)\}/g,(m,k)=>vars[k]??m),active=!!bindingPickActive,u=semanticUsageForNode(selected),count=Number(selectedVisualCount||0);
 return`<div class="semanticBindingRepair ${active?'pick':''}"><div class="sectionHint">${text.help}</div><button id="semanticPickVisualBtn" class="${active?'wizardComplete':'wizardPrimary'}">${active?text.cancelPick:text.assign3d} · ${fill(text.currentCount,{count})}</button>${active?`<div class="semanticPreviewNote warn">${fill(text.pickExact,{id:`<b>${escapeMarkup(selected.id)}</b>`})}</div>`:''}${u.orphanCandidate?`<div class="semanticPreviewNote warn">${text.orphanHelp}</div>`:''}</div>`;
}
function wizardSemanticsBindingPickTransition(selectedNode,selectedId,currentTarget){const i=Number(selectedNode);if(selectedNode===null||selectedNode===undefined||!Number.isInteger(i))return{ok:false,reason:'invalid'};return currentTarget!==null?{ok:true,mode:'cancel',target:null,nodeIndex:i,nodeId:String(selectedId??'')}:{ok:true,mode:'start',target:i,nodeIndex:i,nodeId:String(selectedId??'')};}
function wizardSemanticsBindingAssignmentCommand(input){const nodes=input?.nodes||[],target=Number(input?.targetIndex),targetNode=nodes?.[target],ri=Number(input?.renderNodeIndex);if(!targetNode)return{ok:false,reason:'invalid_target',targetIndex:target,renderNodeIndex:ri};return{ok:true,targetIndex:target,targetId:String(targetNode.id??''),renderNodeIndex:ri,renderId:String(input?.renderNodeId||'RenderNode'),payload:{lodIndex:input?.lodIndex,renderNodeIndex:ri,semanticNodeIndex:target,applyAllLods:input?.applyAllLods!==false}};}

export {semanticBindingHealthHtml,semanticBindingRepairHtml,semanticBindingSummaryHtml,semanticCurrentLodUnbound,semanticOwnedPayloadCount,semanticRenderOwner,semanticUnboundRenderClusterRoot,semanticUsageForNode,semanticVisualLodProfile,wizardSemanticsBindingAssignmentCommand,wizardSemanticsBindingCommandModel,wizardSemanticsBindingPickTransition,wizardSemanticsBindingRowsHtml,wizardSemanticsBindingsBlockModel};
