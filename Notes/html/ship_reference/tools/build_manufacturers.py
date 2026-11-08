#!/usr/bin/env python3
"""Build Notes/html/ship_reference/manufacturers_data.js (window.ELITE_MANUFACTURERS_DATA).

Sources (read-only):
  tools/colonization_lab/canon/industry.json            manufacturers[]
  tools/colonization_lab/canon/ship_naming.json         naming doctrine + recognition of ship builders
  tools/colonization_lab/canon/naming.json              polity endonyms / Russian exonyms
  src/assets/data/galaxy_details/world_sites/sites_3126.json   sites (system, body, placement)
  src/assets/data/economy/hub_modules_3126.json         hub module composition
  Notes/html/ship_reference/data.js                     bloc order / names (coverage.blocs)

Ships per manufacturer are NOT baked in: manufacturers.js reads them live from data.js.
Stdlib only; run from anywhere:  python Notes/html/ship_reference/tools/build_manufacturers.py
"""
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REF = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(REF)))

P_INDUSTRY = 'tools/colonization_lab/canon/industry.json'
P_SHIP_NAMING = 'tools/colonization_lab/canon/ship_naming.json'
P_NAMING = 'tools/colonization_lab/canon/naming.json'
P_SITES = 'src/assets/data/galaxy_details/world_sites/sites_3126.json'
P_HUBS = 'src/assets/data/economy/hub_modules_3126.json'
P_SHIPREF = 'Notes/html/ship_reference/data.js'
OUT = os.path.join(REF, 'manufacturers_data.js')

PLANT_SITE_TYPE = {'orbital': 'orbital_hub'}
PLANT_RU = {
    'orbital': 'орбитальный хаб (верфь на орбите)',
    'industrial_hub': 'промышленный хаб',
    'mining_hub': 'горнодобывающий хаб',
    'military_hub': 'военный хаб',
}
KIND_RU = {
    'state_secret_fab': 'секретная государственная фабрика',
    'state_concern': 'государственный концерн',
    'state_plant': 'государственный завод',
    'corporation': 'корпорация',
    'consortium': 'консорциум',
    'cooperative': 'кооператив',
    'clan_cooperative': 'клановый кооператив',
}
# hub_modules political_bloc -> data.js coverage bloc id
BLOC_ALIAS = {'sol': 'sol_industrial', 'lalande_league': 'lalande_open_market'}
NO_BLOC = {'id': 'none', 'ru': 'Вне блоков'}
BODY_RU = {
    'kuiper_belt': 'пояс Койпера',
    'asteroid_belt': 'пояс астероидов',
    'trojans': 'троянцы',
}
NAMING_FIELDS = ('official_name', 'legal_heritage', 'native_script', 'romanization', 'display_name_ru',
                 'code_prefix', 'languages', 'index_format', 'brand', 'proper_names', 'military_vs_civil',
                 'variants', 'on_hull', 'crew_speech', 'ru_ui', 'recognition')
MODULE_INFO = (  # (key, format)
    ('pads', 'площадок: {}'), ('platforms', 'платформ: {}'), ('sectors', 'секторов: {}'), ('docks', 'доков: {}'),
    ('berths', 'причалов: {}'), ('tanker_berths', 'причалов танкеров: {}'), ('tug_berths', 'причалов буксиров: {}'),
    ('train_berths', 'причалов поезда: {}'), ('inspection_berths', 'досмотровых причалов: {}'),
    ('beds', 'мест: {}'), ('slots', 'мест: {}'), ('staff', 'штат: {}'),
    ('storage_t', '{} т'), ('tank_t', 'баки {} т'), ('teu', '{} TEU'),
    ('coverage_ls', '{} св.с'), ('repair', 'ремонт: {}'), ('tier', 'рынок: {}'), ('scope', '{}'), ('state', '{}'),
)


def load_json(rel):
    with open(os.path.join(ROOT, rel), encoding='utf-8') as f:
        return json.load(f)


