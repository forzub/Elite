function createApplicationStore(initialState,reducer){
 let current=initialState;
 const listeners=new Set();
 function getState(){return current;}
 function dispatch(action){
  const previous=current;
  const next=reducer(previous,action);
  if(next===undefined)throw new Error('Application reducer returned undefined');
  current=next;
  if(next!==previous){for(const listener of [...listeners])listener(next,previous,action);}
  return action;
 }
 function subscribe(listener){
  if(typeof listener!=='function')return()=>{};
  listeners.add(listener);
  return()=>listeners.delete(listener);
 }
 return Object.freeze({getState,dispatch,subscribe});
}

export {createApplicationStore};
