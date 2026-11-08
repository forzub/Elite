// Manufacturers page of the ship reference. Data: manufacturers_data.js (built by
// tools/build_manufacturers.py from the canon, world sites and hub modules) + data.js (ships, live).
// Routes: ?            list grouped by political bloc (?q= filters)
//         ?mfr=<id>    manufacturer detail: plant hubs, the civilization's hubs, module unification
"use strict";

const M = window.ELITE_MANUFACTURERS_DATA || { manufacturers: [], polities: {}, blocs: [], blocks: {}, module_types: {}, meta: {} };
const SR = window.ELITE_SHIP_REF_DATA || { ships: [] };
const $ = s => document.querySelector(s);
const esc = v => String(v == null ? "" : v).replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const mfrMap = new Map(M.manufacturers.map(m => [m.id, m]));
const hubIndex = new Map();
for (const p of Object.values(M.polities)) for (const h of p.hubs) hubIndex.set(h.id, h);
const shipsBy = new Map();
for (const s of SR.ships || []) { if (!shipsBy.has(s.manufacturer)) shipsBy.set(s.manufacturer, []); shipsBy.get(s.manufacturer).push(s); }
const plural = (n, one, few, many) => { const a = Math.abs(n) % 100, b = a % 10; return `${Number(n).toLocaleString("ru-RU")} ${a > 10 && a < 20 ? many : b === 1 ? one : b >= 2 && b <= 4 ? few : many}`; };
const fmt = n => n == null ? "" : Number(n).toLocaleString("ru-RU");
const kv = (k, v) => v ? `<div class="kv"><div class="k">${esc(k)}</div><div>${v}</div></div>` : "";
const modRu = t => (M.module_types[t] && M.module_types[t].ru) || t;
const blockRu = b => M.blocks[b] || b;

