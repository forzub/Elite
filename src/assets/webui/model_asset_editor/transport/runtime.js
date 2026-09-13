import {createDiagnosticTransport} from './diagnostics.js';
import {createCommandTransport} from './commands.js';
import {createBinaryTransferManager} from './binary_transfers.js';
import {createWebSocketLifecycle} from './websocket.js';

function createEditorTransport({
 WebSocketClass,locationObject,windowObject,setTimeoutFn,now,logger,
 status,tr,scheduleBusyModal,commandLabels,diagnosticValue,getDiagnosticContext,
 getCurrentAsset,retainGeometryPayload,deliverMessage
}){
 let getSocket=()=>null;
 const diagnostics=createDiagnosticTransport({
  windowObject,getSocket:()=>getSocket(),WebSocketClass,diagnosticValue,getDiagnosticContext,logger
 });
 const commands=createCommandTransport({
  getSocket:()=>getSocket(),WebSocketClass,status,tr,scheduleBusyModal,commandLabels,
  reportDiagnostic:diagnostics.report
 });
 const transfers=createBinaryTransferManager({
  getCurrentAsset,retainGeometryPayload,deliverMessage,now,logger
 });
 const websocket=createWebSocketLifecycle({
  WebSocketClass,locationObject,setTimeoutFn,
  beforeConnect:()=>{
   transfers.clear();
   status(tr('model_editor.connecting','Connecting to editor backend…'),'','idle');
  },
  onOpen:()=>{
   status(tr('model_editor.connected','connected'),'ok','idle');
   diagnostics.flush();
   commands.send('request_catalog',{}, {modal:false,status:false});
   commands.send('request_settings',{}, {modal:false,status:false});
  },
  onTextMessage:deliverMessage,
  onBinaryMessage:transfers.handleBinary,
  onDispatchError:error=>{
   logger.error(error);
   diagnostics.report('websocket_dispatch',error);
   status(tr('model_editor.status.transport_error','TRANSPORT ERROR: {error}',{error:error.message||error}),'danger','error');
  },
  onSocketError:error=>diagnostics.report('websocket',error),
  onClose:()=>{
   transfers.clear();
   status(tr('model_editor.disconnected','disconnected'),'danger','error');
  }
 });
 getSocket=websocket.getSocket;
 diagnostics.installGlobalHandlers();
 return {
  send:commands.send,
  reportEditorDiagnostic:diagnostics.report,
  connect:websocket.connect,
  beginEditorAssetBinary:transfers.beginAsset,
  beginEditorLodBinary:transfers.beginLod
 };
}

export {createEditorTransport};
