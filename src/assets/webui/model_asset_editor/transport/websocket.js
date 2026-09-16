function createWebSocketLifecycle({
 WebSocketClass,locationObject,setTimeoutFn=setTimeout,reconnectDelay=1200,
 beforeConnect,onOpen,onTextMessage,onBinaryMessage,onDispatchError,onSocketError,onClose
}){
 let socket=null,reconnectTimer=null;

 function connect(){
  beforeConnect?.();
  const scheme=locationObject.protocol==='https:'?'wss':'ws';
  socket=new WebSocketClass(`${scheme}://${locationObject.host}`);
  socket.binaryType='arraybuffer';
  socket.onopen=()=>onOpen?.();
  socket.onmessage=event=>{
   try{
    if(typeof event.data==='string')onTextMessage?.(JSON.parse(event.data));
    else if(event.data instanceof ArrayBuffer)onBinaryMessage?.(event.data);
    else throw new Error('unsupported WebSocket payload type');
   }catch(error){onDispatchError?.(error);}
  };
  socket.onerror=()=>onSocketError?.(new Error('WebSocket error event'));
  socket.onclose=()=>{
   socket=null;
   onClose?.();
   reconnectTimer=setTimeoutFn(()=>{reconnectTimer=null;connect();},reconnectDelay);
  };
  return socket;
 }

 function getSocket(){return socket;}
 return {connect,getSocket};
}

export {createWebSocketLifecycle};
