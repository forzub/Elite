// Model Asset Editor workflow master model. PURE descriptors / presentation helpers only.

const WORKFLOWS=Object.freeze({
  source:Object.freeze([
    {id:'source-summary',target:'.sourceInventory',labelFrom:'.sourceInventoryHeader'},
    {id:'source-maintenance',target:'.maintenanceBlock',labelFrom:'.geometryBlockHead,.maintenanceTop'},
    {id:'source-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  lods:Object.freeze([
    {id:'lod-prepare',target:'.modelPreflightBlock',labelFrom:'.variantAssignHead'},
    {id:'lod-generate',target:'.lodGeneratorBlock',labelFrom:'.variantAssignHead'},
    {id:'lod-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  geometry:Object.freeze([
    {id:'geometry-consolidate',target:'.geometryToolBlock',labelFrom:'.geometryBlockHead'},
    {id:'geometry-variants',target:'.variantAssignBlock',labelFrom:'.variantAssignHead'},
    {id:'geometry-edit',target:'.geometryEditBlock',labelFrom:'.geometryBlockHead'},
    {id:'geometry-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  surfaces:Object.freeze([
    {id:'surface-select',target:'.surfaceBrowser',labelFrom:'.surfaceBrowserHead'},
    {id:'surface-intent',target:'.surfaceBrowser + .surfaceToolBlock',labelFrom:'.geometryBlockHead'},
    {id:'surface-material',target:'.surfaceBrowser + .surfaceToolBlock + .surfaceToolBlock',labelFrom:'.geometryBlockHead'},
    {id:'surface-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  semantics_tree:Object.freeze([
    {id:'semantic-mode',target:'.semanticStructureSummary',labelFrom:'b'},
    {id:'semantic-tree',target:'.semanticTreeBlock',labelFrom:'.geometryBlockHead'},
    {id:'semantic-motion',target:'#semanticMotionHost',labelFrom:'.geometryBlockHead'},
    {id:'semantic-bind',target:'.semanticBindingBlock',labelFrom:'.geometryBlockHead'},
    {id:'semantic-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  semantics_graph:Object.freeze([
    {id:'semantic-mode',target:'.semanticStructureSummary',labelFrom:'b'},
    {id:'graph-select',target:'.structGraphMeshBlock',labelFrom:'.geometryBlockHead'},
    {id:'graph-create',target:'.maintenanceBlock',labelFrom:'.geometryBlockHead'},
    {id:'graph-links',target:'.semanticBindingBlock',labelFrom:'.geometryBlockHead'},
    {id:'semantic-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  physics:Object.freeze([
    {id:'physics-part',target:'#semanticSection',labelFrom:'.title'},
    {id:'physics-body',target:'#semanticInspectorSection',labelFrom:'.title'},
    {id:'physics-collision',target:'#collisionSection',labelFrom:'.title'},
    {id:'physics-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  damage:Object.freeze([
    {id:'damage-part',target:'#semanticSection',labelFrom:'.title'},
    {id:'damage-state',target:'#statesSection',labelFrom:'.title'},
    {id:'damage-visual',target:'#renderAssemblySection',labelFrom:'.title'},
    {id:'damage-targets',target:'#damageSection',labelFrom:'.title'},
    {id:'damage-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  validate:Object.freeze([
    {id:'validate-report',target:'.sourceInventory,.wizardWarning,.wizardOk',labelFrom:'.sourceInventoryHeader,.wizardWarning,.wizardOk'},
    {id:'validate-check',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ]),
  build:Object.freeze([
    {id:'build-target',target:'.wizardGrid',labelFrom:'.wizardGrid'},
    {id:'build-run',target:'#wizardStageCheckBtn',labelFrom:'#wizardStageCheckBtn'}
  ])
});

const normalizeText=value=>String(value??'').replace(/\s+/g,' ').trim();
const compactLabel=(value,fallback='STEP')=>{
  const clean=normalizeText(value).replace(/^\d+\s*[·.:|-]\s*/,'').replace(/[?]+$/,'');
  if(!clean)return fallback;
  const first=clean.split(/\s+[·|]\s+|\n/)[0].trim();
  return first.length<=34?first:first.slice(0,31).trim()+'…';
};
const workflowKey=(stage,structureMode='tree')=>stage==='semantics'?`semantics_${structureMode==='graph'?'graph':'tree'}`:String(stage||'');
const workflowDefinitionFor=(stage,structureMode='tree')=>WORKFLOWS[workflowKey(stage,structureMode)]||Object.freeze([]);
const workflowMasterHtml=steps=>`<nav class="modelAssetWorkflowMaster" aria-label="Workflow">${steps.map((step,index)=>`<button type="button" class="workflowMasterStep" data-workflow-step="${step.id}" data-workflow-target="${step.target}"><span class="workflowMasterNum">${index+1}</span><span class="workflowMasterLabel">${step.label}</span></button>`).join('<span class="workflowMasterArrow">›</span>')}<button type="button" class="workflowHelpButton" data-workflow-help="stage" aria-label="Help">?</button></nav>`;

export {compactLabel,normalizeText,workflowDefinitionFor,workflowKey,workflowMasterHtml};
