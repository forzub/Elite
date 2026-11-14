// Steps 2..6 of a model (MESH_EDITOR_SPEC 16): the assembly document - hierarchy and materials, hit volumes and
// zones, mounts / seams / buses, damage, build. app.js owns the socket; this file owns the document while it is open.
'use strict';

const ROLES = ['hull', 'frame', 'plate', 'structure', 'cockpit', 'reactor', 'service_reactor', 'engine', 'tank', 'jump', 'weapon',
  'sensor', 'control', 'cooling', 'life_support', 'power_panel', 'craft', 'equipment'];
const ROLE_TEXT = { hull: 'корпус', frame: 'рама', plate: 'панель обшивки', structure: 'конструкция', cockpit: 'кабина', reactor: 'реактор',
  service_reactor: 'служебный реактор', engine: 'двигатель', tank: 'бак', jump: 'прыжковая система', weapon: 'оружие', sensor: 'датчик / связь',
  control: 'управление', cooling: 'охлаждение', life_support: 'жизнеобеспечение', power_panel: 'солнечная панель', craft: 'аппарат',
  equipment: 'оборудование' };
const BUSES = { propulsion_power: 'питание тяги', service_power: 'служебное питание', coolant: 'охладитель', heat_rejection: 'сброс тепла',
  data: 'данные', fuel: 'топливо', jump_charge: 'прыжковый заряд' };
const BUS_CSS = { propulsion_power: '#ff8a3d', service_power: '#ffd84a', coolant: '#4ac8ff', heat_rejection: '#ff4a8a', data: '#6dff8a',
  fuel: '#c77dff', jump_charge: '#8ab4ff' };
const ZONES = { keep_out: 'красная — не входить', keep_in: 'синяя — коридор', prefer: 'зелёная — желательно' };

