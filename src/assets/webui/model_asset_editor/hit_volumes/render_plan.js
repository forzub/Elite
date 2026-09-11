// Model Asset Editor portable core. Physical extraction wave7D: hit_volumes_render_plan.
import {stateApplies} from '../core/semantics_transform.js';
import {semanticDisplayWorldMatrix} from '../semantics/world_graph.js';
import {composeMatrix} from '../core/transform_math.js';

function hitVolumeRenderPlan(input){const nodes=input?.nodes||[],transformInput=input?.transformInput||{},rows=[];for(const c of input?.collisionVolumes||[]){if(!c?.enabled||!stateApplies(input?.previewStates,nodes,c))continue;const local=composeMatrix({localPosition:c.localPosition,localRotationDeg:c.localRotationDeg,pivot:[0,0,0]}),world=Number(c.parentNodeIndex)>=0?semanticDisplayWorldMatrix(Number(c.parentNodeIndex),transformInput).multiply(local):local;rows.push({index:Number(c.index),shape:String(c.shape||'box'),radius:Number(c.radius??1),halfHeight:Number(c.halfHeight??1),halfSize:Array.isArray(c.halfSize)?[...c.halfSize]:[1,1,1],selected:input?.selectedCollision!==null&&input?.selectedCollision!==undefined&&Number(c.index)===Number(input.selectedCollision),worldMatrix:world});}return rows;}

export {hitVolumeRenderPlan};
