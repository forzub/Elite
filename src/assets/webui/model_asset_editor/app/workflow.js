import {ApplicationActionType} from './actions.js';

// Canonical top-level authoring workflow. PHYSICS is an asset-authoring stage;
// runtime simulation of an instantiated object remains outside Model Asset Editor.
const WORKFLOW_STAGES=Object.freeze([
 'source',
 'lods',
 'geometry',
 'surfaces',
 'semantics',
 'physics',
 'damage',
 'validate',
 'build'
]);
const WORKFLOW_STAGE_SET=new Set(WORKFLOW_STAGES);

function isWorkflowStage(value){return WORKFLOW_STAGE_SET.has(String(value||''));}
function workflowStageIndex(value){return WORKFLOW_STAGES.indexOf(String(value||''));}

function createWorkflowState(stage='source'){
 const initial=isWorkflowStage(stage)?String(stage):WORKFLOW_STAGES[0];
 return Object.freeze({stage:initial,previousStage:null,revision:0,lastMeta:Object.freeze({})});
}

function reduceWorkflow(workflow,action){
 const current=workflow||createWorkflowState();
 if(action?.type!==ApplicationActionType.WORKFLOW_STAGE_REQUESTED)return current;
 const next=String(action.stage||'');
 if(!isWorkflowStage(next)||next===current.stage)return current;
 return Object.freeze({
  stage:next,
  previousStage:current.stage,
  revision:Number(current.revision||0)+1,
  lastMeta:Object.freeze({...action.meta})
 });
}

export {WORKFLOW_STAGES,isWorkflowStage,workflowStageIndex,createWorkflowState,reduceWorkflow};
