// Adds "PROPOSAL (reference)" ships/parts/coverage to Notes/html/ship_reference/data.js.
// Idempotent: strips earlier PROPOSAL (reference) entries first.
const fs = require("fs");
const path = require("path");
const FILE = process.argv[2];
// ship model names: the single canon (author 2026-11-08) - tools/colonization_lab/canon/ship_naming.json
const NAMING = JSON.parse(fs.readFileSync(path.join(__dirname, "..", "..", "..", "..", "tools", "colonization_lab", "canon", "ship_naming.json"), "utf8"));
const txt = fs.readFileSync(FILE, "utf8");
const sandbox = {};
new Function("window", txt)(sandbox);
const D = sandbox.ELITE_SHIP_REF_DATA;
const ST = "PROPOSAL (reference)";
D.ships = D.ships.filter(s => s.status !== ST);
D.parts = D.parts.filter(p => p.status !== ST);
delete D.coverage;
delete D.naming_doctrines;

const SIGMA = 5.670374419e-8;
const RULES = { hotel_mw_per_person: 0.015, pad_mw_per_m2: 6, rad_eff: 10, consumables: 0.006, c20_tare: 1.7, rcs_share: 0.3 };

// ---------------------------------------------------------------- new parts
const REPL_NOTE_STD = "Ближайший аналог той же кооперационной группы; габарит и интерфейсы сверять по паспорту.";
const REPL_NOTE_FOREIGN = "Внешний/вражеский функциональный аналог. Не plug-and-play: возможны переходная рама, другая разводка питания/охлаждения и ПО.";
const newParts = [
  {
    id: "srw.reactor.sr26", cat: "reactor", mfr: "mfr.sol_reactor", model_asset: "equipment/srw_sr26_reactor", name: "Sol Reactor Works SR-2.6 (малый силовой)", model: "SR-2.6",
    mass_t: 13, price_cr: 520000, mwe: 2600, eff: 0.9, overhaul_h: 30000, life_h: 50000, startup_s: 420, status: ST,
    notes: "PROPOSAL (reference): малый силовой реактор Сола в диапазоне снятых при унификации SR-1.4/2.2/3.3. Покрывает один У-33 (2.4 ГВт) + пик RCS с запасом ~8 %. Сейчас стоит на OCS-4J, GT-3000P, «Камень-С»; кандидат на замену 2×-переразмеренных SR-4.8 на OCS-4, GT-3000, «Камне» и ST-6 (решение автора).",
    geometry: {
      status: ST, envelope_m: [2.85, 2.85, 5.11], service_clearance_m: [3.28, 3.28, 5.88], cooling_pad_side_m: 7.92, cooling_pad_thickness_m: 0.25,
      anchor: "ER-5 cooling-pad scaling by waste heat",
      basis: ["same effective density as SR-4.8 / TR-2.6V (0.313 t/m3)", "pad side = sqrt(1.3 x waste / 6 MW/m2), waste = 2600 x (1/0.9 - 1) = 289 MW", "author 2026-11-09: Sol Reactor Works school = linear FRC collider (formation sections at both ends, compression chamber in the middle) - elongated along the bay axis, same volume"]
    },
    cooperation_group: "sol_industrial",
    replacements: {
      standard: { part_id: "srw.reactor.sr48", name: "Sol Reactor Works SR-4.8", manufacturer: "mfr.sol_reactor", fit: "functional_analog", note: "Старший реактор той же группы; допустим с переразмером ×1.85 (лишние 9 т и пятак 10.78 м)." },
      hostile_or_foreign: { part_id: "th.reactor.tr26v", name: "Тяньхэ ТР-2.6В", manufacturer: "mfr.tianhe_heavy", cooperation_group: "heavenly_pact", fit: "adapter_or_refit_required", note: REPL_NOTE_FOREIGN + " ТР-2.6В — военный, ресурс 8 000 ч." }
    }
  },
  {
    id: "kern.mine.cutter", cat: "mining_rig", mfr: "mfr.kern_heavy", model_asset: "equipment/kern_mk1_cutter", name: "Kern добычной комбайн МК-1 (фреза-дробилка, 200 т/ч)", model: "МК-1",
    mass_t: 45, price_cr: 900000, power_mw: 30, throughput_t_h: 200, overhaul_h: 20000, life_h: 60000, status: ST,
    notes: "PROPOSAL (reference): выносная фреза-дробилка на штанге; режет реголит и мелкие тела, подаёт дроблёнку в трюм/полость. Нет в каноне — единственная добычная деталь справочника.",
    geometry: {
      status: ST, envelope_m: [3.9, 3.9, 2.47], service_clearance_m: [4.37, 4.37, 2.77],
      basis: ["same effective density as kern.tow.heavy (1.2 t/m3)", "drum head on a boom; boom stowed along the hull"]
    },
    cooperation_group: "cognium_pool",
    replacements: {
      standard: { part_id: "kern.tow.heavy", name: "Kern тяжёлый захват (обломки, камни)", manufacturer: "mfr.kern_heavy", fit: "functional_analog", note: "Без фрезы: тело захватывается целиком и буксируется к переработке." },
      hostile_or_foreign: null
    }
  }
];
for (const p of newParts) D.parts.push(p);
const PM = new Map(D.parts.map(p => [p.id, p]));
const SM = new Map(D.ships.map(s => [s.id, s]));

// ---------------------------------------------------------------- slots / groups
// group codes are <model code>A<k> (the model code is the ship's own, author 2026-11-08): names by the A<k> part
const GROUP_NAMES = {};
for (const s of D.ships) for (const g of s.functional_groups) GROUP_NAMES["A" + g.code.match(/(\d+)$/)[1]] = g.name;
GROUP_NAMES["A13"] = GROUP_NAMES["A13"] || "Защита / посадка";
// the proposal ships' model codes (the canon passports carry their own model_code)
const PROPOSAL_MODEL_CODES = {
  "tianhe.ship.t416.tianyi": 73,
  "tianhe.ship.t421.tianti": 74,
  "harmony.ship.he-t8.yishan": 75,
  "bharat.ship.c-30.doot": 76,
  "bharat.ship.h-20.majdoor": 77,
  "bharat.ship.f-120.prahari": 78,
  "bastion.ship.ts-6f.post": 79,
  "bastion.ship.b-12l.heis": 80,
  "bastion.ship.b-20g.hulken": 81,
  "hyp.ship.f-900.atlas": 82,
  "sol.ship.ff-3000.gagarin": 83,
  "stone.ship.t-5000r.hive": 84,
  "stone.ship.ocs-4r.bucket": 85,
  "stone.ship.kb-r.stinger": 86,
  "agora.ship.pc-30ak.akritas": 87,
  "ofu.ship.ocs-4j.strizh": 36,
  "sol.ship.gt-3000p.poputchik": 40,
  "kb.ship.cobrett-i.frnu-inta.a3p": 45,
  "kern.ship.ht-20k-d.krot": 47,
  "harmony.ship.he-120p.chunyun": 53,
  "tianhe.ship.ts-6.dozor": 56,
  "tianhe.ship.b-4t.sokol-t": 57,
  "bharat.ship.c-120t.sahayak": 60,
  "bharat.ship.pk-30b.rakshak": 61,
  "bastion.ship.he-120c.oboz": 62,
  "bastion.ship.ts-6b.zastava": 63,
  "stone.ship.he-120r.brodyaga": 66,
  "stone.ship.kamen-s.salvage": 68,
  "agora.ship.pk-30a.limenarch": 69,
  "aux.craft.boarding-boat": 70,
  "aux.craft.service-drone": 71,
  "aux.craft.lifeboat-20": 72,
};
const CAT_GROUP = {
  drive: "A1", rcs: "A2", radiator: "A3", coolant_loop: "A3", reactor: "A4", service_reactor: "A4", jump_core: "A5",
  computer: "A6", radar: "A6", transponder: "A6", sensor_mast: "A6", ecm: "A6", comm: "A7", life_support: "A8", medbay: "A8",
  container_rack: "A9", bulk_hold: "A9", passenger_module: "A9", train_module: "A9", cryo_tank: "A9",
  escape_capsule: "A10", lifeboat: "A10", docking_port: "A11", tow_rig: "A11", drone_bay: "A11", workshop: "A11", boat_bay: "A11",
  hangar: "A11", mining_rig: "A11", weapon: "A12", armour: "A13", heat_shield: "A13", landing_gear: "A13"
};
const CAT_SLOT = {
  drive: "D", rcs: "Q", radiator: "T", coolant_loop: "L", reactor: "R", service_reactor: "S", jump_core: "J", computer: "M", radar: "SR",
  transponder: "SX", sensor_mast: "SM", ecm: "SE", comm: "SC", life_support: "H", medbay: "V", container_rack: "C", bulk_hold: "C",
  passenger_module: "P", train_module: "U", cryo_tank: "U", escape_capsule: "E", lifeboat: "E", docking_port: "K", tow_rig: "X",
  drone_bay: "BD", workshop: "X", boat_bay: "B", hangar: "BH", mining_rig: "XM", weapon: "W", armour: "A", heat_shield: "Z", landing_gear: "G"
};
function slotFor(p) {
  if (p.cat === "tank") return p.fluid === "jump_charge" ? "JT" : p.fluid === "argon" ? "QT" : "F";
  if (p.cat === "weapon" && p.kind === "point_defence") return "WP";
  return CAT_SLOT[p.cat];
}
function groupFor(p, slot) {
  if (p.cat === "tank") return slot === "JT" ? "A5" : slot === "QT" ? "A2" : "A9";
  return CAT_GROUP[p.cat];
}
function item(pid, qty) {
  const p = PM.get(pid);
  if (!p) throw new Error("unknown SKU " + pid);
  const slot = slotFor(p);
  return { slot_code: slot, legacy_part_id: null, part_id: pid, qty, source_qty: qty, category: p.cat, manufacturer: p.mfr, cooperation_group: p.cooperation_group, geometry_ref: pid };
}
function eqMass(bom) { return bom.reduce((a, x) => a + PM.get(x.part_id).mass_t * x.qty, 0); }
const r1 = x => Math.round(x * 10) / 10, r2 = x => Math.round(x * 100) / 100, r3 = x => Math.round(x * 1000) / 1000;

// ---------------------------------------------------------------- design language additions
const DL = D.context.design_languages;
DL.groups.lalande_open_market = DL.groups.lalande_open_market || {
  title: "Лига Лаланд: открытый рынок, реестры, оснащение чужих корпусов",
  hull: "своих корпусов нет: лицензионные корпуса союзников, оснащённые открытыми стандартными узлами std.* / agora.*",
  philosophy: "договор и реестр вместо флота; минимум оружия, максимум SAR, досмотра и арбитража",
  visual: "чужой узнаваемый корпус в белой реестровой окраске, крупные регистрационные знаки, сменные стандартные модули",
  quality: "средний/высокий; стандартизация открытого рынка",
  redundancy: "через стандартные взаимозаменяемые узлы любого порта"
};
DL.groups.auxiliary_craft = DL.groups.auxiliary_craft || {
  title: "Вспомогательные аппараты (общие для всех блоков)",
  hull: "малые аппараты, которые живут в ангарах/отсеках носителей: катер, дрон, шлюпка",
  philosophy: "один аппарат на все носители: размер задан ангаром/отсеком, а не кораблём",
  visual: "простые формы, стандартные стыковочные и захватные точки, окраска носителя",
  quality: "серийный",
  redundancy: "числом аппаратов на носителе"
};
DL.manufacturers["mfr.agora_modular"] = DL.manufacturers["mfr.agora_modular"] || {
  title: "Agora Modular (оснащение; корпуса по лицензии)",
  construction: "собирает и переоснащает лицензионные корпуса союзников открытыми стандартными узлами",
  cost: "средне",
  signature: "реестровая окраска, стандартные модули std.*, крупные сервисные люки"
};

