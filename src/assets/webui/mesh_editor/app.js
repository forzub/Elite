// Elite mesh editor page (docs/MESH_EDITOR_SPEC.md). Talks JSON to the C++ backend over the WebSocket.
// URL parameters for headless checks: ?dialog=1 opens "Открыть", &cat=<category> shows its cards, &open=<id> opens one,
// &part=<key> selects a part, &follow=1 then opens that part's link (the trail back).
'use strict';

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const state = { ws: null, categories: [], category: null, cards: [], model: null, pollTimer: null, object: {}, settings: null,
  stack: [], pendingBack: null };   // stack: the models a link was opened from (SolidWorks-like 'back to the assembly')
const UNUSED = '__none__';   // the object list entry 'used nowhere'

// the step windows (MESH_EDITOR_SPEC 15 / 16): one list for a module and an assembly (2026-11-14, author away:
// "решение по вопросам - за тобой"); LOD generation and animation are later steps
const STEPS = ['Загрузка и проверка', 'Состав и материалы', 'Хитвалумы и зоны', 'Крепления, швы, шины', 'Повреждения', 'Сборка'];

function send(obj) {
  // a command about the open model works on the open variant (MESH_EDITOR_SPEC 18)
  if (state.model && obj.id === state.model.id && obj.category === state.model.category && obj.cmd !== 'open' && !('variant' in obj))
    obj = { ...obj, variant: state.variant || 'intact' };
  if (state.ws && state.ws.readyState === WebSocket.OPEN) state.ws.send(JSON.stringify(obj));
}

function connect() {
  state.ws = new WebSocket(`ws://${location.host}`);
  state.ws.onopen = () => {
    $('conn').textContent = 'связь есть';
    send({ cmd: 'categories' });
    if (params.get('settings')) send({ cmd: 'settings' });
  };
  state.ws.onclose = () => {
    $('conn').textContent = 'нет связи';
    setTimeout(connect, 1500);
  };
  state.ws.onmessage = (e) => {
    let m;
    try { m = JSON.parse(e.data); } catch (err) { if (params.get('trace')) $('conn').textContent += ' BADJSON(' + e.data.length + ')'; return; }
    if (params.get('trace')) $('conn').textContent += ' ' + m.type;   // headless check: which messages arrived
    if (m.type === 'categories') onCategories(m);
    else if (m.type === 'list') onList(m);
    else if (m.type === 'previews') onPreviews(m);
    else if (m.type === 'opened') onOpened(m);
    else if (m.type === 'settings') onSettings(m);
    else if (m.type === 'picked') onPicked(m);
    else if (m.type === 'inspection') onInspection(m);
    else if (m.type === 'notice') notice(m.text);
    else if (m.type === 'variants') onVariants(m);
    else if (m.type === 'shellDone') {
      if (state.model && m.id === state.model.id && m.category === state.model.category && (m.variant || 'intact') === state.variant) {
        insp.group = Math.max(0, insp.data ? insp.data.groups.filter((g) => !g.name.startsWith('скорлупа')).length : 0);
        send({ cmd: 'inspect', category: m.category, id: m.id });
      }
    }
    else if (m.type === 'assembly') Asm.onAssembly(m);
    else if (m.type === 'library') { Asm.onLibrary(m); if (!$('library').hidden) renderLibrary(); }
    else if (m.type === 'built') { Asm.onBuilt(m); notice(m.ok ? 'Собрано: ' + m.output : 'Сборка не удалась'); }
  };
}

