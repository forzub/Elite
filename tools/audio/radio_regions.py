"""The cockpit radio's REGIONS (author 2026-11-23: «распределим радиостанции по зонам колоний и там где есть маяки. нет
маяков - шум ... мы сможем слушать китайское радио в системах дочерних от китая? в районе земли по идее доступны на
многих языках»): every hub of the economy model (src/assets/data/economy/hub_modules_3126.json) is a transmitter of its
polity's radio; a polity's radio = the internet stations of its culture's countries (radio-browser.info country codes,
or a tag); the Earth's hubs relay the Earth's surface - the countries of its agglomerations. Beacons / relays reach
farther. Writes src/assets/data/audio/radio_regions.json (read by src/audio/CockpitRadio.cpp).

usage: python tools/audio/radio_regions.py
"""
import collections
import json
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
HUBS = os.path.join(ROOT, "src", "assets", "data", "economy", "hub_modules_3126.json")
OUT = os.path.join(ROOT, "src", "assets", "data", "audio", "radio_regions.json")

# the polities' cultures (src/assets/data/galaxy/actors.json) -> whose radio they would carry
POLITIES = {
    "sol_authority": {"name": "Земля (Сол)", "countries": []},          # the Earth's mix: see EARTH_CITY
    "tau_ceti_cognium": {"name": "Когниум", "countries": ["CN", "TW", "SG"]},
    "eps_eri_harmony": {"name": "Синьхэ", "countries": ["CN", "HK"]},
    "gj832_tianhe": {"name": "Тяньхэ", "countries": ["CN"]},
    "hd40307_firmament": {"name": "Небесный Свод", "countries": ["CN"]},
    "hd40307_maland": {"name": "Уджамаа", "countries": ["KE", "TZ", "NG", "ZA", "GH"]},
    "wolf359_citadel": {"name": "Скьольдборг", "countries": ["NO", "SE", "DK", "IS"]},
    "proxima_outpost": {"name": "Застава Проксимы", "countries": ["NO", "SE", "FI"]},
    "lalande_hellas": {"name": "Новая Эллада", "countries": ["GR", "CY", "IT"]},
    "hd20794_hellas2": {"name": "Эллада-2", "countries": ["GR", "IT"]},
    "hd114783_hellas3": {"name": "Эллада-3", "countries": ["GR"]},
    "cyg61_kern": {"name": "Проспекторат Керн", "countries": ["US", "GB", "CA", "AU"]},
    "lalande_mecca": {"name": "Капеху", "countries": ["NZ", "FJ", "WS", "TO"]},
    "hd20794_newindia": {"name": "Дхарма-Мандала", "countries": ["IN", "NP", "LK"]},
    "delta_pav_karma": {"name": "Карма", "countries": ["TH", "LK", "MN", "BT"]},
    "delta_pav_freekarma": {"name": "Свободная Карма", "countries": ["TH", "KH", "LA"]},
    "leo83_zion": {"name": "Ново-Фиваида", "tag": "christian"},
    "gj581_europa": {"name": "Союз Новой Европы", "countries": ["FR", "DE", "BE", "NL", "AT", "CH"]},
    "eps_eri_strife": {"name": "Варпстанис", "tag": "punk"},
    "gj581_freeclans": {"name": "Вольные кланы", "tag": "rock"},
    "gj876_next": {"name": "Некст", "tag": "electronic"},
    "hd69830_verde": {"name": "Сельва-Верде", "countries": ["BR", "PE", "BO"]},
    "gj832_gaia_zone": {"name": "Зона «Филоморфоз»", "countries": []},             # only noise there
}
# the Earth's agglomerations (hub names «агломерация «X»») -> their countries: the Earth relays them all
EARTH_CITY = {
    "Токио": "JP", "Дели": "IN", "Шанхай": "CN", "Сан-Паулу": "BR", "Мехико": "MX", "Каир": "EG", "Мумбаи": "IN",
    "Пекин": "CN", "Дакка": "BD", "Осака": "JP", "Нью-Йорк": "US", "Карачи": "PK", "Буэнос-Айрес": "AR",
    "Чунцин": "CN", "Стамбул": "TR", "Калькутта": "IN", "Манила": "PH", "Лагос": "NG", "Рио-де-Жанейро": "BR",
    "Тяньцзинь": "CN", "Киншаса": "CD", "Гуанчжоу": "CN", "Лос-Анджелес": "US", "Москва": "RU", "Шэньчжэнь": "CN",
    "Лахор": "PK", "Бангалор": "IN", "Париж": "FR", "Богота": "CO", "Джакарта": "ID", "Ченнаи": "IN", "Лима": "PE",
    "Бангкок": "TH", "Сеул": "KR", "Нагоя": "JP", "Хайдарабад": "IN", "Лондон": "GB", "Тегеран": "IR", "Чикаго": "US",
    "Чэнду": "CN", "Нанкин": "CN", "Ухань": "CN", "Хошимин": "VN", "Луанда": "AO", "Ахмадабад": "IN",
    "Куала-Лумпур": "MY", "Сиань": "CN", "Гонконг": "HK", "Дунгуань": "CN", "Ханчжоу": "CN", "Фошань": "CN",
    "Шэньян": "CN", "Эр-Рияд": "SA", "Багдад": "IQ", "Сантьяго": "CL", "Сурат": "IN", "Мадрид": "ES",
    "Санкт-Петербург": "RU", "Берлин": "DE", "Рим": "IT", "Торонто": "CA", "Сидней": "AU", "Киев": "UA",
}
BEACON_TYPES = {"beacon_bridge", "beacon_relay"}


def main():
    doc = json.load(open(HUBS, encoding="utf-8"))
    hubs = doc["hubs"]
    hubs = list(hubs.values()) if isinstance(hubs, dict) else hubs
    earth = collections.Counter()
    transmitters = {}
    unknown = collections.Counter()
    for h in hubs:
        polity = h.get("actor") or ""
        name = h.get("name") or ""
        if h.get("system_id") == 0 and h.get("type") == "planetary_surface_port" and "«" in name:
            city = name.split("«", 1)[1].split("»", 1)[0]
            if city in EARTH_CITY:
                earth[EARTH_CITY[city]] += 1
        if polity not in POLITIES:
            unknown[polity] += 1
            continue
        transmitters[h["id"]] = {"polity": polity, "kind": "beacon" if h.get("type") in BEACON_TYPES else "hub"}
    # the Earth's mix: the most populous agglomeration countries first (one station each)
    POLITIES["sol_authority"]["countries"] = [c for c, _ in earth.most_common()] or ["RU", "US", "GB", "FR", "DE", "CN", "JP"]
    # the Earth's own hubs (earth_orbital_hub, earth_hub.*) are world hubs too: Sol Authority
    out = {"schema": "elite.radio_regions.v1",
           "note": "generated by tools/audio/radio_regions.py - the polities' radio (radio-browser.info countries / tags) "
                   "and the hubs that transmit it",
           "ranges_m": {"hub": 3.0e7, "beacon": 3.0e8},
           "noise_factor": 1000.0,
           "polities": POLITIES, "transmitters": transmitters}
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    json.dump(out, open(OUT, "w", encoding="utf-8", newline="\n"), ensure_ascii=False, indent=1)
    print(f"{len(transmitters)} transmitters, {len(POLITIES)} polities, Earth countries {POLITIES['sol_authority']['countries']}"
          f"; skipped (no polity): {dict(unknown)}")


if __name__ == "__main__":
    main()
