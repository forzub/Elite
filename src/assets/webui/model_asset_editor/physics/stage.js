// Model Asset Editor portable core. Physical extraction wave7D: physics_stage.

function wizardPhysicsStageModel(input){
 const nodes=input?.nodes||[],collisionVolumes=input?.collisionVolumes||[];
 return{semanticPartCount:nodes.length,activeRigidBodyCount:nodes.filter(n=>String(n?.physics?.mode||'disabled')!=='disabled').length,collisionVolumeCount:collisionVolumes.length};
}
function wizardPhysicsStageHtml(model,text,fragments){return `<div class="title">6 · ${text.title}</div><div class="wizardLead">${text.help}</div><div class="wizardGrid"><span>${text.semanticParts}</span><span class="value">${model.semanticPartCount}</span><span>${text.activeBodies}</span><span class="value">${model.activeRigidBodyCount}</span><span>${text.collisionVolumes}</span><span class="value">${model.collisionVolumeCount}</span></div><div class="wizardOk">${text.hint}</div>${fragments.stageCheckControls}`;}

export {wizardPhysicsStageModel,wizardPhysicsStageHtml};