// ---- the Open dialog (MESH_EDITOR_SPEC 5)
function showDialog() {
  $('dialog').hidden = false;
  showCategories();
}
function hideDialog() {
  $('dialog').hidden = true;
  stopPolling();
}
function showCategories() {
  state.category = null;
  stopPolling();
  $('dialogTitle').textContent = 'Открыть модель';
  $('backBtn').hidden = true;
  $('categories').hidden = false;
  $('filterRow').hidden = true;
  $('cards').hidden = true;
  renderCategories();
}
function renderCategories() {
  const box = $('categories');
  box.innerHTML = '';
  for (const c of state.categories) {
    const b = document.createElement('button');
    if (!c.count) b.classList.add('emptycat');
    b.innerHTML = `<span>${esc(c.title)}</span><span class="n">${c.count ? c.count + ' ' + plural(c.count, 'модель', 'модели', 'моделей') : 'пока пусто'}</span>`;
    b.onclick = () => openCategory(c.id);
    box.appendChild(b);
  }
  $('dialogFoot').textContent = state.sourceRoot ? `Источник: ${state.sourceRoot}   ·   Результат: ${state.outputRoot}` : '';
}
function openCategory(id) {
  state.category = id;
  const c = state.categories.find((x) => x.id === id);
  $('dialogTitle').textContent = c ? c.title : id;
  $('backBtn').hidden = false;
  $('categories').hidden = true;
  $('filterRow').hidden = false;
  $('cards').hidden = false;
  $('cards').innerHTML = '<div class="muted">загрузка…</div>';
  send({ cmd: 'list', category: id });
}
function onCategories(m) {
  state.categories = m.items;
  state.sourceRoot = m.sourceRoot;
  state.outputRoot = m.outputRoot;
  renderCategories();
  if (params.get('dialog') && $('dialog').hidden) showDialog();
  if (params.get('obj') && params.get('cat')) state.object[params.get('cat')] = params.get('obj');
  if (params.get('cat') && !state.category) openCategory(params.get('cat'));
}
function onList(m) {
  if (m.category !== state.category) return;
  state.cards = m.items;
  renderObjectFilter();
  renderCards();
  if (state.cards.some((c) => !c.preview)) startPolling();
  if (params.get('open') && !state.model) {
    const id = params.get('open');
    if (state.cards.find((c) => c.id === id)) send({ cmd: 'open', category: m.category, id });
  }
}
function onPreviews(m) {
  if (m.category !== state.category) return;
  let changed = false;
  for (const p of m.items) {
    const c = state.cards.find((x) => x.id === p.id);
    if (c && p.preview && p.preview !== c.preview) {
      c.preview = p.preview;
      changed = true;
      const img = document.querySelector(`.card[data-id="${CSS.escape(c.id)}"] .pic`);
      if (img) img.innerHTML = `<img src="${p.preview}" alt="">`;   // the URL carries the render time
    }
  }
  if (!state.cards.some((c) => !c.preview)) stopPolling();
  return changed;
}
function startPolling() {
  stopPolling();
  state.pollTimer = setInterval(() => send({ cmd: 'previews', category: state.category }), 2000);
}
function stopPolling() {
  if (state.pollTimer) clearInterval(state.pollTimer);
  state.pollTimer = null;
}
// the object list (author 2026-11-13): pick a ship / station / hub module - only what is used on it stays
function renderObjectFilter() {
  const sel = $('objectFilter');
  const counts = new Map();
  for (const c of state.cards) for (const u of c.usedBy || []) counts.set(u, (counts.get(u) || 0) + 1);
  const names = [...counts.keys()].sort((a, b) => a.localeCompare(b, 'ru', { numeric: true }));
  sel.hidden = names.length === 0;
  const current = state.object[state.category] || '';
  const unused = state.cards.filter((c) => !(c.usedBy || []).length).length;
  sel.innerHTML = '<option value="">все объекты</option>' +
    names.map((n) => `<option value="${esc(n)}">${esc(n)} — ${counts.get(n)}</option>`).join('') +
    (unused ? `<option value="${UNUSED}">нигде не используется — ${unused}</option>` : '');
  sel.value = names.includes(current) || (current === UNUSED && unused) ? current : '';
  sel.classList.toggle('on', !!sel.value);
}
function renderCards() {
  const q = $('filter').value.trim().toLowerCase();
  const obj = $('objectFilter').hidden ? '' : $('objectFilter').value;
  const box = $('cards');
  box.innerHTML = '';
  const shown = state.cards
    .filter((c) => !obj || (obj === UNUSED ? !(c.usedBy || []).length : (c.usedBy || []).includes(obj)))
    .filter((c) => !q || [c.id, c.title, c.description, ...(c.usedBy || [])].join(' ').toLowerCase().includes(q));
  $('countInfo').textContent = `${shown.length} из ${state.cards.length}`;
  if (!state.cards.length) box.innerHTML = '<div class="muted">В этом разделе пока нет моделей.</div>';
  for (const c of shown) {
    const el = document.createElement('div');
    el.className = 'card';
    el.dataset.id = c.id;
    const used = c.usedBy && c.usedBy.length
      ? 'где: ' + c.usedBy.slice(0, 4).join(', ') + (c.usedBy.length > 4 ? ` и ещё ${c.usedBy.length - 4}` : '')
      : 'нигде не используется';
    el.innerHTML = `
      <div class="pic">${c.preview ? `<img src="${c.preview}" alt="">` : 'снимок готовится…'}</div>
      <div class="body">
        <div class="row"><span class="t">${esc(c.title)}</span><span class="chip ${c.status}">${esc(c.statusText)}</span></div>
        <div class="id">${esc(c.id)} · LOD ${c.lods} · ${c.meshFiles} ${c.gltf ? 'файлов glTF' : 'мешей'} · ${c.sizeMb.toFixed(1)} МБ</div>
        <div class="d">${esc(c.description || '')}</div>
        <div class="u">${esc(used)}</div>
      </div>`;
    el.onclick = () => { state.stack = []; send({ cmd: 'open', category: c.category, id: c.id }); };
    box.appendChild(el);
  }
}

