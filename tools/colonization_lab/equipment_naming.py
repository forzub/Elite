#!/usr/bin/env python3
"""Equipment names by maker language (author 2026-11-10: "do not russify names - take them from the maker's language").
Rules: tools/colonization_lab/canon/ship_naming.json (maker languages, index formats). Writes the PROPOSAL canon
tools/colonization_lab/canon/equipment_naming.json and applies model_index / official_name / native_script / display name to
the canon parts (ship_parts_unified.json, ship_designs.json), the reference data (Notes/html/ship_reference/data.js) and
the new-equipment proposal. The Russian UI keeps a Russian description after the new index (display_name_ru);
legacy_index keeps the old (often Cyrillic) index for search.
usage: python tools/colonization_lab/equipment_naming.py [--check]
"""
import json
import os
import re
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CANON = os.path.join(ROOT, "tools", "colonization_lab", "canon")
CYR = re.compile("[А-Яа-яЁё]")

# legacy Cyrillic / transliterated indices -> the maker-language index (Latin export form)
REINDEX = {
    # Bastion (nb, blueprint prefix MJ - Mjolner yard)
    "БТ-1": "MJ-P1", "АП-4": "MJ-H4", "БС-1": "MJ-S1", "БВ-1": "MJ-R1", "Б-12": "MJ-B12", "С-50": "MJ-RA50",
    "БП-10": "MJ-SK10", "БТ-4": "MJ-NF4", "ВПУ-16": "MJ-VU16",
    # Tianhe (zh, pinyin initials): 辅助堆 FuZhuDui, 冷却回路 LengQue, 姿控 ZiKong, 散热 SanRe
    "СР-150": "TH-FZ150", "СР-600": "TH-FZ600", "КО-600": "TH-LQ600", "RCS-4Т": "TH-ZK4", "РС-50": "TH-SR50",
    "ТЦ-11": "TZ-11", "ТЦ-16В": "TZ-16V", "ТЦ-28": "TZ-28", "ТР-10": "TR-10", "ТР-2.6В": "TR-2.6V",
    # Harmony (zh, 和 = He): 机库 JiKu
    "Х-5": "He-5", "Х-12": "He-12", "Х-80": "He-80", "АП-2": "He-JK2", "ВПУ-8": "He-VLS8", "ПТ-3": "He-PD3",
    # OFU (en, Ural Latin)
    "У-33": "U-33", "У-65М": "U-65M", "У-70": "U-70", "У-73Л": "U-73L", "ТЩ-12": "OFU-HS12", "ПО-4": "OFU-LG4", "О-4": "OFU-O4",
    # Sol Mass Yards (en; Cyrillic indices reindexed into LM-, naming canon)
    "КО-300": "LM-CL300", "КЦ-1000": "LM-CT1000", "РС-50С": "LM-RS50", "МС-1": "LM-WS1", "АК-1": "LM-BB1", "КЗ-6": "LM-PC6",
    # Agora (el + Latin export): AG-
    "ГО-100": "AG-H100", "ОД-4": "AG-DB4", "СУ-1": "AG-DC1", "СК-4": "AG-EP4", "СШ-20": "AG-LB20", "БЗ-1": "AG-JT1", "БТ-1П": "AG-PT1",
    # Euro-Habitat (EU, Latin): EH-
    "МО-4": "EH-MB4",
    # Zero Point (Beacon Authority, en + zh 零点)
    "Я-0.5": "Z-0.5", "Я-1": "Z-1", "Я-5": "Z-5", "Т-1": "T-1",
    # Firmament (zh): 磁轨炮 CiGuiPao, 点防 DianFang, 陶瓷装甲 TaoCi
    "КП-30": "TL-CG30", "ПТ-2": "TL-DF2", "БК-1": "TL-TC1",
    # Kern (en)
    "МД-40": "MD-40", "МК-1": "MK-1",
    # more legacy forms
    "МП-4800": "He-C4800", "МЦ-4200": "He-T4200", "ТЗ-12": "OFU-HS12", "ТЦ-1.6В": "TZ-16V", "ТЦ-28В": "TZ-28V",
    # Bharat (hi, Latin indices): बसावट BAsavat, शरण SHaran
    "СП-500": "BH-BK500", "СМ-100": "BH-SM100",
    # Uhuru (sw): usingizi = sleep
    "АБ-100": "UH-US100",
}
BRAND = {"mfr.cobretty": "Cobretty", "mfr.sol_reactor": "Sol Reactor Works", "mfr.sol_mass_yards": "Sol Mass Yards", "mfr.ofu": "OFU",
         "mfr.terra_habitat": "Terra Habitat", "mfr.tianhe_heavy": "Tianhe", "mfr.harmony_yards": "Hexie", "mfr.firmament_armour": "Tianlei",
         "mfr.bastion": "Bastion", "mfr.kern_heavy": "Kern", "mfr.agora_modular": "Agora", "mfr.eurohab": "Euro-Habitat",
         "mfr.xinzhi": "Xinzhi", "mfr.hyperion": "Hyperion", "mfr.um_core_fab": "Zero Point", "mfr.kapehu_systems": "Kapehu",
         "mfr.bharat": "Bharat", "mfr.uhuru_bio": "Uhuru"}
