// Model Asset Editor portable core. Physical extraction wave7A.
import * as THREE from 'three';

function deg(v){return THREE.MathUtils.degToRad(v||0)}
function composeMatrix(node){const p=node.localPosition||[0,0,0],r=node.localRotationDeg||[0,0,0],pv=node.pivot||[0,0,0];const T1=new THREE.Matrix4().makeTranslation(p[0]+pv[0],p[1]+pv[1],p[2]+pv[2]);const R=new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(deg(r[0]),deg(r[1]),deg(r[2]),'XYZ'));const T2=new THREE.Matrix4().makeTranslation(-pv[0],-pv[1],-pv[2]);return T1.multiply(R).multiply(T2);}

export {composeMatrix, deg};
