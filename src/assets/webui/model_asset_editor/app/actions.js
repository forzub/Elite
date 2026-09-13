// Application-level actions for Model Asset Editor control state.
// Authored ModelAsset data is intentionally not mutated here.

const ApplicationActionType=Object.freeze({
 WORKFLOW_STAGE_REQUESTED:'app/workflow-stage-requested',
 CONTROL_FIELD_SET:'app/control-field-set'
});

function requestWorkflowStage(stage,meta={}){
 return {type:ApplicationActionType.WORKFLOW_STAGE_REQUESTED,stage:String(stage||''),meta:{...meta}};
}

function setApplicationControlField(field,value,meta={}){
 return {type:ApplicationActionType.CONTROL_FIELD_SET,field:String(field||''),value,meta:{...meta}};
}

export {ApplicationActionType,requestWorkflowStage,setApplicationControlField};
