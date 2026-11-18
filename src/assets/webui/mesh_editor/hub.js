// The hub mode of the mesh editor (docs/MESH_EDITOR_SPEC.md 19; author 2026-11-17: «редактор хабов как режим
// редактора моделей - слои / этажи, карточки модулей на клетках как в SimCity, любой 2D контур, улицы / зелёные коридоры
// со стрелками / красные зоны на той же карте; генератор строит хаб по карте; стартовую раскладку можно править»).
// The page edits a hub MAP (elite.hub_map.v1, tools/models/units/hub_plane.py); what the map builds into (streets,
// lanes, connectors, red zones, docks, piers) comes back from tools/models/hub_map_tool.py (local HTTP, started by the
// backend: {cmd: hubTool}). Top-down view: +X to the right, +Z down the screen.
// URL parameters for headless checks: ?hub=<id> opens that hub, &hubLayer=under, &hubTool=lanes, &hubSel=i,j[,k].
'use strict';

const Hub = {
  port: 0, catalog: null, mods: {}, zones: {}, list: null,
  map: null, derived: null, dirty: false, history: [], future: [],
  layer: 'top', tool: 'select', zoneBrush: 'residential', pick: null, sel: null, hover: null,
  view: { cx: 0, cz: 0, s: 0.08 }, drag: null, deriveTimer: null, deriveSeq: 0, pendingBuild: null,
  ov: { lanes: true, conns: false, red: true, other: true },
  ZONE_COLOR: { downtown: '#5b6fd6', residential: '#4f9e6a', hotel: '#9a6fd0', hab_rot: '#40a0b0', storage: '#a08350',
    industry: '#b06048', port: '#3f86c0', logistics: '#7c8a3c', refuel: '#c09a30', repair: '#8a6a8a', rig: '#607080',
    canopy: '#d0d0a0', dock: '#e8a33d', power: '#d0d040', services: '#4070e0' },
  TOOLS: [['select', 'Выбор', 'V', 'выбрать / двигать карточку (R — повернуть, Del — убрать)'],
    ['blocks', 'Кварталы', 'B', 'клик по пустому месту — новый квартал (зона — из списка), по кварталу — убрать'],
    ['zones', 'Зоны', 'Z', 'клик по кварталу — назначить зону из списка'],
    ['avenues', 'Проспекты', 'A', 'клик по улице — проспект / местная улица'],
    ['lanes', 'Коридоры', 'L', 'клик по коридору — выключить / включить'],
    ['red', 'Красная зона', 'X', 'протянуть прямоугольник — красная зона; клик по ней — убрать']],

  // ---------------------------------------------------------------- the tool process
  enter() {
    $('empty').hidden = true;
    $('model').hidden = true;
    $('hub').hidden = false;
    $('current').textContent = this.map ? 'Хаб: ' + this.map.title : 'Хаб';
    if (!this.port) {
      this.status('Запускаю утилиту карты хаба (генератор импортируется ~6 с)…');
      send({ cmd: 'hubTool' });
    } else if (!this.map) this.showPicker();
    this.resize();
  },
  async onTool(m) {
    if (!m.port) { this.status('Утилита карты хаба не запустилась: ' + (m.error || '?'), true); return; }
    this.port = m.port;
    this.catalog = await this.call('catalog');
    this.mods = Object.fromEntries(this.catalog.modules.map((x) => [x.id, x]));
    this.zones = Object.fromEntries(this.catalog.zones.map((x) => [x.id, x]));
    this.renderZoneSelect();
    this.renderPalette();
    this.status('');
    const want = params.get('hub');
    if (want) await this.open(want);
    else this.showPicker();
  },
  async call(cmd, body = {}) {
    const r = await fetch(`http://127.0.0.1:${this.port}/${cmd}`, { method: 'POST', body: JSON.stringify(body),
      headers: { 'Content-Type': 'application/json' } });
    const j = await r.json();
    if (j.type === 'hubError') { this.status(`Ошибка (${cmd}): ${j.error}`, true); console.error(j.trace); throw new Error(j.error); }
    return j;
  },
  status(text, bad) {
    const el = $('hubStatus');
    el.textContent = text;
    el.classList.toggle('bad', !!bad);
  },

  // ---------------------------------------------------------------- picking a hub
  async showPicker() {
    this.list = await this.call('list');
    const box = $('hubList');
    box.innerHTML = '';
    for (const it of this.list.items) {
      const b = document.createElement('button');
      b.className = 'hub-item';
      b.innerHTML = `<b>${esc(it.title)}</b> <span class="muted">${esc(it.id)}</span><br><span class="muted small">${
        it.saved ? 'карта сохранена: ' + it.cells + ' кварталов' : 'без карты: откроется стартовая раскладка генератора'}</span>`;
      b.onclick = () => { $('hubPicker').hidden = true; this.open(it.id); };
      box.appendChild(b);
    }
    const ruins = Object.entries(this.list.ruins || {}).map(([r, s]) => `${r} (разрушенный ${s})`).join(', ');
    $('hubRuins').textContent = ruins ? 'Разрушенные города строятся из карты основы: ' + ruins + '.' : '';
    $('hubPicker').hidden = false;
  },
  async createNew() {
    const id = $('hubNewId').value.trim();
    const from = $('hubNewFrom').value.trim() || id;
    const title = $('hubNewTitle').value.trim() || id;
    if (!/^[A-Za-z0-9_]+$/.test(id)) { notice('id хаба: латиница, цифры, _'); return; }
    if (this.list && this.list.items.some((x) => x.id === id && x.saved)) { notice('Хаб ' + id + ' уже есть.'); return; }
    const g = await this.call('generate', { hub: id, from, title });
    $('hubPicker').hidden = true;
    this.setMap(g.map, true);
    this.status(`Новый хаб ${id}: стартовая раскладка генератора по «${from}». Не сохранён.`);
  },
  async open(id) {
    const it = (this.list && this.list.items.find((x) => x.id === id)) || null;
    void it;
    const saved = (await this.call('load', { hub: id })).map;
    const m = saved || (await this.call('generate', { hub: id, from: id, title: id })).map;
    this.setMap(m, !saved);
    if (!saved) this.status(`Карты ${id} ещё нет: открыта стартовая раскладка генератора. «Сохранить» запишет карту.`);
  },
  setMap(m, unsaved) {
    this.map = m;
    this.history = [];
    this.future = [];
    this.dirty = !!unsaved;
    this.sel = null;
    this.derived = null;
    $('hubTitle').value = m.title || m.hub_id;
    $('hubId').textContent = m.hub_id;
    $('current').textContent = 'Хаб: ' + (m.title || m.hub_id);
    this.fit();
    this.applyUrl();
    this.rederive(0);
    this.renderSide();
    this.draw();
  },
  applyUrl() {
    if (params.get('hubLayer')) this.layer = params.get('hubLayer');
    if (params.get('hubTool')) this.setTool(params.get('hubTool'));
    const s = params.get('hubSel');
    if (s) {
      const v = s.split(',').map(Number);
      const c = this.cell(v[0], v[1]);
      if (c) this.sel = v.length > 2 ? { kind: 'card', i: v[0], j: v[1], layer: this.layer, k: v[2] } : { kind: 'cell', i: v[0], j: v[1] };
    }
    this.renderLayer();
  },

  // ---------------------------------------------------------------- editing (every change through edit(): undo)
  edit(fn, label) {
    this.history.push(JSON.stringify(this.map));
    if (this.history.length > 200) this.history.shift();
    this.future = [];
    fn(this.map);
    this.dirty = true;
    this.lastEdit = label || '';
    this.rederive();
    this.renderSide();
    this.draw();
  },
  undo() {
    if (!this.history.length) return;
    this.future.push(JSON.stringify(this.map));
    this.map = JSON.parse(this.history.pop());
    this.afterJump();
  },
  redo() {
    if (!this.future.length) return;
    this.history.push(JSON.stringify(this.map));
    this.map = JSON.parse(this.future.pop());
    this.afterJump();
  },
  afterJump() {
    this.dirty = true;
    if (this.sel && (this.sel.kind === 'cell' || this.sel.kind === 'card') && !this.cell(this.sel.i, this.sel.j)) this.sel = null;
    if (this.sel && this.sel.kind === 'card' && !this.cardOf(this.sel)) this.sel = null;
    this.rederive();
    this.renderSide();
    this.draw();
  },
  rederive(delay = 350) {
    clearTimeout(this.deriveTimer);
    this.deriveTimer = setTimeout(async () => {
      const seq = ++this.deriveSeq;
      $('hubDeriving').hidden = false;
      try {
        const d = await this.call('derive', { map: this.map });
        if (seq !== this.deriveSeq) return;
        this.derived = d;
        this.renderSide();
        this.draw();
      } catch (e) { /* shown by call() */ } finally { if (seq === this.deriveSeq) $('hubDeriving').hidden = true; }
    }, delay);
  },
  async save() {
    this.map.title = $('hubTitle').value.trim() || this.map.hub_id;
    const r = await this.call('save', { map: this.map });
    if (!r.ok) { this.status('Не сохранено: ' + r.errors.join('; '), true); return false; }
    this.dirty = false;
    this.status('Сохранено: ' + r.file);
    this.renderSide();
    return true;
  },
  async build() {
    if (!(await this.save())) return;
    const id = this.map.hub_id;
    this.status('Собираю хаб: модели источника и мир (build_equipment_models, hub_belt_world)… это минута-другая');
    $('hubBuild').disabled = true;
    try {
      const r = await this.call('build', { hub: id });
      if (!r.ok) { this.status('Сборка источника не удалась (журнал — в консоли страницы)', true); console.log(r.log); return; }
      this.status('Источник готов; упаковываю hub_' + id + ' для игры (мешэдитор)…');
      this.pendingBuild = 'hub_' + id;
      send({ cmd: 'rescan' });
      send({ cmd: 'build', category: 'hub_modules', id: 'hub_' + id });
    } finally { $('hubBuild').disabled = false; }
  },
  onBuilt(m) {
    if (!this.pendingBuild || m.id !== this.pendingBuild) return false;
    this.pendingBuild = null;
    this.status(m.ok ? `Хаб собран для игры: ${m.output}. Мир и коридоры обновлены — перезапустите игру.`
      : 'Упаковка не удалась: ' + (m.report || ''), !m.ok);
    return true;
  },

  // ---------------------------------------------------------------- the grid (the same formulas as hub_plane._axis)
  cell(i, j) { return this.map.cells.find((c) => c.i === i && c.j === j) || null; },
  gw(ax, i) { return this.map.avenues[ax].includes(i) ? this.map.grid.street_avenue_m : this.map.grid.street_local_m; },
  centre(ax, i) {
    const a = this.map.grid.anchor;
    const ai = ax === 'x' ? a.i : a.j;
    let c = ax === 'x' ? a.x : a.z;
    const C = this.map.grid.cell_m;
    for (let k = ai; k < i; k++) c += C + this.gw(ax, k);
    for (let k = ai; k > i; k--) c -= C + this.gw(ax, k - 1);
    return c;
  },
  line(ax, i) { return this.centre(ax, i) + this.map.grid.cell_m / 2 + this.gw(ax, i) / 2; },
  index(ax, v) {
    // -> {i, inBlock, gap: line index when v is in the street between i and i+1}
    const a = this.map.grid.anchor;
    const C = this.map.grid.cell_m;
    let i = (ax === 'x' ? a.i : a.j) + Math.round((v - (ax === 'x' ? a.x : a.z)) / (C + this.map.grid.street_local_m));
    for (let n = 0; n < 8; n++) {
      const c = this.centre(ax, i);
      if (Math.abs(v - c) <= C / 2) return { i, inBlock: true };
      if (v > c && v < this.centre(ax, i + 1) - C / 2) return { i, inBlock: false, gap: i };
      i += v > c ? 1 : -1;
    }
    return { i, inBlock: false };
  },
  cellCentre(c) { return [this.centre('x', c.i), this.centre('z', c.j)]; },
  cards(c, layer) { return c[layer || this.layer] || (c[layer || this.layer] = []); },
  cardOf(s) { const c = this.cell(s.i, s.j); return c && this.cards(c, s.layer)[s.k]; },
  axes(yaw, down) {
    const r = yaw * Math.PI / 180;
    const Z = [Math.sin(r), Math.cos(r)];
    const X = down ? [-Math.cos(r), Math.sin(r)] : [Math.cos(r), -Math.sin(r)];
    return { X, Z };
  },
  footprint(c, card, layer) {
    // the card's world rectangle (yaw is a multiple of 90: axis-aligned) -> {x0, z0, x1, z1}
    const m = this.mods[card.module];
    const [cx, cz] = this.cellCentre(c);
    const { X, Z } = this.axes(card.yaw || 0, (layer || this.layer) === 'under');
    const lo = m ? m.lo : [-20, 0, -20];
    const hi = m ? m.hi : [20, 0, 20];
    const xs = [], zs = [];
    for (const lx of [lo[0], hi[0]]) for (const lz of [lo[2], hi[2]]) {
      xs.push(cx + card.x + lx * X[0] + lz * Z[0]);
      zs.push(cz + card.z + lx * X[1] + lz * Z[1]);
    }
    return { x0: Math.min(...xs), x1: Math.max(...xs), z0: Math.min(...zs), z1: Math.max(...zs) };
  },
  localCentre(card, layer) {
    const m = this.mods[card.module];
    if (!m) return [0, 0];
    const { X, Z } = this.axes(card.yaw || 0, (layer || this.layer) === 'under');
    const lx = (m.lo[0] + m.hi[0]) / 2, lz = (m.lo[2] + m.hi[2]) / 2;
    return [lx * X[0] + lz * Z[0], lx * X[1] + lz * Z[1]];
  },
  problems(c, layer) {
    // per card: 'overlap' / 'outside' (beyond the block's edge)
    const out = {};
    const L = this.cards(c, layer);
    const fp = L.map((k) => this.footprint(c, k, layer));
    const [cx, cz] = this.cellCentre(c);
    const h = this.map.grid.cell_m / 2 + 1;
    fp.forEach((a, n) => {
      if (a.x0 < cx - h || a.x1 > cx + h || a.z0 < cz - h || a.z1 > cz + h) out[n] = 'outside';
      fp.forEach((b, k) => {
        if (k !== n && a.x0 < b.x1 - 1 && b.x0 < a.x1 - 1 && a.z0 < b.z1 - 1 && b.z0 < a.z1 - 1) out[n] = 'overlap';
      });
    });
    return out;
  },
  openSides(i, j) {
    const has = (a, b) => !!this.cell(a, b);
    return [[1, 0], [-1, 0], [0, 1], [0, -1]].filter(([oi, oj]) => {
      const o = [i + oi, j + oj];
      if (has(...o)) return false;
      return ![[o[0] + 1, o[1]], [o[0] - 1, o[1]], [o[0], o[1] + 1], [o[0], o[1] - 1]].some(([a, b]) => (a !== i || b !== j) && has(a, b));
    });
  },

  // ---------------------------------------------------------------- view
  canvas() { return $('hubCanvas'); },
  resize() {
    const cv = this.canvas();
    const r = cv.parentElement.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    cv.width = Math.max(10, r.width * dpr);
    cv.height = Math.max(10, r.height * dpr);
    cv.style.width = r.width + 'px';
    cv.style.height = r.height + 'px';
    this.dpr = dpr;
    this.draw();
  },
  fit() {
    if (!this.map || !this.map.cells.length) return;
    const xs = [], zs = [];
    for (const c of this.map.cells) { const [x, z] = this.cellCentre(c); xs.push(x); zs.push(z); }
    const pad = 1600;
    const w = Math.max(...xs) - Math.min(...xs) + pad * 2, d = Math.max(...zs) - Math.min(...zs) + pad * 2;
    this.view.cx = (Math.max(...xs) + Math.min(...xs)) / 2;
    this.view.cz = (Math.max(...zs) + Math.min(...zs)) / 2;
    const cv = this.canvas();
    this.view.s = Math.min(cv.width / w, cv.height / d);
    this.draw();
  },
  toScreen(x, z) { const cv = this.canvas(); return [cv.width / 2 + (x - this.view.cx) * this.view.s, cv.height / 2 + (z - this.view.cz) * this.view.s]; },
  toWorld(px, pz) { const cv = this.canvas(); return [(px - cv.width / 2) / this.view.s + this.view.cx, (pz - cv.height / 2) / this.view.s + this.view.cz]; },
  eventWorld(e) { const r = this.canvas().getBoundingClientRect(); return this.toWorld((e.clientX - r.left) * this.dpr, (e.clientY - r.top) * this.dpr); },

  // ---------------------------------------------------------------- drawing
  draw() {
    const cv = this.canvas();
    if (!cv || $('hub').hidden) return;
    const g = cv.getContext('2d');
    g.setTransform(1, 0, 0, 1, 0, 0);
    g.fillStyle = '#090b0e';
    g.fillRect(0, 0, cv.width, cv.height);
    if (!this.map) return;
    const S = (x, z) => this.toScreen(x, z);
    const s = this.view.s;
    const C = this.map.grid.cell_m;
    const lw = this.dpr;
    // the extent (dashed)
    const [i0, j0, i1, j1] = this.map.grid.extent;
    const [ex0, ez0] = S(this.centre('x', i0) - C / 2, this.centre('z', j0) - C / 2);
    const [ex1, ez1] = S(this.centre('x', i1) + C / 2, this.centre('z', j1) + C / 2);
    g.setLineDash([6 * lw, 6 * lw]);
    g.strokeStyle = '#26303a';
    g.lineWidth = lw;
    g.strokeRect(ex0, ez0, ex1 - ex0, ez1 - ez0);
    g.setLineDash([]);
    // the block tool: the free spots of the grid around the city
    if (this.tool === 'blocks') {
      g.strokeStyle = '#2a3542';
      for (let i = i0 - 1; i <= i1 + 1; i++) for (let j = j0 - 1; j <= j1 + 1; j++) {
        if (this.cell(i, j)) continue;
        const [a, b] = S(this.centre('x', i) - C / 2, this.centre('z', j) - C / 2);
        g.strokeRect(a, b, C * s, C * s);
      }
    }
    // streets: the road down every gap between two blocks (avenues wider)
    for (const c of this.map.cells) {
      for (const [di, dj, ax] of [[1, 0, 'x'], [0, 1, 'z']]) {
        if (!this.cell(c.i + di, c.j + dj)) continue;
        const av = this.map.avenues[ax].includes(ax === 'x' ? c.i : c.j);
        g.strokeStyle = av ? '#5a6573' : '#3a434e';
        g.lineWidth = (av ? 26 : 14) * s + lw;
        g.beginPath();
        if (ax === 'x') {
          const x = this.line('x', c.i), z = this.centre('z', c.j);
          g.moveTo(...S(x, z - C / 2 - 50)); g.lineTo(...S(x, z + C / 2 + 50));
        } else {
          const z = this.line('z', c.j), x = this.centre('x', c.i);
          g.moveTo(...S(x - C / 2 - 50, z)); g.lineTo(...S(x + C / 2 + 50, z));
        }
        g.stroke();
      }
    }
    // blocks
    for (const c of this.map.cells) {
      const [cx, cz] = this.cellCentre(c);
      const [a, b] = S(cx - C / 2, cz - C / 2);
      g.fillStyle = (this.ZONE_COLOR[c.zone] || '#666') + '33';
      g.fillRect(a, b, C * s, C * s);
      g.strokeStyle = (this.ZONE_COLOR[c.zone] || '#666') + 'aa';
      g.lineWidth = lw;
      g.strokeRect(a, b, C * s, C * s);
      if (C * s > 60) {
        g.fillStyle = '#c8d2dc99';
        g.font = `${Math.round(11 * lw)}px Segoe UI`;
        g.fillText(`${(this.zones[c.zone] || {}).title || c.zone}  ${c.i},${c.j}`, a + 4 * lw, b + 13 * lw);
      }
    }
    // the other layer's cards, faint
    const other = this.layer === 'top' ? 'under' : 'top';
    if (this.ov.other) {
      g.strokeStyle = '#ffffff22';
      g.lineWidth = lw;
      for (const c of this.map.cells) for (const k of this.cards(c, other)) {
        const f = this.footprint(c, k, other);
        const [a, b] = S(f.x0, f.z0);
        g.strokeRect(a, b, (f.x1 - f.x0) * s, (f.z1 - f.z0) * s);
      }
    }
    // this layer's cards
    for (const c of this.map.cells) {
      const pr = this.problems(c, this.layer);
      this.cards(c, this.layer).forEach((k, n) => {
        const f = this.footprint(c, k, this.layer);
        const [a, b] = S(f.x0, f.z0);
        const w = (f.x1 - f.x0) * s, h = (f.z1 - f.z0) * s;
        const m = this.mods[k.module];
        g.fillStyle = m && m.damaged ? '#6a4a3acc' : '#3a4656cc';
        g.fillRect(a, b, w, h);
        const sel = this.sel && this.sel.kind === 'card' && this.sel.i === c.i && this.sel.j === c.j && this.sel.layer === this.layer && this.sel.k === n;
        g.strokeStyle = sel ? '#e8a33d' : pr[n] === 'overlap' ? '#e06a5a' : pr[n] === 'outside' ? '#e0b04a' : '#8fa0b4';
        g.lineWidth = (sel ? 2.5 : 1) * lw;
        g.strokeRect(a, b, w, h);
        // the module's facing (its +Z): a tick from the centre
        const [lc0, lc1] = this.localCentre(k, this.layer);
        const [cx, cz] = this.cellCentre(c);
        const { Z } = this.axes(k.yaw || 0, this.layer === 'under');
        const o = [cx + k.x + lc0, cz + k.z + lc1];
        const L = Math.min(f.x1 - f.x0, f.z1 - f.z0) * 0.35;
        g.beginPath();
        g.moveTo(...S(o[0], o[1]));
        g.lineTo(...S(o[0] + Z[0] * L, o[1] + Z[1] * L));
        g.stroke();
        if (w > 70 * lw && h > 18 * lw) {
          g.fillStyle = '#dfe6ee';
          g.font = `${Math.round(10 * lw)}px Segoe UI`;
          g.save();
          g.beginPath(); g.rect(a, b, w, h); g.clip();
          g.fillText(k.module.replace(/^hub[a-z]_/, ''), a + 3 * lw, b + 12 * lw);
          g.restore();
        }
      });
    }
    // the cell selection
    if (this.sel && this.sel.kind === 'cell') {
      const c = this.cell(this.sel.i, this.sel.j);
      if (c) {
        const [cx, cz] = this.cellCentre(c);
        const [a, b] = S(cx - C / 2, cz - C / 2);
        g.strokeStyle = '#e8a33d';
        g.lineWidth = 2.5 * lw;
        g.strokeRect(a, b, C * s, C * s);
      }
    }
    const d = this.derived;
    if (d) {
      // piers, docks, spinning bodies (top layer)
      if (d && this.layer === 'top') {
        g.fillStyle = '#4a5a6a88';
        for (const p of d.piers || []) this.poly(g, p.corners, true);
        for (const sp of d.spinning || []) {
          const [a, b] = S(sp.x, sp.z);
          g.strokeStyle = '#40a0b0';
          g.lineWidth = 1.5 * lw;
          g.beginPath(); g.arc(a, b, Math.max(4 * lw, (sp.module === 'hubb_hab_drum' ? 160 : 90) * s), 0, Math.PI * 2); g.stroke();
        }
      }
      if (d) for (const dk of d.docks || []) {
        const [a, b] = S(dk.x, dk.z);
        g.fillStyle = dk.type === 'hub_hex_dock' ? '#e8a33d' : '#a07a40';
        g.beginPath();
        const r = Math.max(5 * lw, (dk.type === 'hub_hex_dock' ? 150 : 90) * s);
        for (let n = 0; n < 6; n++) { const t = n * Math.PI / 3; g.lineTo(a + r * Math.cos(t), b + r * Math.sin(t)); }
        g.closePath(); g.fill();
        g.fillStyle = '#1a1206';
        g.font = `${Math.round(10 * lw)}px Segoe UI`;
        if (dk.type === 'hub_hex_dock') g.fillText('ДОК', a - 10 * lw, b + 4 * lw);
      }
      // red zones
      if (d && this.ov.red) for (const rz of d.red_zones || []) {
        if ((rz.y < 0) !== (this.layer === 'under') && Math.abs(rz.y) > 1) continue;
        g.fillStyle = rz.map ? '#e06a5a44' : '#e06a5a22';
        g.strokeStyle = '#e06a5a';
        g.lineWidth = (this.sel && this.sel.kind === 'red' && this.sel.id === rz.id ? 2.5 : 1) * lw;
        this.poly(g, rz.corners, true, true);
      }
      // connectors
      if (d && this.ov.conns) {
        g.strokeStyle = '#4fbf6f55';
        g.lineWidth = lw;
        for (const cn of d.connectors || []) {
          if ((cn.points[0][1] < 0) !== (this.layer === 'under')) continue;
          g.beginPath();
          cn.points.forEach((q, n) => (n ? g.lineTo : g.moveTo).call(g, ...S(q[0], q[2])));
          g.stroke();
        }
      }
      // lanes: green / blue with arrows; off lanes dashed grey
      if (d && this.ov.lanes) for (const ln of d.lanes_geo || []) {
        if (ln.level !== this.layer) continue;
        const off = this.map.lanes_off.includes(ln.id);
        const sel = this.sel && this.sel.kind === 'lane' && this.sel.id === ln.id;
        g.strokeStyle = sel ? '#e8a33d' : off ? '#6a7480' : ln.kind === 'blue' ? '#5aa0ff' : ln.plaza ? '#9ad070' : '#4fd07a';
        g.lineWidth = (sel ? 3 : ln.avenue ? 2 : 1.4) * lw;
        g.setLineDash(off ? [5 * lw, 5 * lw] : []);
        g.beginPath();
        this.drawn(ln).forEach((q, n) => (n ? g.lineTo : g.moveTo).call(g, ...S(q[0], q[2])));
        g.stroke();
        g.setLineDash([]);
        if (!off) this.arrows(g, this.drawn(ln), S);
      }
    }
    // the red-zone being drawn
    if (this.drag && this.drag.kind === 'red') {
      const [a, b] = S(this.drag.x0, this.drag.z0), [c2, d2] = S(this.drag.x1, this.drag.z1);
      g.strokeStyle = '#e06a5a';
      g.lineWidth = 1.5 * lw;
      g.strokeRect(Math.min(a, c2), Math.min(b, d2), Math.abs(c2 - a), Math.abs(d2 - b));
    }
    // the card about to be placed
    if (this.pick && this.hover) {
      const m = this.mods[this.pick];
      const c = this.hoverCell();
      if (m && c) {
        const card = this.newCard(c, this.pick, this.hover);
        const f = this.footprint(c, card, this.layer);
        const [a, b] = S(f.x0, f.z0);
        g.strokeStyle = '#e8a33d';
        g.setLineDash([4 * lw, 3 * lw]);
        g.strokeRect(a, b, (f.x1 - f.x0) * s, (f.z1 - f.z0) * s);
        g.setLineDash([]);
      }
    }
    // the scale bar
    g.fillStyle = '#8796a8';
    g.font = `${Math.round(11 * lw)}px Segoe UI`;
    const bar = 1000 * s;
    g.fillRect(14 * lw, cv.height - 18 * lw, bar, 2 * lw);
    g.fillText('1 км   +X →  +Z ↓   слой: ' + (this.layer === 'top' ? 'верх' : 'низ'), 14 * lw, cv.height - 24 * lw);
  },
  drawn(ln) {
    // the lane as drawn: the two directions of a street are stacked in height (the same x, z) - on the map each one is
    // shifted to its right (right-hand traffic) so both stay visible and clickable; ~4 px whatever the zoom
    const off = Math.max(25, 4 * this.dpr / this.view.s);
    if (ln._drawnFor === off) return ln._drawn;
    const st = ln.stations;
    const out = st.map((q, n) => {
      const a = st[Math.max(0, n - 1)], b = st[Math.min(st.length - 1, n + 1)];
      const dx = b[0] - a[0], dz = b[2] - a[2];
      const L = Math.hypot(dx, dz) || 1;
      return [q[0] - dz / L * off, q[1], q[2] + dx / L * off];
    });
    ln._drawnFor = off;
    ln._drawn = out;
    return out;
  },
  poly(g, corners, fill, stroke) {
    g.beginPath();
    corners.forEach((q, n) => (n ? g.lineTo : g.moveTo).call(g, ...this.toScreen(q[0], q[1])));
    g.closePath();
    if (fill) g.fill();
    if (stroke) g.stroke();
  },
  arrows(g, st, S) {
    // an arrow head every ~600 m of the lane, pointing along it
    let acc = 300;
    for (let n = 1; n < st.length; n++) {
      const a = st[n - 1], b = st[n];
      const L = Math.hypot(b[0] - a[0], b[2] - a[2]);
      let t = acc;
      while (t < L) {
        const x = a[0] + (b[0] - a[0]) * t / L, z = a[2] + (b[2] - a[2]) * t / L;
        const [px, pz] = S(x, z);
        const ang = Math.atan2(b[2] - a[2], b[0] - a[0]);
        const r = 6 * this.dpr;
        g.beginPath();
        g.moveTo(px + r * Math.cos(ang), pz + r * Math.sin(ang));
        g.lineTo(px + r * Math.cos(ang + 2.5), pz + r * Math.sin(ang + 2.5));
        g.lineTo(px + r * Math.cos(ang - 2.5), pz + r * Math.sin(ang - 2.5));
        g.closePath();
        g.fillStyle = g.strokeStyle;
        g.fill();
        t += 600;
      }
      acc = t - L;
    }
  },

  // ---------------------------------------------------------------- hit tests
  hoverCell() {
    if (!this.hover) return null;
    const ix = this.index('x', this.hover[0]), iz = this.index('z', this.hover[1]);
    return ix.inBlock && iz.inBlock ? this.cell(ix.i, iz.i) : null;
  },
  cardAt(x, z) {
    for (const c of this.map.cells) {
      const L = this.cards(c, this.layer);
      for (let n = L.length - 1; n >= 0; n--) {
        const f = this.footprint(c, L[n], this.layer);
        if (x >= f.x0 && x <= f.x1 && z >= f.z0 && z <= f.z1) return { kind: 'card', i: c.i, j: c.j, layer: this.layer, k: n };
      }
    }
    return null;
  },
  laneAt(x, z) {
    if (!this.derived) return null;
    let best = null;
    const tol = 10 / this.view.s * this.dpr;
    for (const ln of this.derived.lanes_geo || []) {
      if (ln.level !== this.layer) continue;
      const st = this.drawn(ln);
      for (let n = 1; n < st.length; n++) {
        const a = st[n - 1], b = st[n];
        const dx = b[0] - a[0], dz = b[2] - a[2];
        const t = Math.max(0, Math.min(1, ((x - a[0]) * dx + (z - a[2]) * dz) / (dx * dx + dz * dz || 1)));
        const dd = Math.hypot(a[0] + dx * t - x, a[2] + dz * t - z);
        if (dd < tol && (!best || dd < best.d)) best = { d: dd, id: ln.id };
      }
    }
    return best && { kind: 'lane', id: best.id };
  },
  redAt(x, z) {
    for (const rz of (this.derived && this.derived.red_zones) || []) {
      if (!rz.map) continue;
      const xs = rz.corners.map((q) => q[0]), zs = rz.corners.map((q) => q[1]);
      if (x >= Math.min(...xs) && x <= Math.max(...xs) && z >= Math.min(...zs) && z <= Math.max(...zs)) return { kind: 'red', id: rz.id };
    }
    return null;
  },
  newCard(c, module, at) {
    const [cx, cz] = this.cellCentre(c);
    const card = { module, x: 0, z: 0, yaw: this.pickYaw || 0 };
    const [lc0, lc1] = this.localCentre(card, this.layer);
    card.x = this.snap(at[0] - cx - lc0);
    card.z = this.snap(at[1] - cz - lc1);
    return card;
  },
  snap(v) { return Math.round(v / 5) * 5; },

  // ---------------------------------------------------------------- input
  bind() {
    const cv = this.canvas();
    cv.addEventListener('wheel', (e) => {
      e.preventDefault();
      const [wx, wz] = this.eventWorld(e);
      const k = Math.exp(-e.deltaY * 0.0015);
      this.view.s = Math.min(3, Math.max(0.005, this.view.s * k));
      const [nx, nz] = this.eventWorld(e);
      this.view.cx += wx - nx;
      this.view.cz += wz - nz;
      this.draw();
    }, { passive: false });
    cv.addEventListener('contextmenu', (e) => e.preventDefault());
    cv.addEventListener('mousedown', (e) => this.down(e));
    window.addEventListener('mousemove', (e) => this.move(e));
    window.addEventListener('mouseup', (e) => this.up(e));
    window.addEventListener('resize', () => this.resize());
    window.addEventListener('keydown', (e) => this.key(e));
    $('hubBtn').onclick = () => this.enter();
    $('hubOpen').onclick = () => this.showPicker();
    $('hubPickerClose').onclick = () => { $('hubPicker').hidden = true; };
    $('hubNewBtn').onclick = () => this.createNew();
    $('hubSave').onclick = () => this.save();
    $('hubBuild').onclick = () => this.build();
    $('hubFit').onclick = () => this.fit();
    $('hubUndo').onclick = () => this.undo();
    $('hubRedo').onclick = () => this.redo();
    $('hubTitle').onchange = () => this.edit((m) => { m.title = $('hubTitle').value.trim() || m.hub_id; }, 'title');
    for (const b of document.querySelectorAll('#hubLayers button')) b.onclick = () => { this.layer = b.dataset.layer; this.sel = null; this.renderLayer(); this.renderSide(); this.draw(); };
    for (const cb of document.querySelectorAll('#hubOverlays input')) cb.onchange = () => { this.ov[cb.dataset.ov] = cb.checked; this.draw(); };
    $('hubZone').onchange = () => { this.zoneBrush = $('hubZone').value; };
    $('hubPalFilter').oninput = () => this.renderPalette();
    $('hubPalZone').onchange = () => this.renderPalette();
    const tools = $('hubTools');
    for (const [id, title, key, hint] of this.TOOLS) {
      const b = document.createElement('button');
      b.className = 'small';
      b.dataset.tool = id;
      b.textContent = title;
      b.title = `${hint} (${key})`;
      b.onclick = () => this.setTool(id);
      tools.appendChild(b);
    }
    this.setTool('select');
  },
  setTool(id) {
    this.tool = id;
    if (id !== 'select') this.pick = null;
    for (const b of document.querySelectorAll('#hubTools button')) b.classList.toggle('active', b.dataset.tool === id);
    const t = this.TOOLS.find((x) => x[0] === id);
    $('hubHint').textContent = t ? t[3] : '';
    this.renderPalette();
    this.draw();
  },
  renderLayer() { for (const b of document.querySelectorAll('#hubLayers button')) b.classList.toggle('active', b.dataset.layer === this.layer); },
  down(e) {
    if (!this.map) return;
    const [x, z] = this.eventWorld(e);
    if (e.button === 1 || e.button === 2 || (e.button === 0 && e.altKey)) {
      this.drag = { kind: 'pan', sx: e.clientX, sz: e.clientY, cx: this.view.cx, cz: this.view.cz };
      return;
    }
    if (e.button !== 0) return;
    const ix = this.index('x', x), iz = this.index('z', z);
    const c = ix.inBlock && iz.inBlock ? this.cell(ix.i, iz.i) : null;
    if (this.tool === 'select') {
      if (this.pick) {
        if (!c) { notice('Карточку ставят в квартал.'); return; }
        const card = this.newCard(c, this.pick, [x, z]);
        this.edit(() => { this.cards(c).push(card); }, 'place');
        this.sel = { kind: 'card', i: c.i, j: c.j, layer: this.layer, k: this.cards(c).length - 1 };
        if (!e.shiftKey) this.pick = null;
        this.renderPalette();
        this.renderSide();
        this.draw();
        return;
      }
      const hit = this.cardAt(x, z) || this.redAt(x, z) || this.laneAt(x, z);
      if (hit && hit.kind === 'card') {
        const card = this.cardOf(hit);
        this.sel = hit;
        this.drag = { kind: 'card', x0: x, z0: z, cx: card.x, cz: card.z, before: JSON.stringify(this.map), moved: false };
      } else this.sel = hit || (c ? { kind: 'cell', i: c.i, j: c.j } : null);
      this.renderSide();
      this.draw();
      return;
    }
    if (this.tool === 'blocks') {
      if (!ix.inBlock || !iz.inBlock) return;
      if (c) this.edit((m) => { m.cells = m.cells.filter((q) => q !== c); }, 'remove block');
      else this.edit((m) => {
        const zone = this.zoneBrush;
        m.cells.push(this.blankCell(ix.i, iz.i, zone));
        const g = m.grid;
        g.extent = [Math.min(g.extent[0], ix.i), Math.min(g.extent[1], iz.i), Math.max(g.extent[2], ix.i), Math.max(g.extent[3], iz.i)];
      }, 'add block');
      return;
    }
    if (this.tool === 'zones') {
      if (c) this.setZone(c, this.zoneBrush);
      return;
    }
    if (this.tool === 'avenues') {
      const ax = !ix.inBlock && iz.inBlock ? 'x' : ix.inBlock && !iz.inBlock ? 'z' : null;
      if (!ax) return;
      const li = ax === 'x' ? ix.gap : iz.gap;
      if (li === undefined) return;
      this.edit((m) => {
        const L = m.avenues[ax];
        const k = L.indexOf(li);
        if (k >= 0) L.splice(k, 1); else { L.push(li); L.sort((a, b) => a - b); }
      }, 'avenue');
      return;
    }
    if (this.tool === 'lanes') {
      const hit = this.laneAt(x, z);
      if (!hit) return;
      this.sel = hit;
      this.toggleLane(hit.id);
      return;
    }
    if (this.tool === 'red') {
      const hit = this.redAt(x, z);
      if (hit) { this.edit((m) => { m.red_zones = m.red_zones.filter((r) => 'red.map.' + r.id !== hit.id); }, 'remove red'); return; }
      this.drag = { kind: 'red', x0: x, z0: z, x1: x, z1: z };
    }
  },
  move(e) {
    if (!this.map || $('hub').hidden) return;
    const cvr = this.canvas().getBoundingClientRect();
    const inside = e.clientX >= cvr.left && e.clientX <= cvr.right && e.clientY >= cvr.top && e.clientY <= cvr.bottom;
    const [x, z] = this.eventWorld(e);
    this.hover = inside ? [x, z] : null;
    const dr = this.drag;
    if (dr && dr.kind === 'pan') {
      this.view.cx = dr.cx - (e.clientX - dr.sx) * this.dpr / this.view.s;
      this.view.cz = dr.cz - (e.clientY - dr.sz) * this.dpr / this.view.s;
    } else if (dr && dr.kind === 'card') {
      const card = this.cardOf(this.sel);
      if (card) {
        card.x = this.snap(dr.cx + x - dr.x0);
        card.z = this.snap(dr.cz + z - dr.z0);
        dr.moved = dr.moved || Math.abs(x - dr.x0) + Math.abs(z - dr.z0) > 3 / this.view.s;
      }
    } else if (dr && dr.kind === 'red') {
      dr.x1 = x; dr.z1 = z;
    }
    if (inside) {
      const ix = this.index('x', x), iz = this.index('z', z);
      $('hubCursor').textContent = `x ${Math.round(x)}  z ${Math.round(z)}  ` + (ix.inBlock && iz.inBlock ? `квартал ${ix.i},${iz.i}` : 'улица / пусто');
    }
    if (inside || dr) this.draw();
  },
  up() {
    const dr = this.drag;
    this.drag = null;
    if (!dr) return;
    if (dr.kind === 'card') {
      if (!dr.moved) return;
      // the move is one undo step; a card dropped into another block moves into it
      this.history.push(dr.before);
      this.future = [];
      const s = this.sel;
      const c = this.cell(s.i, s.j);
      const card = this.cardOf(s);
      const [lc0, lc1] = this.localCentre(card, s.layer);
      const [cx, cz] = this.cellCentre(c);
      const wx = cx + card.x + lc0, wz = cz + card.z + lc1;
      const ix = this.index('x', wx), iz = this.index('z', wz);
      const to = ix.inBlock && iz.inBlock ? this.cell(ix.i, iz.i) : null;
      if (to && to !== c) {
        this.cards(c, s.layer).splice(s.k, 1);
        const [tx, tz] = this.cellCentre(to);
        card.x = this.snap(wx - tx - lc0);
        card.z = this.snap(wz - tz - lc1);
        this.cards(to, s.layer).push(card);
        this.sel = { kind: 'card', i: to.i, j: to.j, layer: s.layer, k: this.cards(to, s.layer).length - 1 };
      }
      this.dirty = true;
      this.rederive();
      this.renderSide();
      this.draw();
    } else if (dr.kind === 'red') {
      const w = Math.abs(dr.x1 - dr.x0), d = Math.abs(dr.z1 - dr.z0);
      if (w < 30 || d < 30) { this.draw(); return; }
      this.edit((m) => {
        const n = 1 + Math.max(0, ...m.red_zones.map((r) => +String(r.id).replace(/\D/g, '') || 0));
        m.red_zones.push({ id: 'r' + n, x: Math.round((dr.x0 + dr.x1) / 2), z: Math.round((dr.z0 + dr.z1) / 2),
          w: Math.round(w), d: Math.round(d), h: 600 });
      }, 'red zone');
    } else this.draw();
  },
  key(e) {
    if ($('hub').hidden || !this.map) return;
    if (e.target && /INPUT|SELECT|TEXTAREA/.test(e.target.tagName)) return;
    const k = e.key.toLowerCase();
    if ((e.ctrlKey || e.metaKey) && k === 'z') { e.preventDefault(); this.undo(); return; }
    if ((e.ctrlKey || e.metaKey) && k === 'y') { e.preventDefault(); this.redo(); return; }
    if ((e.ctrlKey || e.metaKey) && k === 's') { e.preventDefault(); this.save(); return; }
    if (k === 'escape') { this.pick = null; this.sel = null; this.renderPalette(); this.renderSide(); this.draw(); return; }
    if (k === 'r') {
      if (this.pick) { this.pickYaw = ((this.pickYaw || 0) + (e.shiftKey ? 270 : 90)) % 360; this.draw(); return; }
      if (this.sel && this.sel.kind === 'card') this.rotateSel(e.shiftKey ? 270 : 90);
      return;
    }
    if ((k === 'delete' || k === 'backspace') && this.sel && this.sel.kind === 'card') { this.deleteSel(); return; }
    if (k === '1' || k === '2') { this.layer = k === '1' ? 'top' : 'under'; this.sel = null; this.renderLayer(); this.renderSide(); this.draw(); return; }
    const t = this.TOOLS.find((x) => x[2].toLowerCase() === k);
    if (t && !e.ctrlKey) this.setTool(t[0]);
  },

  // ---------------------------------------------------------------- operations
  blankCell(i, j, zone) {
    const c = { i, j, zone: 'residential', top: [], under: [] };
    this.zoneExtras(c, zone, this.map.cells.concat([c]));
    return c;
  },
  zoneExtras(c, zone, cells) {
    // the per-zone fields: a dock faces an open side, drums / hotels spin, rim zones put out piers
    c.zone = zone;
    delete c.dock_side;
    delete c.spin;
    if (zone === 'dock') {
      const open = this.openSidesIn(c.i, c.j, cells);
      if (open.length) c.dock_side = open[0];
    }
    if (zone === 'hab_rot') c.spin = [9.0];
    if (zone === 'hotel') c.spin = [4.0, -4.0, 4.0];
    if (['port', 'refuel', 'logistics'].includes(zone)) {
      const pier = zone === 'refuel' ? 'hubb_fuel_dock' : zone === 'port' ? 'hubb_pier_l' : 'hubb_pier_m';
      c.piers = {};
      for (const o of this.openSidesIn(c.i, c.j, cells)) c.piers[o.join(',')] = pier;
    } else delete c.piers;
  },
  openSidesIn(i, j, cells) {
    const has = (a, b) => cells.some((q) => q.i === a && q.j === b);
    return [[1, 0], [-1, 0], [0, 1], [0, -1]].filter(([oi, oj]) => {
      const o = [i + oi, j + oj];
      if (has(...o)) return false;
      return ![[o[0] + 1, o[1]], [o[0] - 1, o[1]], [o[0], o[1] + 1], [o[0], o[1] - 1]].some(([a, b]) => (a !== i || b !== j) && has(a, b));
    });
  },
  setZone(c, zone) {
    if (zone === 'dock' && !this.openSides(c.i, c.j).length) { notice('Док ставится в крайний квартал с открытой стороной.'); return; }
    if (zone === 'canopy' && this.map.cells.some((q) => q.zone === 'canopy' && q !== c)) { notice('Диск (центр, площадь) — один на хаб.'); return; }
    this.edit(() => {
      this.zoneExtras(c, zone, this.map.cells);
      if (zone === 'canopy' && !c.top.some((k) => k.module === 'hubv_canopy')) {
        c.top = [{ module: 'hubv_canopy', x: 0, z: 0, yaw: 0 }];
        c.under = [{ module: 'hubv_canopy', x: 0, z: 0, yaw: 0 }];
      }
    }, 'zone');
  },
  rotateSel(by) {
    const s = this.sel;
    const card = this.cardOf(s);
    if (!card) return;
    this.edit(() => {
      // about the footprint's centre: the card stays where it is
      const [a0, a1] = this.localCentre(card, s.layer);
      card.yaw = ((card.yaw || 0) + by) % 360;
      const [b0, b1] = this.localCentre(card, s.layer);
      card.x = this.snap(card.x + a0 - b0);
      card.z = this.snap(card.z + a1 - b1);
    }, 'rotate');
  },
  deleteSel() {
    const s = this.sel;
    this.edit(() => { this.cards(this.cell(s.i, s.j), s.layer).splice(s.k, 1); }, 'delete');
    this.sel = { kind: 'cell', i: s.i, j: s.j };
    this.renderSide();
    this.draw();
  },
  toggleLane(id) {
    this.edit((m) => {
      const k = m.lanes_off.indexOf(id);
      if (k >= 0) m.lanes_off.splice(k, 1); else m.lanes_off.push(id);
    }, 'lane');
  },

  // ---------------------------------------------------------------- side panel + palette
  renderZoneSelect() {
    const opts = this.catalog.zones.map((z) => `<option value="${z.id}">${esc(z.title)}</option>`).join('');
    $('hubZone').innerHTML = opts;
    $('hubZone').value = this.zoneBrush;
    $('hubPalZone').innerHTML = '<option value="">все модули</option><option value="@cell">для зоны выбранного квартала</option>' +
      this.catalog.zones.filter((z) => z.modules.length).map((z) => `<option value="${z.id}">${esc(z.title)}</option>`).join('') +
      '<option value="@damaged">повреждённые</option>';
  },
  renderPalette() {
    const box = $('hubPalette');
    if (!this.catalog) return;
    const q = $('hubPalFilter').value.trim().toLowerCase();
    let zf = $('hubPalZone').value;
    if (zf === '@cell') {
      const c = this.sel && this.cell(this.sel.i, this.sel.j);
      zf = c ? c.zone : '';
    }
    const items = this.catalog.modules.filter((m) => {
      if (q && !(m.id.toLowerCase().includes(q) || m.title.toLowerCase().includes(q))) return false;
      if (zf === '@damaged') return m.damaged;
      if (zf) {
        const z = this.zones[zf];
        const under = z && z.under ? 'under:' + z.under : null;
        return m.zones.includes(zf) || (this.layer === 'under' && under && m.zones.includes(under));
      }
      return true;
    });
    box.innerHTML = '';
    for (const m of items) {
      const el = document.createElement('div');
      el.className = 'hub-card' + (this.pick === m.id ? ' active' : '');
      const w = Math.round(m.hi[0] - m.lo[0]), d = Math.round(m.hi[2] - m.lo[2]), h = Math.round(m.hi[1] - m.lo[1]);
      el.innerHTML = `<img src="previews/hub_modules/${encodeURIComponent(m.id.replace(/_dmg_.*/, ''))}.png" onerror="this.style.visibility='hidden'">` +
        `<div><b>${esc(m.id)}</b><br><span class="muted small">${esc(m.title)}</span><br><span class="small">${w}×${d} м, h ${h}</span></div>`;
      el.title = 'клик — взять карточку, затем клик в квартале (Shift — ставить ещё; R — повернуть)';
      el.onclick = () => {
        this.pick = this.pick === m.id ? null : m.id;
        if (this.tool !== 'select') this.setTool('select');
        this.renderPalette();
        this.draw();
      };
      box.appendChild(el);
    }
    $('hubPalCount').textContent = items.length + ' модулей';
  },
  renderSide() {
    const box = $('hubSide');
    if (!this.map) { box.innerHTML = ''; return; }
    const d = this.derived;
    const s = this.sel;
    let html = '';
    $('hubDirty').textContent = this.dirty ? '● не сохранено' : 'сохранено';
    $('hubDirty').className = this.dirty ? 'warn' : 'muted';
    if (!s) {
      const nCards = this.map.cells.reduce((a, c) => a + (c.top || []).length + (c.under || []).length, 0);
      html += `<h3>Хаб ${esc(this.map.hub_id)}</h3><div class="kv"><span>кварталов</span><b>${this.map.cells.length}</b>` +
        `<span>карточек</span><b>${nCards}</b>`;
      if (d) {
        const lanes = (d.lanes || []);
        html += `<span>узлов сборки</span><b>${d.nodes}</b><span>улиц</span><b>${d.streets}</b>` +
          `<span>коридоров</span><b>${lanes.filter((l) => l.kind !== 'blue').length} зел. + ${lanes.filter((l) => l.kind === 'blue').length} син.</b>` +
          `<span>выключено</span><b>${this.map.lanes_off.length}</b><span>связок</span><b>${(d.connectors || []).length}</b>` +
          `<span>красных зон</span><b>${(d.red_zones || []).length}</b><span>доков</span><b>${(d.docks || []).filter((x) => x.type === 'hub_hex_dock').length}</b>`;
      }
      html += '</div>';
      if (d && d.errors && d.errors.length) html += `<div class="bad">${d.errors.map(esc).join('<br>')}</div>`;
      html += `<p class="muted small">Колесо — масштаб, правая кнопка / Alt — сдвиг. 1 / 2 — слой. Ctrl+Z / Ctrl+Y — отмена / повтор.</p>`;
    } else if (s.kind === 'cell' || s.kind === 'card') {
      const c = this.cell(s.i, s.j);
      if (!c) { this.sel = null; return this.renderSide(); }
      if (s.kind === 'card') {
        const card = this.cardOf(s);
        const m = this.mods[card.module] || {};
        html += `<h3>${esc(card.module)}</h3><div class="muted small">${esc(m.title || '')}</div>` +
          `<div class="kv"><span>квартал</span><b>${c.i},${c.j} · ${this.layer === 'top' ? 'верх' : 'низ'}</b>` +
          `<span>x</span><input type="number" step="5" id="hcX" value="${card.x}"><span>z</span><input type="number" step="5" id="hcZ" value="${card.z}">` +
          `<span>поворот</span><b>${card.yaw || 0}° <button class="small" id="hcRot">↻ R</button></b></div>` +
          `<div class="row"><button class="small" id="hcDup">дублировать</button><button class="small" id="hcSwap">заменить на взятую</button>` +
          `<button class="small bad" id="hcDel">убрать (Del)</button></div>`;
        const pr = this.problems(c, s.layer)[s.k];
        if (pr) html += `<div class="${pr === 'overlap' ? 'bad' : 'warn'}">${pr === 'overlap' ? 'пересекается с другой карточкой' : 'выходит за край квартала'}</div>`;
      }
      const z = this.zones[c.zone] || {};
      html += `<h3>Квартал ${c.i},${c.j}</h3><div class="kv"><span>зона</span><select id="hcZone">${this.catalog.zones.map((q) =>
        `<option value="${q.id}"${q.id === c.zone ? ' selected' : ''}>${esc(q.title)}</option>`).join('')}</select>` +
        `<span>верх / низ</span><b>${(c.top || []).length} / ${(c.under || []).length} карточек</b></div>`;
      if (c.zone === 'dock') {
        const open = this.openSides(c.i, c.j);
        html += `<div class="kv"><span>док смотрит</span><select id="hcDock">${open.map((o) =>
          `<option value="${o.join(',')}"${c.dock_side && o.join(',') === c.dock_side.join(',') ? ' selected' : ''}>${this.sideName(o)}</option>`).join('')}</select></div>`;
        if (!open.length) html += '<div class="bad">нет открытой стороны: док не встанет</div>';
      }
      const open = this.openSides(c.i, c.j);
      if (open.length && !['dock', 'canopy', 'hab_rot', 'hotel'].includes(c.zone)) {
        html += '<div class="muted small">причалы с открытых сторон:</div><div class="kv">';
        for (const o of open) {
          const key = o.join(',');
          const cur = (c.piers || {})[key] || '';
          html += `<span>${this.sideName(o)}</span><select data-pier="${key}"><option value="">нет</option>${this.catalog.piers.map((p) =>
            `<option value="${p}"${p === cur ? ' selected' : ''}>${p}</option>`).join('')}</select>`;
        }
        html += '</div>';
      }
      if (c.spin && c.spin.length) html += `<div class="muted small">вращение: ${c.spin.map((v) => v + '°/с').join(', ')}</div>`;
      html += `<div class="row"><button class="small" id="hcClear">очистить слой «${this.layer === 'top' ? 'верх' : 'низ'}»</button></div>`;
      if (z.modules && z.modules.length) html += `<div class="muted small">модули зоны: ${z.modules.join(', ')}</div>`;
    } else if (s.kind === 'lane') {
      const ln = d && (d.lanes_geo || []).find((l) => l.id === s.id);
      const off = this.map.lanes_off.includes(s.id);
      html += `<h3>Коридор</h3><div class="small">${esc(s.id)}</div>`;
      if (ln) html += `<div class="kv"><span>вид</span><b>${ln.kind === 'blue' ? 'синий (подход к доку)' : ln.plaza ? 'площадь' : 'зелёный'}</b>` +
        `<span>уровень</span><b>${ln.level === 'top' ? 'верх' : 'низ'}</b><span>скорость</span><b>${ln.speed} м/с</b>` +
        `<span>корабли до</span><b>${ln.max_ship_m} м</b><span>станций</span><b>${ln.stations.length}</b></div>`;
      html += `<div class="row"><button class="small" id="hlToggle">${off ? 'включить' : 'выключить'}</button></div>` +
        '<div class="muted small">Выключенный коридор не попадает в игру; связки с ним уходят тоже. Одностороннее движение — выключить встречный.</div>';
    } else if (s.kind === 'red') {
      html += `<h3>Красная зона</h3><div class="small">${esc(s.id)}</div><div class="row"><button class="small bad" id="hrDel">убрать</button></div>`;
    }
    box.innerHTML = html;
    const on = (id, ev, fn) => { const el = document.getElementById(id); if (el) el[ev] = fn; };
    on('hcX', 'onchange', (e) => { const card = this.cardOf(s); this.edit(() => { card.x = +e.target.value; }, 'x'); });
    on('hcZ', 'onchange', (e) => { const card = this.cardOf(s); this.edit(() => { card.z = +e.target.value; }, 'z'); });
    on('hcRot', 'onclick', () => this.rotateSel(90));
    on('hcDel', 'onclick', () => this.deleteSel());
    on('hcDup', 'onclick', () => {
      const card = this.cardOf(s);
      const f = this.footprint(this.cell(s.i, s.j), card, s.layer);
      this.edit(() => { this.cards(this.cell(s.i, s.j), s.layer).push({ ...card, x: this.snap(card.x + (f.x1 - f.x0) + 20) }); }, 'duplicate');
      this.sel = { ...s, k: this.cards(this.cell(s.i, s.j), s.layer).length - 1 };
      this.renderSide(); this.draw();
    });
    on('hcSwap', 'onclick', () => {
      if (!this.pick) { notice('Сначала возьмите карточку в палитре.'); return; }
      const card = this.cardOf(s);
      this.edit(() => { card.module = this.pick; }, 'swap');
    });
    on('hcZone', 'onchange', (e) => this.setZone(this.cell(s.i, s.j), e.target.value));
    on('hcDock', 'onchange', (e) => { const c = this.cell(s.i, s.j); this.edit(() => { c.dock_side = e.target.value.split(',').map(Number); }, 'dock'); });
    on('hcClear', 'onclick', () => { const c = this.cell(s.i, s.j); this.edit(() => { c[this.layer] = []; }, 'clear'); });
    on('hlToggle', 'onclick', () => this.toggleLane(s.id));
    on('hrDel', 'onclick', () => { this.edit((m) => { m.red_zones = m.red_zones.filter((r) => 'red.map.' + r.id !== s.id); }, 'remove red'); this.sel = null; this.renderSide(); this.draw(); });
    for (const el of box.querySelectorAll('select[data-pier]')) el.onchange = () => {
      const c = this.cell(s.i, s.j);
      this.edit(() => {
        c.piers = c.piers || {};
        if (el.value) c.piers[el.dataset.pier] = el.value; else delete c.piers[el.dataset.pier];
      }, 'pier');
    };
    if ($('hubPalZone').value === '@cell') this.renderPalette();
  },
  sideName(o) { return { '1,0': '+X (вправо)', '-1,0': '−X (влево)', '0,1': '+Z (вниз)', '0,-1': '−Z (вверх)' }[o.join(',')]; },
};
window.Hub = Hub;
if (document.readyState === 'loading') window.addEventListener('DOMContentLoaded', () => Hub.bind());
else Hub.bind();