// ---------- recognizable marks (from ship_naming recognition.mark / livery; others: a conventional sign)
const A = (deg, r) => [50 + r * Math.sin(deg * Math.PI / 180), 50 - r * Math.cos(deg * Math.PI / 180)].map(v => v.toFixed(2)).join(" ");
function star(cx, cy, r) {
  let d = "";
  for (let i = 0; i < 10; i++) { const a = Math.PI / 5 * i - Math.PI / 2, rr = i % 2 ? r * 0.45 : r; d += (i ? "L" : "M") + (cx + rr * Math.cos(a)).toFixed(2) + " " + (cy + rr * Math.sin(a)).toFixed(2); }
  return `<path d="${d}Z" fill="#f2e6a0"/>`;
}
const T = (txt, y, size, fill, extra, x = 50) => `<text x="${x}" y="${y}" text-anchor="middle" font-family="Segoe UI,Arial,sans-serif" font-weight="700" font-size="${size}" fill="${fill}" ${extra || ""}>${esc(txt)}</text>`;
const MARKS = {
  "mfr.cobretty": () => `<rect width="100" height="100" fill="#2a2e33"/><rect y="80" width="100" height="9" fill="#f2c230"/>
    <path d="M50 10 L84 44 L50 78 L16 44Z" fill="#3b4148" stroke="#c9cdd1" stroke-width="2.5"/>${T("K", 57, 36, "#eef0f2")}`,
  "mfr.kern_heavy": () => `<rect width="100" height="100" fill="#e8741c"/>
    <path d="M18 34 Q50 12 82 34" stroke="#141414" stroke-width="8" fill="none" stroke-linecap="round"/>${T("K", 86, 58, "#141414")}`,
  "mfr.hyperion": () => `<rect width="100" height="100" fill="#f4f4f1"/>
    <path d="M${A(44, 30)} A30 30 0 1 1 ${A(16, 30)}" stroke="#111" stroke-width="3.5" fill="none"/>
    <circle cx="40" cy="84" r="3" fill="#d22"/><circle cx="50" cy="84" r="3" fill="#d22"/><circle cx="60" cy="84" r="3" fill="#d22"/>${T("HY", 57, 18, "#111")}`,
  "mfr.ofu": () => `<rect width="100" height="100" fill="#5a6e80"/><circle cx="50" cy="50" r="33" stroke="#e9eff3" stroke-width="4" fill="none"/>
    <path d="M28 42 H66 M58 34 L68 42 L58 50 M28 60 H66 M58 52 L68 60 L58 68" stroke="#e9eff3" stroke-width="4.5" fill="none" stroke-linecap="round" stroke-linejoin="round"/>${T("У", 96, 12, "#e9eff3")}`,
  "mfr.sol_mass_yards": () => `<rect width="100" height="100" fill="#12264a"/><circle cx="50" cy="46" r="13" fill="#f4f6f8"/>
    <ellipse cx="50" cy="46" rx="34" ry="34" stroke="#f2c94c" stroke-width="3" fill="none"/><circle cx="${A(60, 34).split(" ")[0]}" cy="${(46 - 34 * Math.cos(Math.PI / 3)).toFixed(2)}" r="5" fill="#f2c94c"/>
    <circle cx="${A(200, 34).split(" ")[0]}" cy="${(46 - 34 * Math.cos(200 * Math.PI / 180)).toFixed(2)}" r="5" fill="#f2c94c"/>${T("ЛМ", 97, 13, "#f4f6f8")}`,
  "mfr.harmony_yards": () => `<rect width="100" height="100" fill="#c8102e"/><circle cx="50" cy="50" r="34" fill="#ffd200"/>${T("和", 65, 42, "#c8102e")}`,
  "mfr.tianhe_heavy": () => `<rect width="100" height="100" fill="#5c6f5f"/>${[200, 235, 270, 305, 340].map((d, i) => { const r = d * Math.PI / 180; return star(50 + 36 * Math.cos(r), 62 + 36 * Math.sin(r), i === 2 ? 7 : 5.5); }).join("")}${T("天", 82, 40, "#d93a2b")}`,
  "mfr.bharat": () => `<rect width="100" height="100" fill="#fbfbf7"/><rect width="100" height="14" fill="#f39a2c"/><rect y="86" width="100" height="14" fill="#1d8a3e"/>
    <circle cx="50" cy="50" r="27" stroke="#1f2f7a" stroke-width="3" fill="none"/>${Array.from({ length: 24 }, (_, i) => `<line x1="50" y1="50" x2="${A(i * 15, 26).split(" ")[0]}" y2="${A(i * 15, 26).split(" ")[1]}" stroke="${i === 0 ? "#d4a017" : "#1f2f7a"}" stroke-width="${i === 0 ? 3 : 1.4}"/>`).join("")}<circle cx="50" cy="50" r="4" fill="#1f2f7a"/>`,
  "mfr.bastion": () => `<rect width="100" height="100" fill="#62676b"/><path d="M50 12 L82 22 V50 Q82 76 50 90 Q18 76 18 50 V22Z" fill="#7b8186" stroke="#f1f1f1" stroke-width="3"/>
    <path d="M50 14 V88 M19 47 H81" stroke="#f1f1f1" stroke-width="3"/>${T("MJ", 40, 13, "#f1f1f1", "", 34)}`,
  "mfr.stone_docks": () => `<defs><linearGradient id="sdg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#77706a"/><stop offset="1" stop-color="#8b4a26"/></linearGradient></defs>
    <rect width="100" height="100" fill="url(#sdg)"/><circle cx="50" cy="46" r="26" fill="none" stroke="#d8d0c4" stroke-width="3" opacity=".8"/>${T("和", 57, 28, "#d8d0c4", 'opacity=".7"')}
    <path d="M20 78 L80 16" stroke="#2a1a12" stroke-width="9" stroke-linecap="round"/>${T("SR", 95, 13, "#f0e6d8")}`,
  "mfr.agora_modular": () => `<rect width="100" height="100" fill="#f7f9fb"/><rect x="14" y="14" width="72" height="72" fill="none" stroke="#123a6b" stroke-width="5"/>
    <path d="M50 14 V86 M14 50 H86" stroke="#123a6b" stroke-width="7"/><rect x="34" y="38" width="32" height="24" rx="3" fill="#f7f9fb"/>${T("ΑΓ", 57, 17, "#123a6b")}`,
};
const UNCANON_GLYPH = { "mfr.um_core_fab": "0", "mfr.xinzhi": "СЧ", "mfr.firmament_armour": "БТ", "mfr.eurohab": "ЕХ", "mfr.mizan": "ДМ", "mfr.uhuru_bio": "UB", "mfr.terra_habitat": "TH", "mfr.sol_reactor": "SR" };
function hue(s) { let h = 0; for (const c of s) h = (h * 31 + c.codePointAt(0)) % 360; return h; }
function markSvg(m, big) {
  const canon = MARKS[m.id];
  let inner, cls = "mark" + (big ? " big" : "");
  if (canon) inner = canon();
  else {
    cls += " uncanon";
    const g = (m.naming && m.naming.code_prefix) || UNCANON_GLYPH[m.id] || m.name.replace(/[«»"]/g, "").split(/\s+/).map(w => w[0]).join("").slice(0, 2).toUpperCase();
    const h = hue(m.polity);
    inner = `<rect width="100" height="100" fill="hsl(${h},28%,24%)"/><circle cx="50" cy="50" r="36" fill="none" stroke="hsl(${h},45%,62%)" stroke-width="2" stroke-dasharray="5 4"/>${T(g, 50 + (g.length > 1 ? 10 : 13), g.length > 1 ? 28 : 38, `hsl(${h},55%,82%)`)}`;
  }
  const title = m.naming && m.naming.recognition ? `${m.naming.recognition.mark}; ${m.naming.recognition.livery}` : "условный знак: в каноне нет recognition";
  return `<span class="${cls}" title="${esc(title)}"><svg viewBox="0 0 100 100" xmlns="http://www.w3.org/2000/svg">${inner}</svg></span>`;
}

// ---------- helpers
const polity = m => M.polities[m.polity] || { ru: m.polity, hubs: [], unification: [] };
function whereText(h) {
  const p = [];
  if (h.system) p.push(`<b>${esc(h.system)}</b>`); else p.push("глубокий космос / вне системы");
  if (h.body) p.push(esc(h.body));
  if (h.placement) p.push(esc(h.placement));
  return p.join(" · ");
}
function hubBlocks(h) {
  if (!h.blocks || !h.blocks.length) return `<div class="sub">модули не заданы в hub_modules</div>`;
  return h.blocks.map(b => `<div class="blockRow"><div class="bn">${esc(blockRu(b.block))}</div><div>${b.modules.map(x =>
    `<span class="mod" title="${esc((M.module_types[x.type] || {}).function || "")}"><span class="n">${x.count}×</span> ${esc(modRu(x.type))}${x.info ? ` <span class="i">(${esc(x.info)})</span>` : ""}</span>`).join("")}</div></div>`).join("");
}
function hubFacts(h) {
  const f = [];
  if (h.level) f.push(`<span class="chip lvl">${esc(h.level)}</span>`);
  if (h.type_ru) f.push(`<span class="chip">${esc(h.type_ru)}</span>`);
  if (h.population) f.push(`<span class="chip">население ${fmt(h.population)}</span>`);
  if (h.docks) f.push(`<span class="chip">доков ${fmt(h.docks)}</span>`);
  if (h.storage_t) f.push(`<span class="chip">склад ${fmt(h.storage_t)} т</span>`);
  if (h.operator) f.push(`<span class="chip">оператор: ${esc(h.operator)}</span>`);
  return f.join(" ");
}
function shipLinks(m) {
  const list = shipsBy.get(m.id) || [];
  return list.map(s => `<a href="index.html?ship=${encodeURIComponent(s.id)}">${esc(s.name)}</a>`).join(", ");
}
function altNames(m) {
  const n = m.naming || {};
  const parts = [];
  if (n.native_script) parts.push(`<span class="native">${esc(n.native_script)}</span>`);
  if (n.romanization) parts.push(esc(n.romanization));
  if (n.official_name && n.official_name !== m.name) parts.push(esc(n.official_name));
  if (n.display_name_ru && n.display_name_ru !== m.name) parts.push(esc(n.display_name_ru));
  return parts.join(" · ");
}
function polityLine(p) {
  return `${esc(p.ru)}${p.formal_ru && p.formal_ru !== p.ru ? ` <span class="sub">(${esc(p.formal_ru)})</span>` : ""}${p.endonym && p.endonym !== p.ru ? ` · <span title="${esc(p.endonym_lang || "")}">${esc(p.endonym)}</span>${p.endonym_translit ? ` <span class="sub">${esc(p.endonym_translit)}</span>` : ""}` : ""}`;
}
function plantSummary(m, max) {
  const ids = m.plants || [];
  if (!ids.length) return `<span class="missing">площадка не найдена в данных</span> <span class="sub">(${esc(m.plant_ru)} в ${esc(polity(m).ru)})</span>`;
  const shown = ids.slice(0, max).map(id => {
    const h = hubIndex.get(id) || { id, name: id };
    return `<div class="plantLine">${esc(h.name)} <span class="where">${whereText(h)}</span></div>`;
  }).join("");
  return `<div class="sub">${esc(m.plant_ru)}: ${ids.length === 1 ? "одна площадка" : plural(ids.length, "площадка", "площадки", "площадок")}</div>${shown}${ids.length > max ? `<div class="sub">… ещё ${ids.length - max}</div>` : ""}`;
}
function haystack(m) {
  const p = polity(m), n = m.naming || {};
  const hubs = (m.plants || []).map(id => hubIndex.get(id)).filter(Boolean);
  return [m.id, m.name, m.note, m.kind_ru, n.official_name, n.native_script, n.romanization, n.display_name_ru, n.code_prefix, n.legal_heritage,
    p.ru, p.formal_ru, p.endonym, p.endonym_translit, p.bloc_ru, ...(p.systems || []), ...hubs.map(h => h.name + " " + (h.body || "")),
    ...(shipsBy.get(m.id) || []).map(s => s.name)].filter(Boolean).join(" ").toLowerCase();
}

// ---------- list
function renderList(q) {
  const terms = (q || "").toLowerCase().split(/\s+/).filter(Boolean);
  const match = m => { const h = haystack(m); return terms.every(t => h.includes(t)); };
  let html = "", total = 0;
  for (const b of M.blocs) {
    const ms = M.manufacturers.filter(m => polity(m).bloc === b.id && match(m));
    if (!ms.length) continue;
    total += ms.length;
    const pols = [...new Set(ms.map(m => m.polity))].map(id => M.polities[id].ru);
    html += `<section class="bloc"><div class="blocHead"><h2>${esc(b.ru)}</h2><span class="sub">${plural(ms.length, "производитель", "производителя", "производителей")} · ${esc(pols.join(", "))}</span></div><div class="mfrCards">`;
    for (const m of ms) html += mfrCard(m);
    html += `</div></section>`;
  }
  if (!total) html = `<div class="empty">Ничего не найдено по «${esc(q)}».</div>`;
  $("#view").innerHTML = html;
  for (const el of document.querySelectorAll(".mfrCard")) el.addEventListener("click", e => {
    if (e.target.closest("a")) return;
    location.href = "?mfr=" + encodeURIComponent(el.dataset.mfr);
  });
}
function mfrCard(m) {
  const p = polity(m), n = m.naming || {}, r = n.recognition || {};
  const doctrine = [n.index_format && `<div><b>Индекс:</b> ${esc(n.index_format)}</div>`, n.proper_names && `<div><b>Имена:</b> ${esc(n.proper_names)}</div>`,
    n.crew_speech && `<div><b>Речь экипажа:</b> ${esc(n.crew_speech)}</div>`].filter(Boolean).join("");
  return `<div class="card mfrCard" data-mfr="${esc(m.id)}">
    <div class="mfrHead">${markSvg(m)}<div class="grow">
      <div class="mfrName"><a href="?mfr=${encodeURIComponent(m.id)}">${esc(m.name)}</a> ${n.code_prefix ? `<span class="badge">${esc(n.code_prefix)}</span>` : ""}</div>
      <div class="mfrAlt">${altNames(m)}</div>
      ${r.tagline && r.tagline !== "—" ? `<div class="tagline">«${esc(r.tagline)}»</div>` : ""}
    </div></div>
    ${kv("Цивилизация", polityLine(p))}
    ${kv("Тип", esc(m.kind_ru))}
    ${kv("Продукция", `<div class="chips">${m.groups.map(g => `<span class="chip grp" title="${esc(g.id)}">${esc(g.ru)}</span>`).join("")}</div>`)}
    ${kv("Площадка", plantSummary(m, 2))}
    ${kv("Состав", `${plural(p.hub_count || 0, "хаб", "хаба", "хабов")} · ${plural(p.module_count || 0, "модуль", "модуля", "модулей")} · ${plural((p.systems || []).length, "система", "системы", "систем")}`)}
    ${kv("Модели", shipLinks(m))}
    ${kv("Знак", r.mark ? esc(r.mark) + (r.livery ? ` <span class="sub">· ${esc(r.livery)}</span>` : "") : "")}
    ${doctrine ? kv("Доктрина", `<div class="doctrine">${doctrine}</div>`) : ""}
    ${m.note ? kv("Заметка", `<span class="sub">${esc(m.note)}</span>`) : ""}
  </div>`;
}

// ---------- detail
function renderDetail(m) {
  const p = polity(m), n = m.naming || {}, r = n.recognition || {};
  const plants = new Set(m.plants || []);
  const siblings = M.manufacturers.filter(x => x.polity === m.polity && x.id !== m.id);
  let html = `<div class="crumbs"><a href="?">Производители</a> › ${esc(p.bloc_ru)} › <b>${esc(m.name)}</b></div>
  <div class="card"><div class="mfrTop">${markSvg(m, true)}<div class="grow">
    <h2>${esc(m.name)} ${n.code_prefix ? `<span class="badge">${esc(n.code_prefix)}</span>` : ""}</h2>
    <div class="mfrAlt">${altNames(m)}</div>
    ${r.tagline && r.tagline !== "—" ? `<div class="tagline">«${esc(r.tagline)}»</div>` : ""}
    <div class="grid2" style="margin-top:10px"><div>
      ${kv("Цивилизация", polityLine(p))}
      ${kv("Блок", esc(p.bloc_ru))}
      ${kv("Тип", esc(m.kind_ru))}
      ${kv("Продукция", `<div class="chips">${m.groups.map(g => `<span class="chip grp" title="${esc(g.id)}">${esc(g.ru)}</span>`).join("")}</div>`)}
      ${kv("Модели", shipLinks(m))}
      ${kv("Заметка", esc(m.note))}
      ${kv("Юр. наследие", esc(n.legal_heritage))}
      ${kv("Языки", esc(n.languages))}
      ${kv("Другие производители", siblings.map(x => `<a href="?mfr=${encodeURIComponent(x.id)}">${esc(x.name)}</a>`).join(", "))}
    </div><div>
      ${kv("Знак", esc(r.mark) || `<span class="sub">в каноне нет — показан условный знак</span>`)}
      ${kv("Ливрея", esc(r.livery))}
      ${kv("Марка", esc(n.brand))}
      ${kv("Индекс", esc(n.index_format))}
      ${kv("Варианты", esc(n.variants))}
      ${kv("Собственные имена", esc(n.proper_names))}
      ${kv("Военные / гражданские", esc(n.military_vs_civil))}
      ${kv("На корпусе", esc(n.on_hull))}
      ${kv("Речь экипажа", esc(n.crew_speech))}
      ${kv("В русском UI", esc(n.ru_ui))}
    </div></div>
  </div></div></div>`;

  html += `<div class="secTitle"><h2>Площадки производителя</h2><span class="sub">${plants.size ? " · " + plural(plants.size, "площадка", "площадки", "площадок") : ""}</span></div>`;
  if (!plants.size) html += `<div class="note warnbox">площадка не найдена в данных: у цивилизации «${esc(p.ru)}» нет площадки типа ${esc(m.plant_site_type)}.</div>`;
  for (const id of plants) {
    const h = hubIndex.get(id) || { id, name: id };
    html += `<div class="hubCard plant"><div class="hubTitle">${esc(h.name)}</div><div class="hubWhere">${whereText(h)}</div>
      <div class="row" style="margin:0 0 8px">${hubFacts(h)}</div>${h.purpose ? `<div class="purpose">${esc(h.purpose)}</div>` : ""}${(m.plant_reasons || {})[id] ? `<div class="purpose sub">Почему здесь: ${esc(m.plant_reasons[id])}</div>` : ""}${hubBlocks(h)}</div>`;
  }

  // the civilization's hubs grouped by system
  const bySys = new Map();
  for (const h of p.hubs) { const k = h.system || "вне систем"; if (!bySys.has(k)) bySys.set(k, []); bySys.get(k).push(h); }
  html += `<div class="secTitle"><h2>Состав цивилизации «${esc(p.ru)}»</h2><span class="sub">${plural(p.hub_count, "хаб", "хаба", "хабов")} · ${plural(p.module_count, "модуль", "модуля", "модулей")} · ${plural(bySys.size, "система", "системы", "систем")}${p.sites_without_modules ? ` · ещё ${p.sites_without_modules} площадок без модулей в hub_modules` : ""}</span>
    <span class="spacer"></span><span class="toolbar"><button class="btn mini" id="openAll">развернуть все</button><button class="btn mini" id="closeAll">свернуть</button></span></div>`;
  for (const [sys, hubs] of bySys) {
    html += `<div class="sysGroup">${esc(sys)} <span class="sub">· ${plural(hubs.length, "хаб", "хаба", "хабов")}</span></div>`;
    for (const h of hubs) {
      html += `<details class="hub${plants.has(h.id) ? " plant" : ""}" data-hub="${esc(h.id)}"><summary><span class="nm">${esc(h.name)}</span>${plants.has(h.id) ? `<span class="badge">площадка</span>` : ""}
        <span class="chip lvl">${esc(h.level || "")}</span><span class="chip">${esc(h.type_ru || h.type || "")}</span>
        <span class="where">${[h.body, h.placement].filter(Boolean).map(esc).join(" · ")}</span><span class="cnt">${h.modules_total} мод.</span></summary>
        <div class="body"><div class="row" style="margin:0 0 6px">${hubFacts(h)}</div>${h.purpose ? `<div class="purpose">${esc(h.purpose)}</div>` : ""}${hubBlocks(h)}</div></details>`;
    }
  }

  // unification
  const inPlant = new Set();
  for (const id of plants) { const h = hubIndex.get(id); if (h) for (const b of h.blocks) for (const x of b.modules) inPlant.add(x.type); }
  html += `<div class="secTitle"><h2>Унификация модулей</h2><span class="sub">какие типы модулей собирают хабы цивилизации «${esc(p.ru)}» — общий стандарт и взаимозаменяемость</span></div>
  <div class="card covTable"><table class="uni"><thead><tr><th>Модуль</th><th class="num">Всего</th><th class="num">Хабов</th><th>Доля хабов</th><th>На площадке</th><th>Назначение</th></tr></thead><tbody>`;
  let lastBlock = null;
  for (const u of p.unification) {
    if (u.block !== lastBlock) { lastBlock = u.block; html += `<tr class="blk"><td colspan="6">${esc(blockRu(u.block))}</td></tr>`; }
    const share = p.hub_count ? u.hubs / p.hub_count : 0;
    html += `<tr><td>${esc(u.ru)} <span class="sub">${esc(u.type)}</span></td><td class="num">${fmt(u.count)}</td><td class="num">${u.hubs} / ${p.hub_count}</td>
      <td><div class="bar${share >= 0.999 ? " full" : ""}"><i style="width:${(share * 100).toFixed(1)}%"></i></div></td>
      <td>${inPlant.has(u.type) ? `<span class="tick">✓</span>` : ""}</td><td class="sub">${esc((M.module_types[u.type] || {}).function || "")}</td></tr>`;
  }
  html += `</tbody></table></div>`;
  $("#view").innerHTML = html;
  document.title = m.name + " — производители Elite 3126";
  $("#openAll").addEventListener("click", () => { for (const d of document.querySelectorAll("details.hub")) d.open = true; });
  $("#closeAll").addEventListener("click", () => { for (const d of document.querySelectorAll("details.hub")) d.open = false; });
  const openKey = "mfrOpen:" + m.id;
  let opened = [];
  try { opened = JSON.parse(sessionStorage.getItem(openKey) || "[]"); } catch { /* */ }
  for (const d of document.querySelectorAll("details.hub")) {
    if (opened.includes(d.dataset.hub)) d.open = true;
    d.addEventListener("toggle", () => {
      const ids = [...document.querySelectorAll("details.hub[open]")].map(x => x.dataset.hub);
      try { sessionStorage.setItem(openKey, JSON.stringify(ids)); } catch { /* */ }
    });
  }
}

// ---------- routing, search, scroll
const scrollKey = () => "mfrScroll:" + location.search;
function saveScroll() { try { sessionStorage.setItem(scrollKey(), String(window.scrollY)); } catch { /* private mode */ } }
function restoreScroll() {
  let y = 0;
  try { y = Number(sessionStorage.getItem(scrollKey()) || 0); } catch { /* */ }
  requestAnimationFrame(() => window.scrollTo(0, y));
}
if ("scrollRestoration" in history) history.scrollRestoration = "manual";
let scrollT = 0;
window.addEventListener("scroll", () => { clearTimeout(scrollT); scrollT = setTimeout(saveScroll, 120); }, { passive: true });
window.addEventListener("beforeunload", saveScroll);

function render() {
  const q = new URLSearchParams(location.search);
  const id = q.get("mfr");
  const m = id && mfrMap.get(id);
  if (m) renderDetail(m);
  else { document.title = "Elite 3126 — производители"; renderList(q.get("q") || ""); }
  restoreScroll();
}
const c = M.meta.counts || {};
$("#refMeta").textContent = `${c.manufacturers || 0} производителей · ${c.polities || 0} цивилизаций · ${c.hubs || 0} хабов · ${M.meta.status || ""}`;
const search = $("#search");
search.value = new URLSearchParams(location.search).get("q") || "";
search.addEventListener("input", () => {
  saveScroll();
  const v = search.value.trim();
  history.replaceState(null, "", v ? "?q=" + encodeURIComponent(v) : location.pathname);
  renderList(v);
});
window.addEventListener("popstate", render);
render();
