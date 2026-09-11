// Model Asset Editor portable core. Physical extraction wave7D: damage_stage.

function wizardDamageStageModel(input){
 const lod=input?.lod,stateVariants=input?.stateVariants||[],collisionVolumes=input?.collisionVolumes||[];
 return{stateVariantCount:stateVariants.length,stateScopedRenderNodeCount:lod?.loaded?(lod.nodes||[]).filter(n=>(n.activeStates||[]).length).length:0,stateScopedCollisionVolumeCount:collisionVolumes.filter(c=>(c.activeStates||[]).length).length,hitRegionCount:(input?.hitRegions||[]).length,openingCount:(input?.openings||[]).length,repairTargetCount:(input?.repairTargets||[]).length};
}
function wizardDamageStageHtml(model,text,fragments){return `<div class="title">7 · ${text.title}</div><div class="wizardLead">${text.help}</div>${fragments.lodSelector}<div class="wizardGrid"><span>${text.stateVariants}</span><span class="value">${model.stateVariantCount}</span><span>${text.renderScopes}</span><span class="value">${model.stateScopedRenderNodeCount}</span><span>${text.collisionScopes}</span><span class="value">${model.stateScopedCollisionVolumeCount}</span><span>${text.targets}</span><span class="value">${model.hitRegionCount} / ${model.openingCount} / ${model.repairTargetCount}</span></div><div class="wizardOk">${text.hint}</div>${fragments.stageCheckControls}`;}

export {wizardDamageStageModel,wizardDamageStageHtml};
