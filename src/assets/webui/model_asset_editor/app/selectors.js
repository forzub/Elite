import {selectApplicationField} from './reducer.js';

function selectWorkflowStage(state){return state?.workflow?.stage||'source';}
function selectWorkflowRevision(state){return Number(state?.workflow?.revision||0);}
function selectApplicationRevision(state){return Number(state?.revision||0);}
function selectControlField(state,field){return selectApplicationField(state,String(field||''));}

function selectApplicationSnapshot(state,editorView=null){
 return Object.freeze({
  workflow:state?.workflow||null,
  session:state?.session||null,
  control:state?.control||null,
  view:editorView||null,
  revision:selectApplicationRevision(state)
 });
}

export {selectWorkflowStage,selectWorkflowRevision,selectApplicationRevision,selectControlField,selectApplicationSnapshot};
