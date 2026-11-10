// Elite 3126 ship reference: catalog (bloc -> manufacturer -> role -> cards), ship view (brief +
// functional A-frame schema with module cards), module view (description, characteristics,
// geometry, image prompt, sketches with notes). One page, the view lives in the URL
// (?ship=ID, ?ship=ID&part=ID, ?view=coverage) so reload and back / forward keep it; the scroll
// position is kept per view across reloads.
"use strict";
const D = window.ELITE_SHIP_REF_DATA;
const $ = (s, r = document) => r.querySelector(s);
const esc = s => String(s ?? "").replace(/[&<>"']/g, c => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", "\"": "&quot;", "'": "&#39;" }[c]));
const fmt = n => typeof n === "number" ? new Intl.NumberFormat("ru-RU", { maximumFractionDigits: 3 }).format(n) : String(n ?? "—");
const partMap = new Map(D.parts.map(p => [p.id, p]));
const shipMap = new Map(D.ships.map(s => [s.id, s]));
const laserMap = new Map(D.weapons.lasers.map(x => [x.id, x]));
const missileMap = new Map(D.weapons.missiles.map(x => [x.id, x]));
const PROP = "PROPOSAL (reference)", isProp = x => !!x && x.status === PROP;
const kv = (k, v) => '<div class="kv"><div class="k">' + esc(k) + '</div><div>' + v + '</div></div>';

// ---------------------------------------------------------------- dictionaries
const CLASS_RU = {
  cobrett_i_n1: "лёгкий быстрый фрахтовик", ocs4_lastochka: "орбитальный челнок", surface_lifter: "подъёмник орбита–поверхность",
  medium_freighter: "средний грузовоз", jump_train: "прыжковый поезд", jump_tanker: "прыжковый танкер", gas_tanker: "газовый танкер",
  ore_tug: "рудный буксир", service_tender: "сервисный тендер", patrol_cutter: "патрульный катер", interceptor: "перехватчик",
  escort_frigate: "эскортный фрегат", line_combatant: "линейный корабль", recon_picket: "разведчик-дозорный", fleet_tender: "флотский тендер",
  impact_defence_tug: "буксир противоастероидной защиты", liner: "лайнер", colonist_transport: "транспорт переселенцев",
  light_jump_freighter: "лёгкий прыжковый грузовоз", raider: "рейдер", miner: "добытчик", small_craft: "малый аппарат",
};
// category: [russian name, what it does (ru), english term for the image prompt, english function]
const CAT = {
  drive: ["Главный двигатель", "создаёт основную тягу корабля полем, без рабочего тела; работает от пропульсивных реакторов", "main field-coupling drive unit", "closed field-coupling engine: solid round cooling pad on the outer end, field coil ring behind it, flange to the reactor; no nozzle, no exhaust, no propellant"],
  rcs: ["Маневровый блок (RCS)", "ориентация и точное маневрирование: пары полевых сопел по осям, без рабочего тела", "field-coupled reaction control cluster", "attitude control quad of small field-coupled emitters with heat-radiating faces, no propellant"],
  reactor: ["Пропульсивный реактор", "питает главные двигатели; отводит тепло через охлаждающий пятак", "propulsion fusion reactor module", "compact reactor block with a flat cooling pad face, armoured shell and power couplings"],
  service_reactor: ["Служебный реактор (ВСУ)", "питает бортовые системы, сенсоры, оружие и жизнеобеспечение", "auxiliary power unit reactor", "small service reactor with power bus connectors"],
  radiator: ["Радиатор", "сбрасывает служебное тепло излучением в космос", "deployable heat radiator panel", "thin deployable radiator panels on a hinged boom with coolant manifolds"],
  coolant_loop: ["Контур охлаждения", "переносит тепло от агрегатов к радиаторам и пятакам", "coolant loop pump module", "pump and heat-exchanger unit with insulated pipework"],
  jump_core: ["Ядро прыжка", "запечатанный стратегический модуль межзвёздного прыжка", "sealed jump-drive core", "sealed armoured jump core canister with field emitters and warning markings"],
  jump_field_emitter: ["Излучатель поля прыжка", "формирует поле ядра вокруг всего корпуса; ставится парами на законцовки крыльев / оконечности корпуса", "jump-field emitter pod", "wingtip field emitter pod on a short pylon"],
  tank: ["Бак", "запас топливных таблеток / заряда прыжка / расходников", "fuel / charge tank", "cylindrical tank with mounting saddles"],
  cryo_tank: ["Криобак", "хранение криогенных жидкостей", "cryogenic tank", "insulated cryogenic dewar tank"],
  computer: ["Бортовой компьютер", "навигация, управление полётом, автоматика", "flight computer rack", "rugged avionics computer rack with cooling ducts"],
  radar: ["Радар", "обнаружение и сопровождение целей", "radar sensor array", "phased-array radar panel on a sensor mount"],
  sensor_mast: ["Сенсорная мачта", "пассивные и активные сенсоры дальнего обзора", "sensor mast", "retractable sensor mast with optical and radar apertures"],
  ecm: ["РЭБ", "радиоэлектронное подавление", "electronic countermeasures pod", "ECM jammer pod with antenna arrays"],
  comm: ["Связь", "дальняя связь и передача данных", "communications array", "high-gain antenna dish and comm transceiver"],
  transponder: ["Транспондер", "опознавание корабля в зонах контроля", "transponder unit", "small transponder box with antenna stub"],
  life_support: ["Жизнеобеспечение", "воздух, вода, температура для экипажа", "life support module", "life support unit with air scrubbers, water recycler and tanks"],
  passenger_module: ["Пассажирская секция", "размещение пассажиров", "passenger habitat module", "pressurised passenger section with seats, windows and hatches"],
  escape_capsule: ["Спасательная капсула", "кабина пилота, отстреливается при критическом повреждении и уходит к ближайшей обитаемой зоне с сигналом SOS", "cockpit escape capsule", "armoured cockpit capsule that detaches from the hull, with its own small engine and SOS beacon"],
  lifeboat: ["Спасательный бот", "эвакуация экипажа и пассажиров", "lifeboat", "small enclosed lifeboat with docking collar"],
  tow_rig: ["Буксирное устройство", "захват и буксировка объектов", "tow rig", "towing clamp and winch assembly"],
  docking_port: ["Стыковочный узел", "стыковка с кораблями и станциями", "docking port", "androgynous docking collar with latches"],
  landing_gear: ["Посадочное шасси", "посадка на поверхность", "landing gear", "heavy retractable landing legs with footpads"],
  heat_shield: ["Теплозащитный экран", "защита при входе в атмосферу", "heat shield", "ablative heat shield panel"],
  armour: ["Броня", "защита корпуса и агрегатов", "armour panel", "layered armour plate module"],
  weapon: ["Вооружение", "лазерная, ракетная или кинетическая установка", "weapon mount", "turret weapon mount"],
  hangar: ["Ангар", "размещение малых аппаратов", "hangar bay", "hangar bay module with doors and craft cradles"],
  boat_bay: ["Отсек катера", "стоянка и пуск бортового катера", "boat bay", "small craft bay with launch rails"],
  drone_bay: ["Отсек дронов", "стоянка и пуск дронов", "drone bay", "drone launch bay with racks"],
  drone: ["Дрон", "ремонтный, разведывательный или ударный (камикадзе) дрон корабля", "drone", "ship-borne drone"],
  train_module: ["Модуль прыжкового поезда", "сцепляемый грузовой модуль поезда", "jump-train cargo module", "coupled cargo module with train couplers at both ends"],
  container_rack: ["Контейнерная рама", "перевозка стандартных контейнеров C20", "container rack", "open container rack frame holding standard cargo containers"],
  bulk_hold: ["Насыпной трюм", "перевозка руды и сыпучих грузов", "bulk hold", "bulk cargo hold with hoppers"],
  workshop: ["Мастерская", "ремонт и обслуживание в рейсе", "workshop module", "onboard repair workshop module"],
  medbay: ["Медотсек", "медицинская помощь", "medical bay module", "medical bay module"],
  mining_rig: ["Горное оборудование", "добыча на астероидах", "mining rig", "asteroid mining rig with cutter head and conveyor"],
};
const catRu = c => (CAT[c] || [c])[0];
const FIELD_RU = {
  model: "Модель", mass_t: "Масса, т", price_cr: "Цена, кр", seats: "Мест", overhaul_h: "Ресурс до капремонта, ч", life_h: "Полный ресурс, ч",
  power_mw: "Мощность, МВт", mwe: "Электрическая мощность, МВт(э)", eff: "КПД", capacity_mw: "Тепловая ёмкость, МВт", pump_mw: "Мощность насоса, МВт",
  thrust_mn: "Тяга, МН", input_mw: "Потребляемая мощность, МВт", gimbal_deg: "Отклонение вектора тяги полем, °", startup_s: "Запуск, с",
  range_ls: "Дальность, св. с", range_km: "Дальность, км", rating_t: "Номинал ядра прыжка, т", sealed: "Запечатан", grade: "Класс",
  kind: "Тип", cells: "Ячеек", persons: "Человек", days: "Суток", ls_persons: "ЖО: человек", ls_days: "ЖО: суток", survival_seats: "Спасательных мест",
  payload_t: "Полезная нагрузка, т", c20_slots: "Мест C20", capacity_t: "Ёмкость, т", tow_t: "Буксировка, т", area_m2: "Площадь, м²",
  temp_k: "Температура, K", eps: "Излучательная способность ε", material: "Материал", fluid: "Теплоноситель", drones: "Дронов", beds: "Коек",
  cold_sleep: "Мест анабиоза", throughput_t_h: "Производительность, т/ч", craft_slots: "Мест для аппаратов", craft: "Аппарат", craft_ship: "Аппарат (корабль)",
  existing: "Уже в игре", runtime_backend: "Рантайм", runtime_preset: "Пресет рантайма", legacy_id: "Прежний id", status: "Статус",
};
// english style of a bloc for the image prompt
const BLOC_STYLE_EN = {
  sol_industrial: "Sol industrial school: mass-produced, modular, readable functional volumes, service hatches, standard mounting zones, minimal decorative cladding",
  cognium_pool: "Cognium school: expensive high-tech modularity, compact integrated shapes, clean surfaces, large sensor apertures",
  heavenly_pact: "Heavenly Pact school: disciplined state shipyard engineering, layered redundancy, crisp panel lines",
  southern_circle: "Southern Circle school: rugged, cheap to repair, robust oversized fittings, field-serviceable",
  citadel_compact: "Citadel school: military, armoured, duplicated critical systems, sealed modules",
  free_belts: "Free Belts school: improvised belt engineering, salvaged and reworked parts, asteroid-rock structures",
  lalande_open_market: "Lalande open-market school: commercial catalogue modules, bright commercial liveries, bolt-on interfaces",
  auxiliary_craft: "auxiliary craft: small, simple, rugged utility design",
};

