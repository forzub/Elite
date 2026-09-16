// Model Asset Editor SEMANTICS workflow model. PURE descriptors / decisions only.
const frozenStep=(id,label,target,mode='')=>Object.freeze({id,label,target,mode});
const t=(translate,key,fallback)=>typeof translate==='function'?translate(key,fallback):fallback;
export const semanticWorkflowSteps=(translate)=>Object.freeze([
 frozenStep('assembly',t(translate,'model_editor.semantics.workflow.assembly','1 · ASSEMBLY'),'.semanticTreeBlock'),
 frozenStep('bindings',t(translate,'model_editor.semantics.workflow.bindings','2 · VISUAL BINDINGS'),'.semanticBindingBlock'),
 frozenStep('motion',t(translate,'model_editor.semantics.workflow.motion','3 · KINEMATICS'),'.semanticMotionBlock'),
 frozenStep('graph',t(translate,'model_editor.semantics.workflow.graph','4 · STRUCTURAL LINKS'),'','graph'),
 frozenStep('check',t(translate,'model_editor.semantics.workflow.check','5 · CHECK'),'#wizardStageCheckBtn')
]);
export const semanticWorkflowDiagnostics=Object.freeze(['.semanticSummary','.semanticStructureSummary','.semanticBindingReady','.semanticBindingWarn','.semanticPreviewNote']);
export const semanticWorkflowHelpIntro=(translate)=>t(translate,'model_editor.semantics.workflow.help_intro','SEMANTICS workflow: author logical structure first, then visual bindings, then kinematics, then the separate structural graph, and finish with CHECK. Technical summaries and diagnostics stay out of the work area and are available through ?.');
export const semanticLegacyVisibility=({hasButton=false,disabled=true}={})=>Object.freeze({visible:!!hasButton&&!disabled,compact:!!hasButton&&!disabled});
export default Object.freeze({semanticWorkflowSteps,semanticWorkflowDiagnostics,semanticWorkflowHelpIntro,semanticLegacyVisibility});
