// Model Asset Editor portable core. Physical extraction wave7D: physics_commands.

function physicsSetCommand(input){return{nodeIndex:Number(input?.nodeIndex),mode:String(input?.mode||'disabled'),densityKgM3:Number(input?.densityKgM3),massKg:Number(input?.massKg),centerOfMass:[...(input?.centerOfMass||[0,0,0])],inertiaDiagonal:[...(input?.inertiaDiagonal||[0,0,0])],inertiaProducts:[...(input?.inertiaProducts||[0,0,0])]};}
function physicsEstimateCommand(input){return{nodeIndex:Number(input?.nodeIndex),densityKgM3:Number(input?.densityKgM3)};}

export {physicsSetCommand,physicsEstimateCommand};