// ---------------------------------------------------------------- catalog structure
const BLOC_ORDER = [...(D.coverage?.blocs || []).map(b => b.id)];
for (const s of D.ships) if (!BLOC_ORDER.includes(s.cooperation_group)) BLOC_ORDER.push(s.cooperation_group);
const blocShort = Object.fromEntries((D.coverage?.blocs || []).map(b => [b.id, b.ru]));
const blocTitle = g => blocShort[g] ? blocShort[g] : (g === "auxiliary_craft" ? "Вспомогательные аппараты" : (D.context.cooperation_groups[g]?.ru || g));
const blocLong = g => D.context.design_languages.groups[g]?.title || D.context.cooperation_groups[g]?.ru || "";
const mfrName = m => D.context.ship_integrators.find(x => x.id === m)?.name || D.context.design_languages.manufacturers[m]?.title || m;
const ROLES = D.coverage?.roles || [];
const roleOf = (() => {
  const map = {};
  for (const r of ROLES) {
    const cells = r.all ? [r.all] : Object.values(r.cells || {});
    for (const c of cells) for (const id of [...(c?.existing || []), ...(c?.added || [])]) map[id] ??= r.id;
  }
  return id => map[id] || "other";
})();
const roleRu = id => ROLES.find(r => r.id === id)?.ru || "Прочее";
const roleIndex = id => { const i = ROLES.findIndex(r => r.id === id); return i < 0 ? 99 : i; };
// the ship model code (canon passport field model_code: the number before "A" in its group codes)
const shipCode = s => String(s.model_code ?? (s.functional_groups[0]?.code.split("A")[0] || "?"));
// every name form of a ship (canon ship_naming.json): search finds old and new names alike
const nameForms = s => [s.name, s.model_index, s.official_name, s.native_script, s.romanization, s.short_name, s.short_name_ru, s.legacy_name].filter(Boolean);
const doctrine = m => (D.naming_doctrines || {})[m] || null;
// the official / native line under the Russian name (only when it says something the Russian form does not)
function officialLine(s) {
  const parts = [];
  if (s.native_script) parts.push(s.native_script);
  if (s.official_name && s.official_name !== s.native_script && s.official_name !== s.name) parts.push(s.official_name);
  if (s.romanization && !parts.some(p => p.includes(s.romanization))) parts.push(s.romanization);
  return parts;
}
const sectionCode = (s, g) => g.code;

// all module keys of a ship (unique parts + the hull) and how many are modelled
function shipModules(s) {
  const ids = new Set();
  for (const g of s.functional_groups) for (const it of g.items) ids.add(it.part_id);
  return ids;
}
function readiness(s) {
  const ids = shipModules(s);
  let ready = 0;
  for (const id of ids) if (Store.isReady(id)) ready++;
  return { ready: ready + (Store.isReady("hull:" + s.id) ? 1 : 0), total: ids.size + 1 };
}

// ---------------------------------------------------------------- pictures
function shipSilhouette(s, label = true) {
  const e = s.frame?.envelope_m || [10, 5, 20], w = e[0] || 1, l = e[2] || e[1] || 1;
  const k = Math.min(150 / l, 70 / w), L = l * k, W = w * k, cx = 120, cy = 60;
  const lay = s.flight?.layout || "I";
  let body;
  if (lay === "X") body = `<path d="M${cx - L / 2} ${cy} L${cx + L / 2} ${cy - W / 6} L${cx + L / 2} ${cy + W / 6} Z"/><path d="M${cx - L / 4} ${cy - W / 2} L${cx + L / 3} ${cy} L${cx - L / 4} ${cy + W / 2}"/>`;
  else if (lay === "Y") body = `<path d="M${cx - L / 2} ${cy} L${cx + L / 2} ${cy - W / 2} L${cx + L / 3} ${cy} L${cx + L / 2} ${cy + W / 2} Z"/>`;
  else body = `<path d="M${cx - L / 2} ${cy} L${cx + L / 2} ${cy - W / 2} L${cx + L / 2} ${cy + W / 2} Z"/>`;
  return `<svg class="silhouette" viewBox="0 0 240 120" xmlns="http://www.w3.org/2000/svg"><g fill="#1a2c3c" stroke="#4f7d9a" stroke-width="1.5">${body}</g>${label ? `<text x="10" y="112" fill="#56738a" font-size="10" font-family="Consolas,monospace">${esc(e.map(fmt).join(" × "))} м</text>` : ""}</svg>`;
}
// fill <img data-blob> placeholders once the stored blobs are read
async function hydrateImages(root = document) {
  for (const img of root.querySelectorAll("img[data-blob]")) {
    const u = await Store.url(img.dataset.blob);
    if (u) { img.src = u; img.closest(".pic")?.classList.add("has"); }
  }
}
function shipPic(s, cls = "") {
  const im = Store.shipImage(s.id);
  return `<div class="pic ${cls}">${im ? `<img data-blob="${esc(im.id)}" alt="">` : ""}${shipSilhouette(s)}</div>`;
}
function sketchBox(part) {
  const g = part.geometry || {}, e = g.envelope_m || [1, 1, 1], w = e[0] || 1, h = e[1] || 1, l = e[2] || 1;
  const max = Math.max(w, h, l, .01), sx = 190 * w / max, sy = 130 * h / max, sl = 110 * l / max, x = 95, y = 70, dx = sl * .38, dy = -sl * .24;
  let extra = "";
  if (part.cat === "drive") extra = '<ellipse cx="' + (x + sx) + '" cy="' + (y + sy / 2) + '" rx="12" ry="' + Math.max(10, sy * .34) + '" fill="none" stroke="#73c8ff" stroke-width="2"/>';
  if (part.cat === "reactor" && g.cooling_pad_side_m) extra += '<rect x="24" y="236" width="170" height="18" fill="none" stroke="#9be28f"/><text x="109" y="274" text-anchor="middle" fill="#9be28f" font-size="12">охлаждающий пятак ' + esc(fmt(g.cooling_pad_side_m)) + ' × ' + esc(fmt(g.cooling_pad_side_m)) + ' м</text>';
  if (part.cat === "radiator") extra += '<line x1="25" y1="225" x2="210" y2="225" stroke="#9be28f" stroke-width="8"/>';
  return '<svg class="sketch" viewBox="0 0 420 310" xmlns="http://www.w3.org/2000/svg"><g stroke="#5f8eaa" fill="#142330" stroke-width="1.5"><rect x="' + x + '" y="' + y + '" width="' + sx + '" height="' + sy + '"/><path d="M ' + x + ' ' + y + ' l ' + dx + ' ' + dy + ' h ' + sx + ' l ' + (-dx) + ' ' + (-dy) + ' M ' + (x + sx) + ' ' + y + ' l ' + dx + ' ' + dy + ' v ' + sy + ' l ' + (-dx) + ' ' + (-dy) + ' M ' + x + ' ' + (y + sy) + ' l ' + dx + ' ' + dy + ' h ' + sx + '"/></g>' + extra + '<g fill="#c7dbe8" font-size="12" font-family="Consolas,monospace"><text x="18" y="24">' + esc(part.name) + '</text><text x="18" y="44">Ш ' + fmt(w) + ' м   В ' + fmt(h) + ' м   Д ' + fmt(l) + ' м</text><text x="18" y="294">зона обслуживания: ' + esc((g.service_clearance_m || []).map(fmt).join(" × ")) + ' м</text></g></svg>';
}
function partThumb(p) {
  const sk = Store.sketches(p.id);
  if (sk.length) return `<div class="pic thumb"><img data-blob="${esc(sk[sk.length - 1].id)}" alt="">${sketchBox(p)}</div>`;
  return `<div class="pic thumb">${sketchBox(p)}</div>`;
}