// ---------------------------------------------------------------- ship specs
function fromParent(id) {
  const p = D.ships.find(s => s.id === id);
  return { parent: p, structure: p.mass.dry_original_t - eqMass(p.bom) };
}
const DEF_REPL = { standard: null, hostile_or_foreign: null };
const specs = [
  // ===== Sol
  {
    id: "ofu.ship.ocs-4j.strizh", name: "OCS-4J", manufacturer: "mfr.ofu", group: "sol_industrial",
    engineering_type: "light_jump_freighter", economic_class: "light_jump_freighter", role: "trade_light",
    purpose: "лёгкий прыжковый грузовик/курьер Сола: 20 C20 между Солом и ближними хабами по лицензионному ядру Когниума",
    parent: "ofu.ship.ocs-4.lastochka", hull: "variant", envelope: [56, 18, 14], form: "box",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 3, passengers: 0, cargo_t: 400, c20: 20, endurance_days: 30,
    thrust_mn: 3.3, input_mw: 2400,
    bom: [["ofu.drive.u33", 1], ["srw.reactor.sr26", 1], ["srw.apu.s60", 1], ["sol.rad.panel50", 1], ["sol.cool.loop300", 1], ["um.core.z1", 1], ["std.tank.jumpcharge", 5], ["std.tank.pellets", 2], ["std.rcs.cluster", 8], ["xz.mc.c1", 1], ["xz.radar.n2", 1], ["xz.comm.c1", 1], ["um.xpdr.t1", 1], ["terra.ls.t10", 1], ["std.surv.pod4", 1], ["std.cargo.c20lock", 20], ["std.dock.collar", 2]],
    variant: "Корпус OCS-4 (ферма-хребет): 4 из 24 гнёзд C20 заняты прыжковым модулем (ядро Я-1 + 5 баков заряда), ЖО T-10 вместо О-4 (30 суток). Реактор SR-2.6 вместо SR-4.8 убирает двукратный переразмер.",
    interchange: ["Все узлы — Сол или открытый рынок; ядро Я-1 — лицензия Когниума (ОФУ–Когниум 4/5, как у PC-30)."],
    repl: { standard: { ship_id: "ofu.ship.ocs-4.lastochka", name: "OCS-4", reason: "тот же корпус без прыжка" }, hostile_or_foreign: { ship_id: "kb.ship.cobrett-i.frnu.n1", name: "Cobrett-I N1", group: "cognium_pool", reason: "внешний аналог роли (6 C20, быстрее и дороже)" } },
    effort: 0.15
  },
  {
    id: "sol.ship.gt-3000p.poputchik", name: "GT-3000P", manufacturer: "mfr.sol_mass_yards", group: "sol_industrial",
    engineering_type: "liner", economic_class: "liner", role: "passenger",
    purpose: "дешёвый рейсовый пассажирский корабль Сола: 600 мест, почта и 90 т срочного груза на ближних линиях",
    parent: "sol.ship.gt-3000.titan", hull: "variant", envelope: [110, 30, 30], form: "cylinder",
    law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3, crew: 24, passengers: 600, cargo_t: 90, c20: 0, endurance_days: 30,
    thrust_mn: 9.9, input_mw: 7200,
    bom: [["ofu.drive.u33", 3], ["srw.reactor.sr26", 3], ["srw.apu.s60", 1], ["sol.rad.panel50", 3], ["sol.cool.loop300", 1], ["um.core.z5", 1], ["std.tank.jumpcharge", 20], ["std.tank.pellets", 10], ["std.rcs.cluster", 8], ["xz.mc.c1", 2], ["xz.radar.n2", 1], ["xz.comm.c1", 2], ["um.xpdr.t1", 2], ["terra.ls.t200", 3], ["terra.ls.t10", 3], ["std.med.bay", 2], ["agora.hab.pax100", 6], ["std.cargo.hold100", 1], ["std.surv.boat20", 32], ["std.dock.collar", 2]],
    variant: "Хребет GT-3000 с теми же тремя У-33: три криоцистерны КЦ-1000 заменены шестью пассажирскими секциями Agora PS-100 (по две на станцию бывшего бака). Добавлены ядро Я-5 и баки заряда. Реакторы SR-2.6 вместо SR-4.8 (баланс 1.08 вместо 2.4).",
    interchange: ["Секции PS-100 — открытый рынок Лаланд (Agora — штатный партнёр Сола).", "Ядро Я-5 — лицензия Когниума (4/5)."],
    repl: { standard: null, hostile_or_foreign: { ship_id: "hyp.ship.l-900.piligrim", name: "L-900 «Эос»", group: "cognium_pool", reason: "премиальный лайнер Когниума на 900 мест" } },
    effort: 0.3
  },
  // ===== Cognium
  {
    id: "kb.ship.cobrett-i.frnu-inta.a3p", name: "Cobrett-I A3P", manufacturer: "mfr.cobretty", group: "cognium_pool",
    engineering_type: "cobrett_a3", economic_class: "patrol_cutter", role: "cutter",
    purpose: "полиция, таможня и перехват для политий Когниума (общей армии нет): досмотр с катером, камера на 6, перехват нарушителей",
    parent: "kb.ship.cobrett-i.frnu-inta.a3", hull: "variant", envelope: [22.2, 26, 5], form: "wedge",
    law: "Assisted", layout: "I", installed: 4, simultaneous: 2, crew: 6, passengers: 6, cargo_t: 10, c20: 0, endurance_days: 30,
    thrust_mn: 19.12, input_mw: 9700,
    bom: [["kb.drive.cr1-vector", 4], ["kb.reactor.er5", 2], ["kb.apu.s150", 1], ["kb.rad.cs-panel", 38], ["kb.cool.loop200", 1], ["um.core.z05", 1], ["std.tank.jumpcharge", 3], ["std.tank.pellets", 2], ["xz.mc.c3", 1], ["xz.radar.n10", 1], ["xz.comm.c3", 1], ["um.xpdr.t1", 1], ["eurohab.ls.l20", 1], ["kb.surv.capsule2", 1], ["std.surv.pod4", 3], ["std.cabin.hold6", 1], ["std.bay.boat", 1], ["std.dock.collar", 1], ["kb.wpn.pd1", 2], ["fa.wpn.coil30", 1], ["std.armour.ceramic", 10]],
    variant: "Клин Cobrett-I A3 с теми же 4 гондолами и 2 ER-5: шесть гнёзд C20 заняты ангаром досмотрового катера и камерой задержанных; флотская авионика C-3/N-10, вторая PD-1, магнитная пушка КП-30, 10 т керамической брони. При ~0.33 тыс. т служебной массы даёт ~6 g — роль перехватчика закрыта тем же корпусом.",
    interchange: ["КП-30 (Твердыня, Пакт) — как на R-7/PC-30: Cobretty–Гармония 3/5.", "Ангар катера и камера — Лунно-Марсианские верфи (Cobretty–ЛМВ 4/5)."],
    open: ["Пятаки 2×ER-5 + 38 кассет C-SiC = 108 % плана клина — унаследовано от A3 (кассеты на обеих гранях).", "Ангар катера толщиной 2.4 м в клине высотой 5 м — проверить при сборке."],
    repl: { standard: { ship_id: "sol.ship.pk-30.strazh", name: "PC-30 «Фарос»", reason: "катер Сола той же роли (Сол–Когниум 4/5)" }, hostile_or_foreign: { ship_id: "bastion.ship.b-4.sokol", name: "B-4 «Фальк»", group: "citadel_compact", reason: "внешний аналог перехвата" } },
    effort: 0.2
  },
  // ===== Heavenly Pact
  {
    id: "tianhe.ship.ts-6.dozor", name: "тип 412 «Тяньван»", manufacturer: "mfr.tianhe_heavy", group: "heavenly_pact",
    engineering_type: "patrol_cutter_tianhe", economic_class: "patrol_cutter", role: "cutter",
    purpose: "сторожевой катер Тяньхэ: кордон «Филоморфоз», таможня и досмотр, SAR, охрана маяков; быстрый (≈3 g) догон нарушителей",
    parent: null, hull: "new", structure_t: 290, envelope: [44, 24, 10], form: "block",
    law: "Assisted", layout: "I", installed: 4, simultaneous: 2, crew: 12, passengers: 6, cargo_t: 20, c20: 0, endurance_days: 30,
    thrust_mn: 20.33, input_mw: 19000,
    bom: [["th.drive.tz11", 4], ["th.reactor.tr10", 2], ["th.apu.s150", 1], ["th.rad.panel50", 8], ["th.cool.loop600", 1], ["um.core.z1", 1], ["std.tank.jumpcharge", 5], ["std.tank.pellets", 4], ["xz.mc.c3", 2], ["xz.radar.n10", 1], ["xz.comm.c3", 2], ["um.xpdr.t1", 2], ["terra.ls.t10", 2], ["std.surv.pod4", 5], ["std.med.bay", 1], ["std.cabin.hold6", 1], ["std.bay.boat", 1], ["std.dock.collar", 1], ["fa.wpn.coil30", 1], ["harmony.wpn.pd3", 2], ["std.armour.ceramic", 40]],
    variant: "НОВЫЙ КОРПУС (единственный новый большой корпус справочника). Лор: Массовые верфи Тяньхэ сдают 3–6 сторожевых катеров по 300–800 т в год. Простой крупноблочный «кирпич» Тяньхэ: две пары ТЦ-11 (носовая/кормовая), два ТР-10 на спине (пятаки 16.4 м), радиаторы — откидные плавники. ТЦ-11 ограничены до 9.5 ГВт (95 %). Этот же корпус используют TS-6B Цитадели.",
    interchange: ["Силовой блок ТЦ-11 + ТР-10 — тот же, что у Хэ-F22 (общие запчасти в портах Пакта).", "Ядро Я-1 — Когниум (Гармония покупает ядра у Синь-Шанхая, 3/5); катер и камера — ЛМВ (3/5), как на Хэ-F22."],
    open: ["Пятаки 2×269 м² + 400 м² радиаторов = 89 % плана 44×24 м: радиаторы вынесены на откидные плавники."],
    repl: { standard: { ship_id: "harmony.ship.he-f22.dun", name: "Хэ-F22 «Шэнхэ»", reason: "старший корабль охраны Пакта" }, hostile_or_foreign: { ship_id: "sol.ship.pk-30.strazh", name: "PC-30 «Фарос»", group: "sol_industrial", reason: "внешний аналог роли" } },
    effort: 1.0
  },
  {
    id: "harmony.ship.he-120p.chunyun", name: "Хэ-120K «Хунъянь»", manufacturer: "mfr.harmony_yards", group: "heavenly_pact",
    engineering_type: "medium_freighter", economic_class: "liner", role: "passenger",
    purpose: "массовый пассажирский корабль Пакта: 800 мест эконом-класса, вахты, переселенцы и сезонные перевозки по линиям Пакта",
    parent: "harmony.ship.he-120", hull: "variant", envelope: [98, 32, 28], form: "box",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 30, passengers: 800, cargo_t: 90, c20: 0, endurance_days: 40,
    thrust_mn: 13, input_mw: 9000,
    bom: [["th.drive.tz28", 1], ["th.reactor.tr10", 1], ["th.apu.s150", 1], ["th.rad.panel50", 4], ["th.cool.loop600", 1], ["harmony.core.h5", 1], ["std.tank.jumpcharge", 30], ["std.tank.pellets", 8], ["std.rcs.cluster_h", 8], ["xz.mc.c1", 1], ["xz.radar.n2", 1], ["xz.comm.c1", 1], ["um.xpdr.t1", 1], ["terra.ls.t200", 4], ["terra.ls.t60", 1], ["std.med.bay", 3], ["agora.hab.pax100", 8], ["std.surv.boat20", 42], ["std.dock.collar", 2]],
    variant: "Стандартный ящик «Хэ» с внутренними гнёздами: 120 мест C20 заняты восемью секциями PS-100 (≈3 200 из 4 600 м³ гнёзд), ЖО T-200 ×4 + T-60, 42 шлюпки СШ-20 по бортам. Двигатель, реактор, ядро — без изменений. Тяга как в паспорте Хэ-120.",
    interchange: ["PS-100 — Agora (штатный партнёр Пакта).", "ЖО Terra (Сол) — как на Хэ-120/Хэ-F22."],
    open: ["Паспорт Хэ-120: 13 МН при 9 ГВт, а линейное ограничение ТЦ-28 дало бы 9 МН — унаследованное расхождение."],
    repl: { standard: { ship_id: "harmony.ship.he-120", name: "Хэ-120 «Синча»", reason: "тот же корпус, грузовой" }, hostile_or_foreign: { ship_id: "bharat.ship.t-5000.iskhod", name: "T-5000 «Правас»", group: "southern_circle", reason: "переселенческий транспорт Южного круга" } },
    effort: 0.25
  },
  {
    id: "tianhe.ship.b-4t.sokol-t", name: "тип 415 «Тяньган»", manufacturer: "mfr.tianhe_heavy", group: "heavenly_pact",
    engineering_type: "interceptor", economic_class: "interceptor", role: "interceptor",
    purpose: "перехватчик Пакта лицензионной сборки Тяньхэ: экипаж ангаров Хэ-B240 и оборона систем; Бастион не может покрыть сотни машин Пакта (0.14–0.25 корпуса в год)",
    parent: "bastion.ship.b-4.sokol", hull: "variant", envelope: [18, 12, 4], form: "dart",
    law: "Assisted", layout: "X", installed: 8, simultaneous: 4, crew: 2, passengers: 0, cargo_t: 2, c20: 0, endurance_days: 5,
    thrust_mn: 6.2, input_mw: 9500,
    bom: [["th.drive.tz16v", 8], ["th.reactor.tr26v", 4], ["th.apu.s150", 1], ["th.rad.panel50", 6], ["th.cool.loop600", 1], ["std.tank.pellets", 2], ["xz.mc.c3", 2], ["xz.radar.n10", 1], ["xz.comm.c3", 2], ["um.xpdr.t1", 2], ["ofu.ls.o4", 2], ["std.surv.pod4", 1], ["fa.wpn.coil30", 1], ["harmony.wpn.pd3", 1], ["std.armour.ceramic", 5]],
    variant: "Корпус B-4 (лицензия Бастиона на корпус; двигатели и реакторы и так Тяньхэ). Заменено то, что Бастион не экспортирует: вычислитель/связь Бастиона → Синьчжи C-3, БП-10 → КП-30 + ПТ-3, броня → керамика Твердыни. Охлаждение Cognium (кассеты C-SiC, контур CL-180) → 6 секций Тяньхэ РС-50 и контур КО-600 — снимает несоответствие матрице кооперации, отмеченное у B-4.",
    interchange: ["Бастион–Тяньхэ 3/5 («поставка силовых агрегатов Тяньхэ для Бастиона») — лицензия корпуса в обратную сторону.", "Ангар Хэ-B240 (harmony.bay.hangar2) теперь может нести свой тип 415 вместо цитадельного B-4."],
    open: ["Масса конструкции B-4 (≈20 т из 147 т) унаследована от паспорта B-4 и выглядит заниженной.", "Площадь: пятаки 4×60.5 = 242 м² разнесены на спину и брюхо (2×216 м²), радиаторы — выдвижные плавники; как и у B-4, на план 18×12 м всё сразу не помещается.", "Нужна ли лицензия Бастиона политически — решение автора."],
    repl: { standard: { ship_id: "bastion.ship.b-4.sokol", name: "B-4 «Фальк»", reason: "исходный цитадельный образец" }, hostile_or_foreign: { ship_id: "kb.ship.cobrett-i.frnu-inta.a3p", name: "Cobrett-I A3P", group: "cognium_pool", reason: "внешний аналог перехвата" } },
    effort: 0.1
  },
  // ===== Southern circle
  {
    id: "bharat.ship.pk-30b.rakshak", name: "PC-30B «Ракшак»", manufacturer: "mfr.bharat", group: "southern_circle",
    engineering_type: "patrol_cutter", economic_class: "patrol_cutter", role: "cutter",
    purpose: "береговая охрана Южного круга (Тауранга, Новая Индия, Новая Маланд): таможня по законам каждого члена, SAR, охрана хабитатов",
    parent: "sol.ship.pk-30.strazh", hull: "variant", envelope: [34, 22, 8], form: "flat",
    law: "Assisted", layout: "I", installed: 4, simultaneous: 2, crew: 9, passengers: 6, cargo_t: 20, c20: 0, endurance_days: 30,
    thrust_mn: 13, input_mw: 9000,
    bom: [["ofu.drive.u65m", 4], ["srw.reactor.sr48", 2], ["srw.apu.s60", 1], ["sol.rad.panel50", 4], ["sol.cool.loop300", 1], ["kapehu.core.k5", 1], ["std.tank.jumpcharge", 5], ["std.tank.pellets", 4], ["kapehu.mc.k1", 2], ["xz.radar.n10", 1], ["xz.comm.c3", 2], ["um.xpdr.t1", 2], ["terra.ls.t10", 2], ["std.surv.pod4", 4], ["std.med.bay", 1], ["std.cabin.hold6", 1], ["std.bay.boat", 1], ["std.dock.collar", 1], ["fa.wpn.coil30", 1], ["fa.wpn.pd2", 1], ["std.armour.ceramic", 20]],
    variant: "Корпус PC-30 лицензионной сборки Бхарат (так же, как C-120 и T-5000 на силовых узлах Сола). Своё: ядро Капеху K-5 и вычислители K-1 (своя математика прыжка). Служебный реактор S-60 вместо 2×S-200 (запас ×1.2 при полном огне вместо ×8), один контур вместо двух.",
    interchange: ["Бхарат–ЛМВ/ОФУ 3/5: гражданская сборка без общей платформы — тот же канал, что у C-120.", "Оружие Твердыни: Бхарат–Гармония 3/5."],
    open: ["K-5 (5 000 т, 40 т) переразмерен для ~0.45 тыс. т: малого ядра Капеху нет; K-1 сэкономил бы ~30 т."],
    repl: { standard: { ship_id: "sol.ship.pk-30.strazh", name: "PC-30 «Фарос»", reason: "исходный корпус Сола" }, hostile_or_foreign: { ship_id: "tianhe.ship.ts-6.dozor", name: "тип 412 «Тяньван»", group: "heavenly_pact", reason: "внешний аналог роли" } },
    effort: 0.1
  },
  {
    id: "bharat.ship.c-120t.sahayak", name: "C-120T «Сахаяк»", manufacturer: "mfr.bharat", group: "southern_circle",
    engineering_type: "fleet_tender", economic_class: "fleet_tender", role: "tender",
    purpose: "тендер снабжения Южного круга: заряд прыжка и таблетки на дальние хабы, контейнеры запчастей, мастерская и дроны на месте",
    parent: "bharat.ship.c-120.konveyer", hull: "variant", envelope: [95, 30, 26], form: "box",
    law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3, crew: 12, passengers: 0, cargo_t: 1200, c20: 60, endurance_days: 60,
    thrust_mn: 13.89, input_mw: 12900,
    bom: [["ofu.drive.u70", 3], ["srw.reactor.sr48", 3], ["srw.apu.s60", 1], ["sol.rad.panel50", 1], ["sol.cool.loop300", 1], ["kapehu.core.k5", 1], ["std.tank.jumpcharge", 330], ["std.tank.pellets", 108], ["std.rcs.cluster", 8], ["kapehu.mc.k1", 1], ["xz.radar.n2", 1], ["xz.comm.c1", 1], ["um.xpdr.t1", 1], ["eurohab.ls.l20", 1], ["std.med.bay", 1], ["std.surv.pod4", 3], ["std.cargo.c20lock", 60], ["sol.workshop.w1", 1], ["std.bay.drone4", 1], ["std.dock.collar", 4]],
    variant: "Открытая ферма C-120: половина из 120 гнёзд C20 — контейнеры запчастей, на второй половине — блоки баков (300 т заряда прыжка и 100 т таблеток для раздачи), мастерская МС-1 и отсек дронов, 4 стыковочных узла. Те же три У-70, но на 4.3 ГВт каждый (12.9 из 14.4 ГВт: баланс 1.12 вместо 1.6 у C-120).",
    interchange: ["Только узлы, уже стоящие на C-120/T-5000, плюс мастерская ЛМВ и дроны Agora."],
    repl: { standard: { ship_id: "bharat.ship.c-120.konveyer", name: "C-120 «Вахак»", reason: "тот же корпус, грузовой" }, hostile_or_foreign: { ship_id: "harmony.ship.he-s240.tylovik", name: "Хэ-B240 «Бутянь»", group: "heavenly_pact", reason: "тендер флота Пакта" } },
    effort: 0.25
  },
  // ===== Citadel
  {
    id: "bastion.ship.ts-6b.zastava", name: "TS-6B «Варде»", manufacturer: "mfr.bastion", group: "citadel_compact",
    engineering_type: "patrol_cutter_tianhe", economic_class: "patrol_cutter", role: "cutter",
    purpose: "досмотровый катер Цитадели: закрытые рынки и досмотры, дальний радарный пост 50 000 км; без ядра (внутрисистемный)",
    parent: "tianhe.ship.ts-6.dozor", hull: "variant", envelope: [44, 24, 10], form: "block",
    law: "Assisted", layout: "I", installed: 4, simultaneous: 2, crew: 12, passengers: 6, cargo_t: 20, c20: 0, endurance_days: 30,
    thrust_mn: 20.33, input_mw: 19000,
    bom: [["th.drive.tz11", 4], ["th.reactor.tr10", 2], ["th.apu.s150", 1], ["th.rad.panel50", 11], ["th.cool.loop600", 1], ["std.tank.pellets", 4], ["bastion.mc.w1", 2], ["bastion.radar.s50", 1], ["bastion.comm.w1", 2], ["um.xpdr.t1", 2], ["terra.ls.t10", 2], ["std.surv.pod4", 5], ["std.med.bay", 1], ["std.cabin.hold6", 1], ["std.bay.boat", 1], ["std.dock.collar", 1], ["bastion.wpn.coil10", 2], ["bastion.wpn.pd4", 2], ["bastion.armour.heavy", 60]],
    variant: "Корпус тип 412 Тяньхэ, оснащение Бастиона: без ядра Когниума (внутрисистемный), вычислитель/связь/радар С-50/оружие/броня — свои. Закрывает разрыв между B-4 (140 т) и B-90 (9 000 т) в досмотре; лор называет «корветы» Бастиона.",
    interchange: ["Корпус и силовой блок — Тяньхэ (Бастион–Тяньхэ 3/5, как ТЦ-28/ТР-10 на B-90).", "Гражданские узлы (ЖО Terra, медотсек, ангар катера ЛМВ) — «отдельные совместимые гражданские узлы», как на B-90."],
    open: ["Пятаки 538 м² + 550 м² радиаторов > плана 44×24: радиаторы на брюхе и плавниках."],
    repl: { standard: { ship_id: "bastion.ship.b-4.sokol", name: "B-4 «Фальк»", reason: "перехват; досмотр не может" }, hostile_or_foreign: { ship_id: "sol.ship.pk-30.strazh", name: "PC-30 «Фарос»", group: "sol_industrial", reason: "внешний аналог роли" } },
    effort: 0.15
  },
  {
    id: "bastion.ship.he-120c.oboz", name: "B-120F", manufacturer: "mfr.bastion", group: "citadel_compact",
    engineering_type: "medium_freighter", economic_class: "medium_freighter", role: "trade_medium",
    purpose: "гражданский грузовик Цитадели: корпус Гармонии, бортовые вычислитель и связь Бастиона, две турели ПРО; сырьё и гражданский груз своих маршрутов",
    parent: "harmony.ship.he-120", hull: "variant", envelope: [98, 32, 28], form: "box",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 7, passengers: 0, cargo_t: 2400, c20: 120, endurance_days: 40,
    thrust_mn: 13, input_mw: 9000,
    bom: [["th.drive.tz28", 1], ["th.reactor.tr10", 1], ["th.apu.s150", 1], ["th.rad.panel50", 5], ["th.cool.loop600", 1], ["harmony.core.h5", 1], ["std.tank.jumpcharge", 30], ["std.tank.pellets", 8], ["std.rcs.cluster_h", 8], ["bastion.mc.w1", 1], ["xz.radar.n2", 1], ["bastion.comm.w1", 1], ["um.xpdr.t1", 1], ["terra.ls.t10", 1], ["std.surv.pod4", 2], ["std.cargo.c20lock", 120], ["std.dock.collar", 2], ["bastion.wpn.pd4", 2], ["bastion.armour.heavy", 40]],
    variant: "Покупной корпус Хэ-120 (почти весь гражданский тоннаж — Гармония; Бастион–Гармония 3/5), доведённый Бастионом: чужие вычислитель и связь не допускаются — свои БВ-1/БС-1; две утопленные ПРО БТ-4, 40 т брони на рубку и ядро, пятая секция радиатора под турели.",
    interchange: ["Навигационный радар N-2 и транспондер Т-1 — «отдельные совместимые гражданские узлы» (Бастион–Синьчжи/УМ 1/5), как Т-1 на B-90."],
    open: ["Ядро Х-5 Гармонии собрано из блоков Когниума; своего малого ядра у Бастиона нет (только Б-12 на 12 000 т). Примет ли его Цитадель — решение автора.", "Тяга как в паспорте Хэ-120 (унаследованное расхождение 13 / 9 МН)."],
    repl: { standard: { ship_id: "harmony.ship.he-120", name: "Хэ-120 «Синча»", reason: "исходный корпус Гармонии" }, hostile_or_foreign: { ship_id: "bharat.ship.c-120.konveyer", name: "C-120 «Вахак»", group: "southern_circle", reason: "внешний аналог роли" } },
    effort: 0.1
  },
  // ===== Free Belts
  {
    id: "stone.ship.he-120r.brodyaga", name: "«Бродяга» (рейдер-переделка Хэ-120)", manufacturer: "mfr.stone_docks", group: "free_belts",
    engineering_type: "medium_freighter", economic_class: "raider", role: "raider",
    purpose: "вооружённый торговец / рейдер Пояса Раздора: контрабанда, абордаж, рейды против Гармонии; половина гнёзд — груз, половина — оружие и абордаж",
    parent: "harmony.ship.he-120", hull: "variant", envelope: [98, 32, 28], form: "box",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 20, passengers: 0, cargo_t: 1200, c20: 60, endurance_days: 60,
    thrust_mn: 13, input_mw: 9000,
    bom: [["th.drive.tz28", 1], ["th.reactor.tr10", 1], ["th.apu.s150", 1], ["th.rad.panel50", 9], ["th.cool.loop600", 1], ["harmony.core.h5", 1], ["std.tank.jumpcharge", 30], ["std.tank.pellets", 10], ["std.rcs.cluster_h", 8], ["xz.mc.c3", 1], ["xz.radar.n10", 1], ["xz.comm.c1", 1], ["terra.ls.t60", 1], ["std.surv.pod4", 5], ["std.med.bay", 1], ["std.cargo.c20lock", 60], ["std.bay.boat", 1], ["std.dock.collar", 2], ["fa.wpn.coil30", 2], ["fa.wpn.pd2", 2], ["harmony.wpn.vls8", 1], ["std.armour.ceramic", 60]],
    variant: "Лор: Пояс Раздора выпускает «вооружённых торговцев, быстрых контрабандистов, восстановленные списанные корпуса Гармонии». Списанный Хэ-120 из Каменных доков: штатная силовая часть и ядро донора, 60 из 120 гнёзд C20 сняты под две КП-30, две ПТ-2, ВПУ-8 и ангар абордажного катера; заплаты из 60 т керамики; транспондера нет (как у «Камня»). Новой сетки не требует: кит-баш и текстура поверх Хэ-120.",
    interchange: ["Каменные доки–Гармония 2/5 (гражданские и восстановленные агрегаты): силовая часть донора штатна.", "Оружие Твердыни и флотская авионика — серый канал (краденое/трофейное), не плановая кооперация."],
    open: ["Тяга как в паспорте Хэ-120 (унаследованное расхождение)."],
    repl: { standard: { ship_id: "stone.ship.kamen.tanker", name: "«Камень»", reason: "другой корпус Каменных доков" }, hostile_or_foreign: { ship_id: "harmony.ship.he-f22.dun", name: "Хэ-F22 «Шэнхэ»", group: "heavenly_pact", reason: "противник: эскорт конвоев Гармонии" } },
    effort: 0.2
  },
  {
    id: "stone.ship.kamen-s.salvage", name: "«Камень-С» (добытчик-спасатель)", manufacturer: "mfr.stone_docks", group: "free_belts",
    engineering_type: "miner", economic_class: "miner", role: "tug_miner",
    purpose: "добытчик и салвидж Пояса: фреза дробит мелкие тела, полость астероида — рудный бункер; тяжёлый захват и мастерская разделывают обломки",
    parent: "stone.ship.kamen.tanker", hull: "variant", envelope: [60, 45, 40], form: "asteroid",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 8, passengers: 0, cargo_t: 6000, c20: 0, endurance_days: 90,
    thrust_mn: 3.3, input_mw: 2400,
    bom: [["ofu.drive.u33", 1], ["srw.reactor.sr26", 1], ["srw.apu.s60", 1], ["sol.rad.panel50", 3], ["sol.cool.loop300", 1], ["std.tank.pellets", 6], ["std.rcs.cluster", 8], ["xz.mc.c1", 1], ["kern.radar.deb5", 1], ["xz.comm.c1", 1], ["eurohab.ls.l20", 1], ["std.surv.pod4", 2], ["std.dock.collar", 1], ["kern.mine.cutter", 1], ["kern.tow.heavy", 1], ["sol.workshop.w1", 1], ["std.bay.drone4", 1]],
    variant: "Тот же процедурный астероид + ферма с У-33: полость вместо цистерны — рудный бункер на 6 000 т; на ферме — фреза МК-1, тяжёлый захват Kern и мастерская; дроны для разделки обломков. SR-2.6 вместо SR-4.8 (баланс 1.08 вместо 2.0).",
    interchange: ["Узлы Kern и ЛМВ — рынок б/у (Каменные доки–Kern/ЛМВ 2/5)."],
    open: ["Один У-33 на 8.6 тыс. т с полным бункером — 0.04 g, как у исходного «Камня»: медленно, но в духе Пояса."],
    repl: { standard: { ship_id: "stone.ship.kamen.tanker", name: "«Камень»", reason: "тот же корпус-астероид" }, hostile_or_foreign: { ship_id: "kern.ship.ht-20k-d.krot", name: "HT-20K-D", group: "cognium_pool", reason: "серийный добытчик Kern" } },
    effort: 0.2
  },
  // ===== Cross-bloc miner (Kern)
  {
    id: "kern.ship.ht-20k-d.krot", name: "HT-20K-D", manufacturer: "mfr.kern_heavy", group: "cognium_pool",
    engineering_type: "miner", economic_class: "miner", role: "tug_miner",
    purpose: "межблоковый добытчик-проспектор Kern: разведка и выемка мелких тел, 1 000 т дроблёнки в трюмах; концессии Kern во многих системах, на Груве-7 такие машины зовут «Хулк»",
    parent: "kern.ship.ht-20k.mul", hull: "variant", envelope: [60, 25, 25], form: "tug",
    law: "Newtonian", layout: "X", installed: 4, simultaneous: 4, crew: 5, passengers: 0, cargo_t: 1000, c20: 0, endurance_days: 90,
    thrust_mn: 4, input_mw: 3000,
    bom: [["kern.drive.kt1", 4], ["kb.reactor.er1", 4], ["srw.apu.s60", 2], ["sol.rad.panel50", 6], ["sol.cool.loop300", 1], ["std.tank.pellets", 12], ["std.rcs.cluster", 8], ["xz.mc.c1", 1], ["kern.radar.deb5", 1], ["xz.radar.n10", 1], ["xz.comm.c1", 1], ["um.xpdr.t1", 1], ["eurohab.ls.l20", 1], ["std.surv.pod4", 2], ["kern.mine.cutter", 2], ["std.cargo.hold100", 10], ["std.bay.drone4", 1], ["std.dock.collar", 1]],
    variant: "Открытая силовая рама HT-20K с теми же 4 KT-1 и 4 ER-1: рама плота снята, на её месте две фрезы МК-1 на штангах и пакет из 10 трюмов ГО-100; обзорный N-10 для разведки, дроны-пробоотборники.",
    interchange: ["Всё — пул Когниума, Сол или открытый рынок; Цитадель может купить машину целиком как «отдельный гражданский узел» (Бастион–Kern 1/5)."],
    repl: { standard: { ship_id: "kern.ship.ht-20k.mul", name: "HT-20K", reason: "тот же корпус, буксир плотов" }, hostile_or_foreign: { ship_id: "stone.ship.kamen-s.salvage", name: "«Камень-С» (добытчик-спасатель)", group: "free_belts", reason: "кустарный добытчик Пояса" } },
    effort: 0.25
  },
  // ===== Lalande League
  {
    id: "agora.ship.pk-30a.limenarch", name: "PC-30A «Лименарх»", manufacturer: "mfr.agora_modular", group: "lalande_open_market",
    engineering_type: "patrol_cutter", economic_class: "patrol_cutter", role: "cutter",
    purpose: "таможня, SAR и защита нейтральной торговли Лиги Лаланд (Агора живёт транзитом); минимум оружия, усиленная медицина и спасение",
    parent: "sol.ship.pk-30.strazh", hull: "variant", envelope: [34, 22, 8], form: "flat",
    law: "Assisted", layout: "I", installed: 4, simultaneous: 2, crew: 9, passengers: 6, cargo_t: 20, c20: 0, endurance_days: 30,
    thrust_mn: 13, input_mw: 9000,
    bom: [["ofu.drive.u65m", 4], ["srw.reactor.sr48", 2], ["srw.apu.s60", 1], ["sol.rad.panel50", 2], ["sol.cool.loop300", 1], ["um.core.z1", 1], ["std.tank.jumpcharge", 5], ["std.tank.pellets", 4], ["xz.mc.c3", 2], ["xz.radar.n10", 1], ["xz.comm.c3", 2], ["um.xpdr.t1", 2], ["terra.ls.t10", 2], ["std.surv.pod4", 4], ["std.surv.boat20", 1], ["std.med.bay", 2], ["std.cabin.hold6", 1], ["std.bay.boat", 1], ["std.dock.collar", 1], ["fa.wpn.pd2", 1], ["std.armour.ceramic", 10]],
    variant: "Корпус PC-30 (ЛМВ по лицензии), оснащение Agora Modular: магнитная пушка снята (слабая милитаризация Лиги), остаётся ПРО ПТ-2 против обломков и ракет; второй медотсек и шлюпка СШ-20 на 20 спасённых; S-60 вместо 2×S-200, вдвое меньше брони и радиаторов. Своих корпусов у Лиги нет — это реестровый вариант.",
    interchange: ["Agora — компонентный партнёр всех групп; корпус и силовая часть — Сол (ЛМВ/ОФУ).", "ПТ-2 — Твердыня, как на PC-30."],
    repl: { standard: { ship_id: "sol.ship.pk-30.strazh", name: "PC-30 «Фарос»", reason: "исходный корпус Сола" }, hostile_or_foreign: { ship_id: "bharat.ship.pk-30b.rakshak", name: "PC-30B «Ракшак»", group: "southern_circle", reason: "соседи по системе Лаланд (Тауранга), тот же корпус" } },
    effort: 0.1
  },
  // ===== Auxiliary craft
  {
    id: "aux.craft.boarding-boat", name: "Досмотровый катер АК-1К", manufacturer: "mfr.sol_mass_yards", group: "auxiliary_craft",
    engineering_type: "small_craft", economic_class: "small_craft", role: "small_craft",
    purpose: "досмотровый/абордажный катер из ангара АК-1 (std.bay.boat): доставка досмотровой группы, стыковка к досматриваемому кораблю",
    parent: null, hull: "craft", structure_t: 10, envelope: [4, 9, 3], form: "boat",
    law: "Newtonian", layout: "I", installed: 1, simultaneous: 1, crew: 2, passengers: 6, cargo_t: 1, c20: 0, endurance_days: 1,
    thrust_mn: 1, input_mw: 750, service_from_propulsion: true,
    bom: [["kern.drive.kt1", 1], ["kb.reactor.er1", 1], ["kb.rad.cs-panel", 2], ["kb.cool.loop200", 1], ["std.tank.pellets", 1], ["kb.rcs.fc-nozzle", 8], ["xz.mc.c1", 1], ["xz.radar.n2", 1], ["xz.comm.c1", 1], ["um.xpdr.t1", 1], ["ofu.ls.o4", 2], ["std.dock.collar", 1]],
    variant: "Новый малый аппарат (габарит 9×4×3 м из craft_envelope_m ангара АК-1). Один KT-1 и ER-1 (пятак 4.48 м на спине); собственного служебного реактора нет — бортовые 3–4 МВт берутся из запаса ER-1 (80 МВт), как у единой силовой установки.",
    interchange: ["Узлы Kern/Cobretty в аппарате ЛМВ: Сол–Когниум 4/5."],
    open: ["Ангар АК-1 (4.9×2.4×6.1 м) не вмещает катер 9×4×3 м — габарит ангара считается по плотности и требует правки.", "Если 18 т ангара не включают катер, PC-30, Хэ-F22 и все новые катера тяжелеют ещё на ~28 т."],
    repl: DEF_REPL, effort: 0.3
  },
  {
    id: "aux.craft.service-drone", name: "Сервисный дрон ОД-4Д", manufacturer: "mfr.agora_modular", group: "auxiliary_craft",
    engineering_type: "small_craft", economic_class: "small_craft", role: "small_craft",
    purpose: "сервисный/разведывательный дрон из отсека ОД-4 (std.bay.drone4, 4 дрона): осмотр корпуса, сварка, пробы, ретрансляция",
    parent: null, hull: "craft", mass_override_t: 0.6, envelope: [1.2, 2.0, 0.8], form: "drone",
    law: "Newtonian", layout: "I", installed: 0, simultaneous: 0, crew: 0, passengers: 0, cargo_t: 0.05, c20: 0, endurance_days: 1,
    thrust_mn: 0, input_mw: 0, lru_carrier: true,
    bom: [["std.bay.drone4", 1]],
    variant: "Дрон — расходная часть LRU ОД-4 (4 дрона входят в 6 т отсека); отдельного SKU нет. Холодный газ/электрические сопла, аккумулятор. Габарит 2.0×1.2×0.8 м, 0.6 т — оценка справочника.",
    interchange: ["Как отсек ОД-4: открытый рынок Лаланд, ставится на R-7, B-90, PDT-7, ST-6, Хэ-B240 и новые C-120T, «Камень-С», HT-20K-D."],
    repl: DEF_REPL, effort: 0.1
  },
  {
    id: "aux.craft.lifeboat-20", name: "Спасательная шлюпка СШ-20 (аппарат)", manufacturer: "mfr.agora_modular", group: "auxiliary_craft",
    engineering_type: "small_craft", economic_class: "small_craft", role: "small_craft",
    purpose: "отстреливаемая шлюпка на 20 мест, 10 суток; самый массовый малый аппарат флота (L-900 несёт 47, GT-3000P — 32, Хэ-120K — 42)",
    parent: null, hull: "craft", mass_override_t: 6, envelope: [2.962, 6.515, 2.073], form: "lifeboat",
    law: "Newtonian", layout: "I", installed: 0, simultaneous: 0, crew: 0, passengers: 20, cargo_t: 0, c20: 0, endurance_days: 10,
    thrust_mn: 0, input_mw: 0, lru_carrier: true,
    bom: [["std.surv.boat20", 1]],
    variant: "Шлюпка — сама LRU std.surv.boat20 (её паспорт — паспорт детали); запись нужна, чтобы шлюпка попала в список моделей. Модуль-убежище СМ-100 (T-5000) не летает и остаётся деталью.",
    interchange: ["Открытый рынок Лаланд: одинакова на всех кораблях."],
    repl: DEF_REPL, effort: 0.15
  }
];

