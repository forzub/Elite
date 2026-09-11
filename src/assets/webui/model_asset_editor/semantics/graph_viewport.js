// Model Asset Editor portable core. Physical extraction wave7C: semantics_graph_viewport.
import * as THREE from 'three';
import {wizardSemanticsBindingAssignmentCommand} from './bindings.js';
import {semanticWorldMatrix} from '../core/semantics_transform.js';
import {semanticRelationKind} from './tree.js';
import {semanticDisplayWorldMatrix} from './world_graph.js';
import {deg} from '../core/transform_math.js';

function semanticGraphStructureSignature(nodes,structuralLinks){const tree=(nodes||[]).map((n,i)=>`${i}:${String(n.id||'')}@${Number(n.parentIndex??-1)}`).join('|'),graph=(structuralLinks||[]).map((l,i)=>`${i}:${l.id}@${l.nodeAIndex}-${l.nodeBIndex}:${l.kind}`).join('|');return tree+'#'+graph;}
function wizardSemanticsGraphObjectModel(nodes,structuralLinks,currentSignature,currentNodeCount){
 nodes=nodes||[];structuralLinks=structuralLinks||[];
 const signature=semanticGraphStructureSignature(nodes,structuralLinks),rebuild=String(currentSignature||'')!==signature||Number(currentNodeCount)!==nodes.length;
 if(!rebuild)return{rebuild:false,signature,nodeIndices:[],semanticLinks:[],structuralLinkIndices:[]};
 return{rebuild:true,signature,nodeIndices:nodes.map((_,i)=>i),semanticLinks:nodes.map((n,i)=>({childIndex:i,parentIndex:Number(n?.parentIndex??-1)})).filter(x=>x.parentIndex>=0&&x.parentIndex<nodes.length),structuralLinkIndices:structuralLinks.map((_,i)=>i)};
}
function wizardSemanticsGraphGizmoPlan(input){
 const nodes=input?.nodes||[],structuralLinks=input?.structuralLinks||[],anchors=input?.anchors||new Map(),lod=input?.lod||null,visualCounts=new Map(),min=input?.minBounds||[0,0,0],max=input?.maxBounds||[1,1,1],diag=Math.max(Math.abs(Number(max[0])-Number(min[0])),Math.abs(Number(max[1])-Number(min[1])),Math.abs(Number(max[2])-Number(min[2])),1),size=diag*.015,mode=String(input?.semanticStructureMode||'tree'),selectedNode=input?.selectedNode,selectedStructuralLink=input?.selectedStructuralLink;
 if(lod?.loaded)for(const rn of lod.nodes||[])if(rn?.enabled&&Number(rn.geometryIndex)>=0&&Number(rn.semanticNodeIndex)>=0)visualCounts.set(Number(rn.semanticNodeIndex),(visualCounts.get(Number(rn.semanticNodeIndex))||0)+1);
 const nodeItems=[];for(let i=0;i<nodes.length;i++){const a=anchors.get(i);if(!a)continue;const selected=i===selectedNode,hasVisual=Number(visualCounts.get(i)||0)>0,selectedMarker=selected&&!hasVisual;nodeItems.push({index:i,position:a.clone(),scale:size*(selectedMarker?1.65:1.15),color:selectedMarker?0xd6ff54:0x79c8ff,opacity:selected&&hasVisual?.32:(selectedMarker?1:.82),visible:true});}
 const semanticLinks=[];for(let i=0;i<nodes.length;i++){const p=Number(nodes[i]?.parentIndex??-1);if(p<0||p>=nodes.length)continue;const a=anchors.get(p),b=anchors.get(i);if(!a||!b||mode==='graph'){semanticLinks.push({childIndex:i,visible:false});continue;}const kind=semanticRelationKind(nodes[i]),selected=i===selectedNode,color=kind==='rotating'?0xffd166:kind==='detachable'?0xff8c55:kind==='rot_detach'?0xff5fc8:0x79c8ff;semanticLinks.push({childIndex:i,visible:true,start:a.clone(),end:b.clone(),markerPosition:a.clone().add(b).multiplyScalar(.5),markerScale:size*(selected?1.45:1),color,lineOpacity:selected?1:.62,markerOpacity:.95});}
 const structural=[];for(let i=0;i<structuralLinks.length;i++){const l=structuralLinks[i],a=anchors.get(Number(l?.nodeAIndex)),b=anchors.get(Number(l?.nodeBIndex));if(mode!=='graph'||!l?.enabled||!a||!b){structural.push({index:i,visible:false});continue;}const selected=i===selectedStructuralLink,color=l.kind==='weld_seam'?0xffb84d:l.kind==='controlled_lock'?0x5dff9a:l.kind==='equipment_mount'?0xc388ff:0xff7b54;structural.push({index:i,visible:true,start:a.clone(),end:b.clone(),markerPosition:a.clone().add(b).multiplyScalar(.5),markerScale:size*(selected?1.55:1.05),color,lineOpacity:selected?1:.8});}
 return{size,nodes:nodeItems,semanticLinks,structuralLinks:structural};
}
function wizardSemanticsJointGizmoPlan(input){
 const nodes=input?.nodes||[],index=input?.selectedNode,n=nodes?.[index];if(input?.wizardStage!=='semantics'||index===null||index===undefined||!n||Number(n.parentIndex)<0)return null;
 const j=n.joint||{},transformInput={nodes,stateVariants:input?.stateVariants||[],previewStates:input?.previewStates,selectedNode:index,wizardStage:input?.wizardStage,previewAngleDeg:input?.previewAngleDeg,previewDetached:input?.previewDetached,minBounds:input?.minBounds||[0,0,0],maxBounds:input?.maxBounds||[1,1,1],graphOffsets:input?.graphOffsets||new Map()},canonicalWorld=semanticWorldMatrix(index,nodes,transformInput.stateVariants,transformInput.previewStates),displayWorld=semanticDisplayWorldMatrix(index,transformInput),pivot=new THREE.Vector3(...(j.pivot||[0,0,0])).applyMatrix4(canonicalWorld),axis=new THREE.Vector3(...(j.axis||[0,1,0])).transformDirection(canonicalWorld);
 if(axis.lengthSq()<1e-10)axis.set(0,1,0);axis.normalize();
 const min=transformInput.minBounds,max=transformInput.maxBounds,diag=Math.max(Math.abs(Number(max[0])-Number(min[0])),Math.abs(Number(max[1])-Number(min[1])),Math.abs(Number(max[2])-Number(min[2])),1),size=diag*.018,originPoint=new THREE.Vector3().setFromMatrixPosition(displayWorld),revolute=String(j.type||'fixed')==='revolute',plan={pivot,axis,originPoint,size,cubeColor:input?.previewDetached?0xffc04d:0x79c8ff,revolute,arrowLength:diag*.10,arrowHeadLength:diag*.025,arrowHeadWidth:diag*.012,arcPoints:[]};
 if(!revolute)return plan;
 const rawA0=deg(Number(j.minAngleDeg??-180)),rawA1=deg(Number(j.maxAngleDeg??180)),fullCircle=Math.abs(rawA1-rawA0)>=Math.PI*2-.001,a0=fullCircle?0:Math.max(-Math.PI*2,rawA0),a1=fullCircle?Math.PI*2:Math.min(Math.PI*2,rawA1),span=Math.max(.001,a1-a0),tmp=Math.abs(axis.y)<.9?new THREE.Vector3(0,1,0):new THREE.Vector3(1,0,0),u=new THREE.Vector3().crossVectors(axis,tmp).normalize(),v=new THREE.Vector3().crossVectors(axis,u).normalize(),steps=64,radius=diag*.055;
 for(let k=0;k<=steps;k++){const a=a0+span*k/steps;plan.arcPoints.push(pivot.clone().addScaledVector(u,Math.cos(a)*radius).addScaledVector(v,Math.sin(a)*radius));}
 return plan;
}
function wizardSemanticsViewportGizmoPickDecision(input){
 const mode=String(input?.semanticStructureMode||'tree'),selectionMode=String(input?.selectionMode||'single'),structural=Number(input?.structuralLinkIndex),link=Number(input?.semanticLinkChildIndex),node=Number(input?.semanticNodeIndex);
 if(mode==='graph'&&Number.isInteger(structural)&&structural>=0)return{action:'structural_link',structuralLinkIndex:structural};
 if(Number.isInteger(link)&&link>=0)return mode==='graph'?{action:'structural_node',nodeIndex:link}:{action:'semantic_node',nodeIndex:link,selectionMode};
 if(Number.isInteger(node)&&node>=0)return mode==='graph'?{action:'structural_node',nodeIndex:node}:{action:'semantic_node',nodeIndex:node,selectionMode};
 return{action:'none'};
}
function wizardSemanticsViewportMeshPickDecision(input){
 const ri=Number(input?.renderNodeIndex),si=Number(input?.semanticNodeIndex??-1),bindingTarget=input?.bindingPickTarget;
 if(bindingTarget!==null&&bindingTarget!==undefined){const command=wizardSemanticsBindingAssignmentCommand({nodes:input?.nodes||[],targetIndex:bindingTarget,lodIndex:input?.lodIndex,renderNodeIndex:ri,renderNodeId:input?.renderNodeId,applyAllLods:input?.applyAllLods});return{action:'binding',command};}
 if(si>=0)return String(input?.semanticStructureMode||'tree')==='graph'?{action:'structural_node',nodeIndex:si,renderNodeIndex:ri}:{action:'semantic_node',nodeIndex:si,renderNodeIndex:ri,selectionMode:String(input?.selectionMode||'single')};
 return{action:'render_node',renderNodeIndex:ri};
}

export {wizardSemanticsGraphGizmoPlan,wizardSemanticsGraphObjectModel,wizardSemanticsJointGizmoPlan,wizardSemanticsViewportGizmoPickDecision,wizardSemanticsViewportMeshPickDecision};