// ---------------------------------------------------------------- technical manual (fault catalog)
// tools/maintenance/build_fault_catalog.py -> fault_data.js: failure modes per equipment class, a cause graph
// through the ship's buses (docs/MAINTENANCE_LAYER_DESIGN.md)
const FD = window.ELITE_FAULT_DATA || null;
const faultClassOf = (() => { const m = new Map(); if (FD) for (const c of FD.catalog.classes) for (const p of c.applies_to) m.set(p, c); return m; })();
const faultModeOf = (() => { const m = new Map(); if (FD) for (const c of FD.catalog.classes) for (const x of c.modes) m.set(x.code, { mode: x, cls: c }); return m; })();
const STAGE_CLASS = { C: "st-c", W: "st-w", F: "st-f" };
const CAUSE_RU = { wear: "износ", thermal: "перегрев", impact: "удар", laser: "лазер", radiation: "радиация", internal: "внутренний дефект" };
const KIND_RU = { bite: "BITE", load_test: "тест под нагрузкой", drone_inspect: "осмотр дроном", cross_check: "перекрёстная проверка", swap: "переключение", bench: "анализ" };
function codeLink(ref) {
  const code = ref.split("/").pop(), bus = ref.startsWith("bus:") ? ref.slice(4).split("/")[0] : null;
  const hit = faultModeOf.get(code);
  const label = esc(code) + (hit ? " " + esc(hit.mode.name_ru) : "");
  const pid = hit ? hit.cls.applies_to[0] : null;
  return (bus ? `<span class="bus">${esc(FD.catalog.buses[bus]?.ru || bus)}</span> → ` : "") +
    (pid ? `<a href="#tm-${esc(code)}" data-tm="${esc(code)}" data-tm-part="${esc(pid)}">${label}</a>` : label);
}
function effectText(e) {
  const parts = [];
  const f = { func: "функция", sec: "вторичная функция", power: "потребление", heat: "тепло", instab: "нестабильность" };
  for (const [k, v] of Object.entries(e || {})) parts.push(`${f[k] || k} ${v > 0 ? "+" : ""}${v} %`);
  return parts.join(", ") || "—";
}
function stepNext(n) {
  if (n == null) return "";
  if (typeof n === "number") return `шаг ${n}`;
  if (n.startsWith("=")) return `неисправность ${codeLink(n.slice(1))}`;
  if (n.startsWith("->bus:")) { const b = n.slice(6); return `искать выше по шине «${esc(FD.catalog.buses[b]?.ru || b)}»`; }
  return esc(n);
}
function manualHTML(p) {
  const c = faultClassOf.get(p.id);
  if (!c) return "";
  const L = FD.catalog.repair_levels, S = FD.catalog.signals;
  let h = `<div class="card tm"><h3>Техническое руководство · ${esc(c.ru)}</h3>
    <div class="sub">Класс оборудования <code>${esc(c.id)}</code> — общий для: ${c.applies_to.map(id => esc(partMap.get(id)?.name || id)).join(", ")}.
    Детали (SRU): ${Object.entries(c.srus).map(([k, v]) => `${esc(v)}${c.sru_class && c.sru_class[k] ? ` <code>${esc(c.sru_class[k])}</code>` : ""}`).join(", ")}.</div>
    ${c.part_notes && c.part_notes[p.id] ? `<div class="note warnbox">Эта модель: ${esc(c.part_notes[p.id])}</div>` : ""}`;
  for (const m of c.modes) {
    h += `<details class="tmMode" id="tm-${esc(m.code)}"><summary><code>${esc(m.code)}</code> ${esc(m.name_ru)}
      ${m.false_alarm ? '<span class="badge">ложная тревога</span>' : ""}${m.intermittent ? '<span class="badge">перемежающийся</span>' : ""}
      <span class="sub">· ${esc(c.srus[m.sru] || m.sru)}</span></summary>
      <div class="tmBody">
      <div class="tmStages">${["C", "W", "F"].filter(s => m.symptoms[s] || m.effect[s]).map(s => `<div class="${STAGE_CLASS[s]}"><b>${esc(m.code)}-${s}</b> ${esc(m.symptoms[s] || "")}<div class="sub">в игре: ${effectText(m.effect[s])}</div></div>`).join("")}</div>
      ${kv("Сигналы BITE", m.signals.map(x => esc(S[x] || x)).join(", "))}
      ${kv("Причины", m.causes.map(c => esc(CAUSE_RU[c] || c)).join(", "))}
      ${m.intermittent ? kv("Проявляется", esc(m.intermittent)) : ""}
      ${m.upstream.length ? kv("Вызывается", m.upstream.map(u => codeLink(u.ref) + ` <span class="sub">×${u.x}</span>`).join("<br>")) : ""}
      ${m.downstream.length ? kv("Если не устранить", m.downstream.map(d => codeLink(d.ref) + ` <span class="sub">через ~${d.after_h} ч${d.when ? ", " + esc(d.when) : ""}</span>`).join("<br>")) : ""}
      ${m.masks.length ? kv("Маскирует", m.masks.map(codeLink).join("<br>")) : ""}
      <h4>Поиск неисправности</h4><table class="tmSteps"><tr><th>Шаг</th><th>Проверка</th><th>Не в норме</th><th>В норме</th></tr>
      ${m.isolation.map(s => `<tr><td>${s.step}</td><td>${esc(s.test)} <span class="sub">(${esc(KIND_RU[s.kind] || s.kind)}, ${s.minutes} мин${s.needs ? "; " + esc(s.needs) : ""})</span>${s.note ? `<div class="note">${esc(s.note)}</div>` : ""}</td><td>${stepNext(s.if_fail)}</td><td>${stepNext(s.if_ok)}</td></tr>`).join("")}</table>
      <h4>Устранение</h4>${m.repair.map(r => kv(L[r.level] || r.level, `${esc(r.action)} · ${r.hours} ч${r.nanite_kg ? ` · наниты ${r.nanite_kg} кг` : ""}${r.spare ? ` · ${esc(r.spare)}` : ""}${r.power_mw ? ` · ${r.power_mw} МВт` : ""}${r.note ? ` <span class="sub">(${esc(r.note)})</span>` : ""}`)).join("")}
      ${m.pmcs ? kv("Профилактика", esc(m.pmcs)) : ""}
      </div></details>`;
  }
  return h + "</div>";
}
function shipFaultHTML(s) {
  const g = FD && FD.ships.find(x => x.id === s.id);
  if (!g || !g.edges.length) return "";
  const rows = g.edges.filter(e => e.kind !== "masks").sort((a, b) => a.source.localeCompare(b.source));
  const masks = g.edges.filter(e => e.kind === "masks");
  return `<details class="card fullPassport"><summary>Цепочки неисправностей корабля (${g.edges.length} связей из каталога оборудования)</summary>
    <div class="sub">Граф собран из установленного оборудования и шин корабля. BITE показывает первый вышедший за допуск сигнал;
    причина может лежать выше по цепочке.</div>
    <table class="tmSteps"><tr><th>Причина</th><th>Связь</th><th>Следствие</th></tr>
    ${rows.map(e => `<tr><td>${codeLink(e.source)}</td><td class="sub">${e.kind === "causes" ? `вызывает / ускоряет ×${e.x}` : `повреждает через ~${e.after_h} ч${e.when ? " (" + esc(e.when) + ")" : ""}`}</td><td>${codeLink(e.target)}</td></tr>`).join("")}</table>
    ${masks.length ? `<h4>Маскирование</h4>${masks.map(e => `<div>${codeLink(e.source)} скрывает ${codeLink(e.target)}</div>`).join("")}` : ""}
    ${g.uncovered_parts.length ? `<div class="note warnbox">Без каталога отказов: ${g.uncovered_parts.map(esc).join(", ")}</div>` : ""}
  </details>`;
}

