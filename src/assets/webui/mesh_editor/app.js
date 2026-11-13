// Elite mesh editor page (docs/MESH_EDITOR_SPEC.md). Talks JSON to the C++ backend over the WebSocket.
// URL parameters for headless checks: ?dialog=1 opens "Открыть", &cat=<category> shows its cards, &open=<id> opens one.
'use strict';

const $ = (id) => document.getElementById(id);
const params = new URLSearchParams(location.search);
const state = { ws: null, categories: [], category: null, cards: [], model: null, pollTimer: null };

// the step windows (MESH_EDITOR_SPEC 6 / 7): to be built one by one, after each is gone through with the author
const PART_STEPS = ['Проверка элементов', 'Состав', 'LOD', 'Материалы', 'Физика и хитвалумы', 'Повреждения',
  'Подключения и анимация', 'Проверка и сборка'];
const ASSEMBLY_STEPS = ['Проверка элементов', 'Гнёзда', 'Корпус', 'LOD корпуса', 'Повреждения корпуса', 'Проверка и сборка'];
const ASSEMBLY_CATEGORIES = new Set(['ships', 'stations', 'hub_modules']);

function send(obj) {
  if (state.ws && state.ws.readyState === WebSocket.OPEN) state.ws.send(JSON.stringify(obj));
}

function connect() {
  state.ws = new WebSocket(`ws://${location.host}`);
  state.ws.onopen = () => {
    $('conn').textContent = 'связь есть';
    send({ cmd: 'categories' });
  };
  state.ws.onclose = () => {
    $('conn').textContent = 'нет связи';
    setTimeout(connect, 1500);
  };
  state.ws.onmessage = (e) => {
    let m;
    try { m = JSON.parse(e.data); } catch { return; }
    if (m.type === 'categories') onCategories(m);
    else if (m.type === 'list') onList(m);
    else if (m.type === 'previews') onPreviews(m);
    else if (m.type === 'opened') onOpened(m);
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
  if (params.get('cat') && !state.category) openCategory(params.get('cat'));
}
function onList(m) {
  if (m.category !== state.category) return;
  state.cards = m.items;
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
    if (c && p.preview && !c.preview) {
      c.preview = p.preview;
      changed = true;
      const img = document.querySelector(`.card[data-id="${CSS.escape(c.id)}"] .pic`);
      if (img) img.innerHTML = `<img src="${p.preview}?t=${Date.now()}" alt="">`;
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
function renderCards() {
  const q = $('filter').value.trim().toLowerCase();
  const box = $('cards');
  box.innerHTML = '';
  const shown = state.cards.filter((c) => !q || [c.id, c.title, c.description, ...(c.usedBy || [])].join(' ').toLowerCase().includes(q));
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
    el.onclick = () => send({ cmd: 'open', category: c.category, id: c.id });
    box.appendChild(el);
  }
}

// ---- the opened model
function onOpened(m) {
  state.model = m;
  hideDialog();
  $('current').textContent = `${m.title}  ·  ${m.category}/${m.id}`;
  $('empty').hidden = true;
  $('model').hidden = false;
  $('modelPreview').src = m.preview || '';
  $('modelTitle').textContent = m.title;
  $('modelMeta').textContent = `${m.id} · ${m.statusText} · ${m.folder}`;
  $('modelDesc').textContent = m.description || '';
  $('modelUsed').textContent = m.usedBy && m.usedBy.length ? 'Используется: ' + m.usedBy.join(', ') : 'Нигде не используется';
  const steps = ASSEMBLY_CATEGORIES.has(m.category) ? ASSEMBLY_STEPS : PART_STEPS;
  const nav = $('steps');
  nav.innerHTML = '';
  steps.forEach((s, i) => {
    const b = document.createElement('button');
    b.textContent = `${i + 1}. ${s}`;
    if (i === 0) b.classList.add('active');
    else { b.classList.add('later'); b.title = 'окно ещё не разобрано с автором'; }
    nav.appendChild(b);
  });
  renderSource(m);
}
function renderSource(m) {
  // until the element check (6.1) is agreed: what the source holds, LOD by LOD
  const body = $('stepBody');
  let html = '<p class="muted">Окно «Проверка элементов» ещё не разобрано. Пока — что лежит в источнике.</p>';
  for (const lod of m.lodList || []) {
    html += `<div class="lod-title">${esc(lod.name)} — ${lod.files.length} ${plural(lod.files.length, 'меш', 'меша', 'мешей')}, ${fmt(lod.vertices)} вершин, ${fmt(lod.faces)} граней</div>`;
    html += '<table><tr><th>Файл</th><th class="num">Вершины</th><th class="num">Грани</th></tr>';
    for (const f of lod.files) html += `<tr><td>${esc(f.name)}</td><td class="num">${fmt(f.vertices)}</td><td class="num">${fmt(f.faces)}</td></tr>`;
    html += '</table>';
  }
  body.innerHTML = html;
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
document.addEventListener('keydown', (e) => { if (e.key === 'Escape' && !$('dialog').hidden) hideDialog(); });
connect();
