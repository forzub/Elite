// Model Asset Editor portable core. Physical extraction wave7C: shared_forms.

function statesInput(prefix,values){return `<input id="${prefix}" type="text" value="${(values||[]).join(', ')}" placeholder="intact, breached">`;}
function vecInputs(prefix,v){return `<div class="grid3"><input id="${prefix}x" type="number" step="0.01" value="${v[0]}"><input id="${prefix}y" type="number" step="0.01" value="${v[1]}"><input id="${prefix}z" type="number" step="0.01" value="${v[2]}"></div>`;}

export {statesInput,vecInputs};