// ---------------------------------------------------------------- views
const view = () => {
  const q = new URLSearchParams(location.search);
  return { ship: q.get("ship") || q.get("id"), part: q.get("part"), coverage: q.get("view") === "coverage", filter: q.get("q") || "" };
};
function go(params) {
  saveScroll();
  const q = new URLSearchParams(params);
  history.pushState(null, "", "?" + q.toString());
  render(true);
}

function renderCatalog(filter) {
  const f = filter.trim().toLowerCase();
  const match = s => !f || [...nameForms(s), s.id, s.purpose, CLASS_RU[s.economic_class], mfrName(s.manufacturer), roleRu(roleOf(s.id)), shipCode(s)].join(" ").toLowerCase().includes(f);
  let html = "";
  for (const g of BLOC_ORDER) {
    const ships = D.ships.filter(s => s.cooperation_group === g && match(s));
    if (!ships.length) continue;
    html += `<section class="bloc"><div class="blocHead"><h2>${esc(blocTitle(g))}</h2><span class="sub">${esc(blocLong(g))}</span></div>`;
    const mfrs = [...new Set(ships.map(s => s.manufacturer))];
    for (const m of mfrs) {
      html += `<div class="mfr"><h3>${esc(mfrName(m))}</h3><div class="roles">`;
      const ms = ships.filter(s => s.manufacturer === m).sort((a, b) => roleIndex(roleOf(a.id)) - roleIndex(roleOf(b.id)) || a.name.localeCompare(b.name));
      for (const role of [...new Set(ms.map(s => roleOf(s.id)))]) {
        html += `<div class="role"><div class="roleHead">${esc(roleRu(role))}</div><div class="cards">`;
        for (const s of ms.filter(x => roleOf(x.id) === role)) {
          const r = readiness(s);
          html += `<a class="shipCard${isProp(s) ? " prop" : ""}" href="?ship=${encodeURIComponent(s.id)}" data-ship="${esc(s.id)}">${shipPic(s)}
            <div class="cardBody"><div class="cardName">${esc(s.name)}</div>
            ${officialLine(s).length ? `<div class="cardOfficial">${esc(officialLine(s)[0])}</div>` : ""}
            <div class="cardSub">${esc(CLASS_RU[s.economic_class] || s.economic_class)} · ${esc(shipCode(s))}${s.short_name_ru ? ` · «${esc(s.short_name_ru.replace(/ \(.*$/, ""))}»` : ""}</div>
            <div class="progress" title="модели модулей: ${r.ready} из ${r.total}"><i style="width:${(100 * r.ready / r.total).toFixed(0)}%"></i></div>
            ${isProp(s) ? '<span class="badge prop">предложение</span>' : ""}</div></a>`;
        }
        html += "</div></div>";
      }
      html += "</div></div>";
    }
    html += "</section>";
  }
  return html || '<div class="card">Ничего не найдено.</div>';
}

function weaponShort(s) {
  const w = s.weapons || {}, out = [];
  for (const x of w.laser_emitters || []) out.push(`${x.emitters} × ${x.laser?.name || "лазер"}`);
  for (const x of w.missile_cells || []) out.push(`${x.cells} ракетных ячеек`);
  for (const x of w.kinetic_mounts || []) out.push(`${x.qty} × ${partMap.get(x.hardware)?.name || x.hardware}`);
  return out.length ? out.map(esc).join("<br>") : "нет";
}
function shipBrief(s) {
  const dl = s.design_language || {}, b = dl.block || {}, m = dl.manufacturer || {}, r = readiness(s);
  return `<div class="brief">
    <div class="dropTarget" data-drop="ship" title="Перетащите сюда изображение корабля">${shipPic(s, "big")}<span class="dropHint">перетащите изображение корабля</span></div>
    <h2>${esc(s.name)}</h2>
    ${officialLine(s).map(x => `<div class="official">${esc(x)}</div>`).join("")}
    <div class="sub">${isProp(s) ? '<span class="badge prop">предложение справочника</span> ' : ""}шифр модели <b>${esc(shipCode(s))}</b></div>
    ${s.short_name ? kv("Экипажи зовут", esc(s.short_name) + (s.short_name_ru && s.short_name_ru !== s.short_name ? ` <span class="sub">· ${esc(s.short_name_ru)}</span>` : "")) : ""}
    ${s.legacy_name && s.legacy_name !== s.name ? kv("Прежнее имя", `<span class="sub">${esc(s.legacy_name)}</span>`) : ""}
    ${kv("Назначение", esc(s.purpose))}
    ${kv("Класс", esc(CLASS_RU[s.economic_class] || s.economic_class))}
    ${kv("Роль", esc(roleRu(roleOf(s.id))))}
    ${kv("Блок", esc(blocTitle(s.cooperation_group)))}
    ${kv("Производитель", esc(mfrName(s.manufacturer)))}
    ${kv("Габарит", esc((s.frame?.envelope_m || []).map(fmt).join(" × ")) + " м")}
    ${kv("Сухая масса", fmt(s.mass.dry_original_t) + " т")}
    ${kv("Полёт", esc(s.flight.law) + "-" + esc(s.flight.layout) + " · " + fmt(s.flight.installed_main_engines) + " гл. дв. · " + fmt(s.flight.rated_directional_thrust_mn) + " МН")}
    ${kv("Прыжок", s.jump_system.fitted ? '<span class="good">есть</span>' : "нет")}
    ${kv("Вооружение", weaponShort(s))}
    <h4>Особенности</h4>
    <p class="feat">${esc(m.construction || b.hull || "")}</p>
    <p class="feat">${esc(m.signature || b.visual || "")}</p>
    <p class="feat sub">${esc(b.philosophy || "")}</p>
    ${(() => { const d = doctrine(s.manufacturer); return d ? `<h4>Как называет ${esc(mfrName(s.manufacturer))}</h4>
      <p class="feat">${esc(d.index_format)}</p><p class="feat">${esc(d.proper_names)}</p><p class="feat sub">На корпусе: ${esc(d.on_hull)}</p>` : ""; })()}
    ${kv("Модели модулей", `<b>${r.ready}</b> из ${r.total}`)}
  </div>`;
}
function moduleCard(s, g, it) {
  const p = partMap.get(it.part_id);
  if (!p) return `<div class="modCard missing">${esc(it.part_id)}</div>`;
  const ready = Store.isReady(p.id), shared = D.ships.filter(x => shipModules(x).has(p.id)).length;
  return `<a class="modCard${ready ? " ready" : ""}" href="?ship=${encodeURIComponent(s.id)}&part=${encodeURIComponent(p.id)}">
    ${partThumb(p)}
    <div class="modBody"><div class="modName">${esc(p.name)}</div>
    <div class="modSub">${esc(catRu(p.cat))} · ${fmt(it.qty)} шт.${shared > 1 ? ` · на ${shared} кораблях` : ""}</div>
    <label class="ready" title="модель этого модуля готова (общая отметка для всех кораблей)"><input type="checkbox" data-ready="${esc(p.id)}"${ready ? " checked" : ""}> модель готова</label></div></a>`;
}
// ---- layout (tools/maintenance/build_ship_layout.py): compartments by LAYOUT with reference designations in the
// maker's standard; the functional chains join units by function (author 2026-11-09)
const LD = window.ELITE_LAYOUT_DATA || null;
const layoutOf = id => LD ? LD.ships.find(x => x.id === id) : null;
function layoutCard(s, n) {
  const p = partMap.get(n.part);
  if (!p) return `<div class="modCard missing">${esc(n.part)}</div>`;
  const ready = Store.isReady(p.id), shared = D.ships.filter(x => shipModules(x).has(p.id)).length;
  const srus = (n.srus || []).map(x => `<span title="${esc(x.ru)}">${esc(x.des.slice(n.des.length))}</span>`).join(" ");
  return `<a class="modCard${ready ? " ready" : ""}" href="?ship=${encodeURIComponent(s.id)}&part=${encodeURIComponent(p.id)}">
    ${partThumb(p)}
    <div class="modBody"><div class="des">${esc(n.des)}${n.inseparable ? ' <span class="badge">неразборный</span>' : ""}</div><div class="modName">${esc(p.name)}</div>
    <div class="modSub">${esc(catRu(p.cat))}${n.qty ? ` · ${fmt(n.qty)} шт.` : ""}${shared > 1 ? ` · на ${shared} кораблях` : ""}</div>
    ${srus ? `<div class="srus">${srus}</div>` : ""}
    <label class="ready" title="модель этого модуля готова (общая отметка для всех кораблей)"><input type="checkbox" data-ready="${esc(p.id)}"${ready ? " checked" : ""}> модель готова</label></div></a>`;
}
const ROLE_RU = { pad: "пятак", drive: "двигатель", reactor: "реактор" };
function chainHTML(L) {
  if (!L || !L.chains.length) return "";
  let h = `<h2 class="viewTitle">Функциональные цепочки</h2><div class="chains">`;
  for (const c of L.chains) {
    if (c.id === "propulsion") {
      // pads drawn as their own items (author 2026-11-09: "пятаков не вижу"): the drive's round hot pad on the
      // drive's side, the reactor's square pad over the reactor; designations of the shown group
      const first = c.instances[0];
      const boxes = c.pattern.map((x, i) => {
        const u = first[i] || {};
        if (x.role === "pad") {
          const drv = partMap.get((c.pattern[i + 1]?.role === "drive" ? c.pattern[i + 1] : c.pattern[i - 1])?.part);
          const dia = drv?.geometry?.nozzle_or_emitter_diameter_m;
          return `<div class="cbox pad"><b><i class="padIcon round"></i>пятак двигателя</b><span>${esc(u.des || "")}${dia ? ` · Ø${fmt(dia)} м` : ""}</span></div>`;
        }
        const p = x.part ? partMap.get(x.part) : null;
        const box = `<div class="cbox ${esc(x.role)}"><b>${esc(ROLE_RU[x.role] || x.role)}</b>${p ? `<span>${esc(p.name)}</span>` : ""}${u.des ? `<span>${esc(u.des)}</span>` : ""}</div>`;
        if (x.role !== "reactor" || !u.pad) return box;
        const side = p?.geometry?.cooling_pad_side_m;
        return `<div class="cstack"><div class="cbox pad"><b><i class="padIcon"></i>пятак реактора</b><span>${esc(u.pad)}${side ? ` · ${fmt(side)}×${fmt(side)} м` : ""}</span></div><div class="clink v">|</div>${box}</div>`;
      }).join('<div class="clink">—</div>');
      // identical groups are drawn once, as a stack, like the identical compartments (author 2026-11-09)
      const n = c.instances.length;
      h += `<div class="card chain wide${n > 1 ? " stacked" + (n > 2 ? " stacked3" : "") : ""}"><div class="chainHead"><b>${esc(c.ru)}</b>${n > 1 ? `<span class="mult">×${n}</span>` : ""}
        <span class="sub">${c.compartments.map(esc).join(", ")}</span></div>
        ${n > 1 ? `<div class="sub">показана группа отсека ${esc(c.compartments[0])}; в ${c.compartments.slice(1).map(esc).join(", ")} - такая же, обозначения с заменой номера отсека</div>` : ""}<div class="cboxes">${boxes}</div>
        <div class="sub">каждый двигатель сбрасывает своё тепло через свой пятак; в общий контур - только при повреждённом пятаке</div></div>`;
    } else {
      const grp = list => { const m = new Map(); for (const u of list) { const k = u.part; m.set(k, (m.get(k) || 0) + (u.qty || 1)); } return [...m].map(([pid, q]) => `${esc(partMap.get(pid)?.name || pid)}${q > 1 ? " ×" + q : ""}`).join(", "); };
      h += `<div class="card chain"><div class="chainHead"><b>${esc(c.ru)}</b></div>
        <div class="cflow"><div class="cbox src">${grp(c.sources)}</div><div class="clink">→</div><div class="cbox">${grp(c.sinks)}</div></div></div>`;
    }
  }
  return h + "</div>";
}
function renderShip(s) {
  const L = layoutOf(s.id);
  if (L) return renderShipLayout(s, L);
  return renderShipFunctional(s);
}
// hull layout sketches (tools/ships/layout_sketch.py) are workshop sketches of the key "hull:<ship id>" (author 2026-11-10)
function hullPic(s) {
  const sk = Store.sketches("hull:" + s.id);
  return sk.length ? `<img data-blob="${esc(sk[0].id)}" alt="">` : shipSilhouette(s);
}
function hullGallery(s) {
  const sk = Store.sketches("hull:" + s.id);
  if (!sk.length) return "";
  return `<div class="card"><h3>Эскиз корпуса и компоновки</h3><div class="gallery">${sk.map(k => `<a class="sketchThumb" title="${esc(k.note || "")}"><div class="pic"><img data-blob="${esc(k.id)}" alt=""></div><span>${esc(k.name)}</span></a>`).join("")}</div></div>`;
}
function renderShipLayout(s, L) {
  const hullReady = Store.isReady("hull:" + s.id);
  const std = (LD.standards || {})[L.standard];
  let schema = `<div class="section hull"><div class="secHead"><b>${esc(L.unit)}</b> · Корпус / рама</div><div class="mods">
    <div class="modCard${hullReady ? " ready" : ""}"><div class="pic thumb">${hullPic(s)}</div><div class="modBody"><div class="modName">Корпус ${esc(s.name)}</div>
    <div class="modSub">${esc((s.frame?.envelope_m || []).map(fmt).join(" × "))} м · ${esc(s.frame?.form || "")} · паспорт рамы отложен</div>
    <label class="ready"><input type="checkbox" data-ready="hull:${esc(s.id)}"${hullReady ? " checked" : ""}> модель готова</label></div></div></div></div>`;
  // identical compartments (e.g. the propulsion bays) are drawn once, as a stack: frames behind
  // the card show how many there are (author 2026-11-09)
  const sig = c => c.key + "|" + c.children.map(n => `${n.part}:${n.qty || 1}:${n.cls}`).join(",");
  const groups = [];
  for (const c of L.compartments) {
    const g = groups.find(x => x.sig === sig(c));
    if (g) g.list.push(c); else groups.push({ sig: sig(c), list: [c] });
  }
  for (const g of groups) {
    const c = g.list[0], n = g.list.length;
    const name = n > 1 ? c.ru.replace(/\s*\d+$/, "") : c.ru;
    const head = n > 1
      ? `<b>${g.list.map(x => esc(x.des)).join(", ")}</b> · ${esc(name)} <span class="stackN">×${n}</span><div class="sub">показан ${esc(c.des)}; остальные такие же, обозначения деталей - с заменой номера отсека</div>`
      : `<b>${esc(c.des)}</b> · ${esc(c.ru)}`;
    schema += `<div class="section${n > 1 ? " stacked" + (n > 2 ? " stacked3" : "") : ""}"><div class="secHead">${head}</div><div class="mods">${c.children.map(x => layoutCard(s, x)).join("")}</div></div>`;
  }
  return `<div class="shipView"><aside class="side">${shipBrief(s)}</aside>
    <div class="mainCol"><div class="crumbs"><a href="?" data-nav>Каталог</a> › ${esc(blocTitle(s.cooperation_group))} › ${esc(mfrName(s.manufacturer))} › <b>${esc(s.name)}</b></div>
    <h2 class="viewTitle">Компоновка · ${esc(s.name)}</h2>
    <div class="sub stdLine">Позиционные обозначения: ${esc(std ? std.ru : L.standard)}${std ? ` · пример <code>${esc(std.example)}</code>` : ""}. Компоновка сгенерирована из оборудования (предложение).</div>
    <div class="schema">${schema}</div>
    ${hullGallery(s)}
    ${chainHTML(L)}
    ${shipFaultHTML(s)}
    <details class="card fullPassport"><summary>Полный инженерный паспорт (баланс, вооружение, замены)</summary>${fullPassport(s)}</details></div></div>`;
}
function renderShipFunctional(s) {
  const hullReady = Store.isReady("hull:" + s.id);
  let schema = `<div class="section hull"><div class="secHead"><b>${esc(shipCode(s))}</b> · Корпус / рама</div><div class="mods">
    <div class="modCard${hullReady ? " ready" : ""}"><div class="pic thumb">${hullPic(s)}</div><div class="modBody"><div class="modName">Корпус ${esc(s.name)}</div>
    <div class="modSub">${esc((s.frame?.envelope_m || []).map(fmt).join(" × "))} м · ${esc(s.frame?.form || "")} · паспорт рамы отложен</div>
    <label class="ready"><input type="checkbox" data-ready="hull:${esc(s.id)}"${hullReady ? " checked" : ""}> модель готова</label></div></div></div></div>`;
  for (const g of s.functional_groups)
    schema += `<div class="section"><div class="secHead"><b>${esc(sectionCode(s, g))}</b> · ${esc(g.name)}</div><div class="mods">${g.items.map(it => moduleCard(s, g, it)).join("")}</div></div>`;
  return `<div class="shipView"><aside class="side">${shipBrief(s)}</aside>
    <div class="mainCol"><div class="crumbs"><a href="?" data-nav>Каталог</a> › ${esc(blocTitle(s.cooperation_group))} › ${esc(mfrName(s.manufacturer))} › <b>${esc(s.name)}</b></div>
    <h2 class="viewTitle">Функциональная схема · ${esc(s.name)}</h2>
    <div class="schema">${schema}</div>
    ${hullGallery(s)}
    ${shipFaultHTML(s)}
    <details class="card fullPassport"><summary>Полный инженерный паспорт (баланс, вооружение, замены)</summary>${fullPassport(s)}</details></div></div>`;
}

// ---- module view
function describePart(p) {
  const c = CAT[p.cat], g = p.geometry || {};
  const used = D.ships.filter(x => shipModules(x).has(p.id));
  let t = `${c ? c[0] : p.cat} «${p.name}» (модель ${p.model || "—"}) производства ${mfrName(p.mfr)}.`;
  if (used.length) t += ` Стоит на ${used.length} ${used.length === 1 ? "корабле" : "кораблях"}.`;
  if (p.notes) t += " " + p.notes;
  if (g.envelope_m) t += ` Заменяемый блок ${g.envelope_m.map(fmt).join(" × ")} м (Ш × В × Д), масса ${fmt(p.mass_t)} т.`;
  return t;
}
function autoPrompt(p) {
  const c = CAT[p.cat] || [p.cat, "", p.cat, p.cat], g = p.geometry || {}, e = g.envelope_m || [];
  const specs = [];
  if (e.length) specs.push(`overall envelope ${e[0]} m wide x ${e[1]} m high x ${e[2]} m long`);
  if (p.mass_t) specs.push(`mass ${p.mass_t} t`);
  if (p.thrust_mn) specs.push(`thrust ${p.thrust_mn} MN`);
  if (p.power_mw) specs.push(`power ${p.power_mw} MW`);
  if (p.mwe) specs.push(`${p.mwe} MWe`);
  if (g.cooling_pad_side_m) specs.push(`cooling pad ${g.cooling_pad_side_m} x ${g.cooling_pad_side_m} m on one face`);
  if (g.deployed_area_m2) specs.push(`deployed area ${g.deployed_area_m2} m2`);
  if (p.seats) specs.push(`${p.seats} seats`);
  return [
    `Concept sketch of a spacecraft component for a hard science-fiction game set in the year 3126: ${c[2]}, model "${p.model || p.id}".`,
    `What it is: ${c[3]}.`,
    specs.length ? `Scale and specs: ${specs.join(", ")}.` : "",
    `Design school: ${BLOC_STYLE_EN[p.cooperation_group] || "industrial spacecraft engineering"}.`,
    `It is a replaceable standardized module that bolts into a ship's A-frame bay: show mounting points, service hatches, power and coolant connectors, lifting lugs.`,
    `Plausible engineering, no fantasy shapes, no decorative greebles beyond function.`,
    `Three-quarter perspective view plus a small orthographic side view, pencil and grey marker on light grey paper, dimension lines, a human figure for scale, neutral background, no text, no logos.`,
  ].filter(Boolean).join("\n");
}
function renderPart(s, p) {
  const g = p.geometry || {}, ready = Store.isReady(p.id), c = CAT[p.cat];
  const used = D.ships.filter(x => shipModules(x).has(p.id));
  const sect = s ? s.functional_groups.find(gr => gr.items.some(i => i.part_id === p.id)) : null;
  let fields = "";
  for (const [k, v] of Object.entries(p)) {
    if (["id", "name", "cat", "mfr", "geometry", "replacements", "notes", "cooperation_group"].includes(k) || v == null || typeof v === "object") continue;
    fields += kv(FIELD_RU[k] || k, esc(typeof v === "boolean" ? (v ? "да" : "нет") : fmt(v)));
  }
  const rp = p.replacements || {}, one = (x, enemy) => x ? `<b>${esc(x.name)}</b><br><span class="${enemy ? "danger" : "good"}">${esc(x.part_id)}</span><br><span class="sub">${esc(x.note || "")}</span>` : "—";
  const prompt = Store.prompt(p.id) ?? autoPrompt(p), edited = Store.prompt(p.id) != null;
  return `<div class="partView">
    <div class="crumbs"><a href="?" data-nav>Каталог</a>${s ? ` › <a href="?ship=${encodeURIComponent(s.id)}" data-nav>${esc(s.name)}</a>${sect ? " › " + esc(sectionCode(s, sect)) + " " + esc(sect.name) : ""}` : ""} › <b>${esc(p.name)}</b></div>
    <div class="partHead"><div class="grow"><h2>${esc(p.name)}</h2><div class="partId">${esc(p.id)} · ${esc(catRu(p.cat))}</div></div>
      <label class="ready big"><input type="checkbox" data-ready="${esc(p.id)}"${ready ? " checked" : ""}> модель готова</label></div>
    <div class="grid2">
      <div class="card"><h3>Что это</h3><p>${esc(describePart(p))}</p>
        ${kv("Назначение", esc(c ? c[1] : "—"))}
        ${kv("Производитель", esc(mfrName(p.mfr)))}
        ${kv("Блок", esc(blocTitle(p.cooperation_group)))}
        ${kv("Используется", used.map(x => `<a href="?ship=${encodeURIComponent(x.id)}&part=${encodeURIComponent(p.id)}" data-nav>${esc(x.name)}</a>`).join(", ") || "—")}
      </div>
      <div class="card"><h3>Физические характеристики</h3>${fields}</div>
    </div>
    <div class="grid2">
      <div class="card"><h3>Геометрия</h3><div class="dim">${esc((g.envelope_m || []).map(fmt).join(" × "))} м</div><div class="sub">Ш × В × Д; заменяемый блок, без монтажной рамы</div>
        ${sketchBox(p)}
        ${kv("Зона обслуживания", (g.service_clearance_m || []).map(fmt).join(" × ") + " м")}
        ${g.cooling_pad_side_m ? kv("Охлаждающий пятак", fmt(g.cooling_pad_side_m) + " × " + fmt(g.cooling_pad_side_m) + " м") : ""}
        ${g.deployed_area_m2 ? kv("Развёрнутая площадь", fmt(g.deployed_area_m2) + " м²") : ""}
        ${kv("3D-модель", p.model_asset ? "assets/models/" + p.model_asset + " (LOD0-3, хитвалумы)" : "нет")}
        ${p.official_name ? kv("Официальное имя", p.official_name) : ""}
        ${p.native_script ? kv("На языке производителя", p.native_script) : ""}
        ${p.legacy_index ? kv("Прежний индекс", p.legacy_index) : ""}</div>
      <div class="card"><h3>Промт для генератора эскиза</h3>
        <textarea id="prompt" class="prompt" spellcheck="false">${esc(prompt)}</textarea>
        <div class="row"><button class="btn" id="copyPrompt">Копировать</button><button class="btn" id="savePrompt" disabled>Сохранить правку</button>
        <button class="btn" id="resetPrompt"${edited ? "" : " disabled"}>Вернуть автоматический</button><span class="sub" id="promptState">${(() => { const i = Store.promptInfo(p.id); return i ? (i.source ? esc(i.source) : "отредактирован") + (i.version ? " · версия " + esc(i.version) : "") : "собран из паспорта"; })()}</span></div>
        <h4>Замены</h4>${kv("Штатная", one(rp.standard, false))}${kv("Внешний / вражеский аналог", one(rp.hostile_or_foreign, true))}</div>
    </div>
    ${manualHTML(p)}
    <div class="card"><h3>Эскизы</h3>
      <div class="dropZone" data-drop="sketch">Перетащите эскизы сюда, вставьте из буфера (Ctrl+V) или <label class="link">выберите файлы<input type="file" accept="image/*" multiple hidden id="pickFiles"></label></div>
      <div class="gallery">${Store.sketches(p.id).map(sk => `<button class="sketchThumb" data-sketch="${esc(sk.id)}" title="${esc(sk.note || sk.name)}"><div class="pic"><img data-blob="${esc(sk.id)}" alt=""></div><span>${esc(new Date(sk.added).toLocaleString("ru-RU"))}${sk.note ? " · 📝" : ""}</span></button>`).join("") || '<span class="sub">Эскизов пока нет.</span>'}</div>
    </div></div>`;
}

// ---- the old full passport (kept: balance, weapons, replacements)
function fullPassport(s) {
  const r = s.replacements || {}, w = s.weapons || {}, rows = [];
  for (const x of w.laser_emitters || []) { const l = x.laser || {}; rows.push(kv("Лазер", x.emitters + " излуч. · <b>" + esc(l.name) + "</b> · " + fmt(l.power_mw) + " МВт · " + fmt(l.range_km) + " км")); }
  for (const x of w.missile_cells || []) rows.push(kv("Ракеты", x.cells + " ячеек: " + x.compatible_missiles.map(m => "<b>" + esc(m.name) + "</b> " + fmt(m.mass_kg) + " кг / " + fmt(m.range_km) + " км").join("; ")));
  for (const x of w.kinetic_mounts || []) rows.push(kv("Кинетика", x.qty + " × " + esc(partMap.get(x.hardware)?.name || x.hardware)));
  let h = '<div class="grid2"><div>' +
    kv("id", esc(s.id)) + kv("Сухая масса (после унификации)", fmt(s.mass.dry_original_t) + " → " + fmt(s.mass.dry_estimated_after_standardization_t) + " т") +
    kv("Главные двигатели (одновременно)", fmt(s.flight.installed_main_engines) + " (" + fmt(s.flight.simultaneous_main_engines) + ")") +
    kv("Потребление тяги / реакторный запас", fmt(s.flight.rated_propulsion_input_mw) + " / " + fmt(s.flight.standardized_reactor_capacity_mw) + " МВт") +
    kv("Служебная мощность", fmt(s.service_power_capacity_mw) + " МВт") + kv("Уникальных запчастей", fmt(s.unique_spare_skus)) +
    '</div><div>' +
    kv("Штатная альтернатива", r.standard ? '<a href="?ship=' + encodeURIComponent(r.standard.ship_id) + '" data-nav>' + esc(r.standard.name) + '</a> <span class="sub">' + esc(r.standard.reason) + '</span>' : "—") +
    kv("Внешний / вражеский аналог", r.hostile_or_foreign ? esc(r.hostile_or_foreign.name) + ' <span class="sub">' + esc(r.hostile_or_foreign.reason) + '</span>' : "—") +
    (rows.join("") || kv("Вооружение", "нет")) + '</div></div>';
  const ref = s.reference;
  if (ref) {
    const b = ref.balance || {};
    h += '<h4>Предложение справочника</h4><p>' + esc(ref.design_note || "") + '</p>' +
      kv("Реакторы / тяга", fmt(b.propulsion_reactor_mw) + " / " + fmt(b.propulsion_demand_mw) + " МВт") +
      kv("Служебная мощность / нагрузка при полном огне", fmt(b.service_capacity_mw) + " / " + fmt(b.hotel_mw_weapons100) + " МВт") +
      kv("Радиаторы / тепло при полном огне", fmt(b.radiator_mw) + " / " + fmt(b.service_heat_mw_weapons100) + " МВт") +
      kv("Полная масса / ускорение", fmt(b.gross_t) + " т · " + fmt(b.accel_g_gross) + " g") +
      ((b.flags || []).length ? '<div class="note warnbox">' + b.flags.map(esc).join("<br>") + '</div>' : "") +
      ((ref.open || []).length ? '<h4>Открыто</h4><ul>' + ref.open.map(x => '<li>' + esc(x) + '</li>').join("") + '</ul>' : "");
  }
  return h;
}

// ---- fleet coverage (kept from the previous version)
function coverageHTML() {
  const C = D.coverage; if (!C) return '<div class="card">Нет данных покрытия.</div>';
  const link = id => { const s = shipMap.get(id); return s ? '<a href="?ship=' + encodeURIComponent(id) + '" data-nav>' + esc(s.name) + '</a>' : esc(id); };
  const cell = c => { if (!c) return "—"; const h = []; for (const id of c.existing || []) h.push('<div>● ' + link(id) + '</div>'); for (const id of c.added || []) h.push('<div class="warn">◆ ' + link(id) + '</div>'); if (c.partial && c.partial.trim() !== "—") h.push('<div class="sub">◐ ' + esc(c.partial) + '</div>'); return h.join("") || "—"; };
  let t = '<table class="coverage"><tr><th>Роль</th>' + C.blocs.map(b => '<th>' + esc(b.ru) + '</th>').join("") + '</tr>';
  for (const r of C.roles) t += '<tr><td class="role">' + esc(r.ru) + '</td>' + (r.all ? '<td colspan="' + C.blocs.length + '">' + cell(r.all) + '</td>' : C.blocs.map(b => '<td>' + cell(r.cells[b.id]) + '</td>').join("")) + '</tr>';
  return '<div class="crumbs"><a href="?" data-nav>Каталог</a> › <b>Покрытие флота</b></div><h2 class="viewTitle">Покрытие флота: блок × роль</h2><div class="sub">' + esc(C.note) + '</div><div class="card covTable">' + t + '</table></div>';
}

// ---------------------------------------------------------------- modal (a sketch with its note)
let modal = null;
function openSketch(partId, id) {
  const list = Store.sketches(partId), i = list.findIndex(x => x.id === id); if (i < 0) return;
  const sk = list[i];
  closeModal(true);
  const m = document.createElement("div");
  m.className = "modal";
  m.innerHTML = `<div class="modalBox"><div class="modalImg"><img alt=""></div>
    <div class="modalSide"><div class="row"><b>${esc(partMap.get(partId)?.name || partId)}</b><span class="spacer"></span><button class="btn mini" data-x>✕</button></div>
    <div class="sub">${esc(sk.name)} · добавлен ${esc(new Date(sk.added).toLocaleString("ru-RU"))}</div>
    ${(() => { const r = Store.review(sk.id); return r ? `<div class="review"><div class="reviewHead">Рецензия${r.author ? " · " + esc(r.author) : ""}${r.updated ? " · " + esc(new Date(r.updated).toLocaleString("ru-RU")) : ""}${r.verdict ? ` · <b>${esc(r.verdict)}</b>` : ""}</div>${(r.items || []).map(i => `<div class="reviewItem ${esc(i.kind || "")}">${esc(i.text)}</div>`).join("")}${r.next ? `<div class="reviewNext">${esc(r.next)}</div>` : ""}</div>` : ""; })()}
    <label class="sub" for="note">Заметки</label><textarea id="note" class="note" placeholder="что поправить, что оставить…">${esc(sk.note || "")}</textarea>
    <div class="row"><button class="btn" data-save disabled>Пересохранить</button><span class="sub" data-state>${sk.updated > sk.added ? "сохранено " + esc(new Date(sk.updated).toLocaleString("ru-RU")) : ""}</span></div>
    <div class="row"><button class="btn mini" data-prev${i ? "" : " disabled"}>← пред.</button><button class="btn mini" data-next${i < list.length - 1 ? "" : " disabled"}>след. →</button>
    <span class="spacer"></span><a class="btn mini" data-dl download="${esc(partId + "_" + sk.file)}">Скачать</a><button class="btn mini danger" data-del>Удалить</button></div></div></div>`;
  document.body.appendChild(m);
  modal = { el: m, partId, id, dirty: false };
  Store.url(id).then(u => { if (u) { m.querySelector("img").src = u; m.querySelector("[data-dl]").href = u; } });
  const note = m.querySelector("#note"), save = m.querySelector("[data-save]");
  note.oninput = () => { modal.dirty = note.value !== (sk.note || ""); save.disabled = !modal.dirty; };
  save.onclick = () => { Store.setNote(partId, id, note.value); modal.dirty = false; save.disabled = true; m.querySelector("[data-state]").textContent = "сохранено " + new Date().toLocaleString("ru-RU"); };
  note.onkeydown = e => { if ((e.ctrlKey || e.metaKey) && e.key === "s") { e.preventDefault(); if (!save.disabled) save.onclick(); } };
  m.querySelector("[data-x]").onclick = () => closeModal();
  m.onclick = e => { if (e.target === m) closeModal(); };
  m.querySelector("[data-prev]").onclick = () => closeModal() && openSketch(partId, list[i - 1].id);
  m.querySelector("[data-next]").onclick = () => closeModal() && openSketch(partId, list[i + 1].id);
  m.querySelector("[data-del]").onclick = async () => { if (!confirm("Удалить эскиз?")) return; closeModal(true); await Store.removeSketch(partId, id); };
  note.focus();
}
function closeModal(force) {
  if (!modal) return true;
  if (!force && modal.dirty && !confirm("Заметка не сохранена. Закрыть без сохранения?")) return false;
  modal.el.remove(); modal = null; return true;
}

// ---------------------------------------------------------------- scroll memory (reload / back)
const scrollKey = () => "scroll:" + location.pathname + location.search;
function saveScroll() {
  try { sessionStorage.setItem(scrollKey(), JSON.stringify({ y: window.scrollY, side: $(".side")?.scrollTop || 0 })); } catch { /* private mode */ }
}
function restoreScroll() {
  let v = null;
  try { v = JSON.parse(sessionStorage.getItem(scrollKey()) || "null"); } catch { /* */ }
  if (!v) return;
  window.scrollTo(0, v.y);
  if ($(".side")) $(".side").scrollTop = v.side;
}
let scrollTimer = null;
addEventListener("scroll", () => { clearTimeout(scrollTimer); scrollTimer = setTimeout(saveScroll, 120); }, { passive: true });
addEventListener("beforeunload", saveScroll);
addEventListener("pagehide", saveScroll);
if ("scrollRestoration" in history) history.scrollRestoration = "manual";

// ---------------------------------------------------------------- render + events
function folderButton() {
  const b = $("#folderBtn"); if (!b) return;
  const st = Store.folder;
  b.textContent = st === "server" ? "Сервер: workshop ↻" : st === "server-error" ? "Сервер недоступен" : st === "connected" ? "Папка: " + Store.folderName : st === "needs-permission" ? "Переподключить папку" : st === "unsupported" ? "Данные в браузере" : "Подключить папку";
  b.className = "btn mini " + (st === "connected" || st === "server" ? "okBtn" : st === "needs-permission" || st === "server-error" ? "warnBtn" : "");
  b.title = st === "server" ? "Всё хранится в репозитории (Notes/html/ship_reference/workshop) через локальный сервер; кнопка обновляет с диска" : st === "connected" ? "Эскизы, заметки и отметки пишутся в эту папку (и в браузер)" :
    st === "unsupported" ? "Браузер не умеет писать в папку: всё хранится только в этом браузере (Chrome / Edge умеют)" :
    "Сейчас всё хранится только в этом браузере. Подключите папку (рекомендуется Notes/html/ship_reference/workshop), чтобы эскизы легли в репозиторий.";
}
function render(fresh) {
  const v = view(), out = $("#view");
  const search = $("#search");
  if (search && document.activeElement !== search) search.value = v.filter;
  const ship = v.ship ? shipMap.get(v.ship) : null, part = v.part ? partMap.get(v.part) : null;
  document.body.dataset.view = part ? "part" : ship ? "ship" : v.coverage ? "coverage" : "catalog";
  if (part) out.innerHTML = renderPart(ship, part);
  else if (ship) out.innerHTML = renderShip(ship);
  else if (v.coverage) out.innerHTML = coverageHTML();
  else out.innerHTML = renderCatalog(v.filter);
  document.title = (part ? part.name + " · " : ship ? ship.name + " · " : "") + "Elite 3126 — корабельный справочник";
  const meta = $("#refMeta");
  if (meta) { let r = 0, t = 0; for (const s of D.ships) { const x = readiness(s); r += x.ready; t += x.total; } meta.textContent = D.ships.length + " кораблей · " + D.parts.length + " модулей · моделей готово " + r + " из " + t + " мест"; }
  folderButton();
  bindView(ship, part);
  hydrateImages(out);
  if (fresh) window.scrollTo(0, 0); else restoreScroll();
}
function bindView(ship, part) {
  const out = $("#view");
  out.querySelectorAll("input[data-ready]").forEach(cb => {
    cb.onclick = e => e.stopPropagation();
    cb.onchange = () => { saveScroll(); Store.setReady(cb.dataset.ready, cb.checked); };
  });
  // a label inside a card link: toggle its box, never follow the link
  out.querySelectorAll("label.ready").forEach(l => l.onclick = e => {
    e.stopPropagation();
    if (e.target.tagName === "INPUT") return;
    e.preventDefault();
    const cb = l.querySelector("input"); cb.checked = !cb.checked; cb.dispatchEvent(new Event("change"));
  });
  out.querySelectorAll("a[data-tm]").forEach(a => a.onclick = e => {
    e.preventDefault();
    const code = a.dataset.tm, pid = a.dataset.tmPart, here = new URLSearchParams(location.search);
    const open = () => { const d = document.getElementById("tm-" + code); if (d) { d.open = true; d.scrollIntoView({ block: "start" }); } };
    if (here.get("part") === pid) open();
    else { go({ ...(here.get("ship") ? { ship: here.get("ship") } : {}), part: pid }); setTimeout(open, 50); }
  });
  out.querySelectorAll("a[href^='?']").forEach(a => a.onclick = e => {
    if (e.ctrlKey || e.metaKey || e.shiftKey || e.button) return;
    e.preventDefault(); go(Object.fromEntries(new URLSearchParams(a.getAttribute("href").slice(1))));
  });
  if (part) {
    const ta = $("#prompt"), sv = $("#savePrompt"), rs = $("#resetPrompt"), st = $("#promptState");
    const base = () => Store.prompt(part.id) ?? autoPrompt(part);
    ta.oninput = () => { sv.disabled = ta.value === base(); };
    sv.onclick = () => { saveScroll(); Store.setPrompt(part.id, ta.value); };
    rs.onclick = () => { saveScroll(); Store.setPrompt(part.id, null); };
    $("#copyPrompt").onclick = async () => { try { await navigator.clipboard.writeText(ta.value); st.textContent = "скопировано"; } catch { ta.select(); document.execCommand("copy"); st.textContent = "скопировано"; } };
    $("#pickFiles").onchange = e => addSketches(part.id, e.target.files);
    out.querySelectorAll("[data-sketch]").forEach(b => b.onclick = () => openSketch(part.id, b.dataset.sketch));
  }
  out.querySelectorAll("[data-drop]").forEach(z => {
    z.ondragover = e => { e.preventDefault(); z.classList.add("over"); };
    z.ondragleave = () => z.classList.remove("over");
    z.ondrop = e => {
      e.preventDefault(); z.classList.remove("over");
      const files = [...e.dataTransfer.files].filter(f => f.type.startsWith("image/"));
      if (!files.length) return;
      if (z.dataset.drop === "ship" && ship) { saveScroll(); Store.setShipImage(ship.id, files[0]); }
      else if (part) addSketches(part.id, files);
    };
  });
}
async function addSketches(partId, files) {
  saveScroll();
  for (const f of [...files]) if (f.type.startsWith("image/")) await Store.addSketch(partId, f);
}

document.addEventListener("DOMContentLoaded", async () => {
  // a dropped image outside a drop zone must not navigate the page away
  addEventListener("dragover", e => e.preventDefault());
  addEventListener("drop", e => e.preventDefault());
  addEventListener("paste", e => {
    const v = view(); if (!v.part || modal) return;
    const files = [...(e.clipboardData?.files || [])].filter(f => f.type.startsWith("image/"));
    if (files.length) { e.preventDefault(); addSketches(v.part, files); }
  });
  addEventListener("keydown", e => { if (e.key === "Escape" && modal) closeModal(); });
  addEventListener("popstate", () => { closeModal(true); render(false); });
  const search = $("#search");
  let t = null;
  search.oninput = () => {
    clearTimeout(t);
    t = setTimeout(() => { const q = search.value.trim(); history.replaceState(null, "", q ? "?q=" + encodeURIComponent(q) : "?"); render(false); }, 150);
  };
  $("#folderBtn").onclick = async () => { try { saveScroll(); await Store.connectFolder(); } catch (e) { if (e.name !== "AbortError") alert("Не удалось подключить папку: " + e.message); } };
  // draw at once from the data; the workshop storage (flags, sketches) arrives a moment later
  render(false);
  try {
    await Promise.race([Store.init(), new Promise((_, ko) => setTimeout(() => ko(new Error("IndexedDB timeout")), 4000))]);
  } catch (e) { console.error("storage unavailable", e); }
  Store.onChange(() => { saveScroll(); render(false); });
  render(false);
});
