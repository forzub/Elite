// Model Asset Editor portable core. Physical extraction wave7D: physics_node.
import {escapeMarkup} from '../core/shared.js';
import {vecInputs} from '../core/shared_forms.js';

function physicsNodeModel(node){
 const ph=node?.physics||{};
 return{id:String(node?.id||''),mode:String(ph.mode||'disabled'),densityKgM3:ph.densityKgM3??780,massKg:ph.massKg||0,centerOfMass:Array.isArray(ph.centerOfMass)?ph.centerOfMass:[0,0,0],inertiaDiagonal:Array.isArray(ph.inertiaDiagonal)?ph.inertiaDiagonal:[0,0,0],inertiaProducts:Array.isArray(ph.inertiaProducts)?ph.inertiaProducts:[0,0,0]};
}
function physicsNodeHtml(model,text,fragments){return `<div class="field"><label>${text.semanticPart}</label><div><b>${escapeMarkup(model.id)}</b></div></div><div class="title" style="margin-top:10px">${text.baseRigidBody}</div><div class="field"><label>${text.massMode}</label><select id="massMode"><option value="disabled" ${model.mode==='disabled'?'selected':''}>${text.disabled}</option><option value="auto_collision" ${model.mode==='auto_collision'?'selected':''}>${text.autoCollision}</option><option value="manual" ${model.mode==='manual'?'selected':''}>${text.manual}</option></select></div><div class="grid3"><input id="density" type="number" step="10" value="${model.densityKgM3}" title="${text.density}"><input id="mass" type="number" step="1" value="${model.massKg}" title="${text.mass}"><button id="estimateMass" class="mini">${text.estimate}</button></div><div class="field"><label>${text.centerOfMass}</label>${vecInputs('pc',model.centerOfMass)}</div><div class="field"><label>${text.inertiaDiagonal}</label>${vecInputs('pi',model.inertiaDiagonal)}</div><div class="field"><label>${text.inertiaProducts}</label>${vecInputs('pp',model.inertiaProducts)}</div><button id="applyPhysics" class="mini">${text.applyPhysics}</button>${fragments.actions}`;}

export {physicsNodeModel,physicsNodeHtml};
