function createCommandTransport({
 getSocket,WebSocketClass,status,tr,scheduleBusyModal,commandLabels,reportDiagnostic
}){
 function send(command,payload={},options={}){
  const socket=getSocket?.();
  if(socket?.readyState!==WebSocketClass.OPEN){
   status(tr('model_editor.status.cannot_send','Cannot send command: editor connection is not open'),'danger','error');
   if(command!=='editor_ui_diagnostic'){
    reportDiagnostic('ui_dispatch',new Error(`Cannot dispatch ${command}: WebSocket is not open`),{commandName:command});
   }
   return false;
  }
  const key=commandLabels[command];
  if(key&&options.status!==false){
   const label=tr(key,command);
   status(`${tr('model_editor.busy.working','WORKING')}: ${label}`,'dirty','working');
   if(options.modal!==false)scheduleBusyModal(label);
  }
  try{
   socket.send(JSON.stringify({command,...payload}));
   return true;
  }catch(error){
   if(command!=='editor_ui_diagnostic')reportDiagnostic('ui_dispatch',error,{commandName:command});
   return false;
  }
 }
 return {send};
}

export {createCommandTransport};
