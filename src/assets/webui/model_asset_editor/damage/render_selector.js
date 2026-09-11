// Model Asset Editor portable core. Physical extraction wave7D: damage_render_selector.
import {escapeMarkup} from '../core/shared.js';
import {statesInput} from '../core/shared_forms.js';

function damageRenderSelectorModel(input){
 const node=input?.renderNode;if(!node)return null;const semanticIndex=Number(node.semanticNodeIndex),nodes=input?.nodes||[];
 return{lodIndex:Number(input?.lodIndex),renderNodeIndex:Number(input?.renderNodeIndex),semanticId:semanticIndex>=0?String(nodes?.[semanticIndex]?.id||`N${semanticIndex}`):'UNBOUND',activeStates:Array.isArray(node.activeStates)?node.activeStates:[],applyAllLods:input?.applyAllLods!==false};
}
function damageRenderSelectorHtml(model,text){return `<div class="inspectorGroup"><div class="inspectorGroupTitle">${text.title}</div><div class="inspectorInfo"><span class="key">${text.semanticBinding}</span><span class="value">${escapeMarkup(model.semanticId)}</span></div><div class="field"><label>${text.activeStates}</label>${statesInput('rnDamageStates',model.activeStates)}</div><label class="semanticApplyAll"><input id="renderStatesApplyAllLods" type="checkbox" ${model.applyAllLods?'checked':''}> <span><b>${text.applyAllLods}</b><br>${text.applyAllLodsHint}</span></label><div class="inspectorActions"><button id="applyRenderStates" class="wizardPrimary">${text.applySelector}</button></div></div>`;}
function damageRenderStatesCommand(input){return{lodIndex:Number(input?.lodIndex),renderNodeIndex:Number(input?.renderNodeIndex),activeStates:Array.isArray(input?.activeStates)?input.activeStates:[],applyAllLods:input?.applyAllLods!==false};}

export {damageRenderSelectorModel,damageRenderSelectorHtml,damageRenderStatesCommand};
