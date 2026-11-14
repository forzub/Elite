// The mesh editor's 3D view (MESH_EDITOR_SPEC 6.1): one LOD group at a time, parts picked by click, problem edges on top.
// Talks to app.js through window.MeshViewer (app.js is a classic script; this is a module).
import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';

const COLORS = { link: 0x8f7cf0, base: 0x9aa6b4, hover: 0xd8dee6, selected: 0xe8a33d, error: 0xc0584a, back: 0xd13fd1, open: 0xff4a3a, bad: 0xff3fd8 };

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
  links: new Set(),
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
      // a sheet (or a volume with holes) is drawn from both sides, like the game will; a closed volume - front only
      entry.sides = r.sides || 1;
      geo.userData.shared = true;
      entry.front = new THREE.Mesh(geo, partMaterial());
      if (entry.sides === 2) entry.front.material.side = THREE.DoubleSide;
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
        : state.problems && state.errors.has(key) ? COLORS.error : state.links.has(key) ? COLORS.link : COLORS.base;
      e.front.material.color.setHex(c);
      e.back.visible = visible && state.backfaces && e.sides === 1;
    }
    const own = !state.links.has(key);   // a link's problems belong to its own card
    if (e.open) e.open.visible = visible && state.problems && own;
    if (e.bad) e.bad.visible = visible && state.problems && own;
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
  if (asm.root) { if (asm.onPick) asm.onPick(pickNode(e)); return; }
  const key = pickAt(e);
  if (state.onPick) state.onPick(key);
});
renderer.domElement.addEventListener('pointermove', (e) => {
  if (e.buttons || asm.root) return;
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


// ---- the assembly view (steps 2..6, MESH_EDITOR_SPEC 16): nodes = instances of the LOD0 parts with their transforms,
// overlays (hit volumes, zones, mounts, seams, buses, hierarchy) and the explode slider: every node moves away from
// the root's centre in proportion to its depth, the overlays that belong to it move with it.
const BUS_COLOURS = { propulsion_power: 0xff8a3d, service_power: 0xffd84a, coolant: 0x4ac8ff, heat_rejection: 0xff4a8a,
  data: 0x6dff8a, fuel: 0xc77dff, jump_charge: 0x8ab4ff };
const ZONE_COLOURS = { keep_out: 0xe0402c, keep_in: 0x3d6dff, prefer: 0x3ccf5a };
const asm = { root: null, nodes: new Map(), doc: null, explode: 0, show: { hit: true, zones: true, mounts: true, seams: true, buses: true, tree: true },
  lines: null, selected: null, base: null };

function eulerDeg(r) { return new THREE.Euler(...(r || [0, 0, 0]).map((d) => THREE.MathUtils.degToRad(d)), 'XYZ'); }
function boxMesh(half, colour, opts = {}) {
  const geo = new THREE.BoxGeometry(half[0] * 2, half[1] * 2, half[2] * 2);
  if (opts.solid) {
    const m = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: colour, transparent: true, opacity: opts.opacity || 0.18, depthWrite: false }));
    const e = new THREE.LineSegments(new THREE.EdgesGeometry(geo), new THREE.LineBasicMaterial({ color: colour }));
    m.add(e);
    return m;
  }
  return new THREE.LineSegments(new THREE.EdgesGeometry(geo), new THREE.LineBasicMaterial({ color: colour, depthTest: opts.depthTest !== false }));
}
function cylMesh(radius, halfH, colour) {
  const geo = new THREE.CylinderGeometry(radius, radius, halfH * 2, 16);   // three's cylinder axis is Y
  return new THREE.LineSegments(new THREE.EdgesGeometry(geo, 30), new THREE.LineBasicMaterial({ color: colour }));
}
function placed(obj, pos, rot) {
  obj.position.set(...(pos || [0, 0, 0]));
  obj.rotation.copy(eulerDeg(rot));
  return obj;
}

function clearAssembly() {
  if (!asm.root) return;
  scene.remove(asm.root);
  asm.root.traverse((o) => {
    if (o.geometry && !o.userData.shared) o.geometry.dispose();
    if (o.material) o.material.dispose();
  });
  asm.root = null;
  asm.nodes.clear();
}

