function createWorkingAssetPersistence({state,updateWorkingSaveStamp,renderStorage,renderLodFiles,updateActionAvailability}){
 function handleWorkingSaved(msg){
  state.dirty=!!msg.dirty;
  if(state.asset){
   state.asset.workingSavedAtUtc=msg.savedAtUtc||state.asset.workingSavedAtUtc||'';
   state.asset.workingSaveRevision=Number(msg.saveRevision||state.asset.workingSaveRevision||0);
   state.asset.sourceAssetDirectory=msg.sourceAssetDirectory||state.asset.sourceAssetDirectory||'';
   state.asset.manifestDirty=false;
   const storage=state.asset.storage||(state.asset.storage={});
   storage.manifestDirty=false;
   storage.workingBinaryPath=msg.workingAssetPath||storage.workingBinaryPath;
   storage.workingManifestBytes=Number(msg.workingManifestBytes||0);
   if(Array.isArray(msg.lodPayloads))storage.lodPayloads=msg.lodPayloads;
   storage.workingPackageBytes=storage.workingManifestBytes+(storage.lodPayloads||[]).reduce((sum,payload)=>sum+Number(payload.bytes||0),0);
   for(const lod of state.asset.renderLods||[])lod.dirty=false;
  }
  updateWorkingSaveStamp();
  renderStorage();
  renderLodFiles();
  updateActionAvailability();
 }
 return {handleWorkingSaved};
}

export {createWorkingAssetPersistence};