// ---- the opened model
function onOpened(m) {
  state.model = m;
  state.variant = params.get('variant') && !state.variantTried ? params.get('variant') : 'intact';   // headless: &variant=<id>
  state.variantTried = true;
  hideDialog();
  renderVariants();
  $('current').textContent = `${m.title}  ·  ${m.category}/${m.id}`;
  $('empty').hidden = true;
  $('model').hidden = false;
  $('modelTitle').textContent = m.title;
  $('modelMeta').textContent = `${m.id} · ${m.statusText}`;
  $('modelMeta').title = m.folder;
  $('modelUsed').textContent = m.usedBy && m.usedBy.length ? 'где: ' + m.usedBy.join(', ') : 'нигде не используется';
  const nav = $('steps');
  nav.innerHTML = '';
  Asm.reset();
  Asm.close();
  STEPS.forEach((s, i) => {
    const b = document.createElement('button');
    b.textContent = `${i + 1}. ${s}`;
    if (i === 0) b.classList.add('active');
    b.onclick = () => {
      for (const x of nav.children) x.classList.toggle('active', x === b);
      if (i === 0) { Asm.close(); whenViewer((v) => v.showGroup(insp.group)); } else Asm.open(i);
    };
    nav.appendChild(b);
  });
  loadVariant();
  if (state.pendingBack) {   // back from a link: the assembly comes back where it was left
    insp.selected = state.pendingBack.selected;
    insp.group = state.pendingBack.group;
    state.pendingBack = null;
  }
  renderTrail();
  if (!Asm.library) send({ cmd: 'library' });
}
// ---- variants (MESH_EDITOR_SPEC 18): "Целая" and the damaged ones; each has its assembly and its shell with LODs
function renderVariants() {
  const nav = $('variants');
  const vs = state.model.variants && state.model.variants.length ? state.model.variants : [{ id: 'intact', title: 'Целая', inGame: 'shell' }];
  nav.innerHTML = '';
  for (const v of vs) {
    const b = document.createElement('button');
    b.innerHTML = `${esc(v.title)}<span class="tag">${v.inGame === 'assembly+shell' ? 'сборка + скорлупа' : 'скорлупа'}</span>`;
    b.title = v.id === 'intact' ? 'целая модель' : `повреждённый вариант (damaged/${v.id})`;
    if (v.id === state.variant) b.classList.add('active');
    b.onclick = () => { if (v.id !== state.variant) { state.variant = v.id; renderVariants(); loadVariant(); } };
    nav.appendChild(b);
  }
  const add = document.createElement('button');
  add.className = 'add';
  add.textContent = '⟳ Обновить';
  add.title = 'перечитать папку модели: повреждённые варианты — это папки damaged/<вариант>/assembly/LOD<N>; '
    + 'положите файлы туда и нажмите «Обновить»';
  add.onclick = () => send({ cmd: 'refresh', category: state.model.category, id: state.model.id });
  nav.appendChild(add);
}
function loadVariant() {
  const m = state.model;
  insp.data = null;
  insp.selected = null;
  insp.group = 0;
  $('parts').innerHTML = '<tr><td class="muted">загрузка мешей…</td></tr>';
  $('detail').innerHTML = '';
  $('summary').textContent = '';
  Asm.reset();
  send({ cmd: 'inspect', category: m.category, id: m.id });
  // the assembly document comes in the background: the steps 2..6 open at once
  send({ cmd: 'assembly', category: m.category, id: m.id });
}
function onVariants(m) {
  if (!state.model || m.id !== state.model.id || m.category !== state.model.category) return;
  state.model.variants = m.variants;
  if (m.refresh) {   // "Обновить": the files may have changed; a variant whose folder is gone falls back to "Целая"
    if (!m.variants.some((v) => v.id === state.variant)) state.variant = 'intact';
    renderVariants();
    loadVariant();
    return;
  }
  renderVariants();
  if (insp.data) renderSummary();
}

// ---- step 1 "Загрузка и проверка" (MESH_EDITOR_SPEC 6.1): 3D view 2/3, parts table 1/3
const insp = { data: null, selected: null, group: 0, sameModel: false };
const KIND = { volume: 'объём', sheet: 'лист' };
function whenViewer(fn) {
  if (window.MeshViewer) fn(window.MeshViewer);
  else window.addEventListener('viewer-ready', () => fn(window.MeshViewer), { once: true });
}
function partStatus(p) {
  if (p.link) return 'link';
  if (!insp.data.checked) return 'unchecked';
  if (p.result === 'error') return 'error';
  if (p.change === 'changed') return 'changed';
  if (p.change === 'new') return 'new';
  return 'ok';
}
const STATUS_TEXT = { unchecked: 'не проверена', ok: 'годна', error: 'ошибка', changed: 'изменена в источнике', new: 'новая деталь',
  link: 'ссылка на модель из библиотеки' };
