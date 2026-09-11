// Model Asset Editor portable core. Physical extraction wave7D: final_assembly_validation.
import {escapeMarkup} from '../core/shared.js';

function finalAssemblyValidationModel(report){
 const hasReport=!!report,rows=hasReport?(report.rows||[]).map(x=>({stage:String(x?.stage||'').toUpperCase(),passed:!!x?.passed,message:String(x?.message||'')})):[];
 return{hasReport,passed:hasReport&&!!report.passed,rows};
}
function finalAssemblyValidationHtml(model,text,fragments){
 const rows=(model.rows||[]).map(x=>`<div class="sourceInventoryRow"><span>${escapeMarkup(x.stage)}</span><span class="${x.passed?'ok':'bad'}">${x.passed?'PASS':'FAIL'}</span><span style="grid-column:3/6">${escapeMarkup(x.message)}</span></div>`).join('');
 return `<div class="title">8 · ${text.title}</div><div class="wizardLead">${text.help}</div>${model.hasReport?`<div class="${model.passed?'wizardOk':'wizardWarning'}">${model.passed?text.pass:text.fail}</div><div class="sourceInventory">${rows}</div>`:`<div class="wizardWarning">${text.none}</div>`}${fragments.stageCheckControls}`;
}

export {finalAssemblyValidationModel,finalAssemblyValidationHtml};
