# Мировые объекты 3126 (PROPOSAL)

Сгенерировано `tools/colonization_lab/world_sites.py` из `canon/world_sites.json`, каталога и out/world_3126 (экономика).

## Объекты

| Тип | Число |
|---|---:|
| горнодобывающий хаб | 117 |
| промышленный хаб | 101 |
| научный хаб | 43 |
| торговый хаб | 41 |
| наземный космопорт | 41 |
| орбитальный хаб | 39 |
| хаб на луне | 37 |
| военный хаб | 23 |
| пиратский анклав | 19 |
| карантинный хаб | 19 |
| хаб пояса | 7 |
| вольный порт | 5 |
| межзвёздная заправка | 3 |
| брошенный хаб | 1 |
| хаб на брошенном корабле | 1 |
| межзвёздный ретранслятор | 1 |

Всего сайтов 498; объектов глубокого космоса 6; агломераций Земли 61.

## Дорога в карман Керна

Цепочка [25, 24, 45, 37]: плечи [30.641, 30.753, 30.407] св. лет (предел 32.0).

## Жопа мира (月工门)

- доступ: [25, 24, 45, 'dso.lotus', 19], плечи [30.641, 30.753, 21.095, 18.386] св. лет
- Cobrett туда-обратно от 83 Leonis: 3.682 т заряда при баке 3.0 т -> баков J20: 1
- мёртвая зона «Лотоса»: сигма без захвата 199573 км, с захватом 998 км; вероятность выйти внутрь 0.045 / 0.0, ожидаемая при маяке 0.62 — 0.017
- экспедиция: стоимость 1029035 Cr, ожидание -257982 Cr (средняя экспедиция убыточна), медиана -1029035 Cr
- лотосит: 73.696 кг/год, цена 9014761 Cr/кг, запасов на 60.044 лет; экспедиций ~534/год, теряется кораблей 25.978/год

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
| dso.lotus | 2.505 | 0.417 |
| HD 102195 | 4.029 | 0.521 |
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

- OK one_id_one_object: 500 ids
- OK site_bodies_exist_in_catalog: ok
- OK every_site_has_placement: ok
- OK access_chains_use_real_hops_only: frontier.worlds_ass max 30.753, frontier.ceti_chest max 25.603, frontier.vega_observatory max 18.304, kern_pocket max 30.753
- OK kern_pocket_reachable_without_exceptions: legs [30.641, 30.753, 30.407]
- OK deep_space_objects_have_world_lines: 6 objects
- OK worlds_ass_is_max_risk_and_reward: frontier.worlds_ass: 250000000 / 0.189, frontier.ceti_chest: 2640000 / 0.016, frontier.vega_observatory: 2000000 / 0.006
- OK average_expedition_not_profitable: EV -257982 Cr, cost 1029035
- OK news_reaches_far_periphery_within_author_target: network diameter 1.75 d, Sol -> farthest frontier 4.029 d, target 7.0 d (AUTHOR_TARGET)
- OK migration_within_transport_capacity: demand 2424245 <= capacity 6005363 per year
- OK no_jump_ship_older_than_jump_drive: ok
- OK records_match_schemas: Hub / DeepSpaceObject / LocalAdministrator
- OK knowledge_never_before_truth: 5 records
- OK every_site_has_jurisdiction: 498 sites
- OK status_present: ok
