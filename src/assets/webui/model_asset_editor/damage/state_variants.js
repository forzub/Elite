// Model Asset Editor portable core. Physical extraction wave7D: damage_state_variants.

function damageStateVariantsModel(input){
 const nodes=input?.nodes||[],selectedNode=input?.selectedNode;
 return{rows:(input?.stateVariants||[]).map(v=>({id:String(v?.id||''),nodeIndex:Number(v?.nodeIndex),nodeId:String(nodes?.[Number(v?.nodeIndex)]?.id||'?'),detached:!!v?.detached,transformOverride:!!v?.transformOverride,physicsOverride:!!v?.physicsOverride,selected:selectedNode!==null&&selectedNode!==undefined&&Number(selectedNode)===Number(v?.nodeIndex)}))};
}
function damageStateVariantsHtml(model,text){const rows=model?.rows||[];if(!rows.length)return `<span class="muted">${text.empty}</span>`;return rows.map(row=>`<div class="listRow ${row.selected?'selected':''}" data-damage-variant-node="${row.nodeIndex}"><span class="badge">${row.id}</span><span class="grow">${row.nodeId}${row.detached?' · '+text.detached:''}</span>${row.transformOverride?'<span class="badge">T</span>':''}${row.physicsOverride?'<span class="badge">P</span>':''}</div>`).join('');}
function damageDefaultStateCommand(input){return{nodeIndex:Number(input?.nodeIndex),stateId:String(input?.stateId||'')};}
function damageAddStateVariantCommand(input){const id=String(input?.id||'').trim(),node=input?.node;if(!id||!node)return null;return{nodeIndex:Number(input?.nodeIndex),id,displayName:id,position:node.localPosition,rotationDeg:node.localRotationDeg,pivot:node.pivot};}
function damageSetStateVariantCommand(input){return{variantIndex:Number(input?.variantIndex),transformOverride:!!input?.transformOverride,position:input?.position||[0,0,0],rotationDeg:input?.rotationDeg||[0,0,0],pivot:input?.pivot||[0,0,0],physicsOverride:!!input?.physicsOverride,detached:!!input?.detached,enabled:!!input?.enabled,densityKgM3:Number(input?.densityKgM3),massKg:Number(input?.massKg),centerOfMass:input?.centerOfMass||[0,0,0],inertiaDiagonal:input?.inertiaDiagonal||[0,0,0],inertiaProducts:input?.inertiaProducts||[0,0,0]};}
function damageDeleteStateVariantCommand(input){return{variantIndex:Number(input?.variantIndex)};}

export {damageStateVariantsModel,damageStateVariantsHtml,damageDefaultStateCommand,damageAddStateVariantCommand,damageSetStateVariantCommand,damageDeleteStateVariantCommand};
