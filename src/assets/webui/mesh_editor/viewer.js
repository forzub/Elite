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

// ---- the skeleton (elite.rig.v1): a joint turns its part and every part below it about its axis through its pivot;
// angle 0 = the modelled pose. A part without a joint stays where it is.
function poseWorld(rig, angles) {
  const byPart = new Map(rig.joints.map((j) => [j.part, j]));
  const cache = new Map();
  const T = (p, k) => new THREE.Matrix4().makeTranslation(p[0] * k, p[1] * k, p[2] * k);
  const world = (part, depth = 0) => {
    if (cache.has(part)) return cache.get(part);
    const j = byPart.get(part);
    const M = new THREE.Matrix4();
    if (j && depth < 64) {
      if (j.parent && j.parent !== part) M.copy(world(j.parent, depth + 1));
      const v = angles[part] || 0;
      if (v && j.type === 'slide') {   // a slider: the part moves v metres along its axis
        const ax = new THREE.Vector3(...j.axis).normalize().multiplyScalar(v);
        M.multiply(new THREE.Matrix4().makeTranslation(ax.x, ax.y, ax.z));
      } else if (v) {                  // a hinge: the part turns v degrees about its axis through the pivot
        M.multiply(T(j.pivot_m, 1)).multiply(new THREE.Matrix4().makeRotationAxis(new THREE.Vector3(...j.axis).normalize(),
          THREE.MathUtils.degToRad(v))).multiply(T(j.pivot_m, -1));
      }
    }
    cache.set(part, M);
    return M;
  };
  return world;
}
// props of the rig (MESH_EDITOR_SPEC 18.18): another model shown with this one - a repair drone's toolbox on its saddle.
// rig.props[i] = {id, ref, part, point_m, rotation_deg, parts: [{key, tris}], rig}; clip keys "<prop id>/<part>" pose the
// prop's own joints (the lid); extra = {props: shown ids, t, events: the clip's events, anglesAt(t)} - see propBase
const propsState = { rig: null, root: null, items: [] };
function propsFor(rig) {
  if (propsState.rig === rig) return propsState;
  if (propsState.root) disposeRoot(propsState.root);
  propsState.rig = rig;
  propsState.root = null;
  propsState.items = [];
  if (!rig || !rig.props || !rig.props.length) return propsState;
  propsState.root = new THREE.Group();
  for (const p of rig.props) {
    const meshes = new Map();
    for (const part of p.parts || []) {
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(new Float32Array(part.tris), 3));
      geo.computeVertexNormals();
      const m = new THREE.Mesh(geo, partMaterial());
      m.material.color.set(0xd8b04a);
      m.material.side = THREE.DoubleSide;
      m.matrixAutoUpdate = false;
      m.userData.prop = p.id;
      propsState.root.add(m);
      meshes.set(part.key, m);
    }
    const r = (p.rotation_deg || [0, 0, 0]).map((a) => THREE.MathUtils.degToRad(a));
    const place = new THREE.Matrix4().makeTranslation(...(p.point_m || [0, 0, 0]))
      .multiply(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(r[0], r[1], r[2], 'XYZ')));
    propsState.items.push({ p, meshes, place });
  }
  return propsState;
}
// events of a clip (MESH_EDITOR_SPEC 18.18): {t, take: "<prop>", hand, grip, handle} - the hand carries the whole prop
// by its handle (position from the hand, orientation eased from the pose at the take to the pose of the next put /
// dock); {t, put: "<prop>", point_m, rotation_deg} - set down in the frame of the prop's drone part; {t, dock: "<prop>"}
// - back on its saddle; {t, take: "<prop>/<part>", hand} / {t, stow} - a tool in the hand (the offset to the hand it
// had at the take) and back into the prop
function placeMatrix(point, rotDeg) {
  const r = (rotDeg || [0, 0, 0]).map((a) => THREE.MathUtils.degToRad(a));
  return new THREE.Matrix4().makeTranslation(...(point || [0, 0, 0]))
    .multiply(new THREE.Matrix4().makeRotationFromEuler(new THREE.Euler(r[0], r[1], r[2], 'XYZ')));
}
function propBase(it, t, ctx, depth = 0) {
  const evs = ctx.events.filter((e) => e.take === it.p.id || e.put === it.p.id || e.dock === it.p.id);
  let k = -1;
  for (let i = 0; i < evs.length; i++) if (evs[i].t <= t) k = i;
  const w = ctx.worldAt(t);
  const saddle = () => w(it.p.part).clone().multiply(it.place);
  if (k < 0 || depth > 16) return saddle();
  const e = evs[k];
  if (e.dock) return saddle();
  if (e.put) return w(it.p.part).clone().multiply(placeMatrix(e.point_m, e.rotation_deg));
  // carried: from the pose just before the take to the pose of the next put / dock
  const start = propBase(it, e.t - 1e-4, ctx, depth + 1);
  const nx = evs[k + 1];
  let end = start, u = 0;
  if (nx) {
    const we = ctx.worldAt(nx.t);
    end = nx.put ? we(it.p.part).clone().multiply(placeMatrix(nx.point_m, nx.rotation_deg)) : we(it.p.part).clone().multiply(it.place);
    u = Math.min(1, Math.max(0, (t - e.t) / Math.max(1e-6, nx.t - e.t)));
    u = u * u * (3 - 2 * u);
  }
  const q0 = new THREE.Quaternion().setFromRotationMatrix(start), q1 = new THREE.Quaternion().setFromRotationMatrix(end);
  const q = q0.clone().slerp(q1, u);
  const hp = new THREE.Vector3(...(e.grip || [0, 0, 0])).applyMatrix4(w(e.hand));
  const off = new THREE.Vector3(...(e.handle || [0, 0, 0])).applyQuaternion(q);
  return new THREE.Matrix4().compose(hp.sub(off), q, new THREE.Vector3(1, 1, 1));
}
function applyProps(world) {
  const pose = state.pose;
  const ps = propsFor(pose ? pose.rig : null);
  if (!ps.root) return;
  const extra = (pose && pose.extra) || {};
  const shown = new Set(extra.props || []);
  ps.root.visible = shown.size > 0;
  if (!ps.root.visible) return;
  if (!ps.root.parent) scene.add(ps.root);
  const angles = pose.angles || {};
  const t = extra.t || 0;
  const cache = new Map();
  const worldAt = (tt) => {
    if (Math.abs(tt - t) < 1e-9 || !extra.anglesAt) return world;
    const key = tt.toFixed(5);
    if (!cache.has(key)) cache.set(key, poseWorld(pose.rig, extra.anglesAt(tt)));
    return cache.get(key);
  };
  const ctx = { events: [...(extra.events || [])].sort((a, b) => a.t - b.t), worldAt };
  const ownPose = (it, ang) => {
    const own = {};
    for (const [k, v] of Object.entries(ang)) if (k.startsWith(it.p.id + '/')) own[k.slice(it.p.id.length + 1)] = v;
    return it.p.rig && it.p.rig.joints ? poseWorld(it.p.rig, own) : () => new THREE.Matrix4();
  };
  for (const it of ps.items) {
    const on = shown.has(it.p.id);
    const pw = ownPose(it, angles);
    const base = propBase(it, t, ctx);
    for (const [key, m] of it.meshes) {
      m.visible = on;
      const full = `${it.p.id}/${key}`;
      let last = null;
      for (const e of ctx.events) if (e.t <= t && (e.take === full || e.stow === full)) last = e;
      if (last && last.take) {
        const a0 = extra.anglesAt ? extra.anglesAt(last.t) : angles;
        const toolAt = propBase(it, last.t, ctx).multiply(ownPose(it, a0)(key));
        const w0 = worldAt(last.t);
        const offset = w0(last.hand).clone().invert().multiply(toolAt);
        m.matrix.copy(world(last.hand)).multiply(offset);
      } else {
        m.matrix.copy(base).multiply(pw(key));
      }
      m.matrixWorldNeedsUpdate = true;
    }
  }
}

