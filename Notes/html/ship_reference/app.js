// Elite 3126 ship reference: catalog (bloc -> manufacturer -> role -> cards), ship view (brief +
// functional A-frame schema with module cards), module view (description, characteristics,
// geometry, image prompt, sketches with notes). One page, the view lives in the URL
// (?ship=ID, ?ship=ID&part=ID, ?view=coverage) so reload and back / forward keep it; the scroll
// position is kept per view across reloads.
"use strict";
const D = window.ELITE_SHIP_REF_DATA;
const MODEL_CODES = window.ELITE_SHIP_MODEL_CODES || {};
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
  drive: ["Главный двигатель", "создаёт основную тягу корабля; работает от пропульсивных реакторов", "main propulsion drive unit", "main thrust engine with nozzle, gimbal mount and feed lines"],
  rcs: ["Маневровый блок (RCS)", "ориентация и точное маневрирование: пары сопел по осям", "reaction control thruster cluster", "attitude control thruster quad with small nozzles"],
  reactor: ["Пропульсивный реактор", "питает главные двигатели; отводит тепло через охлаждающий пятак", "propulsion fusion reactor module", "compact reactor block with a flat cooling pad face, armoured shell and power couplings"],
  service_reactor: ["Служебный реактор (ВСУ)", "питает бортовые системы, сенсоры, оружие и жизнеобеспечение", "auxiliary power unit reactor", "small service reactor with power bus connectors"],
  radiator: ["Радиатор", "сбрасывает служебное тепло излучением в космос", "deployable heat radiator panel", "thin deployable radiator panels on a hinged boom with coolant manifolds"],
  coolant_loop: ["Контур охлаждения", "переносит тепло от агрегатов к радиаторам и пятакам", "coolant loop pump module", "pump and heat-exchanger unit with insulated pipework"],
  jump_core: ["Ядро прыжка", "запечатанный стратегический модуль межзвёздного прыжка", "sealed jump-drive core", "sealed armoured jump core canister with field emitters and warning markings"],
  tank: ["Бак", "запас рабочего тела / заряда прыжка / расходников", "propellant tank", "cylindrical pressure tank with mounting saddles"],
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
  thrust_mn: "Тяга, МН", input_mw: "Потребляемая мощность, МВт", gimbal_deg: "Отклонение кардана, °", startup_s: "Запуск, с",
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
const shipCode = s => String(MODEL_CODES[s.id] ?? "35");
const sectionCode = (s, g) => g.code.replace(/^\d+(?=A)/, shipCode(s));

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
  const match = s => !f || [s.name, s.id, s.purpose, CLASS_RU[s.economic_class], mfrName(s.manufacturer), roleRu(roleOf(s.id)), shipCode(s)].join(" ").toLowerCase().includes(f);
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
            <div class="cardSub">${esc(CLASS_RU[s.economic_class] || s.economic_class)} · ${esc(shipCode(s))}A</div>
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
    <div class="sub">${isProp(s) ? '<span class="badge prop">предложение справочника</span> ' : ""}шифр модели <b>${esc(shipCode(s))}</b></div>
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
function renderShip(s) {
  const hullReady = Store.isReady("hull:" + s.id);
  let schema = `<div class="section hull"><div class="secHead"><b>${esc(shipCode(s))}</b> · Корпус / рама</div><div class="mods">
    <div class="modCard${hullReady ? " ready" : ""}"><div class="pic thumb">${shipSilhouette(s)}</div><div class="modBody"><div class="modName">Корпус ${esc(s.name)}</div>
    <div class="modSub">${esc((s.frame?.envelope_m || []).map(fmt).join(" × "))} м · ${esc(s.frame?.form || "")} · паспорт рамы отложен</div>
    <label class="ready"><input type="checkbox" data-ready="hull:${esc(s.id)}"${hullReady ? " checked" : ""}> модель готова</label></div></div></div></div>`;
  for (const g of s.functional_groups)
    schema += `<div class="section"><div class="secHead"><b>${esc(sectionCode(s, g))}</b> · ${esc(g.name)}</div><div class="mods">${g.items.map(it => moduleCard(s, g, it)).join("")}</div></div>`;
  return `<div class="shipView"><aside class="side">${shipBrief(s)}</aside>
    <div class="mainCol"><div class="crumbs"><a href="?" data-nav>Каталог</a> › ${esc(blocTitle(s.cooperation_group))} › ${esc(mfrName(s.manufacturer))} › <b>${esc(s.name)}</b></div>
    <h2 class="viewTitle">Функциональная схема · ${esc(s.name)}</h2>
    <div class="schema">${schema}</div>
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
        ${g.deployed_area_m2 ? kv("Развёрнутая площадь", fmt(g.deployed_area_m2) + " м²") : ""}</div>
      <div class="card"><h3>Промт для генератора эскиза</h3>
        <textarea id="prompt" class="prompt" spellcheck="false">${esc(prompt)}</textarea>
        <div class="row"><button class="btn" id="copyPrompt">Копировать</button><button class="btn" id="savePrompt" disabled>Сохранить правку</button>
        <button class="btn" id="resetPrompt"${edited ? "" : " disabled"}>Вернуть автоматический</button><span class="sub" id="promptState">${edited ? "отредактирован" : "собран из паспорта"}</span></div>
        <h4>Замены</h4>${kv("Штатная", one(rp.standard, false))}${kv("Внешний / вражеский аналог", one(rp.hostile_or_foreign, true))}</div>
    </div>
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
  b.textContent = st === "connected" ? "Папка: " + Store.folderName : st === "needs-permission" ? "Переподключить папку" : st === "unsupported" ? "Данные в браузере" : "Подключить папку";
  b.className = "btn mini " + (st === "connected" ? "okBtn" : st === "needs-permission" ? "warnBtn" : "");
  b.title = st === "connected" ? "Эскизы, заметки и отметки пишутся в эту папку (и в браузер)" :
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
