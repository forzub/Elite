// Model Asset Editor portable core. Physical extraction wave7D: hit_volumes_commands.

function hitVolumePhysicsUpdateCommand(input){return{collisionIndex:Number(input?.collisionIndex),shape:String(input?.shape||'box'),position:[...(input?.position||[0,0,0])],rotationDeg:[...(input?.rotationDeg||[0,0,0])],halfSize:[...(input?.halfSize||[1,1,1])],radius:Number(input?.radius),halfHeight:Number(input?.halfHeight),enabled:!!input?.enabled};}
function hitVolumeDamageScopeCommand(input){return{collisionIndex:Number(input?.collisionIndex),activeStates:[...(input?.activeStates||[])]};}
function hitVolumeAddCommand(input){const id=String(input?.id||'');if(!id)return null;const node=input?.node||{};return{id,moduleId:node.moduleId||node.id,parentNodeIndex:Number(input?.nodeIndex),shape:String(input?.shape||'box').toLowerCase(),position:[0,0,0],halfSize:[1,1,1],radius:1,halfHeight:1};}
function hitVolumeRadialCommand(input){const count=Number(input?.count),ringRadius=Number(input?.ringRadius),capsuleRadius=Number(input?.capsuleRadius);if(!count||!ringRadius||!capsuleRadius)return null;return{nodeIndex:Number(input?.nodeIndex),count,ringRadius,capsuleRadius,axis:String(input?.axis||'y').toLowerCase(),center:[0,0,0]};}

export {hitVolumePhysicsUpdateCommand,hitVolumeDamageScopeCommand,hitVolumeAddCommand,hitVolumeRadialCommand};