// ---------------------------------------------------------------- build
function balance(sp, bom, dry) {
  const by = c => bom.filter(x => PM.get(x.part_id).cat === c);
  const sum = (c, f) => by(c).reduce((a, x) => a + (PM.get(x.part_id)[f] || 0) * x.qty, 0);
  const persons = sp.crew + sp.passengers;
  const other = bom.filter(x => !["drive", "rcs", "weapon"].includes(PM.get(x.part_id).cat)).reduce((a, x) => a + (PM.get(x.part_id).power_mw || 0) * x.qty, 0);
  const weapons = sum("weapon", "power_mw"), pumps = sum("coolant_loop", "pump_mw");
  const base = persons * RULES.hotel_mw_per_person + other + pumps;
  const hotel30 = base + 0.3 * weapons, hotel100 = base + weapons;
  const apuItems = by("service_reactor");
  const apu = sum("service_reactor", "mwe");
  const apuEff = apuItems.length ? PM.get(apuItems[0].part_id).eff : 0.9;
  const heat30 = hotel30 / apuEff, heat100 = hotel100 / apuEff;
  const rad = by("radiator").reduce((a, x) => { const p = PM.get(x.part_id); return a + RULES.rad_eff * p.eps * SIGMA * p.temp_k ** 4 * p.area_m2 * x.qty * 1e-6; }, 0);
  const radArea = sum("radiator", "area_m2");
  const loop = sum("coolant_loop", "capacity_mw");
  const rx = by("reactor"), rxMwe = sum("reactor", "mwe");
  const rcsPeak = sum("rcs", "input_mw") * RULES.rcs_share;
  const groups = sp.law === "Assisted" ? sp.simultaneous : sp.installed;
  const rxQty = rx.reduce((a, x) => a + x.qty, 0);
  const rxEff = rx.length ? PM.get(rx[0].part_id).eff : 0.9;
  const padReq = 1.3 * sp.input_mw * (1 / rxEff - 1) / RULES.pad_mw_per_m2;
  const padInst = rx.reduce((a, x) => a + (PM.get(x.part_id).geometry.cooling_pad_side_m || 0) ** 2 * x.qty, 0);
  const plan = sp.envelope[0] * sp.envelope[1];
  const tanks = by("tank").reduce((a, x) => a + (PM.get(x.part_id).capacity_t || 0) * x.qty, 0);
  const lsDays = Math.min(...by("life_support").map(x => PM.get(x.part_id).days), 1e9);
  const consumables = persons * Math.min(lsDays === 1e9 ? sp.endurance_days : lsDays, sp.endurance_days) * RULES.consumables;
  const gross = dry + tanks + consumables + 0.09 * persons + sp.cargo_t + RULES.c20_tare * sp.c20;
  const core = by("jump_core")[0];
  const coreRating = core ? PM.get(core.part_id).rating_t * core.qty : 0;
  const lsPersons = sum("life_support", "persons") + sum("passenger_module", "ls_persons");
  const seats = bom.filter(x => ["escape_capsule", "lifeboat"].includes(PM.get(x.part_id).cat)).reduce((a, x) => a + (PM.get(x.part_id).seats || 0) * x.qty, 0) + sum("passenger_module", "survival_seats");
  const svcCap = sp.service_from_propulsion ? rxMwe - sp.input_mw : apu;
  const propNeed = sp.input_mw + rcsPeak + (sp.service_from_propulsion ? hotel100 : 0);
  const b = {
    persons, propulsion_reactor_mw: rxMwe, propulsion_demand_mw: r1(propNeed), propulsion_ratio: propNeed ? r3(rxMwe / propNeed) : null,
    reactor_per_group_mw: groups ? r1(rxMwe / groups) : null, group_demand_mw: groups ? r1(sp.input_mw / groups) : null, reactor_banks_per_group: groups ? r2(rxQty / groups) : null,
    service_capacity_mw: r1(svcCap), hotel_mw_weapons30: r1(hotel30), hotel_mw_weapons100: r1(hotel100),
    service_ratio_weapons100: hotel100 ? r2(svcCap / hotel100) : null,
    service_heat_mw_weapons30: r1(heat30), service_heat_mw_weapons100: r1(heat100), radiator_mw: r1(rad), radiator_area_m2: radArea, coolant_loop_mw: loop,
    radiator_ratio_weapons100: heat100 ? r2(rad / heat100) : null,
    pad_required_m2: r1(padReq), pad_installed_m2: r1(padInst), plan_area_m2: r1(plan), cooling_surface_share_of_plan: plan ? r2((padInst + radArea) / plan) : null,
    gross_t: r1(gross), accel_g_gross: sp.thrust_mn ? r2(sp.thrust_mn * 1e6 / (gross * 1e3) / 9.80665) : null,
    jump_core_rating_t: coreRating || null, life_support_persons: lsPersons, survival_seats: seats
  };
  const flags = [];
  if (!sp.lru_carrier) {
    if (b.propulsion_ratio < 1) flags.push("FAIL power: reactors < propulsion demand");
    else if (b.propulsion_ratio > 1.35) flags.push("WARN power: reactor oversize ×" + b.propulsion_ratio);
    if (groups && b.reactor_per_group_mw < 0.98 * b.group_demand_mw) flags.push("FAIL reactor per group");
    if (b.service_capacity_mw < 1.15 * hotel30) flags.push("FAIL service < 1.15 × hotel");
    if (b.service_capacity_mw < hotel100) flags.push("FAIL service < full-fire hotel");
    if (rad < 1.1 * heat30) flags.push("FAIL radiators < 1.1 × service heat");
    if (rad < heat100) flags.push("WARN radiators < full-fire heat");
    if (loop < 1.1 * heat30) flags.push("FAIL loops < 1.1 × service heat");
    if (padInst < padReq) flags.push("FAIL pads < required");
    if (core && coreRating < gross) flags.push("FAIL jump core rating < gross");
    if (lsPersons < persons) flags.push("FAIL life support places");
    if (seats + (sp.hull === "craft" ? persons : 0) < persons) flags.push("FAIL survival seats");
    if (b.cooling_surface_share_of_plan > 1) flags.push("WARN pads+radiators exceed plan area (fins / both faces)");
  }
  b.flags = flags;
  return b;
}