function applyPose() {
  const g = state.groups[state.current];
  if (!g || !g.meshes) return;
  const world = state.pose ? poseWorld(state.pose.rig, state.pose.angles) : null;
  applyProps(world || (() => new THREE.Matrix4()));
  // a part of the model itself taken into a hand by a clip event (the crab's tools in its back compartment):
  // {t, take: "<part>", hand} .. {t, stow: "<part>"} - the part keeps the offset to the hand it had at the take
  const extra = (state.pose && state.pose.extra) || {};
  const evs = [...(extra.events || [])].filter((e) => e.t <= (extra.t || 0)).sort((a, b) => a.t - b.t);
  const heldBy = new Map();
  for (const e of evs) {
    const k = e.take || e.stow;
    if (k && !k.includes('/') && g.meshes.has(k)) heldBy.set(k, e.take ? e : null);
  }
  // the held part and the parts hanging from it (a telescopic tool's sections, its head cheeks): they keep their own
  // joints' motion relative to the held part
  const parentOf = new Map(((state.pose && state.pose.rig.joints) || []).map((j) => [j.part, j.parent]));
  const heldMatrix = (key) => {
    let root = key;
    for (let i = 0; root && i < 64 && !heldBy.get(root); i++) root = parentOf.get(root);
    const e = root ? heldBy.get(root) : null;
    if (!e || !extra.anglesAt) return null;
    const w0 = poseWorld(state.pose.rig, extra.anglesAt(e.t));
    const H = world(e.hand).clone().multiply(w0(e.hand).clone().invert().multiply(w0(root)));
    return root === key ? H : H.multiply(world(root).clone().invert().multiply(world(key)));
  };
  for (const [key, e] of g.meshes) {
    const M = world ? (heldMatrix(key) || world(key)) : null;
    for (const o of [e.front, e.back, e.open, e.bad]) {
      if (!o) continue;
      o.matrixAutoUpdate = false;
      if (M) o.matrix.copy(M); else o.matrix.identity();
      o.matrixWorldNeedsUpdate = true;
    }
  }
}

