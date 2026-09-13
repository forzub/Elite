import {requestWorkflowStage,setApplicationControlField} from './actions.js';
import {applicationFieldOwned} from './reducer.js';
import {isWorkflowStage} from './workflow.js';

function createApplicationController({store}){
 if(!store?.getState||!store?.dispatch)throw new Error('Application controller requires a store');
 function requestStage(stage,meta={}){
  const requested=String(stage||'');
  if(!isWorkflowStage(requested))return Object.freeze({accepted:false,changed:false,reason:'unknown-stage',stage:requested});
  const before=store.getState().workflow.stage;
  store.dispatch(requestWorkflowStage(requested,meta));
  const after=store.getState().workflow.stage;
  return Object.freeze({accepted:true,changed:before!==after,previousStage:before,stage:after});
 }
 function setControl(field,value,meta={}){
  const key=String(field||'');
  if(!applicationFieldOwned(key))return Object.freeze({accepted:false,changed:false,reason:'unowned-field',field:key});
  const before=store.getState();
  store.dispatch(setApplicationControlField(key,value,meta));
  return Object.freeze({accepted:true,changed:before!==store.getState(),field:key});
 }
 return Object.freeze({requestStage,setControl,dispatch:store.dispatch,subscribe:store.subscribe,getState:store.getState});
}

export {createApplicationController};
