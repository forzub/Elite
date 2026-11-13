// The mesh editor's 3D view (MESH_EDITOR_SPEC 6.1): one LOD group at a time, parts picked by click, problem edges on top.
// Talks to app.js through window.MeshViewer (app.js is a classic script; this is a module).
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const COLORS = { base: 0x9aa6b4, hover: 0xd8dee6, selected: 0xe8a33d, error: 0xc0584a, back: 0xd13fd1, open: 0xff4a3a, bad: 0xff3fd8 };

const host = document.getElementById('viewport');
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setPixelRatio(window.devicePixelRatio || 1);
renderer.setClearColor(0x0a0c0f);
host.appendChild(renderer.domElement);

const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(40, 1, 0.01, 10000);
camera.position.set(10, 8, 14);
scene.add(camera);
scene.add(new THREE.HemisphereLight(0xdfe8f0, 0x20242a, 0.75));
const sun = new THREE.DirectionalLight(0xffffff, 0.8);
sun.position.set(0.4, 0.8, 1);
camera.add(sun);   // the light follows the view: every face the author looks at is lit
const controls = new OrbitControls(camera, renderer.domElement);
controls.enableDamping = true;

const state = {
  groups: [],          // [{name, url, parts, meshes: Map(key -> {front, back, open, bad, box}), root}]
  current: -1,
  selected: null,
  hover: null,
  errors: new Set(),
  wire: false, problems: true, backfaces: false, isolate: false,
  onPick: null,
};

function resize() {
  const w = host.clientWidth, h = host.clientHeight;
  if (!w || !h) return;
  renderer.setSize(w, h, false);
  renderer.domElement.style.width = w + 'px';
  renderer.domElement.style.height = h + 'px';
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}
new ResizeObserver(resize).observe(host);

function partMaterial() {
  return new THREE.MeshStandardMaterial({ color: COLORS.base, metalness: 0.1, roughness: 0.75, flatShading: true,
    polygonOffset: true, polygonOffsetFactor: 1, polygonOffsetUnits: 1 });
}

async function buildGroup(g) {
  const buf = await (await fetch(g.url)).arrayBuffer();
  const all = new Float32Array(buf);
  const triFloats = g.tris * 9, openFloats = g.open * 6;
  const root = new THREE.Group();
  const meshes = new Map();
  for (const r of g.parts) {
    const entry = { box: new THREE.Box3() };
    if (r.tri[1] > 0) {
      const pos = all.slice(r.tri[0] * 9, (r.tri[0] + r.tri[1]) * 9);
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
      geo.computeVertexNormals();
      geo.computeBoundingBox();
      entry.box.copy(geo.boundingBox);
      entry.front = new THREE.Mesh(geo, partMaterial());
      entry.front.userData.key = r.key;
      entry.back = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: COLORS.back, side: THREE.BackSide }));
      entry.back.visible = state.backfaces;
      root.add(entry.front, entry.back);
    }
    const lines = (start, count, color) => {
      if (!count) return null;
      const pos = all.slice(start, start + count * 6);
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
      const l = new THREE.LineSegments(geo, new THREE.LineBasicMaterial({ color, depthTest: false, transparent: true }));
      l.renderOrder = 10;
      l.visible = state.problems;
      root.add(l);
      return l;
    };
    entry.open = lines(triFloats + r.open[0] * 6, r.open[1], COLORS.open);
    entry.bad = lines(triFloats + openFloats + r.bad[0] * 6, r.bad[1], COLORS.bad);
    meshes.set(r.key, entry);
  }
  return { root, meshes };
}

function groupBox(gi) {
  const box = new THREE.Box3();
  for (const e of state.groups[gi].meshes.values()) if (!e.box.isEmpty()) box.union(e.box);
  return box;
}

