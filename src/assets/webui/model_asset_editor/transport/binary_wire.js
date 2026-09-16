const editorWireTextDecoder=new TextDecoder('utf-8');

class EditorWireReader{
 constructor(buffer){this.buffer=buffer;this.view=new DataView(buffer);this.offset=0;}
 require(bytes){if(!Number.isSafeInteger(bytes)||bytes<0||this.offset+bytes>this.buffer.byteLength)throw new Error('truncated editor binary payload');}
 u8(){this.require(1);return this.view.getUint8(this.offset++);}
 u32(){this.require(4);const v=this.view.getUint32(this.offset,true);this.offset+=4;return v;}
 i32(){this.require(4);const v=this.view.getInt32(this.offset,true);this.offset+=4;return v;}
 align4(){this.offset=(this.offset+3)&~3;if(this.offset>this.buffer.byteLength)throw new Error('invalid editor binary alignment');}
 string(){const n=this.u32();this.require(n);const v=editorWireTextDecoder.decode(new Uint8Array(this.buffer,this.offset,n));this.offset+=n;this.align4();return v;}
 f32Array(count){const bytes=count*4;this.require(bytes);if(this.offset&3)throw new Error('unaligned float array');const v=new Float32Array(this.buffer,this.offset,count);this.offset+=bytes;return v;}
 u32Array(count){const bytes=count*4;this.require(bytes);if(this.offset&3)throw new Error('unaligned uint array');const v=new Uint32Array(this.buffer,this.offset,count);this.offset+=bytes;return v;}
 i32Array(count){const bytes=count*4;this.require(bytes);if(this.offset&3)throw new Error('unaligned int array');const v=new Int32Array(this.buffer,this.offset,count);this.offset+=bytes;return v;}
}

function readEditorWireMesh(r){
 const vertexCount=r.u32(),positions=r.f32Array(vertexCount*3),normals=r.f32Array(vertexCount*3),triangleCount=r.u32(),indices=r.u32Array(triangleCount*3),triangleMaterials=r.i32Array(triangleCount),smoothingGroups=r.u32Array(triangleCount),edgeCount=r.u32(),edges=new Array(edgeCount);
 for(let i=0;i<edgeCount;i++)edges[i]={index:i,a:r.u32(),b:r.u32(),triangleA:r.i32(),triangleB:r.i32(),flags:r.u32(),renderMask:r.u8()};
 return{positions,normals,indices,triangleMaterials,smoothingGroups,edges};
}

function readEditorWireRawMesh(r){
 const vertexCount=r.u32(),positions=r.f32Array(vertexCount*3),normals=r.f32Array(vertexCount*3),triangleCount=r.u32(),indices=r.u32Array(triangleCount*3);
 return{positions,normals,indices};
}

function decodeEditorLodGeometry(buffer){
 if(!(buffer instanceof ArrayBuffer)||buffer.byteLength<28)throw new Error('invalid editor binary frame');
 const r=new EditorWireReader(buffer),magic=String.fromCharCode(...new Uint8Array(buffer,0,8));
 r.offset=8;
 if(magic!=='ELWIR001')throw new Error(`unknown editor binary magic ${magic}`);
 const version=r.u32(),messageType=r.u32(),transferId=r.u32(),lodIndex=r.u32(),geometryCount=r.u32();
 if(version!==1||messageType!==1)throw new Error(`unsupported editor binary frame v${version}/type${messageType}`);
 const geometries=[];
 for(let n=0;n<geometryCount;n++){
  const geometryIndex=r.u32(),id=r.string(),mesh=readEditorWireMesh(r),hasRaw=r.u8()!==0;
  r.align4();
  let rawSource=null;
  if(hasRaw)rawSource=readEditorWireRawMesh(r);
  geometries.push({geometryIndex,id,...mesh,rawSource});
 }
 if(r.offset!==buffer.byteLength)throw new Error(`editor binary frame has ${buffer.byteLength-r.offset} trailing byte(s)`);
 return{transferId,lodIndex,geometries};
}

export {EditorWireReader,decodeEditorLodGeometry};
