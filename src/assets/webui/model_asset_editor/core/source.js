// Model Asset Editor portable core. Physical extraction wave7B: source.
import {instanceAliasRecordsForLod,meshSourceRecordsForLod} from './source_maintenance.js';

function wizardSourceStageModel(asset,settings){
 const lods=asset?.renderLods||[],payloads=asset?.storage?.lodPayloads||[],records=asset?.meshSourceRecords||[];
 return {
  displayName:asset?.displayName||'',
  sourceRoot:settings?.sourceAssetsRoot||'?',
  declaredRenderLods:lods.length,
  sourceBackedGeometries:records.filter(r=>!!r?.sourcePath).length,
  savedLodPayloads:payloads.filter(p=>Number(p.bytes)>0).length,
  inventory:lods.map((lod,lodIndex)=>{const geometries=lod.geometries||[],aliases=instanceAliasRecordsForLod(records,lodIndex).length,extras=geometries.filter(g=>g.isSourceVariant).length;return {lodIndex,loaded:!!lod.loaded,baseGeometries:geometries.length-extras+aliases,extraGeometries:extras,sourceMeshes:meshSourceRecordsForLod(records,lodIndex).filter(r=>!!r?.sourcePath).length};})
 };
}
function wizardSourceStageHtml(model,text,fragments){
 const inventory=(model.inventory||[]).map(row=>`<div class="sourceInventoryRow"><span>LOD${row.lodIndex}</span><span class="${row.loaded?'ok':'bad'}">${row.loaded?text.loaded:text.notLoaded}</span><span>${row.baseGeometries}</span><span>${row.extraGeometries}</span><span>${row.sourceMeshes} ${text.sourceFilesShort}</span></div>`).join('');
 return `<div class="title">1 · ${text.stageLabel}</div><div class="wizardLead">${text.description}</div><div class="wizardGrid"><span>${text.sourceRoot}</span><span class="value">${model.sourceRoot}</span><span>${text.declaredLods}</span><span class="value">${model.declaredRenderLods}</span><span>${text.sourceMeshes}</span><span class="value">${model.sourceBackedGeometries}</span><span>${text.savedPayloads}</span><span class="value">${model.savedLodPayloads}</span></div><div class="sourceInventory"><div class="sourceInventoryHeader"><span>LOD</span><span>${text.stateLabel}</span><span>${text.base}</span><span>${text.extra}</span><span>${text.sources}</span></div>${inventory}</div>${fragments.physicalSize}${fragments.maintenanceScan}<details class="advancedBlock"><summary>${text.fullSourceTools}</summary><div class="wizardActions"><button id="wizardSourceRefreshBtn">↻ ${text.refreshSourceSet}</button><button id="wizardSourceReimportBtn" class="wizardPrimary">↻ ${text.reloadAllSourceMeshes}</button></div></details>${fragments.stageCheck}`;
}

export {wizardSourceStageModel,wizardSourceStageHtml};
