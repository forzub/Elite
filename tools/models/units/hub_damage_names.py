"""The names of the damaged hub buildings (no geometry, no imports: hub_plane's ruined city and hub_damage's builders
both read it - importing hub_damage from hub_plane at load time was circular through hub_belt)."""

DAMAGEABLE = ("hubb_hab_tower", "hubb_hab_slab", "hubk_hab_a", "hubk_hab_b", "hubk_hab_c")
# torn / breach: the severe variants (author 2026-11-19: «вырвано половина жилой башни, большие сквозные повреждения»)
KINDS = [("missile", "ракета"), ("laser", "лазер и рельсотрон"), ("gutted", "сорван фасад"),
         ("torn", "вырвана половина"), ("breach", "сквозной пролом")]
DAMAGED_OF = {m: [f"{m}_dmg_{k}" for k, _ in KINDS] for m in DAMAGEABLE}

# the heavy variants of the other modules (tools/models/units/hub_damage_heavy.py): industrial halls with their
# workshops opened, the ship salon, the lattice mast. They are in DAMAGED_OF too - the ruined city swaps them like the
# residential ones (they have no _dmg_missile: the server's damage accounting still takes them intact -> destroyed)
HEAVY_KINDS = {
    "hubb_works": [("breach", "сквозной пролом цеха"), ("torn", "обрушен конец цеха")],
    "hubk_factory": [("breach", "сквозной пролом цеха")],
    "hubb_ship_salon": [("breach", "разбит салон")],
    "hubv_mast": [("bent", "сломана и согнута")],
}
for _m, _ks in HEAVY_KINDS.items():
    DAMAGED_OF[_m] = [f"{_m}_dmg_{k}" for k, _ in _ks]

# the city's frame (girders, trusses): bent / snapped variants. NOT in DAMAGED_OF (the ruin would bend girders far from
# any strike); the ruined city swaps only the frame girders near an epicentre (hub_plane._ruin, RUIN_BLOWN)
FRAME_KINDS = {
    "hubb_girder": [("bent", "погнута"), ("snapped", "переломлена")],
    "hubk_truss": [("bent", "погнута"), ("snapped", "переломлена")],
}
DAMAGED_FRAME = {m: [f"{m}_dmg_{k}" for k, _ in ks] for m, ks in FRAME_KINDS.items()}