// ---------------------------------------------------------------- bloc completeness (author 2026-11-08)
// "проверь, все ли военно-транспортные политические блоки укомплектованы всеми необходимыми им типами кораблей -
// грузовики, пассажирские, военные, охранные, тягачи и пр.; если не хватает - сгенерируй". A role counts as
// covered when the bloc has its own model or a partner's hull at availability >= 3/5; the raider role belongs to
// the free belts only. The 15 gaps below are variants of hulls that already exist (no new meshes).
const derivedSoFar = [];
function parentBom(id) {
  const sp = specs.find(x => x.id === id) || derivedSoFar.find(x => x.id === id);
  if (sp) return sp.bom.map(([p, q]) => [p, q]);
  return SM.get(id).bom.map(x => [x.part_id, x.qty]);
}
// set: {part: qty} (0 removes, a new part is appended)
function derive(id, set = {}) {
  let b = parentBom(id);
  for (const [p, q] of Object.entries(set)) {
    const e = b.find(x => x[0] === p);
    if (q === 0) b = b.filter(x => x[0] !== p);
    else if (e) e[1] = q;
    else b.push([p, q]);
  }
  for (const [p] of b) if (!PM.get(p)) throw new Error("derive: unknown part " + p + " (" + id + ")");
  return b;
}
// rated thrust / input of the simultaneous drives (one drive type per ship)
function drivePower(bom, simultaneous) {
  const d = bom.find(([p]) => PM.get(p).cat === "drive"), P = PM.get(d[0]);
  return { thrust_mn: r2(P.thrust_mn * simultaneous), input_mw: P.input_mw * simultaneous };
}
function variantSpec(o) {
  const bom = derive(o.parent, o.set);
  const pw = drivePower(bom, o.simultaneous);
  const spec = { hull: "variant", repl: DEF_REPL, effort: 0.15, ...o, bom, thrust_mn: pw.thrust_mn, input_mw: pw.input_mw };
  derivedSoFar.push(spec);
  return spec;
}
const TH_PACK = { "th.rad.panel50": 6, "th.cool.loop600": 1 };
const moreSpecs = [
  // ===== Heavenly Pact
  variantSpec({
    id: "tianhe.ship.t416.tianyi", name: "тип 416 «Тяньи»", manufacturer: "mfr.tianhe_heavy", group: "heavenly_pact",
    engineering_type: "light_jump_freighter", economic_class: "light_jump_freighter", role: "trade_light",
    purpose: "лёгкий прыжковый курьер Пакта: почта, документы, срочный груз между столицами блока",
    parent: "tianhe.ship.ts-6.dozor", envelope: [44, 24, 10], form: "block", law: "Assisted", layout: "I", installed: 4, simultaneous: 2,
    crew: 4, passengers: 0, cargo_t: 160, c20: 8, endurance_days: 30,
    set: { "std.armour.ceramic": 0, "fa.wpn.coil30": 0, "harmony.wpn.pd3": 1, "std.bay.boat": 0, "std.cabin.hold6": 0, "std.med.bay": 0, "std.surv.pod4": 2, "terra.ls.t10": 1, "xz.mc.c3": 1, "xz.comm.c3": 1, "um.xpdr.t1": 1, "std.cargo.c20lock": 8 },
    variant: "Корпус 412型 без брони, пушки, катера и камеры: восемь гнёзд C20, одна PD-3.",
    interchange: ["Силовой блок ТЦ-11 + ТР-10 — общий с 412型 и Хэ-F22.", "Ядро Я-1 — Когниум (3/5)."]
  }),
  variantSpec({
    id: "tianhe.ship.t421.tianti", name: "тип 421 «Тяньти»", manufacturer: "mfr.tianhe_heavy", group: "heavenly_pact",
    engineering_type: "surface_lifter", economic_class: "surface_lifter", role: "shuttle",
    purpose: "подъёмник Пакта поверхность ↔ низкая орбита: своя малая силовая пара Тяньхэ вместо покупки SL-12",
    parent: "ofu.ship.sl-12.podyomnik", envelope: [48, 16, 16], form: "lifter", law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3,
    crew: 4, passengers: 0, cargo_t: 260, c20: 12, endurance_days: 3,
    set: { "ofu.drive.u73l": 0, "th.drive.tz11": 3, "srw.reactor.sr48": 0, "th.reactor.tr10": 4, "srw.apu.s60": 0, "th.apu.s150": 1, "sol.rad.panel50": 0, "sol.cool.loop300": 0, ...TH_PACK },
    variant: "Корпус SL-12 по лицензии ОФУ: силовая часть Тяньхэ (три ТЦ-11, четыре ТР-10), опоры и теплозащита ОФУ.",
    interchange: ["Опоры и теплозащита — ОФУ (Сол–Пакт 3/5).", "Силовая часть — общая с 412型 / Хэ-F22."]
  }),
  variantSpec({
    id: "harmony.ship.he-t8.yishan", name: "Хэ-T8 «Ишань»", manufacturer: "mfr.harmony_yards", group: "heavenly_pact",
    engineering_type: "ore_tug", economic_class: "ore_tug", role: "tug_miner",
    purpose: "ледовый и рудный тягач Пакта внутри системы: плоты льда и руды к заводам (без прыжка)",
    parent: "harmony.ship.he-t12.lokomotiv", envelope: [160, 40, 40], form: "train", law: "Newtonian", layout: "X", installed: 2, simultaneous: 2,
    crew: 8, passengers: 0, cargo_t: 0, c20: 0, endurance_days: 60,
    set: { "harmony.core.h80": 0, "std.tank.jumpcharge": 0, "harmony.mod.cargo4800": 0, "th.drive.tz28": 2, "th.reactor.tr10": 6, "std.tank.pellets": 60, "kern.tow.raft20k": 2, "th.rad.panel50": 14 },
    variant: "Тягач поезда Хэ-T12 без ядра и модулей: две ТЦ-28, две рамы плотов Kern 20K (до 40 000 т), запас топлива на 60 суток.",
    interchange: ["Рамы плотов Kern — Когниум (3/5).", "Тяговый блок — общий с поездом Хэ-T12."]
  }),
  // ===== Southern Circle
  variantSpec({
    id: "bharat.ship.c-30.doot", name: "C-30 «Дут»", manufacturer: "mfr.bharat", group: "southern_circle",
    engineering_type: "light_jump_freighter", economic_class: "light_jump_freighter", role: "trade_light",
    purpose: "лёгкий прыжковый грузовик и курьер Южного круга: срочный груз и почта между Новой Индией, Капеху и Маланд",
    parent: "bharat.ship.pk-30b.rakshak", envelope: [34, 22, 8], form: "flat", law: "Assisted", layout: "I", installed: 4, simultaneous: 2,
    crew: 4, passengers: 2, cargo_t: 120, c20: 6, endurance_days: 30,
    set: { "fa.wpn.coil30": 0, "fa.wpn.pd2": 1, "std.armour.ceramic": 0, "std.bay.boat": 0, "std.cabin.hold6": 0, "std.surv.pod4": 2, "std.cargo.c20lock": 6 },
    variant: "Корпус PC-30B (лицензия Сола) без пушки, брони, катера и камеры: шесть гнёзд C20.",
    interchange: ["Корпус и двигатели — Сол (лицензия, как у PC-30B).", "Ядро K-5 — Капеху."]
  }),
  variantSpec({
    id: "bharat.ship.h-20.majdoor", name: "H-20 «Мазду́р»", manufacturer: "mfr.bharat", group: "southern_circle",
    engineering_type: "ore_tug", economic_class: "ore_tug", role: "tug_miner",
    purpose: "рудный буксир Южного круга: плоты руды и льда до 20 000 т по лицензии Керна",
    parent: "kern.ship.ht-20k.mul", envelope: [60, 25, 25], form: "tug", law: "Newtonian", layout: "X", installed: 4, simultaneous: 4,
    crew: 4, passengers: 0, cargo_t: 0, c20: 0, endurance_days: 60,
    set: { "kb.reactor.er1": 0, "srw.reactor.sr48": 1, "xz.mc.c1": 0, "kapehu.mc.k1": 1 },
    variant: "Корпус HT-20K по лицензии Керна: реактор Сола SR-4.8 вместо четырёх ER-1, бортовой компьютер Капеху.",
    interchange: ["Двигатели KT-1 и рама плота — Kern (лицензия).", "Реактор — Сол."]
  }),
  variantSpec({
    id: "bharat.ship.f-120.prahari", name: "F-120 «Прахари»", manufacturer: "mfr.bharat", group: "southern_circle",
    engineering_type: "escort_frigate", economic_class: "escort_frigate", role: "escort_line",
    purpose: "фрегат Южного круга: охрана конвоев и систем Новой Индии и Капеху (линейных у блока — 8–9, своих корпусов не было)",
    parent: "bharat.ship.c-120.konveyer", envelope: [95, 30, 26], form: "box", law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3,
    crew: 60, passengers: 0, cargo_t: 40, c20: 0, endurance_days: 60,
    set: { "std.cargo.c20lock": 0, "srw.reactor.sr48": 5, "sol.rad.panel50": 10, "srw.apu.s60": 0, "srw.apu.s200": 1, "kapehu.mc.k1": 2, "xz.radar.n2": 0, "xz.radar.n10": 2, "xz.comm.c1": 0, "xz.comm.c3": 2,
           "eurohab.ls.l20": 0, "terra.ls.t60": 1, "std.surv.pod4": 0, "std.surv.boat20": 3, "std.med.bay": 1, "std.bay.boat": 1,
           "bastion.wpn.vls16": 1, "fa.wpn.pd2": 4, "std.armour.ceramic": 120 },
    variant: "Корпус C-120 без контейнерных гнёзд: ракетная шахта Бастиона ВПУ-16 (Бхарат — главный поставщик Цитадели), четыре PD-2, 120 т керамики.",
    interchange: ["ВПУ-16 — Бастион (поставщик ↔ покупатель).", "PD-2 и керамика — Твердыня (Пакт), как у PC-30B."]
  }),
  // ===== Citadel
  variantSpec({
    id: "bastion.ship.ts-6f.post", name: "TS-6F", manufacturer: "mfr.bastion", group: "citadel_compact",
    engineering_type: "light_jump_freighter", economic_class: "light_jump_freighter", role: "trade_light",
    purpose: "курьер Скьольдборга: почта, документы и срочный груз своих систем (закрытая страна)",
    parent: "bastion.ship.ts-6b.zastava", envelope: [44, 24, 10], form: "block", law: "Assisted", layout: "I", installed: 4, simultaneous: 2,
    crew: 4, passengers: 2, cargo_t: 120, c20: 6, endurance_days: 30,
    set: { "bastion.wpn.coil10": 0, "bastion.wpn.pd4": 1, "bastion.armour.heavy": 10, "std.bay.boat": 0, "std.cabin.hold6": 0, "std.med.bay": 0, "std.surv.pod4": 2, "bastion.core.b12": 1, "std.tank.jumpcharge": 8, "std.cargo.c20lock": 6 },
    variant: "Корпус TS-6B без пушек и камеры: ядро Б-12 (только своё), шесть гнёзд C20, лёгкая броня.",
    interchange: ["Силовой блок ТЦ-11 + ТР-10 — Тяньхэ, как у TS-6B."]
  }),
  variantSpec({
    id: "bastion.ship.b-12l.heis", name: "B-12L", manufacturer: "mfr.bastion", group: "citadel_compact",
    engineering_type: "surface_lifter", economic_class: "surface_lifter", role: "shuttle",
    purpose: "подъёмник Скьольдборга поверхность ↔ орбита: рудники Jarnvik и Груве-7",
    parent: "tianhe.ship.t421.tianti", envelope: [48, 16, 16], form: "lifter", law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3,
    crew: 4, passengers: 0, cargo_t: 260, c20: 12, endurance_days: 3,
    set: { "xz.mc.c1": 0, "bastion.mc.w1": 1, "xz.radar.n2": 0, "bastion.radar.s50": 1, "xz.comm.c1": 0, "bastion.comm.w1": 1 },
    variant: "Подъёмник 421型 (корпус SL-12, силовая часть Тяньхэ) с авионикой Бастиона.",
    interchange: ["Силовая часть — Тяньхэ; опоры и теплозащита — ОФУ."]
  }),
  variantSpec({
    id: "bastion.ship.b-20g.hulken", name: "B-20G", manufacturer: "mfr.bastion", group: "citadel_compact",
    engineering_type: "miner", economic_class: "miner", role: "tug_miner",
    purpose: "рудный буксир-добытчик Груве-7: пылевой диск Ульвена, плоты руды к Jarnvik",
    parent: "kern.ship.ht-20k-d.krot", envelope: [60, 25, 25], form: "tug", law: "Newtonian", layout: "X", installed: 4, simultaneous: 4,
    crew: 5, passengers: 0, cargo_t: 1000, c20: 0, endurance_days: 60,
    set: { "kb.reactor.er1": 0, "srw.reactor.sr48": 1, "xz.mc.c1": 0, "bastion.mc.w1": 1, "bastion.armour.heavy": 10 },
    variant: "Корпус HT-20K-D (Kern): реактор Сола вместо четырёх ER-1 Когниума (Когниум сдерживает Цитадель), авионика и броня Бастиона.",
    interchange: ["Двигатели KT-1, фрезы — Kern (через вторые руки).", "Реактор — Сол."]
  }),
  // ===== Cognium
  variantSpec({
    id: "hyp.ship.f-900.atlas", name: "F-900 «Атлас»", manufacturer: "mfr.hyperion", group: "cognium_pool",
    engineering_type: "medium_freighter", economic_class: "medium_freighter", role: "trade_medium",
    purpose: "магистральный грузовик Когниума: 120 C20 на премиальных линиях пула вместо покупки C-120 / Хэ-120",
    parent: "hyp.ship.l-900.piligrim", envelope: [150, 34, 28], form: "cylinder", law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3,
    crew: 10, passengers: 0, cargo_t: 2400, c20: 120, endurance_days: 40,
    set: { "agora.hab.pax100": 0, "std.surv.boat20": 2, "eurohab.ls.l200": 0, "eurohab.ls.l20": 1, "std.med.bay": 1, "std.cargo.hold100": 0, "std.cargo.c20lock": 120 },
    variant: "Корпус L-900 без пассажирских секций: 120 гнёзд C20, экипаж 10.",
    interchange: ["Весь силовой набор — Cobretty, как у L-900."]
  }),
  // ===== Sol
  variantSpec({
    id: "sol.ship.ff-3000.gagarin", name: "FF-3000 «Гагарин»", manufacturer: "mfr.sol_mass_yards", group: "sol_industrial",
    engineering_type: "escort_frigate", economic_class: "escort_frigate", role: "escort_line",
    purpose: "фрегат Флота Сола: эскорт и оборона систем (модель обороны даёт Солу 46 эскортов и 12 линейных; своих корпусов не было)",
    parent: "sol.ship.gt-3000.titan", envelope: [110, 30, 30], form: "cylinder", law: "Newtonian", layout: "Y", installed: 3, simultaneous: 3,
    crew: 70, passengers: 0, cargo_t: 40, c20: 0, endurance_days: 60,
    set: { "sol.cryo.t1000": 0, "ofu.drive.u33": 0, "ofu.drive.u70": 3, "srw.reactor.sr48": 5, "srw.apu.s60": 0, "srw.apu.s200": 1, "sol.rad.panel50": 12, "sol.cool.loop300": 2,
           "um.core.z5": 1, "std.tank.jumpcharge": 30, "xz.mc.c1": 0, "xz.mc.c3": 2, "xz.radar.n2": 0, "xz.radar.n10": 2, "xz.comm.c1": 0, "xz.comm.c3": 2,
           "terra.ls.t10": 1, "terra.ls.t60": 1, "std.surv.pod4": 0, "std.surv.boat20": 4, "std.med.bay": 1, "std.bay.boat": 1,
           "fa.wpn.coil30": 2, "fa.wpn.pd2": 6, "std.armour.ceramic": 150 },
    variant: "Хребет GT-3000 без криоцистерн: три У-70, пять SR-4.8, ядро Я-5; орудия и броня Твердыни — «только по межгосударственному решению» (канон).",
    interchange: ["Орудия и броня — Твердыня (Пакт, по межгосударственному решению).", "Ядро Я-5 — лицензия Когниума (4/5)."]
  }),
  // ===== Free Belts
  variantSpec({
    id: "stone.ship.t-5000r.hive", name: "«Улей» (переделка T-5000)", manufacturer: "mfr.stone_docks", group: "free_belts",
    engineering_type: "colonist_transport", economic_class: "colonist_transport", role: "passenger",
    purpose: "пассажирский и переселенческий корабль поясов: списанный T-5000 без анабиоза, койки втрое плотнее, лёгкая самооборона",
    parent: "bharat.ship.t-5000.iskhod", envelope: [220, 50, 40], form: "cylinder", law: "Newtonian", layout: "X", installed: 4, simultaneous: 4,
    crew: 40, passengers: 3000, cargo_t: 400, c20: 0, endurance_days: 40,
    set: { "uhuru.cryo.pod100": 0, "bharat.hab.bunk500": 6, "uhuru.ls.u500": 7, "srw.reactor.sr48": 6, "sol.rad.panel50": 26, "std.surv.refuge100": 31, "std.med.bay": 3, "fa.wpn.pd2": 2, "std.armour.ceramic": 20 },
    variant: "Корпус T-5000 с рынка списанных: анабиоз снят, шесть секций коек, ЖО на 3 000, две PD-2 с чужих корпусов.",
    interchange: ["Всё — вторые руки; ядро K-12 — донорское."]
  }),
  variantSpec({
    id: "stone.ship.ocs-4r.bucket", name: "«Ведро» (переделка OCS-4)", manufacturer: "mfr.stone_docks", group: "free_belts",
    engineering_type: "ocs4_lastochka", economic_class: "ocs4_lastochka", role: "shuttle",
    purpose: "челнок поясов: хаб ↔ рудник ↔ док внутри системы; списанная «Ласточка» с бронёй",
    parent: "ofu.ship.ocs-4.lastochka", envelope: [56, 18, 14], form: "box", law: "Newtonian", layout: "I", installed: 1, simultaneous: 1,
    crew: 3, passengers: 0, cargo_t: 300, c20: 16, endurance_days: 10,
    set: { "std.cargo.c20lock": 16, "fa.wpn.pd2": 1, "std.armour.ceramic": 6 },
    variant: "OCS-4 с рынка списанных: 16 гнёзд C20 из 24, PD-2 и керамика.",
    interchange: ["Всё — ОФУ / вторые руки."]
  }),
  variantSpec({
    id: "stone.ship.kb-r.stinger", name: "«Жало» (переделка Cobrett-I)", manufacturer: "mfr.stone_docks", group: "free_belts",
    engineering_type: "cobrett_n1", economic_class: "interceptor", role: "interceptor",
    purpose: "перехватчик поясов: охрана своих доков и конвоев от пиратов и досмотровых катеров Гармонии",
    parent: "kb.ship.cobrett-i.frnu.n1", envelope: [22.2, 26, 5], form: "wedge", law: "Newtonian", layout: "I", installed: 1, simultaneous: 1,
    crew: 2, passengers: 0, cargo_t: 20, c20: 1, endurance_days: 20,
    set: { "std.cargo.c20lock": 1, "kb.wpn.pd1": 2, "fa.wpn.coil30": 1, "std.armour.ceramic": 5 },
    variant: "Cobrett-I N1 с рынка подержанных: пять гнёзд C20 из шести заняты пушкой КП-30, второй PD-1 и керамикой.",
    interchange: ["Всё — Cobretty / вторые руки; КП-30 — Твердыня (трофей)."]
  }),
  // ===== Lalande League
  variantSpec({
    id: "agora.ship.pc-30ak.akritas", name: "PC-30AK «Акритас»", manufacturer: "mfr.agora_modular", group: "lalande_open_market",
    engineering_type: "patrol_cutter", economic_class: "escort_frigate", role: "escort_line",
    purpose: "корвет Лиги Лаланд: охрана нейтральной торговли и подходов к Ставродроми (слабая милитаризация — минимум, но не ноль)",
    parent: "agora.ship.pk-30a.limenarch", envelope: [34, 22, 8], form: "flat", law: "Assisted", layout: "I", installed: 4, simultaneous: 2,
    crew: 12, passengers: 0, cargo_t: 10, c20: 0, endurance_days: 30,
    set: { "fa.wpn.coil30": 1, "fa.wpn.pd2": 2, "std.armour.ceramic": 30, "std.surv.boat20": 0, "std.med.bay": 1, "sol.rad.panel50": 4 },
    variant: "PC-30A с пушкой КП-30, второй PD-2 и 30 т керамики; шлюпка снята.",
    interchange: ["Корпус и двигатели — Сол (лицензия)."]
  })
];
for (const s of moreSpecs) specs.push(s);