const Asm = {
  doc: null, file: '', dirty: false, step: 0, selected: null, library: null, built: null, sel: null,

  // ---- wiring from app.js
  open(step) {
    this.step = step;
    $('asmSide').hidden = false;
    $('stepSide').hidden = true;
    $('asmTools').hidden = false;
    $('viewTools').hidden = true;
    $('lodBar').hidden = true;
    if (!this.library) send({ cmd: 'library' });
    if (!this.doc || this.doc.model !== `${state.model.category}/${state.model.id}`) {
      $('asmSide').innerHTML = '<div class="muted">загрузка документа сборки…</div>';
      if (!this.pending) send({ cmd: 'assembly', category: state.model.category, id: state.model.id });
      this.pending = true;
    } else {
      this.render();
      whenViewer(async (v) => { v.onPickNode((id) => this.pick(id)); await v.showAssembly(this.doc); v.fitAssembly(); v.selectNode(this.selected, false); });
    }
  },
  close() {
    this.step = 0;
    $('asmSide').hidden = true;
    $('stepSide').hidden = false;
    $('asmTools').hidden = true;
    $('viewTools').hidden = false;
    $('lodBar').hidden = false;
    whenViewer((v) => v.hideAssembly());
  },
  reset() { this.doc = null; this.dirty = false; this.selected = null; this.built = null; this.sel = null; this.pending = false; this.step = 0; },
  onAssembly(m) {
    if (!state.model || m.id !== state.model.id) return;
    this.doc = m.doc;
    this.file = m.file;
    this.dirty = false;
    this.pending = false;
    if (!this.step) return;   // fetched in the background; shown when a step 2..6 opens
    whenViewer(async (v) => { v.onPickNode((id) => this.pick(id)); await v.showAssembly(this.doc); v.fitAssembly(); });
    this.render();
  },
  onLibrary(m) { this.library = m.lib; if (this.step) this.render(); },
  onBuilt(m) { this.built = m; this.render(); },
  changed() {
    this.dirty = true;
    whenViewer((v) => { v.showAssembly(this.doc); v.selectNode(this.selected, false); });
    this.render();
  },
  save() {
    send({ cmd: 'saveAssembly', category: state.model.category, id: state.model.id, doc: this.doc });
    this.dirty = false;
    notice('Документ сборки сохранён: ' + this.file);
  },
  pick(id) {
    this.selected = id;
    whenViewer((v) => v.selectNode(id, false));
    this.render();
  },

  // ---- rendering of the side panel by step
  render() {
    if (!this.doc) return;
    const d = this.doc;
    const head = `<div class="asm-head"><div><b>${esc(STEPS[this.step])}</b>
        <div class="muted small">${d.seeded ? 'заготовка (' + esc((d.seededFrom || []).join(', ')) + ') — сохраните, чтобы закрепить' : 'документ: ' + esc(this.file)}</div></div>
      <button id="asmSave" class="${this.dirty || d.seeded ? 'primary' : ''}">Сохранить</button></div>`;
    const body = [null, () => this.stepComposition(), () => this.stepVolumes(), () => this.stepLinks(), () => this.stepDamage(), () => this.stepBuild()][this.step];
    $('asmSide').innerHTML = head + (body ? body() : '');
    $('asmSave').onclick = () => this.save();
    for (const el of $('asmSide').querySelectorAll('[data-act]')) el.onclick = (e) => { e.stopPropagation(); this.act(el.dataset.act, el.dataset); };
    for (const el of $('asmSide').querySelectorAll('[data-set]')) el.onchange = () => this.set(el.dataset.set, el.dataset, el.value);
    for (const tr of $('asmSide').querySelectorAll('tr[data-node]')) tr.onclick = () => { this.pick(tr.dataset.node); whenViewer((v) => v.selectNode(tr.dataset.node, true)); };
    for (const tr of $('asmSide').querySelectorAll('tr[data-item]')) tr.onclick = () => { this.sel = { list: tr.dataset.list, id: tr.dataset.item }; this.render(); };
  },
  nodeOptions(value, empty) {
    return (empty ? `<option value="">${empty}</option>` : '') +
      this.doc.nodes.map((n) => `<option value="${esc(n.id)}" ${n.id === value ? 'selected' : ''}>${esc(n.id)}</option>`).join('');
  },
  vecInputs(list, id, field, v) {
    return `<span class="vec">${[0, 1, 2].map((k) => `<input type="number" step="0.01" value="${(v || [0, 0, 0])[k]}" data-set="vec" data-list="${list}" data-id="${esc(id)}" data-field="${field}" data-k="${k}">`).join('')}</span>`;
  },

  // step 2: hierarchy, roles, materials
  stepComposition() {
    const d = this.doc;
    const kids = new Map();
    for (const n of d.nodes) { const p = n.parent || ''; if (!kids.has(p)) kids.set(p, []); kids.get(p).push(n); }
    const ids = new Set(d.nodes.map((n) => n.id));
    const rows = [];
    const walk = (parent, depth) => {
      for (const n of kids.get(parent) || []) {
        rows.push([n, depth]);
        if (depth < 12) walk(n.id, depth + 1);
      }
    };
    walk('', 0);
    for (const n of d.nodes) if (n.parent && !ids.has(n.parent) && !rows.some(([x]) => x === n)) rows.push([n, 0]);
    const lib = (this.library && this.library.materials) || [];
    const sel = d.nodes.find((n) => n.id === this.selected);
    let detail = '<div class="muted">Щёлкните узел в дереве или на модели.</div>';
    if (sel) {
      const part = d.parts[sel.part] || {};
      const srcMats = part.sourceMaterials || [];
      detail = `<h3>${esc(sel.id)}</h3>
        ${sel.ref ? `<div class="linkbox">ссылка на модуль <b>${esc(sel.ref)}</b> <button class="small" data-act="openRef" data-ref="${esc(sel.ref)}">Открыть модуль</button></div>` : ''}
        <div class="form">
          <label>родитель</label><select data-set="parent" data-id="${esc(sel.id)}">${this.nodeOptions(sel.parent, '— корень —')}</select>
          <label>роль</label><select data-set="role" data-id="${esc(sel.id)}">${ROLES.map((r) => `<option value="${r}" ${r === sel.role ? 'selected' : ''}>${ROLE_TEXT[r]}</option>`).join('')}</select>
          <label>положение</label>${this.vecInputs('nodes', sel.id, 'position', sel.position)}
          <label>поворот, °</label>${this.vecInputs('nodes', sel.id, 'rotation_deg', sel.rotation_deg)}
          ${sel.ref ? '' : `<label>материал детали</label><select data-set="partMaterial" data-part="${esc(sel.part)}">
            <option value="">по исходным (${esc(srcMats.join(', ') || '—')})</option>
            ${lib.map((m) => `<option value="${esc(m.id)}" ${m.id === part.material ? 'selected' : ''}>${esc(m.name)} · ${esc(m.id)}</option>`).join('')}</select>`}
        </div>`;
    }
    const used = new Set(Object.values(d.materials || {}));
    for (const p of Object.values(d.parts)) if (p.material) used.add(p.material);
    const matRows = Object.entries(d.materials || {}).map(([src, id]) => {
      const m = lib.find((x) => x.id === id);
      const sw = m ? `<i class="sw" style="background:${rgb(m.baseColor)}"></i>` : '<i class="sw" style="background:#555"></i>';
      return `<tr><td>${esc(src)}</td><td>${sw}<select data-set="matMap" data-src="${esc(src)}">
        ${lib.map((x) => `<option value="${esc(x.id)}" ${x.id === id ? 'selected' : ''}>${esc(x.name)}</option>`).join('')}
        ${m ? '' : `<option value="${esc(id)}" selected>${esc(id)} (нет в библиотеке)</option>`}</select></td></tr>`;
    }).join('');
    return `<div class="table-wrap short"><table class="tree">${rows.map(([n, depth]) => `<tr data-node="${esc(n.id)}" class="${n.id === this.selected ? 'sel' : ''}">
        <td style="padding-left:${6 + depth * 14}px">${n.ref ? '<span class="lnk">⧉</span> ' : ''}${esc(n.id)}</td><td class="muted">${esc(ROLE_TEXT[n.role] || n.role || '')}</td></tr>`).join('')}</table></div>
      <div class="detail">${detail}</div>
      <details class="detail" open><summary>Материалы: исходное имя → глобальная библиотека (${lib.length})</summary>
        <table class="mats">${matRows || '<tr><td class="muted">в мешах нет материалов — всё «по умолчанию»</td></tr>'}</table>
        <div class="muted small">Библиотека: assets/materials/material_library.json · ${used.size} используется.
        <button class="small" data-act="libEdit">Править библиотеку</button></div></details>`;
  },

  // step 3: hit volumes and zones
  stepVolumes() {
    const d = this.doc;
    const s = this.sel && this.sel.list;
    const hit = d.hit_volumes.map((h) => `<tr data-item="${esc(h.id)}" data-list="hit_volumes" class="${s === 'hit_volumes' && this.sel.id === h.id ? 'sel' : ''}">
      <td>${esc(h.id)}</td><td>${esc(h.node || '')}</td><td>${h.shape === 'cylinder' ? 'цилиндр' : 'коробка'}</td><td class="muted">${esc(h.source || '')}</td></tr>`).join('');
    const zones = d.zones.map((z) => `<tr data-item="${esc(z.id)}" data-list="zones" class="${s === 'zones' && this.sel.id === z.id ? 'sel' : ''}">
      <td><i class="sw" style="background:${{ keep_out: '#e0402c', keep_in: '#3d6dff', prefer: '#3ccf5a' }[z.kind]}"></i>${esc(z.id)}</td><td colspan="3">${esc(ZONES[z.kind] || z.kind)}</td></tr>`).join('');
    return `<div class="btnrow"><button class="small" data-act="hitFromSel">+ хитвалум по выбранному узлу</button>
        <button class="small" data-act="hitAuto">заново: по коробке каждой детали</button></div>
      <div class="table-wrap short"><table><tr><th>Хитвалум (${d.hit_volumes.length})</th><th>узел</th><th>форма</th><th>откуда</th></tr>${hit}</table></div>
      <div class="btnrow">${Object.entries(ZONES).map(([k, t]) => `<button class="small" data-act="zoneAdd" data-kind="${k}">+ ${t.split(' — ')[0]}</button>`).join('')}</div>
      <div class="table-wrap short"><table><tr><th>Зона (${d.zones.length})</th><th colspan="3">вид</th></tr>${zones}</table></div>
      ${this.itemEditor()}`;
  },

  // step 4: mounts, seams, buses
  stepLinks() {
    const d = this.doc;
    const s = this.sel && this.sel.list;
    const mounts = d.mounts.map((m) => `<tr data-item="${esc(m.id)}" data-list="mounts" class="${s === 'mounts' && this.sel.id === m.id ? 'sel' : ''}">
      <td>${esc(m.id)}</td><td>${esc(m.a)} ← ${esc(m.b)}</td><td class="muted">${m.position ? '' : 'без точки'}</td></tr>`).join('');
    const seams = d.seams.map((x) => `<tr data-item="${esc(x.id)}" data-list="seams" class="${s === 'seams' && this.sel.id === x.id ? 'sel' : ''}">
      <td>${esc(x.id)}</td><td>${esc(x.a)} | ${esc(x.b)}</td><td class="muted">${(x.proxies || []).length} кол.</td></tr>`).join('');
    const conns = d.connections.map((c) => `<tr data-item="${esc(c.id)}" data-list="connections" class="${s === 'connections' && this.sel.id === c.id ? 'sel' : ''}">
      <td><i class="sw" style="background:${BUS_CSS[c.bus] || '#fff'}"></i>${esc(BUSES[c.bus] || c.bus)}</td><td>${esc(c.from)} → ${esc(c.to)}</td><td class="muted">${esc(c.source || '')}</td></tr>`).join('');
    return `<div class="btnrow"><button class="small" data-act="mountAdd">+ крепление выбранного узла к родителю</button>
        <button class="small" data-act="seamAdd">+ шов выбранного узла с родителем</button></div>
      <div class="table-wrap short"><table><tr><th>Крепление (${d.mounts.length})</th><th>к чему ← что</th><th></th></tr>${mounts}</table></div>
      <div class="table-wrap short"><table><tr><th>Шов-«колбаска» (${d.seams.length})</th><th>детали</th><th></th></tr>${seams}</table></div>
      <div class="btnrow"><select id="connFrom">${this.nodeOptions(this.selected, 'откуда')}</select><select id="connBus">${Object.entries(BUSES).map(([k, t]) => `<option value="${k}">${t}</option>`).join('')}</select>
        <select id="connTo">${this.nodeOptions('', 'куда')}</select><button class="small" data-act="connAdd">+ шина</button></div>
      <div class="table-wrap short"><table><tr><th>Шина (${d.connections.length})</th><th>откуда → куда</th><th></th></tr>${conns}</table></div>
      ${this.itemEditor()}`;
  },

  // step 5: damage
  stepDamage() {
    const d = this.doc;
    const det = new Set(d.damage.detachable || []);
    const states = d.damage.states || [];
    return `<div class="muted small">Отрывается — узел, который отделяется, когда разрушены все его швы / крепления (правило из
        DAMAGE: «все крепления разрушены → модуль свободен»). Состояния — подмена вида узла (например, сорванная плита открывает отсек).</div>
      <div class="table-wrap"><table><tr><th>Узел</th><th>роль</th><th>отрывается</th><th>швов / креплений</th></tr>
      ${d.nodes.map((n) => {
        const links = d.seams.filter((x) => x.a === n.id || x.b === n.id).length + d.mounts.filter((x) => x.a === n.id || x.b === n.id).length;
        return `<tr><td>${esc(n.id)}</td><td class="muted">${esc(ROLE_TEXT[n.role] || '')}</td>
          <td><input type="checkbox" data-set="detach" data-id="${esc(n.id)}" ${det.has(n.id) ? 'checked' : ''}></td><td class="num">${links || '<span class="warn">0</span>'}</td></tr>`;
      }).join('')}</table></div>
      <div class="detail"><b>Состояния повреждений (${states.length})</b>
        ${states.map((s) => `<div>${esc(s.node)}: ${esc(s.id)} — ${esc(s.display || '')}</div>`).join('') || '<div class="muted">нет</div>'}</div>`;
  },

  // step 6: build
  stepBuild() {
    const d = this.doc;
    const cal = d.calibration || {};
    const b = this.built;
    const sum = [['узлов', d.nodes.length], ['ссылок на модули', d.nodes.filter((n) => n.ref).length], ['хитвалумов', d.hit_volumes.length],
      ['зон', d.zones.length], ['креплений', d.mounts.length], ['швов', d.seams.length], ['шин', d.connections.length]];
    const warn = [];
    if (!d.hit_volumes.length) warn.push('нет хитвалумов');
    const ids = new Set(d.nodes.map((n) => n.id));
    for (const c of d.connections) if (!ids.has(c.from) || !ids.has(c.to)) warn.push(`шина ${c.id}: нет узла ${!ids.has(c.from) ? c.from : c.to}`);
    for (const m of d.mounts) if (!ids.has(m.a) || !ids.has(m.b)) warn.push(`крепление ${m.id}: нет узла`);
    return `<div class="detail"><table>${sum.map(([t, n]) => `<tr><td>${t}</td><td class="num">${n}</td></tr>`).join('')}</table>
        ${warn.length ? `<ul>${warn.slice(0, 12).map((w) => `<li class="note">${esc(w)}</li>`).join('')}</ul>` : ''}</div>
      <div class="detail"><b>Масштаб</b> <span class="muted small">— исходник не в метрах (станция в километрах): длина по оси = столько метров</span>
        <div class="form"><label>ось</label><select data-set="calAxis">${['x', 'y', 'z'].map((a) => `<option ${a === (cal.axis || 'z') ? 'selected' : ''}>${a}</option>`).join('')}</select>
          <label>метров (0 — исходник в метрах)</label><input type="number" step="1" value="${cal.targetMeters || 0}" data-set="calMeters">
          <span class="muted small" style="grid-column:1/3">${esc(cal.from || '')}</span></div></div>
      <div class="btnrow"><button class="primary" data-act="build">Собрать</button>
        <span class="muted small">сохраняет документ и пишет пакет в ${esc(state.outputRoot || '')}/${esc(state.model.category)}/${esc(state.model.id)}/</span></div>
      ${b ? `<div class="detail"><div class="${b.ok ? 'good' : 'bad'}">${b.ok ? 'Собрано' : 'Ошибка'}: ${esc(b.report)}</div>
        <table>${(b.files || []).map((f) => `<tr><td>${esc(f.name)}</td><td class="num">${fmt(Math.round(f.bytes / 1024))} КБ</td></tr>`).join('')}</table></div>` : ''}`;
  },

  // the numbers of the selected hit volume / zone / mount / seam / bus
  itemEditor() {
    if (!this.sel) return '<div class="detail muted">Щёлкните строку, чтобы править её.</div>';
    const list = this.doc[this.sel.list];
    const it = list && list.find((x) => x.id === this.sel.id);
    if (!it) return '';
    const L = this.sel.list, id = it.id;
    let f = '';
    if (L === 'hit_volumes') {
      f = `<label>узел</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="node">${this.nodeOptions(it.node, '—')}</select>
        <label>центр</label>${this.vecInputs(L, id, 'position', it.position)}<label>поворот, °</label>${this.vecInputs(L, id, 'rotation_deg', it.rotation_deg)}
        ${it.shape === 'cylinder' ? `<label>радиус</label><input type="number" step="0.01" value="${it.radius}" data-set="num" data-list="${L}" data-id="${esc(id)}" data-field="radius">
          <label>полувысота</label><input type="number" step="0.01" value="${it.half_height}" data-set="num" data-list="${L}" data-id="${esc(id)}" data-field="half_height">`
          : `<label>полуразмер</label>${this.vecInputs(L, id, 'half_size', it.half_size)}`}`;
    } else if (L === 'zones') {
      f = `<label>вид</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="kind">${Object.entries(ZONES).map(([k, t]) => `<option value="${k}" ${k === it.kind ? 'selected' : ''}>${t}</option>`).join('')}</select>
        <label>центр</label>${this.vecInputs(L, id, 'position', it.position)}<label>поворот, °</label>${this.vecInputs(L, id, 'rotation_deg', it.rotation_deg)}
        <label>полуразмер</label>${this.vecInputs(L, id, 'half_size', it.half_size)}`;
    } else if (L === 'mounts') {
      f = `<label>к чему</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="a">${this.nodeOptions(it.a)}</select>
        <label>что</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="b">${this.nodeOptions(it.b)}</select>
        ${it.position ? `<label>точка</label>${this.vecInputs(L, id, 'position', it.position)}<label>полуразмер</label>${this.vecInputs(L, id, 'half_size', it.half_size)}` : ''}`;
    } else if (L === 'seams') {
      const p = (it.proxies || [])[0];
      f = `<label>деталь A</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="a">${this.nodeOptions(it.a)}</select>
        <label>деталь B</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="b">${this.nodeOptions(it.b)}</select>
        ${p ? `<label>колбаска: центр</label>${this.vecInputs('seamProxy', id, 'position', p.position)}<label>поворот, °</label>${this.vecInputs('seamProxy', id, 'rotation_deg', p.rotation_deg)}
        <label>полуразмер</label>${this.vecInputs('seamProxy', id, 'half_size', p.half_size)}` : ''}`;
    } else if (L === 'connections') {
      f = `<label>шина</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="bus">${Object.entries(BUSES).map(([k, t]) => `<option value="${k}" ${k === it.bus ? 'selected' : ''}>${t}</option>`).join('')}</select>
        <label>откуда</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="from">${this.nodeOptions(it.from)}</select>
        <label>куда</label><select data-set="field" data-list="${L}" data-id="${esc(id)}" data-field="to">${this.nodeOptions(it.to)}</select>`;
    }
    return `<div class="detail"><h3>${esc(id)} <button class="small danger" data-act="del">удалить</button></h3><div class="form">${f}</div></div>`;
  },

  // ---- edits
  find(list, id) { return list === 'nodes' ? this.doc.nodes.find((n) => n.id === id) : (this.doc[list] || []).find((x) => x.id === id); },
  set(kind, ds, value) {
    const d = this.doc;
    if (kind === 'parent') this.find('nodes', ds.id).parent = value || null;
    else if (kind === 'role') this.find('nodes', ds.id).role = value;
    else if (kind === 'partMaterial') d.parts[ds.part].material = value;
    else if (kind === 'matMap') d.materials[ds.src] = value;
    else if (kind === 'detach') {
      const s = new Set(d.damage.detachable || []);
      if (event.target.checked) s.add(ds.id); else s.delete(ds.id);
      d.damage.detachable = [...s];
    } else if (kind === 'calAxis' || kind === 'calMeters') {
      d.calibration = d.calibration || { axis: 'z', targetMeters: 0 };
      if (kind === 'calAxis') d.calibration.axis = value; else d.calibration.targetMeters = Number(value);
      d.calibration.from = 'задано в редакторе';
      delete d.calibration.sourceExtent;
    } else if (kind === 'vec') {
      const it = ds.list === 'seamProxy' ? this.find('seams', ds.id).proxies[0] : this.find(ds.list, ds.id);
      it[ds.field] = (it[ds.field] || [0, 0, 0]).slice();
      it[ds.field][Number(ds.k)] = Number(value);
    } else if (kind === 'num') this.find(ds.list, ds.id)[ds.field] = Number(value);
    else if (kind === 'field') this.find(ds.list, ds.id)[ds.field] = value;
    this.changed();
  },
  uid(prefix, list) {
    let i = list.length;
    while (list.some((x) => x.id === `${prefix}${i}`)) i++;
    return `${prefix}${i}`;
  },
  partBox(nodeId) {
    const n = this.doc.nodes.find((x) => x.id === nodeId);
    const p = n && this.doc.parts[n.part];
    if (!p || !p.centre) return { c: (n && n.position) || [0, 0, 0], h: [0.5, 0.5, 0.5] };
    const pos = n.position || [0, 0, 0];
    return { c: p.centre.map((x, k) => +(x + pos[k]).toFixed(3)), h: p.size.map((x) => +Math.max(x / 2, 0.02).toFixed(3)) };
  },
  act(a, ds) {
    const d = this.doc;
    const nodeId = this.selected;
    if (a === 'openRef') { const p = { key: ds.ref, link: { category: ds.ref.split('/')[0], id: ds.ref.split('/')[1] } }; this.close(); openLink(p); return; }
    if (a === 'libEdit') { showLibrary(); return; }
    if (a === 'build') { this.save(); this.built = null; this.render(); send({ cmd: 'build', category: state.model.category, id: state.model.id }); notice('Сборка идёт…'); return; }
    if (a === 'del') {
      d[this.sel.list] = d[this.sel.list].filter((x) => x.id !== this.sel.id);
      this.sel = null;
    } else if (a === 'hitFromSel') {
      if (!nodeId) { notice('Сначала выберите узел на модели или в дереве (шаг «Состав»).'); return; }
      const b = this.partBox(nodeId);
      const id = this.uid(`${nodeId}.hit`, d.hit_volumes);
      d.hit_volumes.push({ id, node: nodeId, shape: 'box', position: b.c, rotation_deg: [0, 0, 0], half_size: b.h, source: 'editor' });
      this.sel = { list: 'hit_volumes', id };
    } else if (a === 'hitAuto') {
      d.hit_volumes = d.nodes.filter((n) => d.parts[n.part]).map((n) => {
        const b = this.partBox(n.id);
        return { id: `${n.id}.hit`, node: n.id, shape: 'box', position: b.c, rotation_deg: [0, 0, 0], half_size: b.h, source: 'editor-auto' };
      });
    } else if (a === 'zoneAdd') {
      const b = nodeId ? this.partBox(nodeId) : { c: [0, 0, 0], h: [10, 10, 10] };
      const id = this.uid(`zone_${ds.kind}_`, d.zones);
      d.zones.push({ id, kind: ds.kind, shape: 'box', position: b.c, rotation_deg: [0, 0, 0], half_size: b.h.map((x) => +(x * 1.2).toFixed(3)), source: 'editor' });
      this.sel = { list: 'zones', id };
    } else if (a === 'mountAdd' || a === 'seamAdd') {
      const n = d.nodes.find((x) => x.id === nodeId);
      if (!n || !n.parent) { notice('Выберите узел, у которого есть родитель (шаг «Состав»).'); return; }
      const b = this.partBox(nodeId);
      if (a === 'mountAdd') {
        const id = this.uid('mount_', d.mounts);
        d.mounts.push({ id, a: n.parent, b: nodeId, kind: 'equipment_mount', position: b.c, rotation_deg: [0, 0, 0], half_size: [0.08, 0.08, 0.08], source: 'editor' });
        this.sel = { list: 'mounts', id };
      } else {
        const id = this.uid('seam_', d.seams);
        d.seams.push({ id, a: n.parent, b: nodeId, source: 'editor', proxies: [{ id: id + '.p', position: b.c, rotation_deg: [0, 0, 0], half_size: [Math.max(...b.h), 0.04, 0.04] }] });
        this.sel = { list: 'seams', id };
      }
    } else if (a === 'connAdd') {
      const from = $('connFrom').value, to = $('connTo').value;
      if (!from || !to || from === to) { notice('Выберите, откуда и куда идёт шина.'); return; }
      const id = this.uid('c', d.connections);
      d.connections.push({ id, from, to, bus: $('connBus').value, source: 'editor' });
      this.sel = { list: 'connections', id };
    }
    this.changed();
  },
};

