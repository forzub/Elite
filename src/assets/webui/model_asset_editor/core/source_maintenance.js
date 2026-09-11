// Model Asset Editor portable core. Physical extraction wave7B: source_maintenance.

function meshSourceRecordsForLod(records,lodIndex){return (records||[]).filter(r=>Number(r?.lodIndex)===Number(lodIndex));}
function meshSourceRecordForGeometry(records,geometryId,lodIndex){const id=String(geometryId||'');return meshSourceRecordsForLod(records,lodIndex).find(r=>String(r?.geometryId||'')===id)||null;}
function instanceAliasRecordsForLod(records,lodIndex){return meshSourceRecordsForLod(records,lodIndex).filter(r=>String(r?.representation||'geometry')==='instance'&&!!r?.instanceOfGeometryId);}
function instanceAliasRecordForRenderNode(records,renderNodeId,lodIndex){const id=String(renderNodeId||'');if(!id)return null;return meshSourceRecordsForLod(records,lodIndex).find(r=>!!r?.instanceOfGeometryId&&(r?.instanceRenderNodeIds||[]).some(x=>String(x)===id))||null;}
function maintenanceRows(rows){return (rows||[]).filter(r=>String(r.kind||'')!=='unchanged');}
function sourceChangeRowAcceptedByStageCheck(renderLods,row,stage){const kind=String(row?.kind||'');if(!['added','replaced'].includes(kind))return false;const li=Number(row?.lodIndex),geometryId=String(row?.geometryId||'');const geometry=renderLods?.[li]?.geometries?.find(g=>String(g?.id||'')===geometryId);return !!geometry&&meshStageCheckValue(geometry,stage)==='passed';}
function maintenanceRowStatus(row){const k=String(row.kind||'');return ({replaced:'REPLACED · CHECKS CLEARED',added:'ADDED · CHECKS CLEARED',missing_source:'SOURCE FILE MISSING',hash_error:'HASH ERROR',ambiguous_filename:'AMBIGUOUS FILE NAME',add_failed:'ADD FAILED',replace_failed:'REPLACE FAILED',missing_geometry_identity:'IDENTITY ERROR',instance_source_changed:'SOURCE CHANGED · INSTANCE LINK KEPT'})[k]||k.toUpperCase();}
function maintenanceStatusGlyph(kind){return ({replaced:'Δ',added:'+',missing_source:'−',hash_error:'!',ambiguous_filename:'?',add_failed:'!',replace_failed:'!',missing_geometry_identity:'!',instance_source_changed:'↗'})[String(kind||'')]||'!';}
function maintenanceFileName(path){const p=String(path||'').replace(/\\/g,'/');return p.split('/').pop()||p;}
function maintenanceFileDir(path){const p=String(path||'').replace(/\\/g,'/'),i=p.lastIndexOf('/');return i>=0?p.slice(0,i+1):'';}
function maintenancePendingComponents(maintenance){return maintenance?.components||[];}
function maintenancePendingIds(maintenance){return new Set(maintenancePendingComponents(maintenance).map(x=>String(x.componentId||'')).filter(Boolean));}
function meshValidationPending(g){return !!g?.sourcePath&&!!g?.validationPending;}
function meshStageCheckValue(g,stage){if(!g?.sourcePath)return '';if(g?.sourceMissing)return 'missing';const checks=g?.stageChecks||{};return String(checks?.[stage]||'not_checked');}
function meshStageVisualClass(g,stage){if(!g?.sourcePath)return '';const value=meshStageCheckValue(g,stage);if(value==='missing')return 'meshSourceMissing';return value==='passed'?'meshStagePassed':'meshValidationPending';}
function maintenanceGeometryPending(g){return !!(g?.maintenanceIssues||[]).length||meshValidationPending(g);}
function maintenanceChangeCount(sourceRows,maintenance){const source=maintenanceRows(sourceRows).length,pending=maintenancePendingComponents(maintenance).length;return source+pending;}

export {meshSourceRecordsForLod,meshSourceRecordForGeometry,instanceAliasRecordsForLod,instanceAliasRecordForRenderNode,maintenanceRows,sourceChangeRowAcceptedByStageCheck,maintenanceRowStatus,maintenanceStatusGlyph,maintenanceFileName,maintenanceFileDir,maintenancePendingComponents,maintenancePendingIds,meshValidationPending,meshStageCheckValue,meshStageVisualClass,maintenanceGeometryPending,maintenanceChangeCount};