async function showGroup(gi, refit) {
  const g = state.groups[gi];
  if (!g) return;
  if (!g.meshes) {
    const built = await buildGroup(g);
    // another model / variant was loaded while the meshes came: drop them, else they stay in the scene for good
    if (!state.groups.includes(g)) { disposeRoot(built.root); return; }
    Object.assign(g, built);
  }
  for (const other of state.groups) if (other.root) other.root.visible = false;
  if (!g.root.parent) scene.add(g.root);
  g.root.visible = true;
  const first = state.current < 0;
  state.current = gi;
  restyle();
  applyPose();   // another LOD keeps the pose of the player
  if (first || refit) frame(groupBox(gi));
}

function disposeRoot(root) {
  if (!root) return;
  scene.remove(root);
  root.traverse((o) => { if (o.geometry) o.geometry.dispose(); if (o.material) o.material.dispose(); });
}
function dispose() {
  for (const g of state.groups) disposeRoot(g.root);
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
// overlays (hit volumes, zones, mounts, seams, buses, hierarchy) and the explode slider (applyExplode: the parts fly
// apart from the model's centre - along the main axis or straight away from it), the overlays move with their node.
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
function cylMesh(radius, halfH, colour, radiusTop) {
  // three's cylinder axis is Y; radiusTop (a frustum, a conical pad) is the radius at +Y
  const top = radiusTop >= 0 ? radiusTop : radius;
  const geo = new THREE.CylinderGeometry(top, radius, halfH * 2, 16);
  return new THREE.LineSegments(new THREE.EdgesGeometry(geo, 30), new THREE.LineBasicMaterial({ color: colour }));
}
// a hexagonal prism hit volume (a box with hex_top): section in local XY with vertices (+-hx, 0), (+-top, +-hy),
// along local Z - drawn as its 18 edges
function hexMesh(half, top, colour) {
  const [hx, hy, hz] = half;
  const ring = [[hx, 0], [top, hy], [-top, hy], [-hx, 0], [-top, -hy], [top, -hy]];
  const pts = [];
  for (let i = 0; i < 6; i++) {
    const [x0, y0] = ring[i], [x1, y1] = ring[(i + 1) % 6];
    for (const z of [-hz, hz]) pts.push(x0, y0, z, x1, y1, z);
    pts.push(x0, y0, -hz, x0, y0, hz);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(pts, 3));
  return new THREE.LineSegments(geo, new THREE.LineBasicMaterial({ color: colour }));
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
    // the model's own meshes are in the model frame, like its hit volumes: a node's position / rotation says where the
    // referenced module goes in the package (a set "unit + pad" carries placed copies) - not applied to them again
    const holder = new THREE.Group();
    offset.add(holder);
    const part = g.meshes.get(n.part);
    let centre = new THREE.Vector3();
    let nbox = null;
    if (part && part.front) {
      const m = new THREE.Mesh(part.front.geometry, partMaterial());
      m.geometry.userData = m.geometry.userData || {};
      m.userData = { node: n.id, shared: true };
      if (part.sides === 2) m.material.side = THREE.DoubleSide;
      holder.add(m);
      holder.updateMatrixWorld(true);
      centre = part.box.getCenter(new THREE.Vector3()).applyMatrix4(holder.matrix);
      nbox = part.box.clone().applyMatrix4(holder.matrix);
    } else {
      // a link without a mesh in this model (a Blender module): a violet marker the size of a metre
      const mk = boxMesh([0.5, 0.5, 0.5], COLORS.link, { solid: true, opacity: 0.35 });
      mk.userData = { node: n.id };
      holder.add(mk);
      centre = new THREE.Vector3(...(n.position || [0, 0, 0]));
    }
    asm.nodes.set(n.id, { node: n, offset, holder, centre, box: nbox, depth: depth(n), overlays: new THREE.Group() });
    offset.add(asm.nodes.get(n.id).overlays);
  }
  const free = new THREE.Group();   // overlays not tied to a node (zones)
  asm.root.add(free);
  asm.free = free;
  const into = (nodeId) => (asm.nodes.get(nodeId) || { overlays: free }).overlays;
  for (const h of doc.hit_volumes || []) {
    const o = h.shape === 'cylinder' ? cylMesh(h.radius, h.half_height, 0x3fd0d8, h.radius_top)
      : h.hex_top >= 0 ? hexMesh(h.half_size, h.hex_top, 0x3fd0d8) : boxMesh(h.half_size, 0x3fd0d8);
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

// the explode (author 2026-11-16: «все объекты на главной оси разъезжаются по этой оси, то, что сбоку - отлетает в сторону
// по прямой, по которой оно соединяется с центральной осью. все запчасти как бы разлетаются от 0 в разные стороны»):
// the main axis = the longest extent of the model, through the centre of its box; a part the axis passes through moves
// along the axis (away from the centre; the one at the centre stays); any other part moves straight away from the axis,
// perpendicular to it. The offset grows with the distance from the centre (+ a little for everyone), so the parts keep
// their order and fly apart from one point.
function explodeFrame() {
  const box = new THREE.Box3();
  for (const v of asm.nodes.values()) if (v.box) box.union(v.box); else box.expandByPoint(v.centre);
  if (box.isEmpty()) return { c: new THREE.Vector3(), axis: 2, size: 1 };
  const s = box.getSize(new THREE.Vector3());
  const axis = s.x >= s.y && s.x >= s.z ? 0 : s.y >= s.z ? 1 : 2;
  return { c: box.getCenter(new THREE.Vector3()), axis, size: Math.max(s.x, s.y, s.z) };
}
function applyExplode() {
  if (!asm.root) return;
  const { c, axis, size } = explodeFrame();
  const k = asm.explode;
  const others = [0, 1, 2].filter((i) => i !== axis);
  for (const v of asm.nodes.values()) {
    const d = v.centre.clone().sub(c).toArray();
    // on the axis: the axis line (through c) crosses the part's box in both cross coordinates
    const onAxis = v.box ? others.every((i) => v.box.min.getComponent(i) <= c.getComponent(i) + 1e-6 &&
                                               v.box.max.getComponent(i) >= c.getComponent(i) - 1e-6)
                         : others.every((i) => Math.abs(d[i]) < size * 0.02);
    const off = [0, 0, 0];
    if (onAxis) {
      const a = d[axis];
      if (Math.abs(a) > size * 0.01) off[axis] = k * (a + Math.sign(a) * size * 0.15);
    } else {
      const r = Math.hypot(d[others[0]], d[others[1]]) || 1;
      for (const i of others) off[i] = k * (d[i] + (d[i] / r) * size * 0.15);
    }
    v.offset.position.set(...off);
  }
  drawLines();
}

function nodeAt(id) {
  const v = asm.nodes.get(id);
  return v ? v.centre.clone().add(v.offset.position) : null;
}

function drawLines() {
  if (!asm.root) return;   // a layer switched before the assembly is built: drawn with it later
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
  // camera from direction (x, y, z) onto the current model, re-fitted (scripted shots: top / side / front views)
  viewFrom(x, y, z) {
    camera.position.copy(controls.target.clone().add(new THREE.Vector3(x, y, z)));
    frame(groupBox(state.current));
    controls.update();
  },
  onPick(cb) { state.onPick = cb; },
  // the assembly view
  async showAssembly(doc) {
    const g = state.groups[0];
    if (g && !g.meshes) {
      const built = await buildGroup(g);
      if (!state.groups.includes(g)) { disposeRoot(built.root); return; }   // a newer load meanwhile
      Object.assign(g, built);
    }
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
  // the player (app.js Anim): angles in degrees per part; null = the modelled pose
  setPose(rig, angles, extra) { state.pose = rig && angles ? { rig, angles, extra } : null; if (!state.pose) applyProps(() => new THREE.Matrix4()); applyPose(); },
  // check: the model roots in the scene (a stale load used to leave the previous model there)
  stats() { return { inScene: scene.children.filter((o) => o.isGroup).length, groups: state.groups.length }; },
};
window.dispatchEvent(new Event('viewer-ready'));
