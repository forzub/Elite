// Model Asset Editor UI chrome model. PURE descriptors / decisions only.
const topic=(titleKey,titleFallback,introKey,introFallback)=>Object.freeze({titleKey,titleFallback,introKey,introFallback});
const topics=Object.freeze({
 'stage:source':topic('model_editor.wizard.source','SOURCE','model_editor.chrome.stage.source.intro','Authored source files, source identity and the link to the WORKING asset. Use this stage for source changes and physical scale.'),
 'stage:lods':topic('model_editor.wizard.lods','LODS','model_editor.chrome.stage.lods.intro','Validate and prepare render LOD documents, then analyze or generate detail levels.'),
 'stage:geometry':topic('model_editor.wizard.geometry','GEOMETRY','model_editor.chrome.stage.geometry.intro','Active-LOD geometry work: duplicates, instances, variants and placement.'),
 'stage:surfaces':topic('model_editor.wizard.surfaces','SURFACES','model_editor.chrome.stage.surfaces.intro','Surface intent and render material authoring for selected meshes.'),
 'stage:semantics':topic('model_editor.wizard.semantics','SEMANTICS','model_editor.chrome.stage.semantics.intro','Asset-wide logical parts, transform links, motion preview and per-LOD visual bindings.'),
 'stage:physics':topic('model_editor.wizard.physics','PHYSICS','model_editor.chrome.stage.physics.intro','Rigid-body and collision data belongs to semantic parts rather than render meshes.'),
 'stage:damage':topic('model_editor.wizard.damage','DAMAGE','model_editor.chrome.stage.damage.intro','Damage states, openings, repair targets and state-scoped representations.'),
 'stage:validate':topic('model_editor.wizard.validate','VALIDATE','model_editor.chrome.stage.validate.intro','Final asset-readiness validation before BUILD.'),
 'stage:build':topic('model_editor.wizard.build','BUILD','model_editor.chrome.stage.build.intro','Build the production/runtime package from the current WORKING asset.'),
 'section:sharedStageMeshSection':topic('model_editor.section.active_lod_meshes','ACTIVE LOD MESHES','model_editor.chrome.section.active_lod_meshes.intro','Shared active-LOD visual mesh list for the current stage.'),
 'section:semanticSection':topic('model_editor.v4.section.semantic','SEMANTIC ASSEMBLY','model_editor.chrome.section.semantic.intro','Asset-wide gameplay structure; it is not owned by an individual render LOD.'),
 'section:statesSection':topic('model_editor.v4.section.states','PART STATES','model_editor.chrome.section.states.intro','Part states may override transform, collision and visual representation.'),
 'section:lodSection':topic('model_editor.v4.section.lods','RENDER LOD FILES','model_editor.chrome.section.lods.intro','Files and state of independent render LOD documents.'),
 'section:renderAssemblySection':topic('model_editor.v4.section.render_assembly','RENDER ASSEMBLY','model_editor.chrome.section.render_assembly.intro','Render graph of the active LOD only; other LODs may use completely different structures.'),
 'section:geometrySection':topic('model_editor.v4.section.geometry','ACTIVE LOD GEOMETRY','model_editor.chrome.section.geometry.intro','Active-LOD geometry inventory and source identity.'),
 'section:activeLodSection':topic('model_editor.section.active_lod','ACTIVE LOD DETAILS','model_editor.chrome.section.active_lod.intro','Statistics and technical state for the current render document.'),
 'section:storageSection':topic('model_editor.section.storage','ASSET STORAGE','model_editor.chrome.section.storage.intro','WORKING/production paths, payload sizes and persistence state.'),
 'section:semanticInspectorSection':topic('model_editor.v4.section.selected_semantic','SELECTED SEMANTIC NODE','model_editor.chrome.section.selected_semantic.intro','Inspector for the selected logical part.'),
 'section:renderInspectorSection':topic('model_editor.v4.section.selected_render','SELECTED LOD ELEMENT','model_editor.chrome.section.selected_render.intro','Inspector for the selected active-LOD RenderNode.'),
 'section:collisionSection':topic('model_editor.section.hit_volumes','HIT VOLUMES','model_editor.chrome.section.hit_volumes.intro','Collision volumes and their physical parameters.'),
 'section:socketSection':topic('model_editor.section.sockets','SOCKETS','model_editor.chrome.section.sockets.intro','Attachment, camera and weapon points associated with semantic parts.'),
 'section:damageSection':topic('model_editor.v4.section.damage','DAMAGE / REPAIR','model_editor.chrome.section.damage.intro','Hit regions, openings and repair targets.'),
 'section:materialsSection':topic('model_editor.section.materials','MATERIALS','model_editor.chrome.section.materials.intro','Asset materials and their render parameters.'),
 'section:editor':topic('model_editor.brand','MODEL ASSET EDITOR','model_editor.chrome.section.editor.intro','SAVE persists WORKING. RESTORE discards unsaved changes. CHECK validates a stage; BUILD writes production.')
});
const normalizeText=value=>String(value??'').replace(/\s+/g,' ').trim();
const keepVisible=(text,className='')=>{const value=normalizeText(text),classes=String(className||'').toLowerCase();if(!value)return true;if(/(?:warn|error|danger|status|issue|block|missing|invalid|ready|passed|failed)/.test(classes))return true;return /(?:⚠|⛔|ERROR|WARNING|WARN|FAILED|MISSING|INVALID|BLOCKED|UNAVAILABLE|REQUIRED|READY|PASS|FAIL|NOT\s+(?:LOADED|LINKED|CALIBRATED|BOUND))/i.test(value);};
const topicFor=(key,translate=(k,fallback)=>fallback)=>{const row=topics[String(key||'')];if(!row)return{title:translate('model_editor.common.help','HELP'),intro:''};return{title:translate(row.titleKey,row.titleFallback),intro:translate(row.introKey,row.introFallback)};};
export default Object.freeze({normalizeText,keepVisible,topicFor});
