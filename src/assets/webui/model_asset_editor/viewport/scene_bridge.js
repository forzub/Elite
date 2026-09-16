function createViewportSceneBridge(){
 let target=null;
 const requireTarget=()=>{
  if(!target)throw new Error('viewport scene effects are not bound');
  return target;
 };
 return {
  rebuildScene:(...args)=>requireTarget().rebuildScene(...args),
  updateVisibility:(...args)=>requireTarget().updateVisibility(...args),
  bind(next){
   if(target)throw new Error('viewport scene effects already bound');
   if(!next?.rebuildScene||!next?.updateVisibility)throw new Error('invalid viewport scene effects');
   target=next;
  }
 };
}

export {createViewportSceneBridge};