def load_shipref_blocs():
    path = os.path.join(ROOT, P_SHIPREF)
    try:
        with open(path, encoding='utf-8') as f:
            txt = f.read()
        m = re.match(r'\s*window\.ELITE_SHIP_REF_DATA\s*=\s*', txt)
        body = txt[m.end():].rstrip().rstrip(';') if m else txt
        data = json.loads(body)
        return [{'id': b['id'], 'ru': b['ru']} for b in data.get('coverage', {}).get('blocs', [])]
    except Exception as e:  # concurrent edits may leave data.js mid-write; fall back to hub bloc ids
        print('warning: data.js blocs unavailable (%s)' % e, file=sys.stderr)
        return []


def num(v):
    if isinstance(v, float) and v.is_integer():
        v = int(v)
    if isinstance(v, int):
        return '{:,}'.format(v).replace(',', ' ')
    return str(v)


def body_name(body_id):
    if not body_id:
        return None
    last = body_id.split('.')[-1]
    return BODY_RU.get(last, last.replace('_', ' '))


def placement_text(pl):
    if not pl:
        return None
    if 'orbit' in pl:
        o = pl['orbit'] or {}
        s = 'орбита %s км' % num(o.get('altitude_km'))
        inc = o.get('inclination_deg')
        if inc:
            s += ', накл. %s°' % num(inc)
        return s
    if 'belt' in pl:
        b = pl['belt'] or {}
        return 'пояс r=%s а.е., долгота %s°' % (round(b.get('radius_au', 0), 2), round(b.get('longitude_deg', 0), 1))
    if 'surface' in pl:
        s = pl['surface'] or {}
        lat, lon = s.get('lat_deg'), s.get('lon_deg')
        if lat is None:
            return 'поверхность'
        return 'поверхность, %s° %s, долг. %s°' % (round(abs(lat), 1), 'с.ш.' if lat >= 0 else 'ю.ш.', round(lon, 1))
    if 'lagrange_point' in pl:
        lp = pl['lagrange_point'] or {}
        return 'точка %s (%s – %s)' % (lp.get('point'), body_name(lp.get('primary')), body_name(lp.get('secondary')))
    if 'heliocentric' in pl:
        h = pl['heliocentric'] or {}
        s = 'гелиоцентр. орбита a=%s а.е.' % round(h.get('a_au', 0), 2)
        if h.get('anchor_name'):
            s += ', у %s' % h['anchor_name']
            if h.get('offset_km'):
                s += ' (%s км)' % num(h['offset_km'])
        return s
    k = next(iter(pl))
    return k


def module_info(m):
    out = []
    for key, fmt in MODULE_INFO:
        if key in m and m[key] not in (None, ''):
            out.append(fmt.format(num(m[key])))
    return ', '.join(out)