LANG = {"mfr.tianhe_heavy": "zh", "mfr.harmony_yards": "zh", "mfr.firmament_armour": "zh", "mfr.xinzhi": "zh", "mfr.um_core_fab": "zh",
        "mfr.hyperion": "zh2", "mfr.bastion": "nb", "mfr.agora_modular": "el", "mfr.bharat": "hi", "mfr.uhuru_bio": "sw",
        "mfr.kapehu_systems": "mi"}
NATIVE_BRAND = {"mfr.tianhe_heavy": "天河重机", "mfr.harmony_yards": "和谐船厂", "mfr.firmament_armour": "天雷装甲", "mfr.xinzhi": "信智电子",
                "mfr.um_core_fab": "零点", "mfr.hyperion": "许珀里翁", "mfr.agora_modular": "Αγορά", "mfr.bharat": "भारत असेंबली",
                "mfr.uhuru_bio": "Uhuru", "mfr.bastion": "Bastion", "mfr.kapehu_systems": "Kāpehu"}
# category words: English export / native
WORD = {
    "drive": ("Main Drive", {"zh": "主推进器", "nb": "hoveddrift", "el": "κύρια πρόωση", "hi": "मुख्य इंजन", "sw": "injini kuu", "mi": "pūkaha matua"}),
    "reactor": ("Propulsion Reactor", {"zh": "推进反应堆", "nb": "drivreaktor", "el": "αντιδραστήρας πρόωσης", "hi": "प्रणोदन रिएक्टर", "sw": "kinu cha msukumo", "mi": "tauhohenga"}),
    "service_reactor": ("Service Reactor", {"zh": "辅助反应堆", "nb": "hjelpereaktor", "el": "βοηθητικός αντιδραστήρας", "hi": "सहायक रिएक्टर", "sw": "kinu kisaidizi", "mi": "tauhohenga āwhina"}),
    "radiator": ("Radiator", {"zh": "散热板", "nb": "radiator", "el": "ψυκτικό πάνελ", "hi": "रेडिएटर", "sw": "kipoza joto", "mi": "rauangi wera"}),
    "coolant_loop": ("Coolant Loop", {"zh": "冷却回路", "nb": "kjølekrets", "el": "κύκλωμα ψύξης", "hi": "शीतलन परिपथ", "sw": "mzunguko wa kupoza", "mi": "porowhita mātao"}),
    "jump_core": ("Jump Core", {"zh": "跃迁核心", "nb": "hoppkjerne", "el": "πυρήνας άλματος", "hi": "जंप कोर", "sw": "kiini cha kuruka", "mi": "iho peke"}),
    "tank": ("Tank", {"zh": "储罐", "nb": "tank", "el": "δεξαμενή", "hi": "टंकी", "sw": "tangi", "mi": "tānga"}),
    "cryo_tank": ("Cryo Tank", {"zh": "低温储罐", "nb": "kryotank", "el": "κρυοδεξαμενή", "hi": "क्रायो टंकी", "sw": "tangi baridi", "mi": "tānga hauhunga"}),
    "rcs": ("RCS Cluster", {"zh": "姿控推进簇", "nb": "styredyser", "el": "συστοιχία ελέγχου στάσης", "hi": "आरसीएस समूह", "sw": "kundi la RCS", "mi": "RCS"}),
    "computer": ("Ship Computer", {"zh": "舰载计算机", "nb": "regnemaskin", "el": "υπολογιστής", "hi": "कंप्यूटर", "sw": "kompyuta", "mi": "rorohiko"}),
    "comm": ("Comm Suite", {"zh": "通信系统", "nb": "samband", "el": "επικοινωνίες", "hi": "संचार", "sw": "mawasiliano", "mi": "whakawhitiwhiti"}),
    "radar": ("Radar", {"zh": "雷达", "nb": "radar", "el": "ραντάρ", "hi": "रडार", "sw": "rada", "mi": "rāta"}),
    "weapon": ("Weapon", {"zh": "武器", "nb": "våpen", "el": "όπλο", "hi": "हथियार", "sw": "silaha", "mi": "patu"}),
    "armour": ("Armour", {"zh": "装甲", "nb": "panser", "el": "θωράκιση", "hi": "कवच", "sw": "deraya", "mi": "pukupuku"}),
    "hangar": ("Hangar", {"zh": "机库", "nb": "hangar", "el": "υπόστεγο", "hi": "हैंगर", "sw": "banda", "mi": "whare waka"}),
    "life_support": ("Life Support", {"zh": "生命保障", "nb": "livsstøtte", "el": "υποστήριξη ζωής", "hi": "जीवन रक्षा", "sw": "msaada wa uhai", "mi": "tautoko ora"}),
    "passenger_module": ("Passenger Module", {"zh": "客舱模块", "nb": "passasjermodul", "el": "τμήμα επιβατών", "hi": "यात्री खंड", "sw": "sehemu ya abiria", "mi": "wāhanga pāhihi"}),
    "escape_capsule": ("Escape Module", {"zh": "逃生舱", "nb": "rømningsmodul", "el": "κάψουλα διαφυγής", "hi": "बचाव कैप्सूल", "sw": "kifaa cha kutoroka", "mi": "kapuhera oranga"}),
    "lifeboat": ("Lifeboat", {"zh": "救生艇", "nb": "livbåt", "el": "σωσίβια λέμβος", "hi": "जीवन नौका", "sw": "mashua ya uokoaji", "mi": "poti oranga"}),
    "drone_bay": ("Drone Bay", {"zh": "无人机舱", "nb": "dronebukt", "el": "υπόστεγο drone", "hi": "ड्रोन खंड", "sw": "chumba cha ndege nyuki", "mi": "kōpae rererangi"}),
    "drone": ("Drone", {"zh": "无人机", "nb": "drone", "el": "drone", "hi": "ड्रोन", "sw": "ndege nyuki", "mi": "rererangi"}),
}


