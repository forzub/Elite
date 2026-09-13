function createEditorTransportBridge(){
 let target=null;
 const pendingDiagnostics=[];
 const requireTarget=()=>{
  if(!target)throw new Error('editor transport is not bound');
  return target;
 };
 const bridge={
  send:(...args)=>requireTarget().send(...args),
  reportEditorDiagnostic:(...args)=>{
   if(target)return target.reportEditorDiagnostic(...args);
   pendingDiagnostics.push(args);
  },
  connect:(...args)=>requireTarget().connect(...args),
  beginEditorAssetBinary:(...args)=>requireTarget().beginEditorAssetBinary(...args),
  beginEditorLodBinary:(...args)=>requireTarget().beginEditorLodBinary(...args),
  bind(next){
   if(target)throw new Error('editor transport already bound');
   if(!next?.send||!next?.reportEditorDiagnostic||!next?.connect)throw new Error('invalid editor transport');
   target=next;
   while(pendingDiagnostics.length)target.reportEditorDiagnostic(...pendingDiagnostics.shift());
  },
  isBound:()=>!!target
 };
 return bridge;
}

export {createEditorTransportBridge};