def main():
    industry = load_json(P_INDUSTRY)
    ship_naming = load_json(P_SHIP_NAMING).get('manufacturers', {})
    naming = load_json(P_NAMING).get('entities', {})
    sites_doc = load_json(P_SITES)
    hubs_doc = load_json(P_HUBS)

    sites = {s['id']: s for s in sites_doc['sites']}
    for r in sites_doc.get('relays', []):
        sites.setdefault(r['id'], {'id': r['id'], 'parent_body_id': r.get('placement', {}).get('parent_body_id'),
                                   'placement': {k: v for k, v in r.get('placement', {}).items() if k != 'parent_body_id'}})
    sys_names = {s['system_id']: s['name'] for s in sites_doc.get('systems', [])}

    module_types = hubs_doc['module_types']
    blocks_ru = hubs_doc['blocks']
    block_order = [b for b in ('admin', 'passenger', 'cargo', 'service', 'industry', 'science', 'military', 'beacon') if b in blocks_ru]
    block_order += [b for b in blocks_ru if b not in block_order]
    group_ru = {}
    for grp in industry.get('block_groups', {}).values():
        group_ru.update(grp)

    mfrs_src = industry['manufacturers']
    mfr_polities = []
    for m in mfrs_src:
        if m['polity'] not in mfr_polities:
            mfr_polities.append(m['polity'])

    hubs_by_polity = collections.defaultdict(list)
    for h in hubs_doc['hubs']:
        if h.get('polity') in mfr_polities:
            hubs_by_polity[h['polity']].append(h)

    def hub_record(h):
        s = sites.get(h['id'], {})
        blocks = []
        mtotal = 0
        for b in h.get('blocks', []):
            mods = []
            for m in b['modules']:
                mods.append({'type': m['type'], 'count': m.get('count', 1), 'info': module_info(m)})
                mtotal += m.get('count', 1)
            blocks.append({'block': b['block'], 'modules': mods})
        sid = h.get('system_id', s.get('system_id'))
        rec = {
            'id': h['id'], 'name': h.get('name') or s.get('name'), 'type': h.get('type'), 'type_ru': h.get('type_ru'),
            'level': h.get('level'), 'system_id': sid,
            'system': h.get('system_name') or sys_names.get(sid),
            'body': body_name(s.get('parent_body_id')), 'body_id': s.get('parent_body_id'),
            'placement': placement_text(s.get('placement')),
            'population': s.get('population'), 'docks': s.get('docks'),
            'storage_t': s.get('storage_t') if s.get('storage_t') is not None else (h.get('totals') or {}).get('storage_t'),
            'operator': h.get('operator') if isinstance(h.get('operator'), str) and h.get('operator') != h.get('polity') else None,
            'purpose': h.get('purpose'), 'source': h.get('source'), 'hidden': bool(h.get('hidden')),
            'modules_total': mtotal, 'blocks': blocks,
        }
        return {k: v for k, v in rec.items() if v not in (None, '')}

    bloc_list = load_shipref_blocs()
    bloc_ru = {b['id']: b['ru'] for b in bloc_list}

    polities = {}
    for pid in mfr_polities:
        hubs = sorted(hubs_by_polity.get(pid, []), key=lambda h: (h.get('system_id') if h.get('system_id') is not None else 9999, h.get('type') or '', h['id']))
        recs = [hub_record(h) for h in hubs]
        uni = {}
        for r in recs:
            for b in r['blocks']:
                for m in b['modules']:
                    u = uni.setdefault(m['type'], {'type': m['type'], 'count': 0, 'hubs': set()})
                    u['count'] += m['count']
                    u['hubs'].add(r['id'])
        unification = []
        for t, u in uni.items():
            mt = module_types.get(t, {})
            unification.append({'type': t, 'ru': mt.get('ru', t), 'block': mt.get('block'), 'count': u['count'], 'hubs': len(u['hubs'])})
        unification.sort(key=lambda u: (block_order.index(u['block']) if u['block'] in block_order else 99, -u['hubs'], u['type']))
        blocs = collections.Counter(h.get('political_bloc') for h in hubs if h.get('political_bloc'))
        raw_bloc = blocs.most_common(1)[0][0] if blocs else None
        bloc = BLOC_ALIAS.get(raw_bloc, raw_bloc) if raw_bloc else NO_BLOC['id']
        ent = (naming.get(pid) or {}).get('polity') or {}
        endo = ent.get('endonym') or {}
        site_name = next((s.get('polity_name') for s in sites_doc['sites'] if s.get('polity') == pid and s.get('polity_name')), None)
        systems = []
        for r in recs:
            if r.get('system') and r['system'] not in systems:
                systems.append(r['system'])
        polities[pid] = {
            'id': pid,
            'ru': (ent.get('exonyms') or {}).get('ru') or site_name or pid,
            'formal_ru': (ent.get('formal_exonyms') or {}).get('ru'),
            'endonym': endo.get('text'), 'endonym_translit': endo.get('translit'), 'endonym_lang': endo.get('lang'),
            'bloc': bloc, 'bloc_ru': bloc_ru.get(bloc) or (NO_BLOC['ru'] if bloc == NO_BLOC['id'] else bloc),
            'systems': systems,
            'hubs': recs,
            'hub_count': len(recs),
            'module_count': sum(r['modules_total'] for r in recs),
            'unification': unification,
            'sites_without_modules': sum(1 for s in sites_doc['sites'] if s.get('polity') == pid and s['id'] not in {h['id'] for h in hubs}),
        }
        polities[pid] = {k: v for k, v in polities[pid].items() if v is not None}

    unresolved = []
    manufacturers = []
    for m in mfrs_src:
        site_type = PLANT_SITE_TYPE.get(m['plant'], m['plant'])
        pol = polities[m['polity']]
        plants = [r['id'] for r in pol['hubs'] if r.get('type') == site_type]
        if not plants:  # a plant site may exist without module data
            plants_sites = [s['id'] for s in sites_doc['sites'] if s.get('polity') == m['polity'] and s.get('type') == site_type]
            plants = plants_sites
        if not plants:
            unresolved.append('%s: plant=%s (site type %s) — no site of polity %s' % (m['id'], m['plant'], site_type, m['polity']))
        nm = ship_naming.get(m['id'])
        rec = {
            'id': m['id'], 'name': m['name'], 'polity': m['polity'], 'kind': m.get('kind'),
            'kind_ru': KIND_RU.get(m.get('kind'), m.get('kind')),
            'groups': [{'id': g, 'ru': group_ru.get(g, g)} for g in m.get('groups', [])],
            'plant': m['plant'], 'plant_site_type': site_type, 'plant_ru': PLANT_RU.get(m['plant'], m['plant']),
            'plants': plants, 'note': m.get('note'),
            'naming': {k: nm[k] for k in NAMING_FIELDS if nm and k in nm} if nm else None,
        }
        manufacturers.append({k: v for k, v in rec.items() if v is not None})

    order = [b['id'] for b in bloc_list]
    used_blocs = []
    for m in manufacturers:
        b = polities[m['polity']]['bloc']
        if b not in used_blocs:
            used_blocs.append(b)
    used_blocs.sort(key=lambda b: order.index(b) if b in order else len(order))
    blocs_out = [{'id': b, 'ru': bloc_ru.get(b) or (NO_BLOC['ru'] if b == NO_BLOC['id'] else b)} for b in used_blocs]
    manufacturers.sort(key=lambda m: (used_blocs.index(polities[m['polity']]['bloc']), mfr_polities.index(m['polity'])))

    used_types = sorted({u['type'] for p in polities.values() for u in p['unification']})
    out = {
        'meta': {
            'status': 'PROPOSAL (generated)',
            'generated_by': 'Notes/html/ship_reference/tools/build_manufacturers.py',
            'sources': [P_INDUSTRY, P_SHIP_NAMING, P_NAMING, P_SITES, P_HUBS, P_SHIPREF],
            'counts': {'manufacturers': len(manufacturers), 'with_naming': sum(1 for m in manufacturers if m.get('naming')),
                       'polities': len(polities), 'hubs': sum(p['hub_count'] for p in polities.values()),
                       'module_types': len(used_types), 'module_types_catalog': len(module_types)},
            'unresolved': unresolved,
        },
        'blocs': blocs_out,
        'blocks': {b: blocks_ru[b] for b in block_order},
        'module_types': {t: {'ru': v.get('ru', t), 'block': v.get('block'), 'function': v.get('function')} for t, v in sorted(module_types.items())},
        'polities': polities,
        'manufacturers': manufacturers,
    }
    js = 'window.ELITE_MANUFACTURERS_DATA=' + json.dumps(out, ensure_ascii=False, separators=(',', ':')) + ';\n'
    with open(OUT, 'w', encoding='utf-8', newline='\n') as f:
        f.write(js)
    c = out['meta']['counts']
    print('wrote %s (%d bytes): %s' % (os.path.relpath(OUT, ROOT), len(js.encode('utf-8')), json.dumps(c)))
    for u in unresolved:
        print('unresolved:', u)


if __name__ == '__main__':
    main()