def name_for(p):
    old = p.get("model") or ""
    idx = REINDEX.get(old, old)
    if CYR.search(idx):                                    # any leftover Cyrillic index -> transliterated export form
        idx = idx.translate(str.maketrans("АВЕКМНОРСТУХБГДЗИЛПФЦЧШЯЁ", "ABEKMHOPCTYXBGDZILPFCCSYE"))
    mfr = p.get("mfr", "")
    brand = BRAND.get(mfr, mfr.split(".")[-1].title())
    lang = LANG.get(mfr)
    word = WORD.get(p.get("cat"), (p.get("cat", "").replace("_", " ").title(), {}))
    official = f"{brand} {idx} {word[0]}"
    native = None
    if lang in ("zh", "zh2") and "zh" in word[1]:
        native = f"{NATIVE_BRAND.get(mfr, brand)} {idx} {word[1]['zh']}"
    elif lang in word[1]:
        native = f"{NATIVE_BRAND.get(mfr, brand)} {idx} {word[1][lang]}"
    if p.get("official_name") and not CYR.search(p["official_name"]):
        official = p["official_name"]                      # already authored (new items, Cobra weapons...)
    ru = p.get("name", "")
    ru_desc = re.sub(r"^\s*" + re.escape(old) + r"\s*", "", ru) if old else ru
    for o, n in REINDEX.items():
        ru_desc = ru_desc.replace(o, n)
    display = ru_desc if ru_desc.startswith(idx) else f"{idx} · {ru_desc}"
    out = {"model_index": idx, "official_name": official, "display_name_ru": display}
    if native:
        out["native_script"] = native
    if old and old != idx:
        out["legacy_index"] = old
    return out