function buildAssembly(doc) {
  clearAssembly();
  asm.doc = doc;
  const g = state.groups[0];
  if (!g || !g.meshes) return;
  for (const gr of state.groups) if (gr.root) gr.root.visible = false;
  asm.root = new THREE.Group();
  scene.add(asm.root);
  const byId = new Map(doc.nodes.map((n) => [n.id, n]));
  const depth = (n) => { let d = 0, p = n; while (p && p.parent && byId.has(p.parent) && d < 32) { p = byId.get(p.parent); d++; } return d; };
  for (const n of doc.nodes) {
    const offset = new THREE.Group();   // the explode offset of this node and everything that belongs to it
    asm.root.add(offset);
    const holder = placed(new THREE.Group(), n.position, n.rotation_deg);
    offset.add(holder);
    const part = g.meshes.get(n.part);
    let centre = new THREE.Vector3();
    if (part && part.front) {
      const m = new THREE.Mesh(part.front.geometry, partMaterial());
      m.geometry.userData = m.geometry.userData || {};
      m.userData = { node: n.id, shared: true };
      if (part.sides === 2) m.material.side = THREE.DoubleSide;
      holder.add(m);
      holder.updateMatrixWorld(true);
      centre = part.box.getCenter(new THREE.Vector3()).applyMatrix4(holder.matrix);
    } else {
      // a link without a mesh in this model (a Blender module): a violet marker the size of a metre
      const mk = boxMesh([0.5, 0.5, 0.5], COLORS.link, { solid: true, opacity: 0.35 });
      mk.userData = { node: n.id };
      holder.add(mk);
      centre = new THREE.Vector3(...(n.position || [0, 0, 0]));
    }
    asm.nodes.set(n.id, { node: n, offset, holder, centre, depth: depth(n), overlays: new THREE.Group() });
    offset.add(asm.nodes.get(n.id).overlays);
  }
  const free = new THREE.Group();   // overlays not tied to a node (zones)
  asm.root.add(free);
  asm.free = free;
  const into = (nodeId) => (asm.nodes.get(nodeId) || { overlays: free }).overlays;
  for (const h of doc.hit_volumes || []) {
    const o = h.shape === 'cylinder' ? cylMesh(h.radius, h.half_height, 0x3fd0d8) : boxMesh(h.half_size, 0x3fd0d8);
    o.userData = { kind: 'hit', id: h.id };
    into(h.node).add(placed(o, h.position, h.rotation_deg));
  }
  for (const z of doc.zones || []) {
    const o = boxMesh(z.half_size, ZONE_COLOURS[z.kind] || 0xffffff, { solid: true, opacity: 0.12 });
    o.userData = { kind: 'zone', id: z.id };
    free.add(placed(o, z.position, z.rotation_deg));
  }
  for (const m of doc.mounts || []) {
    if (!m.position) continue;
    const o = boxMesh(m.half_size || [0.08, 0.08, 0.08], 0xff9d2e, { solid: true, opacity: 0.6 });
    o.userData = { kind: 'mount', id: m.id };
    into(m.b).add(placed(o, m.position, m.rotation_deg));
  }
  for (const sm of doc.seams || [])
    for (const p of sm.proxies || []) {
      const o = boxMesh(p.half_size, 0xffe04a, { solid: true, opacity: 0.5 });
      o.userData = { kind: 'seam', id: sm.id };
      into(sm.b).add(placed(o, p.position, p.rotation_deg));
    }
  applyExplode();
  restyleAssembly();
}

function rootCentre() {
  const box = new THREE.Box3();
  for (const v of asm.nodes.values()) box.expandByPoint(v.centre);
  return box.isEmpty() ? new THREE.Vector3() : box.getCenter(new THREE.Vector3());
}

function applyExplode() {
  if (!asm.root) return;
  const c = rootCentre();
  const size = (() => { const b = new THREE.Box3(); for (const v of asm.nodes.values()) b.expandByPoint(v.centre); return b.isEmpty() ? 1 : b.getSize(new THREE.Vector3()).length(); })();
  for (const v of asm.nodes.values()) {
    const dir = v.centre.clone().sub(c);
    if (dir.lengthSq() < 1e-8) dir.set(0, 1, 0);
    dir.normalize();
    // depth 0 (the root) stays; deeper nodes travel further; the slider is 0..1 of the model size
    v.offset.position.copy(dir.multiplyScalar(asm.explode * size * 0.6 * (0.6 + 0.4 * Math.min(v.depth, 3))));
  }
  drawLines();
}

function nodeAt(id) {
  const v = asm.nodes.get(id);
  return v ? v.centre.clone().add(v.offset.position) : null;
}

