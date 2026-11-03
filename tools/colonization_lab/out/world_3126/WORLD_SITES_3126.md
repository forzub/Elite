# Мировые объекты 3126 (PROPOSAL)

Сгенерировано `tools/colonization_lab/world_sites.py` из `canon/world_sites.json`, каталога и out/world_3126 (экономика).

## Объекты

| Тип | Число |
|---|---:|
| горнодобывающий хаб | 117 |
| промышленный хаб | 101 |
| научный хаб | 43 |
| торговый хаб | 42 |
| наземный космопорт | 41 |
| орбитальный хаб | 39 |
| хаб на луне | 37 |
| военный хаб | 24 |
| карантинный хаб | 20 |
| пиратский анклав | 19 |
| хаб пояса | 7 |
| вольный порт | 5 |
| межзвёздная заправка | 4 |
| хаб на брошенном корабле | 2 |
| брошенный хаб | 1 |
| межзвёздный ретранслятор | 1 |

Всего сайтов 503; объектов глубокого космоса 9; агломераций Земли 61.

## Дорога в карман Керна

Цепочка [25, 24, 45, 37]: плечи [30.641, 30.753, 30.407] св. лет (предел 32.0).

## Жопа мира (月工门)

- доступ: [25, 24, 45, 41, 'dso.far_berth', 19], плечи [30.641, 30.753, 29.05, 21.023, 22.326] св. лет
- Cobrett туда-обратно от HD 104067: 4.042 т заряда при баке 3.0 т -> баков J20: 1
- поле «Лотоса»: прыжок HD 104067 → «Дальний Причал», коридор 1.5 а.е. (3.502 сут в одну сторону), в поле 6.681 сут
- запас: 2220 тел, извлекаемо всего 1588.665 г, осталось 959.276 г (верхняя оценка)
- цена 219221576 Cr/г; рейдов 192.175/год, тел 36.375/год, лотосита 39.248 г/год; первичный запас кончится через 25.81 лет
- средний рейд -112733 Cr; потерянных кораблей 43.393/год; жертв торговли людьми 51.793/год (AUTHOR_TRUTH)
- +3 св. года: новые прямые рёбра 17, новые достижимые системы ['GJ 1214', '9 Ceti']

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

Межзвёздных поездок 91 239 811/год  переселенцев 2 402 148  беженцев 22 097; поверхность ↔ орбита 405 173 280.

| Класс | Кораблей |
|---|---:|
| liner | 7291.3 |
| colonist_transport | 109.7 |
| cobrett_i_n1_pax | 121.9 |
| surface_lifter | 2447.2 |

Эвакуация 1.2 млн с кольца Нового Шанхая: 2.11 сут.

## Проверки

- OK one_id_one_object: 505 ids
- OK site_bodies_exist_in_catalog: ok
- OK every_site_has_placement: ok
- OK access_chains_use_real_hops_only: frontier.worlds_ass max 30.753, frontier.ceti_chest max 25.603, frontier.vega_observatory max 18.304, kern_pocket max 30.753
- OK kern_pocket_reachable_without_exceptions: legs [30.641, 30.753, 30.407]
- OK deep_space_objects_have_world_lines: 9 objects
- OK worlds_ass_is_max_risk_and_reward: frontier.worlds_ass: 250000000 / 0.386, frontier.ceti_chest: 2640000 / 0.016, frontier.vega_observatory: 2000000 / 0.006
- OK average_expedition_not_profitable: average visitor EV -112733 Cr, run cost 563663
- OK rare_successes_make_the_profit: best segment 10794436 Cr > 0, average -112733 Cr < 0
- OK buying_dearer_than_self_sourcing: 54805394 > 5606277 Cr/g
- OK lotosite_primary_stock_finite: 959.276 g left of 1588.665 g, ~25.81 years
- OK phenomena_have_developer_category: 13 phenomena: physical, sensor, subjective
- OK outbreaks_within_population: OB-3093-MANDALA 200/76; OB-3115-STELLA 126/52; OB-3121-PEREVAL 180/15
- OK lotus_identity_known_to_developers: NIRVANA-1 (CANON, AUTHOR_TRUTH)
- OK news_reaches_far_periphery_within_author_target: network diameter 1.75 d, Sol -> farthest frontier 3.959 d, target 7.0 d (AUTHOR_TARGET)
- OK migration_within_transport_capacity: demand 2424245 <= capacity 6005363 per year
- OK no_jump_ship_older_than_jump_drive: ok
- OK records_match_schemas: Hub / DeepSpaceObject / LocalAdministrator
- OK knowledge_never_before_truth: 5 records
- OK every_site_has_jurisdiction: 503 sites
- OK status_present: ok
