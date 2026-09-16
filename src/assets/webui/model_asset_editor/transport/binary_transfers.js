import {decodeEditorLodGeometry} from './binary_wire.js';

function createBinaryTransferManager({
 getCurrentAsset,retainGeometryPayload,deliverMessage,now=()=>performance.now(),logger=console
}){
 const transfers=new Map();

 function applyEditorLodGeometry(lod,frame){
  if(!lod)throw new Error(`missing LOD${frame.lodIndex} metadata terminal`);
  const targets=lod.geometries||[];
  if(targets.length!==frame.geometries.length)throw new Error(`LOD${frame.lodIndex} geometry count mismatch: metadata=${targets.length}, binary=${frame.geometries.length}`);
  for(const payload of frame.geometries){
   const target=targets[payload.geometryIndex];
   if(!target||target.id!==payload.id)throw new Error(`LOD${frame.lodIndex} geometry identity mismatch at G${payload.geometryIndex}`);
   target.positions=payload.positions;
   target.normals=payload.normals;
   target.indices=payload.indices;
   target.triangleMaterials=payload.triangleMaterials;
   target.smoothingGroups=payload.smoothingGroups;
   target.edges=payload.edges;
   if(payload.rawSource)target.rawSource=payload.rawSource;
   else delete target.rawSource;
  }
  return lod;
 }

 function reuseEditorLodGeometry(target,source,lodIndex){
  if(!target||!source)throw new Error(`LOD${lodIndex} transport delta has no resident browser base`);
  const targets=target.geometries||[],sources=source.geometries||[];
  if(targets.length!==sources.length)throw new Error(`LOD${lodIndex} transport delta geometry count mismatch`);
  for(let i=0;i<targets.length;i++){
   if(targets[i]?.id!==sources[i]?.id)throw new Error(`LOD${lodIndex} transport delta geometry identity mismatch at G${i}`);
   retainGeometryPayload(targets[i],sources[i]);
   if(Number(targets[i].vertexCount||0)>0&&!targets[i].positions?.length)throw new Error(`LOD${lodIndex} transport delta is missing resident vertex payload for ${targets[i].id}`);
   if(Number(targets[i].triangleCount||0)>0&&!targets[i].indices?.length)throw new Error(`LOD${lodIndex} transport delta is missing resident index payload for ${targets[i].id}`);
   if(Number(targets[i].edgeCount||0)>0&&!targets[i].edges?.length)throw new Error(`LOD${lodIndex} transport delta is missing resident edge payload for ${targets[i].id}`);
  }
 }

 function logTiming(transfer,terminalMs){
  const totalMs=now()-transfer.startedAt,wireMiB=transfer.wireBytes/(1024*1024);
  logger.info(`[ModelAssetEditor][transport] ${transfer.kind} transfer=${transfer.transferId} wire=${wireMiB.toFixed(2)} MiB decode=${transfer.decodeMs.toFixed(1)} ms terminal=${terminalMs.toFixed(1)} ms total=${totalMs.toFixed(1)} ms`);
 }

 function finishAsset(transfer){
  transfers.delete(transfer.transferId);
  transfer.asset.geometryPayloadIncluded=true;
  const terminalStarted=now();
  deliverMessage({type:'asset',dirty:transfer.dirty,asset:transfer.asset,preserveUiSelection:transfer.preserveUiSelection});
  logTiming(transfer,now()-terminalStarted);
 }

 function beginAsset(msg){
  const transferId=Number(msg.transferId)>>>0,remaining=new Set((msg.payloadLods||[]).map(Number));
  if(!msg.asset)throw new Error('asset binary transfer has no metadata');
  const reuse=!!msg.reuseExistingPayloads,current=getCurrentAsset?.();
  if(reuse){
   if(!current||current.assetId!==msg.asset.assetId)throw new Error('asset transport delta has no matching browser base');
   for(let li=0;li<(msg.asset.renderLods||[]).length;li++){
    const target=msg.asset.renderLods[li];
    if(!target?.loaded||remaining.has(li))continue;
    reuseEditorLodGeometry(target,current.renderLods?.[li],li);
   }
  }else{
   for(let li=0;li<(msg.asset.renderLods||[]).length;li++){
    if(msg.asset.renderLods[li]?.loaded&&!remaining.has(li))throw new Error(`full asset transfer omitted resident LOD${li}`);
   }
  }
  const transfer={kind:'asset',transferId,dirty:!!msg.dirty,asset:msg.asset,remaining,preserveUiSelection:!!msg.preserveUiSelection,startedAt:now(),decodeMs:0,wireBytes:0};
  transfers.set(transferId,transfer);
  if(!remaining.size)finishAsset(transfer);
 }

 function beginLod(msg){
  const transferId=Number(msg.transferId)>>>0,lodIndex=Number(msg.lodIndex);
  if(!Number.isInteger(lodIndex)||lodIndex<0||!msg.lod)throw new Error('LOD binary transfer has invalid metadata');
  transfers.set(transferId,{kind:'lod',transferId,dirty:!!msg.dirty,lodIndex,lod:msg.lod,startedAt:now(),decodeMs:0,wireBytes:0});
 }

 function handleBinary(buffer){
  const decodeStarted=now(),frame=decodeEditorLodGeometry(buffer),decodeMs=now()-decodeStarted,transfer=transfers.get(frame.transferId);
  if(!transfer)throw new Error(`orphan editor binary transfer ${frame.transferId}`);
  transfer.decodeMs+=decodeMs;
  transfer.wireBytes+=buffer.byteLength;
  if(transfer.kind==='asset'){
   if(!transfer.remaining.has(frame.lodIndex))throw new Error(`unexpected LOD${frame.lodIndex} in asset transfer ${frame.transferId}`);
   applyEditorLodGeometry(transfer.asset.renderLods?.[frame.lodIndex],frame);
   transfer.remaining.delete(frame.lodIndex);
   if(!transfer.remaining.size)finishAsset(transfer);
   return;
  }
  if(frame.lodIndex!==transfer.lodIndex)throw new Error(`LOD binary transfer mismatch: expected ${transfer.lodIndex}, got ${frame.lodIndex}`);
  applyEditorLodGeometry(transfer.lod,frame);
  transfers.delete(frame.transferId);
  const terminalStarted=now();
  deliverMessage({type:'lod_payload',lodIndex:frame.lodIndex,dirty:transfer.dirty,lod:transfer.lod});
  logTiming(transfer,now()-terminalStarted);
 }

 function clear(){transfers.clear();}
 return {beginAsset,beginLod,handleBinary,clear};
}

export {createBinaryTransferManager};
