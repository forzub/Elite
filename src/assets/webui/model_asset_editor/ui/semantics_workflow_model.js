// Model Asset Editor SEMANTICS workflow model. PURE descriptors / decisions only.
const frozenStep=(id,label,target,mode='')=>Object.freeze({id,label,target,mode});

export const semanticWorkflowSteps=(locale='en')=>{
  const lang=String(locale||'en').toLowerCase().split(/[-_]/)[0];
  const ru=lang==='ru'||lang==='uk';
  return Object.freeze([
    frozenStep('assembly',ru?'1 · СТРУКТУРА':'1 · ASSEMBLY','.semanticTreeBlock'),
    frozenStep('bindings',ru?'2 · ВИЗУАЛЬНЫЕ СВЯЗИ':'2 · VISUAL BINDINGS','.semanticBindingBlock'),
    frozenStep('motion',ru?'3 · КИНЕМАТИКА':'3 · KINEMATICS','.semanticMotionBlock'),
    frozenStep('graph',ru?'4 · КОНСТРУКЦИОННЫЕ СВЯЗИ':'4 · STRUCTURAL LINKS','', 'graph'),
    frozenStep('check',ru?'5 · ПРОВЕРКА':'5 · CHECK','#wizardStageCheckBtn')
  ]);
};

export const semanticWorkflowDiagnostics=Object.freeze([
  '.semanticSummary',
  '.semanticStructureSummary',
  '.semanticBindingReady',
  '.semanticBindingWarn',
  '.semanticPreviewNote'
]);

export const semanticWorkflowHelpIntro=(locale='en')=>{
  const lang=String(locale||'en').toLowerCase().split(/[-_]/)[0];
  return (lang==='ru'||lang==='uk')
    ? 'Рабочая последовательность SEMANTICS: сначала логическая структура, затем visual bindings, затем кинематика, затем отдельный structural graph и финальный CHECK. Технические сводки и диагностические тексты скрыты из рабочей области и доступны через ?.'
    : 'SEMANTICS workflow: author logical structure first, then visual bindings, then kinematics, then the separate structural graph, and finish with CHECK. Technical summaries and diagnostics stay out of the work area and are available through ?.';
};

export const semanticLegacyVisibility=({hasButton=false,disabled=true}={})=>Object.freeze({
  visible:!!hasButton&&!disabled,
  compact:!!hasButton&&!disabled
});

export default Object.freeze({
  semanticWorkflowSteps,
  semanticWorkflowDiagnostics,
  semanticWorkflowHelpIntro,
  semanticLegacyVisibility
});