def apply(p, n):
    p["model"] = n["model_index"]
    p["name"] = n["display_name_ru"]
    p["official_name"] = n["official_name"]
    if "native_script" in n:
        p["native_script"] = n["native_script"]
    if "legacy_index" in n:
        p["legacy_index"] = n["legacy_index"]


def dump(path, d, indent):
    raw = open(path, "rb").read()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    s = json.dumps(d, ensure_ascii=False, indent=indent) + ("\n" if raw.endswith(b"\n") else "")
    open(path, "wb").write(s.replace("\n", nl).encode("utf-8"))


def main():
    ref = os.path.join(ROOT, "Notes", "html", "ship_reference", "data.js")
    txt = open(ref, encoding="utf-8").read()
    head, body = txt.split("=", 1)
    body = body.rstrip()
    semi = body.endswith(";")
    D = json.loads(body.rstrip(";"))
    table = {}
    for p in D["parts"]:
        table[p["id"]] = name_for(p)
    for p in json.load(open(os.path.join(ROOT, "tools", "models", "briefs", "new_parts_proposal.json"), encoding="utf-8"))["parts"]:
        table.setdefault(p["id"], name_for(p))
    if "--check" in sys.argv:
        left = [k for k, v in table.items() if CYR.search(v["model_index"]) or CYR.search(v["official_name"])]
        print("Cyrillic left in indices / official names:", left)
        sys.exit(1 if left else 0)
    doc = {"schema": "equipment_naming/1", "status": "PROPOSAL (author 2026-11-10: names in the maker's language, not russified)",
           "rules": "tools/colonization_lab/canon/ship_naming.json (maker languages); display_name_ru = index + Russian description",
           "parts": table}
    open(os.path.join(CANON, "equipment_naming.json"), "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=1) + "\n")
    for p in D["parts"]:
        apply(p, table[p["id"]])
    open(ref, "w", encoding="utf-8", newline="").write(head + "=" + json.dumps(D, ensure_ascii=False, separators=(",", ":")) + (";" if semi else "") + ("\n" if txt.endswith("\n") else ""))
    for name, key, ind in (("ship_parts_unified.json", "parts", 2), ("ship_designs.json", "products", 1)):
        path = os.path.join(CANON, name)
        d = json.load(open(path, encoding="utf-8"))
        for p in d[key]:
            if p["id"] in table:
                apply(p, table[p["id"]])
        dump(path, d, ind)
    pp = os.path.join(ROOT, "tools", "models", "briefs", "new_parts_proposal.json")
    d = json.load(open(pp, encoding="utf-8"))
    for p in d["parts"]:
        apply(p, table[p["id"]])
    open(pp, "w", encoding="utf-8", newline="\n").write(json.dumps(d, ensure_ascii=False, indent=1) + "\n")
    print(f"named {len(table)} parts")


if __name__ == "__main__":
    main()
