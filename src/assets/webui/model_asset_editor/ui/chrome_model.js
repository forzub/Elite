// Model Asset Editor UI chrome model. Pure descriptors / decisions only.
export default Object.freeze({
  normalizeText: value => String(value ?? '').replace(/\s+/g, ' ').trim(),
  keepVisible: (text, className='') => {
    const value=String(text||'').replace(/\s+/g,' ').trim();
    const classes=String(className||'').toLowerCase();
    if(!value)return true;
    if(/(?:warn|error|danger|status|issue|block|missing|invalid|ready|passed|failed)/.test(classes))return true;
    return /(?:⚠|⛔|ERROR|WARNING|WARN|FAILED|MISSING|INVALID|BLOCKED|UNAVAILABLE|REQUIRED|READY|PASS|FAIL|NOT\s+(?:LOADED|LINKED|CALIBRATED|BOUND))/i.test(value);
  },
  topicFor: (key,locale='en') => {
    const lang=String(locale||'en').toLowerCase().split(/[-_]/)[0];
    const ru=lang==='ru'||lang==='uk';
    const topics={
      'stage:source': ru?['SOURCE','Исходные файлы, их идентичность и связь с WORKING asset. Здесь проверяется источник, изменения файлов и физический масштаб.']:['SOURCE','Authored source files, source identity and the link to the WORKING asset. Use this stage for source changes and physical scale.'],
      'stage:lods': ru?['LODS','Проверка и подготовка render LOD, анализ и генерация уровней детализации.']:['LODS','Validate and prepare render LOD documents, then analyze or generate detail levels.'],
      'stage:geometry': ru?['GEOMETRY','Работа с геометрией активного LOD: дубликаты, инстансы, варианты и размещение.']:['GEOMETRY','Active-LOD geometry work: duplicates, instances, variants and placement.'],
      'stage:surfaces': ru?['SURFACES','Типы поверхностей и материалы выбранных мешей.']:['SURFACES','Surface intent and render material authoring for selected meshes.'],
      'stage:semantics': ru?['SEMANTICS','Общие логические части asset, transform-связи, motion preview и visual bindings по LOD.']:['SEMANTICS','Asset-wide logical parts, transform links, motion preview and per-LOD visual bindings.'],
      'stage:physics': ru?['PHYSICS','Rigid-body и collision-данные принадлежат semantic parts, а не render mesh.']:['PHYSICS','Rigid-body and collision data belongs to semantic parts rather than render meshes.'],
      'stage:damage': ru?['DAMAGE','Состояния повреждений, openings, repair targets и state-scoped представления.']:['DAMAGE','Damage states, openings, repair targets and state-scoped representations.'],
      'stage:validate': ru?['VALIDATE','Финальная проверка готовности asset перед BUILD.']:['VALIDATE','Final asset-readiness validation before BUILD.'],
      'stage:build': ru?['BUILD','Сборка production/runtime пакета из текущего WORKING asset.']:['BUILD','Build the production/runtime package from the current WORKING asset.'],
      'section:sharedStageMeshSection': ru?['ACTIVE LOD MESHES','Общий список визуальных мешей активного LOD для текущего этапа.']:['ACTIVE LOD MESHES','Shared active-LOD visual mesh list for the current stage.'],
      'section:semanticSection': ru?['SEMANTIC ASSEMBLY','Общая gameplay-структура asset. Она не принадлежит отдельному LOD.']:['SEMANTIC ASSEMBLY','Asset-wide gameplay structure; it is not owned by an individual render LOD.'],
      'section:statesSection': ru?['PART STATES','Состояния части могут менять transform, collision и визуальное представление.']:['PART STATES','Part states may override transform, collision and visual representation.'],
      'section:lodSection': ru?['RENDER LOD FILES','Файлы и состояние независимых render LOD.']:['RENDER LOD FILES','Files and state of independent render LOD documents.'],
      'section:renderAssemblySection': ru?['RENDER ASSEMBLY','Render graph только активного LOD. Другие LOD могут иметь совершенно другую структуру.']:['RENDER ASSEMBLY','Render graph of the active LOD only; other LODs may use completely different structures.'],
      'section:geometrySection': ru?['ACTIVE LOD GEOMETRY','Инвентарь геометрии активного LOD и её source identity.']:['ACTIVE LOD GEOMETRY','Active-LOD geometry inventory and source identity.'],
      'section:activeLodSection': ru?['ACTIVE LOD DETAILS','Статистика и техническое состояние текущего render document.']:['ACTIVE LOD DETAILS','Statistics and technical state for the current render document.'],
      'section:storageSection': ru?['ASSET STORAGE','WORKING/production пути, размеры payload и состояние сохранения.']:['ASSET STORAGE','WORKING/production paths, payload sizes and persistence state.'],
      'section:semanticInspectorSection': ru?['SELECTED SEMANTIC NODE','Инспектор выбранной логической части.']:['SELECTED SEMANTIC NODE','Inspector for the selected logical part.'],
      'section:renderInspectorSection': ru?['SELECTED LOD ELEMENT','Инспектор выбранного RenderNode активного LOD.']:['SELECTED LOD ELEMENT','Inspector for the selected active-LOD RenderNode.'],
      'section:collisionSection': ru?['HIT VOLUMES','Collision volumes и их физические параметры.']:['HIT VOLUMES','Collision volumes and their physical parameters.'],
      'section:socketSection': ru?['SOCKETS','Attachment/camera/weapon точки, связанные с semantic parts.']:['SOCKETS','Attachment, camera and weapon points associated with semantic parts.'],
      'section:damageSection': ru?['DAMAGE / REPAIR','Hit regions, openings и repair targets.']:['DAMAGE / REPAIR','Hit regions, openings and repair targets.'],
      'section:materialsSection': ru?['MATERIALS','Материалы asset и их render-параметры.']:['MATERIALS','Asset materials and their render parameters.'],
      'section:editor': ru?['MODEL ASSET EDITOR','SAVE фиксирует WORKING. RESTORE откатывает несохранённые изменения. CHECK валидирует этап, BUILD пишет production.']:['MODEL ASSET EDITOR','SAVE persists WORKING. RESTORE discards unsaved changes. CHECK validates a stage; BUILD writes production.']
    };
    const row=topics[String(key||'')]||null;
    return row?{title:row[0],intro:row[1]}:{title:'HELP',intro:''};
  }
});
