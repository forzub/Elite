function createDiagnosticTransport({
 windowObject,getSocket,WebSocketClass,diagnosticValue,getDiagnosticContext,
 logger=console,maxQueue=100
}){
 const queue=[];

 function record(category,error,extra={}){
  const detail=diagnosticValue(error),context=getDiagnosticContext?.()||{};
  return {
   command:'editor_ui_diagnostic',
   timestamp:new Date().toISOString(),
   category,
   ...context,
   message:detail.message,
   stack:detail.stack,
   ...extra
  };
 }

 function flush(){
  const socket=getSocket?.();
  if(socket?.readyState!==WebSocketClass.OPEN)return;
  while(queue.length){
   const item=queue.shift();
   try{socket.send(JSON.stringify(item));}
   catch(error){
    queue.unshift(item);
    logger.error('editor diagnostic dispatch failed',error);
    break;
   }
  }
 }

 function report(category,error,extra={}){
  if(queue.length>=maxQueue)queue.shift();
  queue.push(record(category,error,extra));
  flush();
 }

 function installGlobalHandlers(){
  windowObject.addEventListener('error',event=>report('js_exception',event.error||event.message,{
   source:event.filename||'',line:Number(event.lineno||0),column:Number(event.colno||0)
  }));
  windowObject.addEventListener('unhandledrejection',event=>report('unhandledrejection',event.reason));
 }

 return {report,flush,installGlobalHandlers};
}

export {createDiagnosticTransport};