const added = [];
for (const sp of specs) {
  const bom = sp.bom.map(([pid, q]) => item(pid, q));
  let structure, parent = null;
  if (sp.parent) { const fp = fromParent(sp.parent); parent = fp.parent; structure = fp.structure; } else structure = sp.structure_t || 0;
  const dry = sp.mass_override_t != null ? sp.mass_override_t : r1(structure + eqMass(bom));
  const groupsMap = {};
  for (const it of bom) { const g = groupFor(PM.get(it.part_id), it.slot_code); (groupsMap[g] ??= []).push(it); }
  const modelCode = PROPOSAL_MODEL_CODES[sp.id];
  if (modelCode == null) throw new Error("no model code for " + sp.id + " (PROPOSAL_MODEL_CODES)");
  const functional_groups = Object.keys(groupsMap).sort((a, b) => Number(a.slice(1)) - Number(b.slice(1))).map(g => ({ code: modelCode + "F" + g.slice(1), name: GROUP_NAMES[g], items: groupsMap[g] }));
  const weapons = { laser_emitters: [], missile_cells: [], kinetic_mounts: [], total_laser_emitters: 0, total_missile_cells: 0 };
  for (const it of bom.filter(x => x.category === "weapon")) {
    const bnd = D.weapons.bindings[it.part_id] || {};
    if (bnd.laser_id) { const l = D.weapons.lasers.find(x => x.id === bnd.laser_id); weapons.laser_emitters.push({ hardware: it.part_id, qty_mounts: it.qty, emitters: it.qty * bnd.emitters_per_mount, laser_id: l.id, laser: l }); weapons.total_laser_emitters += it.qty * bnd.emitters_per_mount; }
    else if (bnd.compatible_missiles) { weapons.missile_cells.push({ hardware: it.part_id, qty_mounts: it.qty, cells: it.qty * bnd.cells_per_mount, compatible_missiles: bnd.compatible_missiles.map(id => D.weapons.missiles.find(m => m.id === id)) }); weapons.total_missile_cells += it.qty * bnd.cells_per_mount; }
    else weapons.kinetic_mounts.push({ hardware: it.part_id, qty: it.qty, kind: PM.get(it.part_id).kind });
  }
  const cores = bom.filter(x => x.category === "jump_core").map(x => ({ part_id: x.part_id, qty: x.qty, rating_t: PM.get(x.part_id).rating_t }));
  const jt = bom.filter(x => x.part_id === "std.tank.jumpcharge").reduce((a, x) => a + x.qty, 0);
  const rxMwe = bom.filter(x => x.category === "reactor").reduce((a, x) => a + PM.get(x.part_id).mwe * x.qty, 0);
  const apu = bom.filter(x => x.category === "service_reactor").reduce((a, x) => a + PM.get(x.part_id).mwe * x.qty, 0);
  const bal = balance(sp, bom, dry);
  const ship = {
    schema: "elite.ship_passport.engineering.v1", status: ST, id: sp.id, engineering_type: sp.engineering_type, economic_class: sp.economic_class,
    name: sp.name, model_code: modelCode, manufacturer: sp.manufacturer, cooperation_group: sp.group, purpose: sp.purpose,
    frame: { status: "DEFERRED", envelope_m: sp.envelope, form: sp.form, note: sp.parent ? "variant: frame/mesh of " + sp.parent + " reused; module layout differs" : "frame/structural passport will be authored during model build" },
    flight: { law: sp.law, layout: sp.layout, installed_main_engines: sp.installed, simultaneous_main_engines: sp.simultaneous, rated_directional_thrust_mn: sp.thrust_mn, rated_propulsion_input_mw: sp.input_mw, standardized_reactor_capacity_mw: rxMwe },
    jump_system: { fitted: cores.length > 0, cores, jump_charge_tanks: jt, note: null },
    service_power_capacity_mw: sp.service_from_propulsion ? rxMwe - sp.input_mw : apu,
    mass: { standardized_parts_delta_t: 0, dry_estimated_after_standardization_t: dry, delta_fraction: 0, frame_revalidation_required: false, dry_original_t: dry },
    bom, unique_spare_skus: new Set(bom.map(x => x.part_id)).size,
    geometry_source: "Notes/html/ship_reference/data.js (PROPOSAL reference)",
    design_language: { block: DL.groups[sp.group], manufacturer: DL.manufacturers[sp.manufacturer] },
    replacements: sp.repl, functional_groups, weapons,
    reference: {
      status: ST, role: sp.role, hull: sp.hull, variant_of: sp.parent, variant_of_name: parent ? parent.name : null,
      hull_structure_t: sp.mass_override_t != null ? null : r1(structure), crew: sp.crew, passengers: sp.passengers, cargo_t: sp.cargo_t, c20: sp.c20, endurance_days: sp.endurance_days,
      design_note: sp.variant, interchangeability: sp.interchange || [], open: sp.open || [], model_effort_hull_eq: sp.effort, balance: bal
    }
  };
  D.ships.push(ship);
  added.push(ship);
}

