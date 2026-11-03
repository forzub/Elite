# Промышленность и спасательные службы 3126 (PROPOSAL)

Сгенерировано `tools/colonization_lab/industry.py`.

## Блоки

| Блок | Аналог | Члены | Конструкторская школа | Ядра | Производителей корпусов | Доля главного |
|---|---|---|---|---|---:|---:|
| Технократический пул Когниума | Запад / США | tau_ceti_cognium, cyg61_kern, hd20794_hellas2, delta_pav_freekarma | дорогие модульные | tau_ceti_cognium | 2 | 0.887 |
| Небесный пакт | СССР / Китай — плановый блок | eps_eri_harmony, gj832_tianhe, hd40307_firmament | простые однокорпусные | eps_eri_harmony | 2 | 0.744 |
| Лига Лаланд | Европа | lalande_hellas, gj581_europa, hd69830_verde | модульные гражданские, много фирм | tau_ceti_cognium | 3 | 0.5 |
| Южный круг | неприсоединившиеся / Глобальный Юг | hd20794_newindia, hd40307_maland, lalande_mecca | сборка из комплектов по лицензии | lalande_mecca | 3 | 0.414 |
| Пакт Цитадели | военная олигархия | wolf359_citadel, proxima_outpost | тяжёлые военные | wolf359_citadel | 2 | 0.523 |
| Вольные пояса | кланы и чёрный рынок | eps_eri_strife, gj581_freeclans | выдолбленные астероиды и переделки | — | 2 | 0.75 |
| Сол | метрополия-исполин | sol_authority | массовый конвейер | tau_ceti_cognium | 1 | 1.0 |
| Вне блоков | закрытые и малые | delta_pav_karma, gj876_next, leo83_zion, hd114783_hellas3, gj832_gaia_zone | своего нет | — | 4 | 0.991 |

## Кто строит корабли

| Полития | Блок | Роль | Корпус | Ядро | Реактор | Двигатель | Авионика |
|---|---|---|---|---|---|---|---|
| cyg61_kern | cognium_pool | делает узлы и запчасти | parts | import | parts | parts | parts |
| delta_pav_freekarma | cognium_pool | делает узлы и запчасти | parts | import | parts | parts | parts |
| delta_pav_karma | autarkies | строит только внутрисистемный флот (отказ от сети) | assembly | import | assembly | assembly | assembly |
| eps_eri_harmony | heavenly_pact | строит корабли целиком | concern | assembly | concern | concern | concern |
| eps_eri_strife | free_belts | делает узлы и запчасти | concern | parts | assembly | assembly | assembly |
| gj581_europa | lalande_league | делает узлы и запчасти | parts | import | parts | parts | parts |
| gj581_freeclans | free_belts | делает узлы и запчасти | assembly | parts | parts | parts | parts |
| gj832_gaia_zone | autarkies | покупает корабли | import | import | import | import | import |
| gj832_tianhe | heavenly_pact | строит корабли целиком | assembly | import | concern | concern | assembly |
| gj876_next | autarkies | покупает корабли | import | import | import | import | parts |
| hd114783_hellas3 | autarkies | покупает корабли | import | import | import | import | import |
| hd20794_hellas2 | cognium_pool | строит корабли целиком | concern | import | concern | concern | assembly |
| hd20794_newindia | southern_circle | строит корабли целиком | concern | import | concern | concern | concern |
| hd40307_firmament | heavenly_pact | делает узлы и запчасти | parts | import | parts | parts | parts |
| hd40307_maland | southern_circle | строит корабли целиком | assembly | import | assembly | assembly | assembly |
| hd69830_verde | lalande_league | делает узлы и запчасти | parts | import | parts | parts | parts |
| lalande_hellas | lalande_league | делает узлы и запчасти | parts | import | parts | parts | parts |
| lalande_mecca | southern_circle | строит корабли целиком | concern | concern | concern | concern | concern |
| leo83_zion | autarkies | покупает корабли | import | import | import | import | import |
| proxima_outpost | citadel_compact | делает узлы и запчасти | assembly | parts | assembly | assembly | assembly |
| sol_authority | sol | строит корабли целиком | concern | import | concern | concern | concern |
| tau_ceti_cognium | cognium_pool | строит корабли целиком | concern | concern | concern | concern | concern |
| wolf359_citadel | citadel_compact | строит корабли целиком | concern | concern | assembly | assembly | concern |

## Проверки

- OK every_polity_in_exactly_one_bloc: missing [], dup []
- OK core_makers_match_jump_lineage: ['lalande_mecca', 'tau_ceti_cognium', 'wolf359_citadel'] == ['lalande_mecca', 'tau_ceti_cognium', 'wolf359_citadel']
- OK every_block_has_a_source: ok
- OK citadel_cores_not_exported: ok
- OK allied_fleets_uniform: Pact main hull maker 0.744 (uniform), League 0.5 (many vendors)
- OK production_sites_exist: 43 sites
- OK rescue_covers_every_site: 518 sites
