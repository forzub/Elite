const WIZARD_STAGE_RENDERER_IDS=Object.freeze([
 'source','lods','geometry','surfaces','semantics','physics','damage','validate','build'
]);

function createWizardStageRendererRegistry(renderers){
 const source=renderers||{};
 for(const id of WIZARD_STAGE_RENDERER_IDS){
  if(typeof source[id]!=='function')throw new Error(`Missing Model Asset Editor stage renderer: ${id}`);
 }
 return Object.freeze({
  source:({root,state})=>source.source(root,state.asset,state.settings),
  lods:({root,lods,payloads})=>source.lods(root,lods,payloads),
  geometry:({root,lods})=>source.geometry(root,lods),
  surfaces:({root,lods})=>source.surfaces(root,lods),
  semantics:({root,lods})=>source.semantics(root,lods),
  physics:({root,state})=>source.physics(root,state.asset),
  damage:({root,state,lods})=>source.damage(root,state.asset,lods),
  validate:({root,state})=>source.validate(root,state.wizardValidationReport),
  build:({root,state,lods})=>source.build(root,state.asset,lods,state.dirty)
 });
}

function renderWizardStage(registry,stage,context){
 const id=String(stage||''),render=registry?.[id];
 if(typeof render!=='function')throw new Error(`Unknown Model Asset Editor workflow stage: ${id}`);
 return render(context);
}

export {
 WIZARD_STAGE_RENDERER_IDS,
 createWizardStageRendererRegistry,
 renderWizardStage
};
