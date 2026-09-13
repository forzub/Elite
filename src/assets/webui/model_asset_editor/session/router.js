function createBackendMessageRouter(handlers){
 const registry=Object.freeze({...handlers});
 return function routeBackendMessage(message){
  const type=String(message?.type||'');
  if(!type)return false;
  const handler=registry[type];
  if(typeof handler!=='function')return false;
  handler(message);
  return true;
 };
}

export {createBackendMessageRouter};