// ---------------------------------------------------------------- coverage matrix
const BLOCS = [
  ["sol_industrial", "Сол"], ["cognium_pool", "Когниум"], ["heavenly_pact", "Небесный пакт"], ["southern_circle", "Южный круг"],
  ["citadel_compact", "Цитадель"], ["free_belts", "Свободные пояса"], ["lalande_open_market", "Лига Лаланд"]
];
const E = (...ids) => ids; // existing ship ids
const cov = {
  status: ST,
  note: "● есть паспорт · ◆ добавлено справочником (PROPOSAL) · ◐ закрывается чужим корпусом / частично (в скобках канал и балл кооперации) · — нет и не требуется по лору или открыто",
  blocs: BLOCS.map(([id, ru]) => ({ id, ru })),
  roles: [
    { id: "trade_light", ru: "Торговля: курьер / лёгкий (прыжок)", cells: {
      sol_industrial: { added: ["ofu.ship.ocs-4j.strizh"] }, cognium_pool: { existing: E("kb.ship.cobrett-i.frnu.n1", "kb.ship.cobrett-i.frnu-inta.a3") },
      heavenly_pact: { partial: "Хэ-120 (крупнее)" }, southern_circle: { partial: "Cobrett-I (2/5)" }, citadel_compact: { partial: "B-120F" },
      free_belts: { partial: "«Бродяга» как контрабандист" }, lalande_open_market: { partial: "Cobrett-I, OCS-4J — покупка/реестр" } } },
    { id: "trade_medium", ru: "Торговля: средний / магистральный (прыжок)", cells: {
      sol_industrial: { partial: "C-120 на силовой части Сола (Бхарат 3/5)" }, cognium_pool: { partial: "C-120 / Хэ-120 (2–3/5)" },
      heavenly_pact: { existing: E("harmony.ship.he-120", "harmony.ship.he-t12.lokomotiv") }, southern_circle: { existing: E("bharat.ship.c-120.konveyer") },
      citadel_compact: { added: ["bastion.ship.he-120c.oboz"] }, free_belts: { added: ["stone.ship.he-120r.brodyaga"] }, lalande_open_market: { partial: "реестр чужих корпусов" } } },
    { id: "tanker", ru: "Танкер (система / прыжок)", cells: {
      sol_industrial: { existing: E("sol.ship.gt-3000.titan") }, cognium_pool: { partial: "Хэ-T12G (3/5)" }, heavenly_pact: { existing: E("harmony.ship.he-t12c.tanker") },
      southern_circle: { partial: "Хэ-T12G (3/5); заряд — C-120T" }, citadel_compact: { partial: "Хэ-T12G (3/5)" }, free_belts: { existing: E("stone.ship.kamen.tanker") },
      lalande_open_market: { partial: "Хэ-T12G / GT-3000" } } },
    { id: "passenger", ru: "Пассажиры: лайнер / переселенцы", cells: {
      sol_industrial: { added: ["sol.ship.gt-3000p.poputchik"] }, cognium_pool: { existing: E("hyp.ship.l-900.piligrim") }, heavenly_pact: { added: ["harmony.ship.he-120p.chunyun"] },
      southern_circle: { existing: E("bharat.ship.t-5000.iskhod") }, citadel_compact: { partial: "Хэ-120K (3/5); закрытая страна — мало пассажиров" },
      free_belts: { partial: "— (переселенцы на попутных; T-5000 б/у)" }, lalande_open_market: { partial: "L-900 / GT-3000P" } } },
    { id: "shuttle", ru: "Челнок / подъёмник (орбита–поверхность)", cells: {
      sol_industrial: { existing: E("ofu.ship.ocs-4.lastochka", "ofu.ship.sl-12.podyomnik") }, cognium_pool: { partial: "OCS-4 / SL-12 (4/5)" },
      heavenly_pact: { partial: "OCS-4 / SL-12 (3/5) — малой силовой пары Тяньхэ нет" }, southern_circle: { partial: "OCS-4 / SL-12 (3/5)" },
      citadel_compact: { partial: "OCS-4 / SL-12 (2/5, гражданские узлы)" }, free_belts: { partial: "OCS-4 б/у (2/5)" }, lalande_open_market: { partial: "OCS-4 / SL-12" } } },
    { id: "tug_miner", ru: "Буксир / добыча", cells: {
      sol_industrial: { existing: E("sol.ship.pao-7.shchit"), partial: "добыча: HT-20K-D (4/5)" }, cognium_pool: { existing: E("kern.ship.ht-20k.mul"), added: ["kern.ship.ht-20k-d.krot"] },
      heavenly_pact: { partial: "HT-20K-D (3/5); ледовые тягачи Гармонии без паспорта" }, southern_circle: { partial: "HT-20K-D (2/5)" },
      citadel_compact: { partial: "HT-20K-D «Хулк» целиком (1/5)" }, free_belts: { added: ["stone.ship.kamen-s.salvage"] }, lalande_open_market: { partial: "HT-20K-D" } } },
    { id: "tender", ru: "Логистика: сервисный / флотский тендер", cells: {
      sol_industrial: { existing: E("sol.ship.st-6.masterovoy") }, cognium_pool: { partial: "ST-6 (4/5)" }, heavenly_pact: { existing: E("harmony.ship.he-s240.tylovik") },
      southern_circle: { added: ["bharat.ship.c-120t.sahayak"] }, citadel_compact: { partial: "Хэ-B240 (3/5)" }, free_belts: { partial: "«Камень-С» (мастерская)" },
      lalande_open_market: { partial: "ремонт в доках Агоры" } } },
    { id: "cutter", ru: "Безопасность: катер (таможня, SAR)", cells: {
      sol_industrial: { existing: E("sol.ship.pk-30.strazh") }, cognium_pool: { added: ["kb.ship.cobrett-i.frnu-inta.a3p"] }, heavenly_pact: { added: ["tianhe.ship.ts-6.dozor"] },
      southern_circle: { added: ["bharat.ship.pk-30b.rakshak"] }, citadel_compact: { added: ["bastion.ship.ts-6b.zastava"] },
      free_belts: { partial: "«Бродяга» с абордажным катером" }, lalande_open_market: { added: ["agora.ship.pk-30a.limenarch"] } } },
    { id: "interceptor", ru: "Безопасность: перехватчик", cells: {
      sol_industrial: { partial: "A3P (4/5) — открыто" }, cognium_pool: { added: ["kb.ship.cobrett-i.frnu-inta.a3p"] }, heavenly_pact: { added: ["tianhe.ship.b-4t.sokol-t"] },
      southern_circle: { partial: "тип 415 (3/5) / A3P (2/5)" }, citadel_compact: { existing: E("bastion.ship.b-4.sokol") }, free_belts: { partial: "—" }, lalande_open_market: { partial: "A3P" } } },
    { id: "escort_line", ru: "Военные: эскорт / линейный / разведка", cells: {
      sol_industrial: { partial: "— (модель обороны даёт Солу 46 эскортов и 12 линейных — открыто)" }, cognium_pool: { existing: E("hyp.ship.r-7.argus"), partial: "линейных нет — открыто" },
      heavenly_pact: { existing: E("harmony.ship.he-f22.dun") }, southern_circle: { partial: "— (8–9 линейных у Тауранги и Новой Индии — открыто)" },
      citadel_compact: { existing: E("bastion.ship.b-90.tsitadel") }, free_belts: { partial: "«Бродяга» (ВПУ-8)" }, lalande_open_market: { partial: "— (слабая милитаризация)" } } },
    { id: "raider", ru: "Рейдер / вооружённый торговец", cells: {
      sol_industrial: { partial: "—" }, cognium_pool: { partial: "—" }, heavenly_pact: { partial: "—" }, southern_circle: { partial: "—" }, citadel_compact: { partial: "—" },
      free_belts: { added: ["stone.ship.he-120r.brodyaga"] }, lalande_open_market: { partial: "—" } } },
    { id: "small_craft", ru: "Малые аппараты (катер, дрон, шлюпка)", all: { added: ["aux.craft.boarding-boat", "aux.craft.service-drone", "aux.craft.lifeboat-20"] } }
  ]
};
// bloc completeness: the generated gap fillers enter their cells; the raider role is the free belts' own
const addCov = (role, bloc, id) => { const c = (cov.roles.find(r => r.id === role).cells[bloc] ??= {}); (c.added ??= []).push(id); delete c.partial; };
addCov("trade_light", "heavenly_pact", "tianhe.ship.t416.tianyi");
addCov("shuttle", "heavenly_pact", "tianhe.ship.t421.tianti");
addCov("tug_miner", "heavenly_pact", "harmony.ship.he-t8.yishan");
addCov("trade_light", "southern_circle", "bharat.ship.c-30.doot");
addCov("tug_miner", "southern_circle", "bharat.ship.h-20.majdoor");
addCov("escort_line", "southern_circle", "bharat.ship.f-120.prahari");
addCov("trade_light", "citadel_compact", "bastion.ship.ts-6f.post");
addCov("shuttle", "citadel_compact", "bastion.ship.b-12l.heis");
addCov("tug_miner", "citadel_compact", "bastion.ship.b-20g.hulken");
addCov("trade_medium", "cognium_pool", "hyp.ship.f-900.atlas");
addCov("escort_line", "sol_industrial", "sol.ship.ff-3000.gagarin");
addCov("passenger", "free_belts", "stone.ship.t-5000r.hive");
addCov("shuttle", "free_belts", "stone.ship.ocs-4r.bucket");
addCov("interceptor", "free_belts", "stone.ship.kb-r.stinger");
addCov("escort_line", "lalande_open_market", "agora.ship.pc-30ak.akritas");
for (const [b, c] of Object.entries(cov.roles.find(r => r.id === "raider").cells)) if (b !== "free_belts") c.partial = "не нужен государству (роль вольных поясов)";
// model counts
const exHulls = 19; // review: 21 passports, N1/A3 share a hull, T12/T12C share a tractor
const newHulls = added.filter(s => s.reference.hull === "new");
const variants = added.filter(s => s.reference.hull === "variant");
const crafts = added.filter(s => s.reference.hull === "craft");
cov.models = {
  existing_passports: D.ships.length - added.length, existing_hull_meshes: exHulls, existing_hull_meshes_in_runtime: 1,
  added_passports: added.length, added_new_hulls: newHulls.map(s => s.id), added_variants: variants.map(s => s.id), added_small_craft: crafts.map(s => s.id),
  total_passports: D.ships.length, total_large_hull_meshes: exHulls + newHulls.length, total_small_craft_meshes: crafts.length,
  variant_effort_hull_eq: r2(variants.reduce((a, s) => a + s.reference.model_effort_hull_eq, 0)),
  new_parts: newParts.map(p => p.id),
  note: "Большие корпуса: 19 существующих сеток (+1 новая тип 412) = 20; одна (Cobrett-I) уже есть в рантайме. Варианты — та же сетка с другим набором модулей/текстурой; их трудоёмкость дана в долях корпуса. Малые аппараты: катер и дрон — новые сетки, шлюпка СШ-20 — сетка детали, которая и так нужна."
};
D.coverage = cov;
D.meta.ships = D.ships.length; D.meta.parts = D.parts.length;
D.meta.reference_proposals = { status: ST, ships: added.length, parts: newParts.length, note: "Записи со status = \"PROPOSAL (reference)\" добавлены вручную в справочник (не из канона tools/colonization_lab/canon); при перегенерации data.js их нужно сохранить или перенести в канон." };