function frame(box) {
  if (box.isEmpty()) return;
  const c = box.getCenter(new THREE.Vector3());
  const r = Math.max(box.getSize(new THREE.Vector3()).length() / 2, 0.05);
  const dir = camera.position.clone().sub(controls.target);
  if (dir.lengthSq() < 1e-9) dir.set(1, 0.7, 1.2);
  dir.normalize();
  const dist = r / Math.sin(THREE.MathUtils.degToRad(camera.fov / 2)) * 1.05;
  controls.target.copy(c);
  camera.position.copy(c.clone().add(dir.multiplyScalar(dist)));
  camera.near = Math.max(dist / 1000, 0.001);
  camera.far = dist * 100;
  camera.updateProjectionMatrix();
}

function restyle() {
  const g = state.groups[state.current];
  if (!g || !g.meshes) return;
  for (const [key, e] of g.meshes) {
    const visible = !state.isolate || !state.selected || key === state.selected;
    if (e.front) {
      e.front.visible = visible;
      e.front.material.wireframe = state.wire;
      const c = key === state.selected ? COLORS.selected : key === state.hover ? COLORS.hover
        : state.problems && state.errors.has(key) ? COLORS.error : COLORS.base;
      e.front.material.color.setHex(c);
      e.back.visible = visible && state.backfaces;
    }
    if (e.open) e.open.visible = visible && state.problems;
    if (e.bad) e.bad.visible = visible && state.problems;
  }
}

async function showGroup(gi, refit) {
  const g = state.groups[gi];
  if (!g) return;
  if (!g.meshes) Object.assign(g, await buildGroup(g));
  for (const other of state.groups) if (other.root) other.root.visible = false;
  if (!g.root.parent) scene.add(g.root);
  g.root.visible = true;
  const first = state.current < 0;
  state.current = gi;
  restyle();
  if (first || refit) frame(groupBox(gi));
}

function dispose() {
  for (const g of state.groups) {
    if (!g.root) continue;
    scene.remove(g.root);
    g.root.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
  }
  state.groups = [];
}

// picking: a click that is not a drag
const ray = new THREE.Raycaster();
let downAt = null;
renderer.domElement.addEventListener('pointerdown', (e) => { downAt = [e.clientX, e.clientY]; });
renderer.domElement.addEventListener('pointerup', (e) => {
  if (!downAt || Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]) > 4) return;
  const key = pickAt(e);
  if (state.onPick) state.onPick(key);
});
renderer.domElement.addEventListener('pointermove', (e) => {
  if (e.buttons) return;
  const key = pickAt(e);
  if (key !== state.hover) { state.hover = key; restyle(); }
});
function pickAt(e) {
  const g = state.groups[state.current];
  if (!g || !g.meshes) return null;
  const rect = renderer.domElement.getBoundingClientRect();
  ray.setFromCamera({ x: ((e.clientX - rect.left) / rect.width) * 2 - 1, y: -((e.clientY - rect.top) / rect.height) * 2 + 1 }, camera);
  const targets = [];
  for (const m of g.meshes.values()) if (m.front && m.front.visible) targets.push(m.front);
  const hit = ray.intersectObjects(targets, false)[0];
  return hit ? hit.object.userData.key : null;
}

function loop() {
  requestAnimationFrame(loop);
  controls.update();
  renderer.render(scene, camera);
}
resize();
loop();

window.MeshViewer = {
  // groups from the "inspection" message; keeps the current group index when the model is the same
  async load(groups, keepGroup) {
    const gi = keepGroup && state.current >= 0 && state.current < groups.length ? state.current : 0;
    const refit = !keepGroup;
    dispose();
    state.groups = groups.map((g) => ({ ...g }));
    if (refit) state.current = -1;
    await showGroup(gi, refit);
    return state.current;
  },
  showGroup: (gi) => showGroup(gi, false),
  setErrors(keys) { state.errors = new Set(keys); restyle(); },
  select(key, fit) {
    state.selected = key;
    restyle();
    const e = state.groups[state.current] && state.groups[state.current].meshes && state.groups[state.current].meshes.get(key);
    if (fit && e && !e.box.isEmpty()) frame(e.box);
  },
  hover(key) { state.hover = key; restyle(); },
  set(option, value) { state[option] = value; restyle(); },
  fitAll() { frame(groupBox(state.current)); },
  onPick(cb) { state.onPick = cb; },
};
window.dispatchEvent(new Event('viewer-ready'));