function onInspection(m) {
  if (!state.model || m.id !== state.model.id || m.category !== state.model.category) return;
  if ((m.variant || 'intact') !== state.variant) return;   // an answer for the tab left meanwhile
  const same = !!insp.data;
  insp.data = m;
  if (!same && params.get('group')) insp.group = Number(params.get('group'));   // headless check: &group=<index>
  if (insp.group >= m.groups.length) insp.group = 0;
  renderLodBar();
  renderSummary();
  renderParts();
  renderDetail();
  whenViewer(async (v) => {
    v.onPick((key) => selectPart(key, false));
    insp.group = await v.load(m.groups, same, insp.group);
    if (insp.data !== m) return;   // another model / variant was opened while the meshes came
    v.setErrors(m.parts.filter((p) => p.result === 'error' && !p.link).map((p) => p.key));
    v.setLinks(m.parts.filter((p) => p.link).map((p) => p.key));
    v.select(insp.selected, false);
    Anim.setup(m.rig || null);
    renderLodBar();
    renderParts();
    if (params.get('part') && !insp.selected) {
      selectPart(params.get('part'), true);
      const p = m.parts.find((x) => x.key === params.get('part'));
      if (params.get('follow') && p && p.link) openLink(p);   // headless check of the trail
    }
    if (params.get('step') && !insp.stepped) {   // headless check of a step window: &step=<2..6>
      insp.stepped = true;
      const b = $('steps').children[Number(params.get('step')) - 1];
      if (b) b.click();
      if (params.get('explode')) setTimeout(() => { $('explode').value = params.get('explode'); $('explode').oninput(); }, 1500);
    }
  });
}
function renderLodBar() {
  // two sections of the variant (MESH_EDITOR_SPEC 18): the assembly and its shell, each with its LODs
  const bar = $('lodBar');
  bar.innerHTML = '';
  const st = insp.data.shellStatus || 'none';
  const rows = [
    { label: 'Сборка', cls: '', groups: [] },
    { label: { none: 'Скорлупа: нет', built: 'Скорлупа', stale: 'Скорлупа: устарела', manual: 'Скорлупа (ручная)' }[st], cls: st === 'stale' ? 'stale' : '', groups: [] },
  ];
  insp.data.groups.forEach((g, i) => rows[g.name.startsWith('скорлупа') ? 1 : 0].groups.push([g, i]));
  for (const r of rows) {
    const row = document.createElement('div');
    row.className = 'row';
    row.innerHTML = `<span class="row-label ${r.cls}">${r.label}</span>`;
    for (const [g, i] of r.groups) {
      const b = document.createElement('button');
      b.textContent = g.name.replace(/^скорлупа /, '');
      b.title = `${g.name}: ${fmt(g.tris)} треугольников, ${g.parts.length} ${plural(g.parts.length, 'деталь', 'детали', 'деталей')}`;
      if (i === insp.group) b.classList.add('active');
      b.onclick = () => { insp.group = i; whenViewer((v) => v.showGroup(i)); renderLodBar(); renderParts(); };
      row.appendChild(b);
    }
    bar.appendChild(row);
  }
}
function renderSummary() {
  const s = insp.data.summary;
  const parts = [`<b>${s.parts}</b> ${plural(s.parts, 'деталь', 'детали', 'деталей')}`];
  if (!insp.data.checked) parts.push('<span class="muted">ещё не проверялась — нажмите «Проверить»</span>');
  else {
    parts.push(`<span style="color:var(--good)">${s.ok} годны</span>`);
    if (s.error) parts.push(`<span style="color:var(--bad)">${s.error} с ошибками</span>`);
    if (s.changed) parts.push(`<span style="color:var(--warn)">${s.changed} изменены</span>`);
    if (s.new) parts.push(`<span style="color:var(--info)">${s.new} новых</span>`);
    if (s.missing) parts.push(`<span style="color:var(--bad)">${s.missing} пропали</span>`);
  }
  if (s.links) parts.push(`<span style="color:var(--link)">${s.links} ${plural(s.links, 'ссылка', 'ссылки', 'ссылок')} на модули</span>` +
    (s.linkErrors ? ` <span class="muted">(у ${s.linkErrors} ошибки — чинятся в самом модуле)</span>` : ''));
  const cappable = insp.data.parts.filter((p) => !p.link && p.holes > 0 && !p.caps);
  // a damaged variant: its holes are the damage itself - no "close the holes" offer (it would patch the wounds)
  if (state.variant !== 'intact') parts.push('<span class="muted">повреждённый вариант: дыры — это повреждения, не ошибки</span>');
  else if (cappable.length) parts.push(`<button id="capAll" class="small">Закрыть дыры у всех объёмов (${cappable.length})</button>`);
  const fixed = insp.data.parts.filter((p) => p.fixedTotal > 0).length;
  if (fixed) parts.push(`<span class="muted">${fixed} ${plural(fixed, 'починена', 'починены', 'починено')} сами</span>`);
  // what this variant puts into the game (MESH_EDITOR_SPEC 18)
  const inGame = insp.data.inGame || 'shell';
  parts.push(`<span class="ingame">в игру: <label><input type="radio" name="inGame" value="shell" ${inGame === 'shell' ? 'checked' : ''}> только скорлупа</label>` +
    `<label><input type="radio" name="inGame" value="assembly+shell" ${inGame === 'assembly+shell' ? 'checked' : ''}> сборка + скорлупа</label></span>`);
  // the shell (MESH_EDITOR_SPEC 17, 18): what the game and the Blender library use
  {
    const sh = insp.data.shell;
    const st = insp.data.shellStatus || 'none';
    if (st === 'stale') parts.push('<span style="color:var(--warn)">скорлупа устарела: сборка изменилась</span>');
    if (st === 'manual') parts.push('<span class="muted">скорлупа ручная — редактор её не перестраивает</span>');
    if (sh) {
      const s0 = sh.stats[0];
      const cut = s0 && s0.in ? Math.round(100 * (1 - s0.out / s0.in)) : 0;
      parts.push(`<span title="${esc(sh.package || sh.packageReport || '')}\n${esc(sh.blend)}">скорлупа: ${s0 ? `${esc(s0.lod)} ${fmt(s0.out)} из ${fmt(s0.in)} треуг. (−${cut}%)` : ''}, ` +
        `${sh.links} ${plural(sh.links, 'звено', 'звена', 'звеньев')}` +
        `${sh.package ? ', пакет .elmodel' : state.variant !== 'intact' ? '' : sh.packageReport ? ', <b class="bad">пакет не записан</b>' : ''}</span>`);
    }
    parts.push(`<button id="shellOne" class="small">${sh ? 'Перестроить скорлупу' : 'Построить скорлупу'}</button>`);
    if (state.model.category.startsWith('equipment') && state.variant === 'intact')
      parts.push(`<button id="shellAll" class="small" title="все модели раздела; файлы Blender — рядом с пакетами">Скорлупы всех модулей</button>`);
  }
  $('summary').innerHTML = parts.join(' · ');
  if ($('capAll')) $('capAll').onclick = () => setCaps(cappable.map((p) => p.key), true);
  for (const r of document.querySelectorAll('input[name=inGame]'))
    r.onchange = () => send({ cmd: 'setInGame', category: state.model.category, id: state.model.id, inGame: r.value });
  if ($('shellOne')) $('shellOne').onclick = () => { notice('Строю скорлупу…'); send({ cmd: 'shell', category: state.model.category, id: state.model.id }); };
  if ($('shellAll')) $('shellAll').onclick = () => { notice('Строю скорлупы всех модулей раздела…'); send({ cmd: 'shellAll', category: state.model.category }); };
}
function renderParts() {
  const lods = insp.data.groups.filter((g) => !g.name.startsWith('скорлупа')).map((g) => g.name);
  const gname = insp.data.groups[insp.group] ? insp.data.groups[insp.group].name : '';
  let html = `<tr><th></th><th>Деталь</th><th>Вид</th><th class="num">Треуг. ${esc(gname)}</th><th>LOD</th></tr>`;
  for (const p of insp.data.parts) {
    const st = partStatus(p);
    const here = p.lods[gname];
    const chips = lods.map((l) => `<span class="${p.lods[l] ? 'on' : ''}">${esc(l.replace('LOD', ''))}</span>`).join('');
    html += `<tr class="prow ${here ? '' : 'absent'} ${p.key === insp.selected ? 'sel' : ''}" data-key="${esc(p.key)}">
      <td><i class="dot st-${st}" title="${STATUS_TEXT[st]}"></i></td><td>${p.link ? '<span class="lnk" title="ссылка на модуль">⧉</span> ' : ''}${esc(p.key)}${p.fixedTotal ? ' <span class="muted" title="починено само">✓</span>' : ''}</td>
      <td>${KIND[p.kind]}${p.kindSet ? '*' : ''}</td><td class="num">${here ? fmt(here.tris) : '—'}</td><td class="lodchips">${chips}</td></tr>`;
  }
  for (const k of insp.data.missing) html += `<tr class="gone"><td><i class="dot st-error"></i></td><td colspan="4">${esc(k)} — пропала из источника</td></tr>`;
  $('parts').innerHTML = html;
  for (const tr of $('parts').querySelectorAll('tr.prow')) {
    tr.onclick = () => selectPart(tr.dataset.key, true);
    tr.onmouseenter = () => whenViewer((v) => v.hover(tr.dataset.key));
    tr.onmouseleave = () => whenViewer((v) => v.hover(null));
  }
}
function selectPart(key, fit) {
  insp.selected = key;
  whenViewer((v) => v.select(key, fit));
  renderParts();
  renderDetail();
  if (key) {
    const tr = $('parts').querySelector(`tr[data-key="${CSS.escape(key)}"]`);
    if (tr) tr.scrollIntoView({ block: 'nearest' });
  }
}
function renderDetail() {
  const box = $('detail');
  const p = insp.data && insp.data.parts.find((x) => x.key === insp.selected);
  if (!p) {
    box.innerHTML = '<span class="muted">Щёлкните деталь в таблице или на модели.</span>';
    return;
  }
  const st = partStatus(p);
  const f = p.fixed;
  const fixed = [];
  if (f.welded) fixed.push(`спаяно двойных вершин: ${f.welded}`);
  if (f.degenerate) fixed.push(`убрано вырожденных треугольников: ${f.degenerate}`);
  if (f.repeated) fixed.push(`убрано повторных граней: ${f.repeated}`);
  if (f.flipped) fixed.push(`вывернутых целиком кусков развёрнуто наружу: ${f.flipped}`);
  if (f.reoriented) fixed.push(`граней развёрнуто по соседям / наружу: ${fmt(f.reoriented)}`);
  if (f.split) fixed.push(`разведено касаний кусков по ребру: ${f.split}`);
  if (f.capped) fixed.push(`закрыто дыр: ${f.capped}`);
  if (f.membrane) fixed.push(`убрано граней-перегородок между сросшимися объёмами: ${f.membrane}`);
  const size = p.size.map((x) => x.toFixed(2)).join(' × ');
  const lodLine = Object.entries(p.lods).map(([n, l]) => `${n}: ${fmt(l.tris)}`).join(' · ');
  const g = insp.data.groups[insp.group];
  const linkBox = p.link ? `<div class="linkbox">Модуль из библиотеки: <b>${esc(p.link.title)}</b>
      <span class="muted">${esc(p.link.category)}/${esc(p.link.id)}</span>
      <div class="muted">Его сетка правится и проверяется в его карточке — один раз для всех кораблей, где он стоит.</div>
      <button id="openLink" class="primary small">Открыть модуль</button></div>` : '';
  box.innerHTML = `<h3><i class="dot st-${st}"></i> ${esc(p.key)}</h3>${linkBox}
    <div class="muted">${STATUS_TEXT[st]}${p.changeText ? ' — ' + esc(p.changeText) : ''} · ${size} м</div>
    <div class="muted">рендер: ${esc(p.render || '')}</div>
    <div class="muted">треугольники — ${esc(lodLine)}</div>
    <div style="margin-top:6px">Вид: <select id="kindSel">
      <option value="">авто (${KIND[p.kindAuto]})</option><option value="volume">объём — должен быть замкнут</option><option value="sheet">лист — края открыты</option>
    </select></div>
    <div class="btnrow">${p.canCap && !p.link ? `<label><input type="checkbox" id="capSel" ${p.caps ? 'checked' : ''}> закрыть дыры (простые кольца)</label>` : ''}
      <button id="partBlender" class="small">Открыть в Blender</button></div>
    <ul>${p.issues.map((i) => `<li class="${i.sev}">${esc(i.text)}${i.sev === 'error' ? (p.link ? ' — в модуле' : p.holes && !p.caps ? ' — «закрыть дыры» или Blender' : ' — поправить в Blender') : ''}</li>`).join('')}
    ${fixed.map((t) => `<li class="fixed">${esc(t)}</li>`).join('')}
    ${!p.issues.length && !fixed.length ? '<li class="fixed">замечаний нет</li>' : ''}</ul>`;
  $('kindSel').value = p.kindSet ? p.kind : '';
  $('kindSel').onchange = () => send({ cmd: 'setKind', category: state.model.category, id: state.model.id, part: p.key, kind: $('kindSel').value });
  if ($('capSel')) $('capSel').onchange = () => setCaps([p.key], $('capSel').checked);
  if ($('openLink')) $('openLink').onclick = () => openLink(p);
  $('partBlender').onclick = () => p.link
    ? send({ cmd: 'blender', category: p.link.category, id: p.link.id, group: '', part: '' })
    : send({ cmd: 'blender', category: state.model.category, id: state.model.id, group: g ? g.name : '', part: p.key });
}
function setCaps(parts, on) {
  send({ cmd: 'setCaps', category: state.model.category, id: state.model.id, parts, on });
}
// links (SolidWorks-like): open the referenced model, the trail above the steps leads back, any depth
function openLink(p) {
  state.stack.push({ category: state.model.category, id: state.model.id, title: state.model.title, selected: p.key, group: insp.group });
  send({ cmd: 'open', category: p.link.category, id: p.link.id });
}
function goBack(depth) {
  const target = state.stack[depth];
  state.stack = state.stack.slice(0, depth);
  state.pendingBack = target;
  send({ cmd: 'open', category: target.category, id: target.id });
}
function renderTrail() {
  const box = $('trail');
  if (!state.stack.length) { box.hidden = true; box.innerHTML = ''; return; }
  box.hidden = false;
  const last = state.stack[state.stack.length - 1];
  box.innerHTML = `<button id="backModel" class="small">← назад к «${esc(last.title)}»</button> ` +
    state.stack.map((s, i) => `<a href="#" data-depth="${i}">${esc(s.title)}</a>`).join(' › ') + ` › <b>${esc(state.model.title)}</b>`;
  $('backModel').onclick = () => goBack(state.stack.length - 1);
  for (const a of box.querySelectorAll('a')) a.onclick = (e) => { e.preventDefault(); goBack(Number(a.dataset.depth)); };
}
// ---- the test animation player (rig.json, elite.rig.v1): clip, play / pause, the time slider, loop, rest pose
const Anim = {
  rig: null, clip: null, t: 0, playing: false, last: 0,
  setup(rig) {
    const same = this.rig && rig && JSON.stringify(this.rig) === JSON.stringify(rig);
    if (same) { this.apply(); return; }
    this.stop();
    this.rig = rig && rig.joints && rig.joints.length ? rig : null;
    $('animBar').hidden = !this.rig;
    if (!this.rig) { whenViewer((v) => v.setPose(null)); return; }
    const props = this.rig.props || [];
    $('animPropsLabel').hidden = !props.length;
    $('animPropsName').textContent = props.map((p) => p.title || p.id).join(', ');
    const clips = this.rig.clips || [];
    $('animClip').innerHTML = clips.map((c, i) => `<option value="${i}">${esc(c.title || c.id)}</option>`).join('') ||
      '<option value="">нет клипов</option>';
    this.pick(clips.length ? 0 : -1);
  },
  pick(i) {
    this.stop();
    this.clip = i >= 0 ? (this.rig.clips || [])[i] : null;
    if (i >= 0) $('animClip').value = String(i);
    this.t = 0;
    if (this.clip) $('animLoop').checked = this.clip.loop !== false;
    this.apply();
  },
  dur() { return this.clip ? Math.max(0.01, this.clip.duration_s || 1) : 1; },
  // angle of a part at time t: keys [[t, deg], ...], eased between neighbours (smoothstep)
  angles(t) {
    const out = {};
    if (!this.clip) return out;
    for (const [part, keys] of Object.entries(this.clip.keys || {})) {
      if (!keys.length) continue;
      let a = keys[0][1];
      if (t >= keys[keys.length - 1][0]) a = keys[keys.length - 1][1];
      else for (let k = 0; k + 1 < keys.length; k++) {
        const [t0, a0] = keys[k], [t1, a1] = keys[k + 1];
        if (t >= t0 && t <= t1) { const u = t1 > t0 ? (t - t0) / (t1 - t0) : 1; const s = u * u * (3 - 2 * u); a = a0 + (a1 - a0) * s; break; }
      }
      out[part] = a;
    }
    return out;
  },
  apply() {
    const d = this.dur();
    $('animTime').value = String(Math.round((this.t / d) * 1000));
    $('animLabel').textContent = `${this.t.toFixed(1)} / ${d.toFixed(1)} с`;
    $('animPlay').textContent = this.playing ? '⏸' : '▶';
    const angles = this.angles(this.t);
    whenViewer((v) => v.setPose(this.rig, angles, this.extra(this.t)));
  },
  // props shown (the checkbox, or the clip uses them) and the tools held at time t (clip events take / stow)
  extra(t) {
    const props = new Set();
    if ($('animProps').checked) for (const p of this.rig.props || []) props.add(p.id);
    for (const id of (this.clip && this.clip.props) || []) props.add(id);
    const last = new Map();
    for (const e of [...((this.clip && this.clip.events) || [])].sort((a, b) => a.t - b.t)) {
      if (e.t > t) break;
      const tool = e.take || e.stow;
      if (tool) last.set(tool, e);
    }
    const held = [];
    for (const [tool, e] of last) if (e.take) held.push({ tool, hand: e.hand, at: this.angles(e.t) });
    return { props: [...props], held };
  },
  play() {
    if (!this.clip) return;
    if (this.t >= this.dur()) this.t = 0;
    this.playing = true;
    this.last = performance.now();
    const step = (now) => {
      if (!this.playing) return;
      this.t += (now - this.last) / 1000;
      this.last = now;
      if (this.t >= this.dur()) {
        if ($('animLoop').checked) this.t %= this.dur();
        else { this.t = this.dur(); this.playing = false; }
      }
      this.apply();
      if (this.playing) requestAnimationFrame(step);
    };
    requestAnimationFrame(step);
    this.apply();
  },
  stop() { this.playing = false; if (this.rig) $('animPlay').textContent = '▶'; },
};
$('animClip').onchange = () => Anim.pick(Number($('animClip').value));
$('animProps').onchange = () => Anim.apply();
$('animPlay').onclick = () => (Anim.playing ? (Anim.stop(), Anim.apply()) : Anim.play());
$('animTime').oninput = () => { Anim.stop(); Anim.t = (Number($('animTime').value) / 1000) * Anim.dur(); Anim.apply(); };
$('animRest').onclick = () => { Anim.stop(); Anim.t = 0; $('animTime').value = '0'; whenViewer((v) => v.setPose(null)); $('animPlay').textContent = '▶'; };
document.addEventListener('keydown', (e) => {
  if (e.code !== 'Space' || $('animBar').hidden || /INPUT|SELECT|TEXTAREA/.test(document.activeElement.tagName)) return;
  e.preventDefault();
  $('animPlay').click();
});