// sanity
for (const s of D.ships) for (const b of s.bom) if (!PM.get(b.part_id)) throw new Error(s.id + " unresolved " + b.part_id);
for (const r of cov.roles) for (const c of Object.values(r.cells || { all: r.all })) for (const id of [...(c.existing || []), ...(c.added || [])]) if (!D.ships.find(s => s.id === id)) throw new Error("coverage id " + id);
for (const s of added) for (const k of ["standard", "hostile_or_foreign"]) { const x = s.replacements[k]; if (x && !D.ships.find(t => t.id === x.ship_id)) throw new Error(s.id + " replacement " + x.ship_id); }

// ---------------------------------------------------------------- names (canon ship_naming.json) for every ship
// The canon passports already carry them (ship_naming.py --write mirrors); the proposals get them here. Every
// ship: name = display_name_ru, the naming fields, model_code = manufacturer prefix + number, group codes
// <model_code>A<k>, replacement names by ship id.
const NAME_FIELDS = ["model_index", "official_name", "native_script", "romanization", "short_name", "short_name_ru", "legacy_name"];
for (let i = 0; i < D.ships.length; i++) {
  const s = D.ships[i], n = NAMING.ships[s.id];
  if (!n) throw new Error("no ship_naming.json entry for " + s.id);
  const prefix = NAMING.manufacturers[s.manufacturer]?.code_prefix;
  if (!prefix) throw new Error("no code_prefix for " + s.manufacturer);
  const number = String(s.model_code).match(/(\d+)$/)[1];
  const code = prefix + number;
  const out = {};
  for (const [k, v] of Object.entries(s)) {
    if (NAME_FIELDS.includes(k) || k === "model_code") continue;
    if (k === "name") {
      out.name = n.display_name_ru; out.model_code = code;
      for (const f of NAME_FIELDS) if (n[f]) out[f] = n[f];
      continue;
    }
    out[k] = v;
  }
  for (const g of out.functional_groups) g.code = code + "F" + g.code.match(/(\d+)$/)[1];   // F: functional group; A is a layout unit
  for (const r of Object.values(out.replacements || {})) if (r && r.ship_id && NAMING.ships[r.ship_id]) r.name = NAMING.ships[r.ship_id].display_name_ru;
  if (out.reference && out.reference.variant_of && NAMING.ships[out.reference.variant_of]) out.reference.variant_of_name = NAMING.ships[out.reference.variant_of].display_name_ru;
  D.ships[i] = out;
}
D.naming_doctrines = NAMING.manufacturers;

fs.writeFileSync(FILE, "window.ELITE_SHIP_REF_DATA=" + JSON.stringify(D) + ";\n");
for (const s of added) {
  const b = s.reference.balance;
  console.log(s.id.padEnd(36), "dry", s.mass.dry_original_t, "struct", s.reference.hull_structure_t, "gross", b.gross_t, "g", b.accel_g_gross, "| P", b.propulsion_ratio, "grp", b.reactor_per_group_mw + "/" + b.group_demand_mw,
    "| svc", b.service_capacity_mw, "h", b.hotel_mw_weapons30 + "/" + b.hotel_mw_weapons100, "| heat", b.service_heat_mw_weapons30 + "/" + b.service_heat_mw_weapons100, "rad", b.radiator_mw, "loop", b.coolant_loop_mw,
    "| pad", b.pad_required_m2 + "/" + b.pad_installed_m2, "cool/plan", b.cooling_surface_share_of_plan, "| core", b.jump_core_rating_t, "| LS", b.life_support_persons, "seats", b.survival_seats, "pers", b.persons, b.flags.join("; "));
}
console.log(JSON.stringify(cov.models));
