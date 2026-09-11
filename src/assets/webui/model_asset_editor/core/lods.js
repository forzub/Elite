// Model Asset Editor portable core. Physical extraction wave7B: lods.
import {meshStageCheckValue} from './source_maintenance.js';

function wizardLodsStageModel(lods,payloads){
 lods=lods||[];payloads=payloads||[];
 const declared=lods.length;
 const saved=payloads.filter(p=>Number(p.bytes)>0&&p.declared!==false).length;
 const staleSaved=payloads.filter(p=>Number(p.bytes)>0&&p.declared===false).length;
 const missing=lods.filter((l,i)=>!l.loaded&&!(payloads.find(p=>Number(p.lod)===i&&p.declared!==false&&Number(p.bytes)>0))).length;
 const pendingPrepare=lods.reduce((total,lod)=>total+(lod.geometries||[]).filter(g=>!!g.sourcePath&&!g.sourceMissing&&meshStageCheckValue(g,'lods')!=='passed').length,0);
 const untrackedPrepare=lods.reduce((total,lod)=>total+(lod.geometries||[]).filter(g=>!g.sourcePath&&!g.sourceMissing).length,0);
 return{declared,saved,staleSaved,missing,pendingPrepare,untrackedPrepare};
}
function wizardLodsStageHtml(model,text,fragments){
 return `<div class="geometryStickyToolbar">${fragments.lodSelector}</div><div class="title">2 · ${text.stageLabel}</div><div class="wizardLead">${text.description}</div><div class="wizardGrid"><span>${text.declared}</span><span class="value">${model.declared}</span><span>${text.saved}</span><span class="value">${model.saved}</span><span>${text.staleSaved}</span><span class="value ${model.staleSaved?'warnText':''}">${model.staleSaved}</span><span>${text.missing}</span><span class="value">${model.missing}</span><span>${text.needsPrepare}</span><span class="value ${model.pendingPrepare?'warnText':'ok'}">${model.pendingPrepare}</span></div><div class="modelPreflightBlock"><div class="variantAssignHead"><span class="grow">${text.preflightTitle}</span><button id="modelPreflightPrepareBtn" class="lodTechButton wizardPrimary">⚙ ${text.prepare}${model.pendingPrepare?` · ${model.pendingPrepare}`:''}</button><button id="modelPreflightCheckBtn" class="lodTechButton wizardPrimary">✓ ${text.check}</button></div><div id="modelPreflightBody" class="modelPreflightBody"></div></div><div class="lodGeneratorBlock"><div class="variantAssignHead"><span class="grow">${text.generatorTitle}</span><button id="lodGeneratorAnalyzeBtn" class="lodTechButton wizardPrimary">⌁ ${text.analyze}</button></div><div id="lodGeneratorBody"></div></div>${fragments.stageCheck}`;
}

export {wizardLodsStageModel,wizardLodsStageHtml};
