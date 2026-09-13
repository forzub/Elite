import {ApplicationActionType} from './actions.js';
import {createWorkflowState,reduceWorkflow} from './workflow.js';

// These are editor-control values, not authored ModelAsset payload. They are
// projected back onto the legacy state object during the shell migration so
// existing feature/effect code can keep its current reads and writes.
const SESSION_FIELDS=Object.freeze(['dirty','busy','locale']);
const CONTROL_FIELDS=Object.freeze([
 'settingsSaving','ignoreNextStatusNotice','edgeEdit',
 'geometryReference','geometryVariantSelected','surfaceMaterialSelection',
 'surfaceApplyAllLods','semanticApplyAllLods','damageApplyAllLods','wizardValidationReport',
 'selectedCollision','selectedSocket','damageSelectionKind','damageSelectionIndex',
 'semanticPreviewAngleDeg','semanticPreviewDetached','semanticPreviewRateDegPerSec',
 'semanticMotionPlaying','semanticMotionDirection','semanticMotionLastTs',
 'semanticBindingPickTarget','semanticJointPivotPickTarget',
 'semanticGraphEnabled','semanticGraphExplode','semanticStructureMode',
 'structuralGraphRoot','structuralNodeA','structuralNodeB','selectedStructuralLink','selectedStructuralProxy',
 'surfaceAnalysisReady','surfaceAnalysisRequested','radialRenderNode',
 'lodGeneratorLevel','lodGeneratorApplying','lodGeneratorMeshSelection',
 'lodDiagnosticWireframe','lodDiagnosticFaceNormals','meshViewportMode',
 'axisModalDirectMapping','axisModalInitialMapping'
]);
const SESSION_FIELD_SET=new Set(SESSION_FIELDS);
const CONTROL_FIELD_SET=new Set(CONTROL_FIELDS);

function applicationFieldOwned(field){return SESSION_FIELD_SET.has(field)||CONTROL_FIELD_SET.has(field);}

function createApplicationState(legacy={}){
 const session={};
 for(const field of SESSION_FIELDS)session[field]=legacy[field];
 const control={};
 for(const field of CONTROL_FIELDS)control[field]=legacy[field];
 return Object.freeze({
  workflow:createWorkflowState(legacy.wizardStage),
  session:Object.freeze(session),
  control:Object.freeze(control),
  revision:0
 });
}

function applicationReducer(state,action){
 const current=state||createApplicationState();
 const workflow=reduceWorkflow(current.workflow,action);
 if(workflow!==current.workflow){
  return Object.freeze({...current,workflow,revision:Number(current.revision||0)+1});
 }
 if(action?.type!==ApplicationActionType.CONTROL_FIELD_SET)return current;
 const field=String(action.field||'');
 if(!applicationFieldOwned(field))return current;
 if(SESSION_FIELD_SET.has(field)){
  if(Object.is(current.session[field],action.value))return current;
  return Object.freeze({...current,session:Object.freeze({...current.session,[field]:action.value}),revision:Number(current.revision||0)+1});
 }
 if(Object.is(current.control[field],action.value))return current;
 return Object.freeze({...current,control:Object.freeze({...current.control,[field]:action.value}),revision:Number(current.revision||0)+1});
}

function selectApplicationField(state,field){
 if(SESSION_FIELD_SET.has(field))return state?.session?.[field];
 if(CONTROL_FIELD_SET.has(field))return state?.control?.[field];
 return undefined;
}

export {SESSION_FIELDS,CONTROL_FIELDS,applicationFieldOwned,createApplicationState,applicationReducer,selectApplicationField};