let noticeTimer = null;
function notice(text) {
  const n = $('notice');
  n.textContent = text;
  n.hidden = false;
  clearTimeout(noticeTimer);
  noticeTimer = setTimeout(() => { n.hidden = true; }, 5000);
}
function runCheck() {
  if (!state.model) return;
  send({ cmd: 'inspect', category: state.model.category, id: state.model.id, commit: true });
}

// ---- settings: the source folder, the result folder, Blender (MESH_EDITOR_SPEC 4)
function showSettings() {
  $('settings').hidden = false;
  send({ cmd: 'settings' });
}
function hideSettings() { $('settings').hidden = true; }
function onSettings(m) {
  state.settings = m;
  $('setSource').value = m.sourceRoot;
  $('setOutput').value = m.outputRoot;
  $('setBlender').value = m.blender;
  $('setFile').textContent = 'файл настроек: ' + m.file;
  $('setSourceWarn').textContent = m.sourceExists ? '' : 'Такой папки нет — разделы будут пустыми.';
  $('setOutputWarn').textContent = m.outputExists ? '' : 'Такой папки пока нет — будет создана при первой сборке.';
  $('setBlenderWarn').textContent = m.blenderExists ? '' : 'Файл не найден — снимки моделей рисоваться не будут.';
  if (params.get('settings')) $('settings').hidden = false;
}
function onPicked(m) {
  if (m.path && $(m.field)) $(m.field).value = m.path;
}
function saveSettings() {
  send({ cmd: 'saveSettings', sourceRoot: $('setSource').value.trim(), outputRoot: $('setOutput').value.trim(),
    blender: $('setBlender').value.trim() });
  hideSettings();
}

