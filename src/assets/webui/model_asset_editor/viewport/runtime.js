function clearGroup(group){
 if(!group?.children)return;
 while(group.children.length){
  const object=group.children.pop();
  object.parent=null;
  object.traverse?.(node=>{
   if(node.geometry&&!node.userData?.sharedGeometry)node.geometry.dispose?.();
   if(node.material)(Array.isArray(node.material)?node.material:[node.material]).forEach(material=>material?.dispose?.());
  });
 }
}

function createViewportRuntime({
 state,$,THREE,OrbitControls,documentObject,ResizeObserverClass,requestAnimationFrameFn,getDevicePixelRatio,
 updateAxisLegend,onFrame,onPick,localStatus,tr
}){
 const raycaster=new THREE.Raycaster();
 raycaster.params.Line.threshold=.8;
 const mouse=new THREE.Vector2();

 function makeAxisLabel(text,color){
  const canvas=documentObject.createElement('canvas');
  canvas.width=420;
  canvas.height=96;
  const ctx=canvas.getContext('2d');
  ctx.clearRect(0,0,420,96);
  ctx.font='bold 42px sans-serif';
  ctx.textAlign='center';
  ctx.textBaseline='middle';
  ctx.lineWidth=8;
  ctx.strokeStyle='rgba(0,0,0,.9)';
  ctx.strokeText(text,210,50);
  ctx.fillStyle=color;
  ctx.fillText(text,210,50);
  const texture=new THREE.CanvasTexture(canvas);
  const material=new THREE.SpriteMaterial({map:texture,transparent:true,depthTest:false,depthWrite:false});
  const sprite=new THREE.Sprite(material);
  sprite.scale.set(17,3.9,1);
  return sprite;
 }

 function initWorldAxes(){
  const half=50,end=58,group=new THREE.Group();
  const ground=new THREE.Mesh(
   new THREE.PlaneGeometry(100,100),
   new THREE.MeshBasicMaterial({color:0x102538,transparent:true,opacity:.10,side:THREE.DoubleSide,depthWrite:false})
  );
  ground.rotation.x=-Math.PI/2;
  ground.renderOrder=-5;
  group.add(ground);
  const grid=new THREE.GridHelper(100,20,0x3e6688,0x20374b);
  grid.material.transparent=true;
  grid.material.opacity=.72;
  group.add(grid);
  const addArrow=(dir,color,label,pos)=>{
   const arrow=new THREE.ArrowHelper(dir.clone().normalize(),new THREE.Vector3(0,0,0),half,color,3.3,1.7);
   arrow.line.material.depthTest=false;
   arrow.cone.material.depthTest=false;
   arrow.renderOrder=2;
   group.add(arrow);
   const sprite=makeAxisLabel(label,color===0xd85d5d?'#ff7e7e':color===0x55b96d?'#83ec9c':'#86b8ff');
   sprite.position.copy(pos);
   group.add(sprite);
  };
  addArrow(new THREE.Vector3(1,0,0),0xd85d5d,'RIGHT +X',new THREE.Vector3(end,0,0));
  addArrow(new THREE.Vector3(-1,0,0),0x8f3f3f,'LEFT -X',new THREE.Vector3(-end,0,0));
  addArrow(new THREE.Vector3(0,1,0),0x55b96d,'UP +Y',new THREE.Vector3(0,end,0));
  addArrow(new THREE.Vector3(0,-1,0),0x367c49,'DOWN -Y',new THREE.Vector3(0,-end,0));
  addArrow(new THREE.Vector3(0,0,-1),0x5d86cf,'NOSE -Z',new THREE.Vector3(0,0,-end));
  addArrow(new THREE.Vector3(0,0,1),0x395c96,'TAIL +Z',new THREE.Vector3(0,0,end));
  state.scene.add(group);
  updateAxisLegend();
 }

 function resize(){
  const view=$('view');
  if(!state.renderer)return;
  const width=view.clientWidth,height=view.clientHeight;
  state.renderer.setSize(width,height,false);
  state.camera.aspect=Math.max(1,width)/Math.max(1,height);
  state.camera.updateProjectionMatrix();
 }

 function loop(timestamp=0){
  requestAnimationFrameFn(loop);
  onFrame?.(timestamp);
  state.controls?.update();
  if(state.renderer&&state.scene&&state.camera){
   state.renderer.setViewport(0,0,$('view').clientWidth,$('view').clientHeight);
   state.renderer.render(state.scene,state.camera);
  }
 }

 function initScene(){
  state.scene=new THREE.Scene();
  state.scene.background=new THREE.Color(0x090d12);
  state.camera=new THREE.PerspectiveCamera(50,1,.05,1e8);
  state.camera.position.set(15,10,15);
  const view=$('view');
  state.renderer=new THREE.WebGLRenderer({antialias:true});
  state.renderer.setPixelRatio(Math.min(Number(getDevicePixelRatio?.()||1),2));
  view.prepend(state.renderer.domElement);
  state.controls=new OrbitControls(state.camera,state.renderer.domElement);
  state.controls.enableDamping=true;
  state.scene.add(new THREE.HemisphereLight(0xcadfff,0x20242a,1.1));
  const directional=new THREE.DirectionalLight(0xffffff,1.1);
  directional.position.set(2,3,4);
  state.scene.add(directional);
  state.scene.add(state.root);
  state.scene.add(state.collisionGroup);
  state.scene.add(state.socketGroup);
  state.scene.add(state.structuralProxyGroup);
  state.scene.add(state.cameraReferenceGroup);
  if(state.semanticGraphGroup.parent!==state.semanticGizmoGroup)state.semanticGizmoGroup.add(state.semanticGraphGroup);
  if(state.semanticJointGizmoGroup.parent!==state.semanticGizmoGroup)state.semanticGizmoGroup.add(state.semanticJointGizmoGroup);
  state.scene.add(state.semanticGizmoGroup);
  initWorldAxes();
  resize();
  new ResizeObserverClass(resize).observe(view);
  const pickCanvas=state.renderer.domElement;
  let pickStart=null;
  pickCanvas.addEventListener('pointerdown',event=>{
   if(event.button===0)pickStart={id:event.pointerId,x:event.clientX,y:event.clientY};
  });
  pickCanvas.addEventListener('pointerup',event=>{
   if(!pickStart||pickStart.id!==event.pointerId)return;
   const moved=Math.hypot(event.clientX-pickStart.x,event.clientY-pickStart.y);
   pickStart=null;
   if(moved<=4)onPick?.(event);
  });
  pickCanvas.addEventListener('pointercancel',()=>{pickStart=null;});
  requestAnimationFrameFn(loop);
 }

 function fitView(report=false){
  if(!state.asset){
   if(report)localStatus(tr('model_editor.axis_rotation.no_asset','NO CHANGES: no asset loaded'));
   return;
  }
  const box=new THREE.Box3().setFromObject(state.root);
  if(box.isEmpty()){
   if(report)localStatus(tr('model_editor.status.no_visible_fit','NO CHANGES: nothing visible to fit'));
   return;
  }
  const size=new THREE.Vector3(),center=new THREE.Vector3();
  box.getSize(size);
  box.getCenter(center);
  const distance=Math.max(size.x,size.y,size.z,1);
  state.controls.target.copy(center);
  state.camera.position.copy(center).add(new THREE.Vector3(distance*.85,distance*.55,distance*.85));
  state.camera.near=Math.max(.01,distance/10000);
  state.camera.far=Math.max(1000,distance*100);
  state.camera.updateProjectionMatrix();
  state.controls.update();
  raycaster.params.Line.threshold=Math.max(.01,distance*.003);
  $('overlay').textContent=`${tr('model_editor.overlay.size','size {size}',{size:`${size.x.toFixed(2)} × ${size.y.toFixed(2)} × ${size.z.toFixed(2)}`})}\n${tr('model_editor.overlay.selected','selected: {item}',{item:state.selectedNode===null?tr('model_editor.common.none','none'):state.asset.nodes[state.selectedNode].id})}`;
  if(report)localStatus(tr('model_editor.status.view_fitted','View fitted to {size}',{size:`${size.x.toFixed(2)} × ${size.y.toFixed(2)} × ${size.z.toFixed(2)}`}));
 }

 return {clearGroup,initScene,resize,fitView,raycaster,mouse};
}

export {clearGroup,createViewportRuntime};