function rgb(c) { return `rgb(${(c || [0.6, 0.6, 0.6]).map((x) => Math.round(Math.min(1, x) * 255)).join(',')})`; }

// ---- the global material library (MESH_EDITOR_SPEC 16.4)
function showLibrary() {
  $('library').hidden = false;
  renderLibrary();
}
function renderLibrary() {
  const lib = Asm.library;
  if (!lib) { send({ cmd: 'library' }); return; }
  const q = ($('libFilter').value || '').toLowerCase();
  $('libRows').innerHTML = lib.materials.filter((m) => !q || (m.id + ' ' + m.name).toLowerCase().includes(q)).map((m, i) => `<tr data-i="${lib.materials.indexOf(m)}">
    <td><input type="color" value="${hex(m.baseColor)}" data-f="baseColor"></td><td>${esc(m.id)}</td><td><input value="${esc(m.name)}" data-f="name"></td>
    <td><input type="number" min="0" max="1" step="0.05" value="${m.metallic}" data-f="metallic"></td>
    <td><input type="number" min="0" max="1" step="0.05" value="${m.roughness}" data-f="roughness"></td>
    <td><input type="color" value="${hex(m.emissive)}" data-f="emissive"></td>
    <td><input type="checkbox" ${m.twoSided ? 'checked' : ''} data-f="twoSided"></td>
    <td><input value="${esc(m.physical || '')}" data-f="physical" placeholder="elite.material.*"></td></tr>`).join('');
  for (const inp of $('libRows').querySelectorAll('input')) inp.onchange = () => {
    const m = lib.materials[Number(inp.closest('tr').dataset.i)];
    const f = inp.dataset.f;
    if (f === 'baseColor' || f === 'emissive') m[f] = unhex(inp.value);
    else if (f === 'twoSided') m[f] = inp.checked;
    else if (f === 'metallic' || f === 'roughness') m[f] = Number(inp.value);
    else m[f] = inp.value || (f === 'physical' ? null : '');
    Asm.libDirty = true;
  };
}
function hex(c) { return '#' + (c || [0, 0, 0]).map((x) => Math.round(Math.min(1, Math.max(0, x)) * 255).toString(16).padStart(2, '0')).join(''); }
function unhex(h) { return [1, 3, 5].map((i) => +(parseInt(h.substr(i, 2), 16) / 255).toFixed(4)); }
