// Model Asset Editor portable core. Physical extraction wave7A.
import * as THREE from 'three';
import {composeMatrix} from './transform_math.js';

function previewStateForNode(previewStates,nodes,index){return previewStates?.get(index)||nodes?.[index]?.defaultStateId||'intact';}
function semanticNodePose(nodes,stateVariants,previewStates,index){const base=nodes?.[index];if(!base)return null;const selected=previewStateForNode(previewStates,nodes,index);const variant=(stateVariants||[]).find(v=>v.nodeIndex===index&&v.id===selected&&v.transformOverride);return variant?{...base,localPosition:variant.localPosition,localRotationDeg:variant.localRotationDeg,pivot:variant.pivot}:base;}
function semanticWorldMatrix(index,nodes,stateVariants,previewStates){let cursor=Number(index);if(cursor<0||!nodes?.[cursor])return new THREE.Matrix4();const chain=[];while(cursor>=0&&nodes?.[cursor]){chain.push(cursor);cursor=Number(nodes[cursor]?.parentIndex??-1);}const world=new THREE.Matrix4();for(let i=chain.length-1;i>=0;i--)world.multiply(composeMatrix(semanticNodePose(nodes,stateVariants||[],previewStates,chain[i])));return world;}
function stateApplies(previewStates,nodes,item){const states=item?.activeStates||[];if(!states.length)return true;const si=Number(item?.semanticNodeIndex??item?.parentNodeIndex??-1);if(si<0)return true;return states.includes(previewStateForNode(previewStates,nodes,si));}

export {previewStateForNode, semanticNodePose, semanticWorldMatrix, stateApplies};
