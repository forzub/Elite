// Model Asset Editor portable core. Physical extraction wave7D: hit_volumes_list.

function hitVolumeListModel(input){const selected=input?.selectedCollision;return{rows:(input?.collisionVolumes||[]).map(c=>({index:Number(c.index),id:String(c.id||''),shape:String(c.shape||'box'),activeStates:Array.isArray(c.activeStates)?c.activeStates:[],selected:selected!==null&&selected!==undefined&&Number(selected)===Number(c.index)}))};}
function hitVolumeListHtml(model){return(model?.rows||[]).map(row=>`<div class="listRow ${row.selected?'selected':''}" data-hit-volume-index="${row.index}"><span class="grow">${row.id}</span><span class="badge">${row.shape.toUpperCase()}</span><span class="badge">${row.activeStates.length?row.activeStates.join('|'):'*'}</span></div>`).join('');}

export {hitVolumeListModel,hitVolumeListHtml};
