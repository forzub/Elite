// Model Asset Editor portable core. Physical extraction wave7D: final_assembly_build.
import {escapeMarkup} from '../core/shared.js';

function finalAssemblyBuildModel(input){
 const storage=input?.storage||{},lods=input?.lods||[],path=storage.binaryPath||input?.binaryPath||'',savedLodPayloadCount=(storage.lodPayloads||[]).filter(p=>Number(p?.bytes)>0).length;
 return{productionManifest:String(path||''),savedLodPayloadCount,lodCount:lods.length,dirty:!!input?.dirty};
}
function finalAssemblyBuildHtml(model,text,fragments){return `<div class="title">9 · ${text.title}</div><div class="wizardLead">${text.help}</div><div class="wizardGrid"><span>${text.target}</span><span class="value">${escapeMarkup(model.productionManifest||'—')}</span><span>${text.lods}</span><span class="value">${model.savedLodPayloadCount} / ${model.lodCount}</span><span>${text.dirty}</span><span class="value">${model.dirty?'YES':'NO'}</span></div><div class="wizardWarning">${text.warning}</div>${fragments.stageCheckControls}`;}

export {finalAssemblyBuildModel,finalAssemblyBuildHtml};