function drawLines() {
  if (asm.lines) { asm.root.remove(asm.lines); asm.lines.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); }); }
  asm.lines = new THREE.Group();
  asm.root.add(asm.lines);
  const seg = (a, b, colour, dashed) => {
    if (!a || !b) return;
    const geo = new THREE.BufferGeometry().setFromPoints([a, b]);
    const mat = dashed ? new THREE.LineDashedMaterial({ color: colour, dashSize: 0.25, gapSize: 0.15, depthTest: false, transparent: true })
      : new THREE.LineBasicMaterial({ color: colour, depthTest: false, transparent: true });
    const l = new THREE.Line(geo, mat);
    if (dashed) l.computeLineDistances();
    l.renderOrder = 20;
    asm.lines.add(l);
  };
  if (asm.show.tree) for (const v of asm.nodes.values()) if (v.node.parent) seg(nodeAt(v.node.id), nodeAt(v.node.parent), 0x8a96a6, true);
  if (asm.show.buses) {
    const count = new Map();
    for (const c of asm.doc.connections || []) {
      // parallel buses between the same pair are fanned out a little so each stays visible
      const key = [c.from, c.to].sort().join('|');
      const k = count.get(key) || 0;
      count.set(key, k + 1);
      const a = nodeAt(c.from), b = nodeAt(c.to);
      if (!a || !b) continue;
      const lift = new THREE.Vector3(0, 0.15 * k, 0);
      seg(a.clone().add(lift), b.clone().add(lift), BUS_COLOURS[c.bus] || 0xffffff, false);
    }
  }
}

function restyleAssembly() {
  if (!asm.root) return;
  asm.root.traverse((o) => {
    const k = o.userData && o.userData.kind;
    if (k === 'hit') o.visible = asm.show.hit;
    if (k === 'zone') o.visible = asm.show.zones;
    if (k === 'mount') o.visible = asm.show.mounts;
    if (k === 'seam') o.visible = asm.show.seams;
    if (o.isMesh && o.userData.node) {
      const sel = o.userData.node === asm.selected;
      if (o.material.color) o.material.color.setHex(sel ? COLORS.selected : asm.doc && linkOf(o.userData.node) ? COLORS.link : COLORS.base);
      o.material.wireframe = state.wire;
    }
  });
}
function linkOf(id) { const v = asm.nodes.get(id); return v && v.node.ref; }

function pickNode(e) {
  if (!asm.root) return null;
  const rect = renderer.domElement.getBoundingClientRect();
  ray.setFromCamera({ x: ((e.clientX - rect.left) / rect.width) * 2 - 1, y: -((e.clientY - rect.top) / rect.height) * 2 + 1 }, camera);
  const targets = [];
  asm.root.traverse((o) => { if (o.isMesh && o.userData.node) targets.push(o); });
  const hit = ray.intersectObjects(targets, false)[0];
  return hit ? hit.object.userData.node : null;
}

window.MeshViewer = {
  // groups from the "inspection" message; keeps the current group index when the model is the same
  async load(groups, keepGroup, wantGroup) {
    const gi = keepGroup && state.current >= 0 && state.current < groups.length ? state.current
      : wantGroup > 0 && wantGroup < groups.length ? wantGroup : 0;
    const refit = !keepGroup;
    dispose();
    state.groups = groups.map((g) => ({ ...g }));
    if (refit) state.current = -1;
    await showGroup(gi, refit);
    return state.current;
  },
  showGroup: (gi) => showGroup(gi, false),
  setErrors(keys) { state.errors = new Set(keys); restyle(); },
  setLinks(keys) { state.links = new Set(keys); restyle(); },
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
  // the assembly view
  async showAssembly(doc) {
    const g = state.groups[0];
    if (g && !g.meshes) Object.assign(g, await buildGroup(g));
    buildAssembly(doc);
  },
  hideAssembly() {
    clearAssembly();
    const g = state.groups[state.current];
    if (g && g.root) g.root.visible = true;
  },
  setExplode(x) { asm.explode = x; applyExplode(); },
  setOverlay(name, on) { asm.show[name] = on; restyleAssembly(); drawLines(); },
  selectNode(id, fit) {
    asm.selected = id;
    restyleAssembly();
    const v = asm.nodes.get(id);
    if (fit && v) {
      const b = new THREE.Box3().setFromObject(v.holder);
      if (!b.isEmpty()) frame(b.translate(v.offset.position));
    }
  },
  onPickNode(cb) { asm.onPick = cb; },
  fitAssembly() { if (asm.root) frame(new THREE.Box3().setFromObject(asm.root)); },
};
window.dispatchEvent(new Event('viewer-ready'));