// ---- helpers
function esc(s) {
  return String(s ?? '').replace(/[&<>"]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch]));
}
function fmt(n) { return Number(n).toLocaleString('ru-RU'); }
function plural(n, one, few, many) {
  const a = n % 10, b = n % 100;
  if (a === 1 && b !== 11) return one;
  if (a >= 2 && a <= 4 && (b < 12 || b > 14)) return few;
  return many;
}

$('openBtn').onclick = showDialog;
$('closeBtn').onclick = hideDialog;
$('backBtn').onclick = showCategories;
$('filter').oninput = renderCards;
$('objectFilter').onchange = () => {
  state.object[state.category] = $('objectFilter').value;
  $('objectFilter').classList.toggle('on', !!$('objectFilter').value);
  renderCards();
};
$('settingsBtn').onclick = showSettings;
$('checkBtn').onclick = runCheck;
$('fitBtn').onclick = () => whenViewer((v) => v.fitAll());
$('asmFit').onclick = () => whenViewer((v) => v.fitAssembly());
$('explode').oninput = () => whenViewer((v) => v.setExplode(Number($('explode').value)));
for (const el of document.querySelectorAll('[data-ov]')) el.onchange = () => whenViewer((v) => v.setOverlay(el.dataset.ov, el.checked));
$('libClose').onclick = () => { $('library').hidden = true; };
$('libFilter').oninput = () => renderLibrary();
$('libSave').onclick = () => { send({ cmd: 'saveLibrary', lib: Asm.library }); Asm.libDirty = false; notice('Библиотека материалов сохранена'); };
$('blenderBtn').onclick = () => {
  if (!state.model) return;
  const g = insp.data && insp.data.groups[insp.group];
  send({ cmd: 'blender', category: state.model.category, id: state.model.id, group: g ? g.name : '', part: insp.selected || '' });
};
for (const [id, opt] of [['optProblems', 'problems'], ['optBack', 'backfaces'], ['optWire', 'wire'], ['optIsolate', 'isolate']])
  $(id).onchange = () => whenViewer((v) => v.set(opt, $(id).checked));
$('settingsClose').onclick = hideSettings;
$('settingsCancel').onclick = hideSettings;
$('settingsSave').onclick = saveSettings;
for (const b of document.querySelectorAll('[data-pick]'))
  b.onclick = () => send({ cmd: 'pick', field: b.dataset.pick, kind: b.dataset.kind, current: $(b.dataset.pick).value });
document.addEventListener('keydown', (e) => {
  if (e.key !== 'Escape') return;
  if (!$('settings').hidden) hideSettings();
  else if (!$('dialog').hidden) hideDialog();
});
connect();
