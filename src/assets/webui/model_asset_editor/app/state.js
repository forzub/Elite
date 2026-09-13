import {createApplicationController} from './controller.js';
import {CONTROL_FIELDS,SESSION_FIELDS,applicationReducer,createApplicationState,selectApplicationField} from './reducer.js';
import {selectApplicationSnapshot,selectWorkflowStage} from './selectors.js';
import {createApplicationStore} from './store.js';

const APPLICATION_STATE_MARKER=Symbol.for('elite.modelAssetEditor.applicationState');
const PROJECTED_FIELDS=Object.freeze([...SESSION_FIELDS,...CONTROL_FIELDS]);

// Compatibility projection for the current HTML shell. Existing feature/effect
// code may keep using state.foo reads/writes, but the authoritative value is now
// owned by the application reducer. Non-serializable THREE/runtime caches and
// authored ModelAsset data deliberately stay outside this reducer.
function installApplicationState(legacyState){
 if(!legacyState||typeof legacyState!=='object')throw new Error('Model Asset Editor state object is required');
 if(legacyState[APPLICATION_STATE_MARKER])return legacyState[APPLICATION_STATE_MARKER];

 const store=createApplicationStore(createApplicationState(legacyState),applicationReducer);
 const controller=createApplicationController({store});

 Object.defineProperty(legacyState,'wizardStage',{
  enumerable:true,
  configurable:true,
  get:()=>selectWorkflowStage(store.getState()),
  set:value=>{controller.requestStage(value,{source:'legacy-state-projection'});}
 });
 for(const field of PROJECTED_FIELDS){
  Object.defineProperty(legacyState,field,{
   enumerable:true,
   configurable:true,
   get:()=>selectApplicationField(store.getState(),field),
   set:value=>{controller.setControl(field,value,{source:'legacy-state-projection'});}
  });
 }

 const bridge=Object.freeze({
  store,
  controller,
  snapshot:()=>selectApplicationSnapshot(store.getState(),legacyState.editorView||null)
 });
 Object.defineProperties(legacyState,{
  applicationStore:{enumerable:false,configurable:false,value:store},
  applicationController:{enumerable:false,configurable:false,value:controller},
  applicationState:{enumerable:false,configurable:false,get:bridge.snapshot},
  [APPLICATION_STATE_MARKER]:{enumerable:false,configurable:false,value:bridge}
 });
 return bridge;
}

export {PROJECTED_FIELDS,installApplicationState};
