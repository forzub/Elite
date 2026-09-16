import {installApplicationState} from './state.js';

// Dedicated application bootstrap boundary.
// The HTML shell creates runtime/domain objects; this module only installs the
// authoritative application control state over that compatibility object.
function bootstrapModelAssetEditorApplication(state){
 return installApplicationState(state);
}

export {bootstrapModelAssetEditorApplication};
