# Промышленность и спасательные службы 3126 (PROPOSAL)

Сгенерировано `tools/colonization_lab/industry.py`. Блоки — `canon/political_order.json` (political.py); школы — `industry.json industrial_schools`.

## Блоки (политика) и школы (промышленность)

| Блок | Единицы | Конструкторские школы | Ядра | Производителей корпусов | Доля главного |
|---|---|---|---|---:|---:|
| Технократический пул Когниума | cyg61_kern, delta_pav_freekarma, hd20794_hellas2, tau_ceti_cognium | дорогие модульные | tau_ceti_cognium | 2 | 0.887 |
| Небесный пакт | eps_eri_harmony, gj832_tianhe, hd40307_firmament | простые однокорпусные | eps_eri_harmony | 2 | 0.744 |
| Лига Лаланд | gj581_europa, hd114783_hellas3, lalande_hellas | модульные гражданские, много фирм, своего нет | tau_ceti_cognium, — | 2 | 0.625 |
| Южный круг | hd20794_newindia, hd40307_maland, lalande_mecca | сборка из комплектов по лицензии | lalande_mecca | 3 | 0.414 |
| Пакт Цитадели | proxima_outpost, wolf359_citadel | тяжёлые военные | wolf359_citadel | 2 | 0.523 |
| Вольные пояса | eps_eri_strife, gj581_freeclans | выдолбленные астероиды и переделки | — | 2 | 0.75 |
| Сол | sol_authority | массовый конвейер | tau_ceti_cognium | 1 | 1.0 |
| вне блоков | gj832_gaia_zone, hd69830_verde, delta_pav_karma, gj876_next, leo83_zion | своего нет | — | — | — |

## Кто строит корабли

| Полития | Блок | Роль | Корпус | Ядро | Реактор | Двигатель | Авионика |
|---|---|---|---|---|---|---|---|
| cyg61_kern | cognium_pool | делает узлы и запчасти | parts | import | parts | parts | parts |
| delta_pav_freekarma | cognium_pool | делает узлы и запчасти | parts | import | parts | parts | parts |
| delta_pav_karma | вне блоков | строит только внутрисистемный флот (отказ от сети) | assembly | import | assembly | assembly | assembly |
| eps_eri_harmony | heavenly_pact | строит корабли целиком | concern | assembly | concern | concern | concern |
| eps_eri_strife | free_belts | делает узлы и запчасти | concern | parts | assembly | assembly | assembly |
| gj581_europa | lalande_league | делает узлы и запчасти | parts | import | parts | parts | parts |
| gj581_freeclans | free_belts | делает узлы и запчасти | assembly | parts | parts | parts | parts |
| gj832_gaia_zone | вне блоков | покупает корабли | import | import | import | import | import |
| gj832_tianhe | heavenly_pact | строит корабли целиком | assembly | import | concern | concern | assembly |
| gj876_next | вне блоков | покупает корабли | import | import | import | import | parts |
| hd114783_hellas3 | lalande_league | покупает корабли | import | import | import | import | import |
| hd20794_hellas2 | cognium_pool | строит корабли целиком | concern | import | concern | concern | assembly |
| hd20794_newindia | southern_circle | строит корабли целиком | concern | import | concern | concern | concern |
| hd40307_firmament | heavenly_pact | делает узлы и запчасти | parts | import | parts | parts | parts |
| hd40307_maland | southern_circle | строит корабли целиком | assembly | import | assembly | assembly | assembly |
| hd69830_verde | вне блоков | делает узлы и запчасти | parts | import | parts | parts | parts |
| lalande_hellas | lalande_league | делает узлы и запчасти | parts | import | parts | parts | parts |
| lalande_mecca | southern_circle | строит корабли целиком | concern | concern | concern | concern | concern |
| leo83_zion | вне блоков | покупает корабли | import | import | import | import | import |
| proxima_outpost | citadel_compact | делает узлы и запчасти | assembly | parts | assembly | assembly | assembly |
| sol_authority | sol | строит корабли целиком | concern | import | concern | concern | concern |
| tau_ceti_cognium | cognium_pool | строит корабли целиком | concern | concern | concern | concern | concern |
| wolf359_citadel | citadel_compact | строит корабли целиком | concern | concern | assembly | assembly | concern |

## Проверки

- OK every_unit_has_school_and_political_status: missing []
- OK schools_are_not_blocs: industry.json holds no political blocs (political_order.json)
- OK core_makers_match_jump_lineage: ['lalande_mecca', 'tau_ceti_cognium', 'wolf359_citadel'] == ['lalande_mecca', 'tau_ceti_cognium', 'wolf359_citadel']
- OK every_block_has_a_source: ok
- OK citadel_cores_not_exported: ok
- OK trade_core_flows_legal: all drive_cores flows of trade.py run through open / licensed / restricted strategic channels
- OK cognium_core_flows_only_to_licence_holders: holders: cyg61_kern, delta_pav_freekarma, eps_eri_harmony, gj581_europa, gj832_tianhe, gj876_next, hd20794_hellas2, hd20794_newindia, hd40307_firmament, hd40307_maland, hd69830_verde, lalande_hellas, sol_authority, tau_ceti_cognium
- OK new_karma_no_strategic_trade: ok
- OK citadel_serial_cores_stay_in_pact: ok
- OK svarog_serial_from_citadel_with_own_copies: Citadel share 1.00 of Meru core imports; own pirated copies (jump_technology how = stolen)
- OK tauranga_core_technology_own: own jump mathematics; not a licence holder; no imported cores
- OK southern_circle_survives_cognium_cutoff: Cognium cut-off: coverage 0.124, mobilised 0.678 (criterion >= survival_coverage)
- OK southern_circle_peacetime_share_in_accepted_range: peacetime Tauranga share 0.533 in accepted range [0.45, 0.6] (author 2026-11-05)
- OK trade_core_flows_from_makers_only: ok
- OK allied_fleets_uniform: Pact main hull maker 0.744 (uniform), League 0.625 (many vendors)
- OK production_sites_exist: 43 sites
- OK rescue_covers_every_site: 513 sites
