"""The names of the damaged hub buildings (no geometry, no imports: hub_plane's ruined city and hub_damage's builders
both read it - importing hub_damage from hub_plane at load time was circular through hub_belt)."""

DAMAGEABLE = ("hubb_hab_tower", "hubb_hab_slab", "hubk_hab_a", "hubk_hab_b", "hubk_hab_c")
KINDS = [("missile", "ракета"), ("laser", "лазер и рельсотрон"), ("gutted", "сорван фасад")]
DAMAGED_OF = {m: [f"{m}_dmg_{k}" for k, _ in KINDS] for m in DAMAGEABLE}
