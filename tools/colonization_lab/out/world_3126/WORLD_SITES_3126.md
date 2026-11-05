# Мировые объекты 3126 (PROPOSAL)

Сгенерировано `tools/colonization_lab/world_sites.py` из `canon/world_sites.json`, каталога и out/world_3126 (экономика).

## Объекты

| Тип | Число |
|---|---:|
| горнодобывающий хаб | 113 |
| промышленный хаб | 98 |
| орбитальный хаб | 48 |
| наземный космопорт | 47 |
| хаб на луне | 43 |
| научный хаб | 42 |
| торговый хаб | 36 |
| военный хаб | 25 |
| карантинный хаб | 20 |
| пиратский анклав | 19 |
| хаб пояса | 9 |
| вольный порт | 5 |
| межзвёздная заправка | 4 |
| хаб на брошенном корабле | 2 |
| брошенный хаб | 1 |
| межзвёздный ретранслятор | 1 |

Всего сайтов 513; объектов глубокого космоса 9; агломераций Земли 61.

## Дорога в карман Керна

Цепочка [25, 24, 45, 37]: плечи [30.641, 30.753, 30.407] св. лет (предел 32.0).

## Жопа мира (月工门)

- доступ: [25, 24, 45, 41, 'dso.far_berth', 19], плечи [30.641, 30.753, 29.05, 21.023, 22.326] св. лет
- Cobrett туда-обратно от HD 104067: 4.042 т заряда при баке 3.0 т -> баков J20: 1
- поле «Лотоса»: прыжок HD 104067 → «Дальний Причал», коридор 1.5 а.е. (3.502 сут в одну сторону), в поле 6.681 сут
- запас: 2220 тел, извлекаемо всего 1588.665 г, осталось 959.276 г (верхняя оценка)
- цена 3108998 Cr/г; рейдов 60.212/год, тел 48.345/год, лотосита 45.827 г/год; первичный запас кончится через 20.933 лет
- средний рейд -424017 Cr; потерянных кораблей 12.528/год; жертв торговли людьми 0.0/год (AUTHOR_TRUTH)
- стабилизатор: 8.925 г установлено на кораблях аномалий; обычная дальность прыжка не меняется

## Связь

Диаметр сети (обычная новость): 1.75 сут (AUTHOR_TARGET 7.0).

| Куда | Обычная, сут | Срочная, сут |
|---|---:|---:|
| Epsilon Eridani | 0.106 | 0.104 |
| Tau Ceti | 0.106 | 0.104 |
| Gliese 357 | 0.117 | 0.104 |
| Gliese 436 | 0.544 | 0.208 |
| 83 Leonis | 0.982 | 0.312 |
| HD 114783 | 1.419 | 0.417 |
| HD 102195 | 3.959 | 0.625 |
| 9 Ceti | 3.816 | 0.417 |
| HD 93083 | 3.649 | 0.625 |

## Пассажиры

Межзвёздных поездок 91 239 784/год  переселенцев 2 402 147  беженцев 22 097; поверхность ↔ орбита 405 173 284.

| Класс | Кораблей |
|---|---:|
| liner | 7291.7 |
| colonist_transport | 109.7 |
| cobrett_i_n1_pax | 122.5 |
| surface_lifter | 2447.2 |

Эвакуация 1.2 млн с кольца Нового Шанхая: 2.11 сут.

## Проверки

- OK one_id_one_object: 515 ids
- OK site_bodies_exist_in_catalog: ok
- OK every_site_has_placement: ok
- OK access_chains_use_real_hops_only: frontier.worlds_ass max 30.753, frontier.ceti_chest max 25.603, frontier.vega_observatory max 18.304, kern_pocket max 30.753
- OK kern_pocket_reachable_without_exceptions: legs [30.641, 30.753, 30.407]
- OK deep_space_objects_have_world_lines: 9 objects
- OK worlds_ass_is_max_risk_and_reward: frontier.worlds_ass: 250000000 / 0.368, frontier.ceti_chest: 2640000 / 0.016, frontier.vega_observatory: 2000000 / 0.006
- OK average_expedition_not_profitable: average visitor EV -424017 Cr, run cost 191573
- OK rare_successes_make_the_profit: best segment 861033 Cr > 0, average -424017 Cr < 0
- OK networks_self_source_cheaper_than_buying: connected 333323 < buy 777250 < informed 941793 Cr/g
- OK lotosite_primary_stock_finite: 959.276 g left of 1588.665 g, ~20.933 years
- OK phenomena_have_developer_category: 13 phenomena: physical, sensor, subjective
- OK outbreaks_within_population: OB-3093-MANDALA 200/76; OB-3115-STELLA 126/52; OB-3121-PEREVAL 180/15
- OK lotosite_price_in_author_target: 3108998 Cr/g, target [2000000, 5000000] (AUTHOR_TARGET); = 0.916 Cobrett-I
- OK lotosite_has_no_jump_range_effect: stabiliser only in anomalies; no +3 ly
- OK stabiliser_demand_small_and_specialised: stabilisers 0.852 g/y of 45.827 g/y; installed 8.925 g
- OK medical_is_main_demand: medical 31.787 g/y, research 13.188, stabilisers 0.852
- OK map_complexity_within_tier_budgets: core max 29; frontier max 2; colony max 6; developed max 11; periphery max 2
- OK sol.earth_major_hubs_in_author_target: 7 Earth complexes (target 6-8)
- OK sol.sol_major_hubs_within_budget: 29 / 30 major hubs (preferred [24, 30])
- OK sol.region_major_hubs_within_budget: belt 5, deep_space 1, earth 7, earth_cislunar 3, jupiter 4, mars 3, mercury 1, outer 2, saturn 2, venus 1
- OK sol.unique_sol_hub_ids: 38 ids
- OK sol.unique_sol_hub_names: 38 names
- OK sol.earth_hubs_orbital_separation: min 74 km between ('earth_hub.tikhiy', 'earth_hub.kourou') over 2 days
- OK sol.earth_hubs_above_atmosphere: all >= 400 km
- OK sol.every_spaceport_reaches_a_passenger_plane: ok
- OK sol.earth_demand_within_orbital_capacity: demand 398 422 537 legs/y <= 80 % of capacity 642 967 468
- OK sol.major_hub_utilisation_in_band: 6 major hubs within 0.4-0.75
- OK sol.surface_traffic_conserved: 398 422 537 of 398 422 537
- OK sol.routes_have_real_ends: 41 routes
- OK sol.every_driver_has_a_hub: ok
- OK sol.every_hub_has_traffic: ok
- OK sol.legacy_generic_ids_mapped_once: 26 legacy ids
- OK sol.earth_capacity_ge_demand_plus_reserve: 57 060 movements/day vs capacity 92 238 (utilisation 0.62)
- OK sol.relay_names_neutral_codes: 19 / 19 beacons named by region code + number
- OK lotus_identity_known_to_developers: NIRVANA-1 (CANON, AUTHOR_TRUTH)
- OK news_reaches_far_periphery_within_author_target: network diameter 1.75 d, Sol -> farthest frontier 3.959 d, target 7.0 d (AUTHOR_TARGET)
- OK migration_within_transport_capacity: demand 2424243 <= capacity 6005480 per year
- OK no_jump_ship_older_than_jump_drive: ok
- OK records_match_schemas: Hub / DeepSpaceObject / LocalAdministrator
- OK knowledge_never_before_truth: 5 records
- OK every_site_has_jurisdiction: 513 sites
- OK status_present: ok
