# Корабли 3126 — проектные паспорта

Статус: PROPOSAL. Генерирует `tools/colonization_lab/ship_designer.py` из `canon/ship_designs.json`; не править руками.

| Модель | Класс | Производитель | Схема | Сухая, т | Груз | Тяга, МН | g груж./лёгк. | Реакторы, МВт | Ядро | Цена, млн Cr | Проверки |
|---|---|---|---|---:|---|---:|---|---:|---|---:|---|
| Cobrett-I N1 | cobrett_i_n1 | Cobretty Corporation | NewtonianI | 260 | 120 т | 9.56 | 2.451/3.644 | 5000 | Я-0.5 | 4.0 | OK · WARN 2 |
| Cobrett-I A3 (вариант) | cobrett_i_n1 | Cobretty Corporation | AssistedI | 260 | 110 т | 19.12 | 5.054/7.342 | 10000 | Я-0.5 | 4.9 | OK · WARN 3 |
| OCS-4 | ocs4_lastochka | Orbital Freight Union «Ural-Kourou» | NewtonianI | 380 | 409 т | 3.3 | 0.404/0.88 | 2600 | — | 2.3 | OK · WARN 3 |
| SL-12 | surface_lifter | Orbital Freight Union «Ural-Kourou» | NewtonianY | 620 | 240 т, 144 пасс. | 21.99 | 2.494/3.583 | 27000 | — | 7.0 | OK · WARN 1 |
| C-120 «Вахак» | medium_freighter | Бхарат Ассембли | NewtonianY | 1900 | 2400 т | 12.99 | 0.292/0.683 | 9900 | М-5 | 14.9 | OK · WARN 3 |
| Хэ-120 «Синча» (вариант) | medium_freighter | Верфи Гармонии | NewtonianI | 1900 | 2400 т | 13.0 | 0.292/0.683 | 10000 | Х-5 | 13.8 | OK · WARN 2 |
| Хэ-T12 «Хэсе» | jump_train | Верфи Гармонии | NewtonianX | 10800 | 57600 т | 90.0 | 0.124/0.809 | 80000 | Х-80 | 89.9 | OK · WARN 2 |
| Хэ-T12G «Хэсе» | jump_tanker | Верфи Гармонии | NewtonianX | 12000 | 50000 т | 90.0 | 0.147/0.732 | 80000 | Х-80 | 93.5 | OK · WARN 2 |
| GT-3000 «Титан» (вариант) | gas_tanker | Лунно-Марсианские верфи | NewtonianY | 900 | 3000 т | 6.99 | 0.182/0.782 | 6600 | — | 4.6 | OK · WARN 1 |
| «Камень» (каменный танкер) (вариант) | gas_tanker | Каменные доки Раздора | NewtonianI | 2400 | 6000 т | 3.3 | 0.04/0.14 | 2600 | — | 4.3 | OK · WARN 3 |
| HT-20K | ore_tug | Kern Heavy Mining | NewtonianX | 700 | 0 т | 4.0 | 0.571/0.571 | 3320 | — | 3.3 | OK · WARN 3 |
| ST-6 | service_tender | Лунно-Марсианские верфи | AssistedI | 500 | 150 т | 3.0 | 0.459/0.604 | 2800 | — | 2.6 | OK · WARN 3 |
| PC-30 «Фарос» | patrol_cutter | Лунно-Марсианские верфи | AssistedI | 420 | 20 т, 6 пасс. | 13.0 | 2.926/3.065 | 9600 | Я-1 | 15.8 | OK · WARN 1 |
| B-4 класса «Фальк» | interceptor | Арсенал «Бастион» | AssistedX | 140 | 2 т | 6.2 | 4.383/4.445 | 10000 | — | 20.6 | OK · WARN 2 |
| Хэ-F22 класса «Шэнхэ» | escort_frigate | Верфи Гармонии | AssistedY | 2200 | 80 т | 32.1 | 1.385/1.434 | 32100 | Х-5 | 77.0 | OK · WARN 1 |
| B-90 класса «Ингрид Хауглан» | line_combatant | Арсенал «Бастион» | NewtonianX | 9000 | 300 т | 110.0 | 1.148/1.185 | 120000 | Б-12 | 800.4 | OK · WARN 1 |
| R-7 «Аргус» | recon_picket | Верфи Гипериона | NewtonianX | 600 | 10 т | 12.0 | 1.932/1.963 | 11000 | Я-1 | 37.0 | OK · WARN 1 |
| Хэ-B240 класса «Бутянь» | fleet_tender | Верфи Гармонии | NewtonianY | 5000 | 6000 т | 39.0 | 0.342/0.762 | 32100 | Х-12 | 59.8 | OK · WARN 2 |
| PDT-7 «Тунгуска» | impact_defence_tug | Лунно-Марсианские верфи | NewtonianX | 1600 | 200 т | 28.0 | 1.555/1.745 | 28000 | — | 33.1 | OK · WARN 1 |
| L-900 «Эос» | liner | Верфи Гипериона | NewtonianY | 2600 | 90 т, 900 пасс. | 12.0 | 0.407/0.431 | 15000 | Я-5 | 36.0 | OK · WARN 1 |
| T-5000 «Правас» | colonist_transport | Бхарат Ассембли | NewtonianX | 6500 | 600 т, 5000 пасс. | 17.32 | 0.196/0.222 | 13200 | М-12 | 58.4 | OK · WARN 1 |

## Cobrett-I N1 (Cobrett-I N1)

- Класс: `cobrett_i_n1`.
- Производитель: Cobretty Corporation (hd20794_hellas2, блок cognium_pool, школа modular_premium); верфь: industrial_hub.
- Назначение: быстрый гибкий фрахт: ценный и срочный груз, курьер, малый чартер.
- Корпус: wedge, 22.2 × 26.0 × 5.0 м; несущий клиновидный монокок; алюминий-литиевый сплав, C-SiC обшивка; силовой набор 175.6 т.
- Схема управления: Newtonian I (рантайм `NewtonianI`, existing:EliteCobraMk1:NewtonianI).
- Экипаж 2, пассажиры 0; груз 120 т, C20 6; автономность 30 сут; ресурс 40 лет.
- Масса: сухая 260 т (оборудование 84.4 т), в рабочем снаряжении 267.54 т, полная 397.74 т (груз 120.0 т + тара C20 10.2 т).
- Тяга 9.56 МН: 2.451 g гружёный (полная масса), 3.644 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 1 × ER-5 = 5000 МВт; бытовая нагрузка 4.3 МВт от 70 МВт; площадка реактора 120.4 м².
- Охлаждение: 380 м² радиаторов при 1050 K = 235.7 МВт; контуры 180 МВт; излучатель двигателя ⌀ 2.74 м.
- Цена: детали 2.27 млн, корпус 0.65 млн, прейскурант 3.99 млн Cr.
- Единичные точки отказа: Cobrett-I N1-A1A1, Cobrett-I N1-A2A1, Cobrett-I N1-A2A2, Cobrett-I N1-A3A2, Cobrett-I N1-A4A1, Cobrett-I N1-A6A1, Cobrett-I N1-A6A2, Cobrett-I N1-A6A3, Cobrett-I N1-A6A4, Cobrett-I N1-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| Cobrett-I N1-A1A1 | Cobretty N1 Axial Main Drive | 1 | Cobretty Corporation | yard, LRU, 36 ч |
| Cobrett-I N1-A2A1 | Cobretty ER-5 Propulsion Reactor | 1 | Cobretty Corporation | yard, LRU, 48 ч |
| Cobrett-I N1-A2A2 | Cobretty SR-70 Service Reactor | 1 | Cobretty Corporation | station, LRU, 12 ч |
| Cobrett-I N1-A3A1 | Cobretty C-SiC radiator panel set (10 m2) | 38 | Cobretty Corporation | crew, LRU, 1.5 ч |
| Cobrett-I N1-A3A2 | Cobretty coolant loop + pump (180 MW) | 1 | Cobretty Corporation | station, LRU, 8 ч |
| Cobrett-I N1-A4A1 | Ядро «Нулевая точка» Я-0.5 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | factory |
| Cobrett-I N1-A4A2 | Бак заряда прыжка (1 т) | 3 | Agora Modular | station, LRU, 6 ч |
| Cobrett-I N1-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 2 | Agora Modular | station, LRU, 6 ч |
| Cobrett-I N1-A5A1 | Cobretty field-coupled RCS nozzle | 24 | Cobretty Corporation | crew, LRU, 2 ч |
| Cobrett-I N1-A4A4 | Cobretty argon RCS tank | 1 | Cobretty Corporation | station, LRU, 6 ч |
| Cobrett-I N1-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| Cobrett-I N1-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| Cobrett-I N1-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| Cobrett-I N1-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| Cobrett-I N1-A7A1 | Евро-Хабитат ЖО L-4 (4 чел.) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| Cobrett-I N1-A7A2 | Cobretty катапультируемая капсула кабины (2 места) | 1 | Cobretty Corporation | station, LRU, 6 ч |
| Cobrett-I N1-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 6 | Agora Modular | crew, LRU, 3 ч |
| Cobrett-I N1-A9A1 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |
| Cobrett-I N1-A11A1 | Cobretty PD-1 оборонительная турель | 1 | Cobretty Corporation | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 1 drives for NewtonianI (1 aft + 0 fore)
- OK `one_generator_per_group` — 1 propulsion reactors for 1 groups
- OK `generator_covers_group_demand` — 5000 MWe per group vs 4850 MW demand
- OK `equipment_fits_dry_mass` — hull structure 176 t = 68% of dry 260 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 68% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 9.56 MN: 2.45 g loaded, 3.64 g in service (class burn 1.0 g)
- OK `thrust_near_class_target` — 9.6 MN vs class 9.56 MN
- OK `service_power_covers_hotel` — service 70 MWe vs hotel 4.3 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 24 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 480 MW installed, 144 MW simultaneous, from propulsion spare 150 MW
- OK `propulsion_power_near_class_target` — 5000 MWe vs class 5000 MWe
- OK `coolant_loops_carry_service_heat` — loops 180 MW vs service heat 9.6 MW
- OK `radiators_reject_service_heat` — 380 m2 at 1050 K = 236 MW vs 9.6 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 500 t vs gross 398 t
- OK `jump_charge_capacity` — 3 t charge vs class 3.0 t
- OK `life_support_capacity` — 4 places for 2 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 30 days
- OK `survival_seats_for_everyone_awake` — 2 survival seats for 2 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 6 C20 places vs 6
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 120 t vs class 120 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 0.38/0.47 M, reactor 0.52/0.57 M, jump_drive 0.65/0.76 M, electronics 0.20/0.24 M, life_support 0.07/0.09 M, rcs_thermal 0.29/0.17 M, weapons 0.12/0.15 M
- WARN `no_single_point_of_failure_in_critical_functions` — Cobrett-I N1-A1A1, Cobrett-I N1-A2A1, Cobrett-I N1-A2A2, Cobrett-I N1-A3A2, Cobrett-I N1-A6A1, Cobrett-I N1-A6A3, Cobrett-I N1-A6A4, Cobrett-I N1-A7A1

## Cobrett-I A3 (Cobrett-I A3)

- Класс: `cobrett_i_n1` — вариант kb.ship.cobrett-i.frnu.n1: другое поколение линии с законом Assisted: широкий вектор тяги CR-1 в четырёх гондолах, два реактора ER-5 (уже существует в игре).
- Производитель: Cobretty Corporation (hd20794_hellas2, блок cognium_pool, школа modular_premium); верфь: industrial_hub.
- Назначение: как N1, для пилотов с законом Assisted; дороже в обслуживании.
- Корпус: wedge, 22.2 × 26.0 × 5.0 м; несущий клиновидный монокок, гондолы двигателей; алюминий-литиевый сплав, C-SiC обшивка; силовой набор 119.7 т.
- Схема управления: Assisted I (рантайм `AssistedI`, existing:EliteCobraMk1:AssistedI).
- Экипаж 2, пассажиры 0; груз 110 т, C20 6; автономность 30 сут; ресурс 40 лет.
- Масса: сухая 260 т (оборудование 140.3 т), в рабочем снаряжении 265.54 т, полная 385.74 т (груз 110.0 т + тара C20 10.2 т).
- Тяга 19.12 МН: 5.054 g гружёный (полная масса), 7.342 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 2 × ER-5 = 10000 МВт; бытовая нагрузка 4.3 МВт от 70 МВт; площадка реактора 120.4 м².
- Охлаждение: 380 м² радиаторов при 1050 K = 235.7 МВт; контуры 180 МВт; излучатель двигателя ⌀ 2.74 м.
- Цена: детали 2.98 млн, корпус 0.65 млн, прейскурант 4.95 млн Cr.
- Единичные точки отказа: Cobrett-I A3-A2A2, Cobrett-I A3-A3A2, Cobrett-I A3-A4A1, Cobrett-I A3-A6A1, Cobrett-I A3-A6A2, Cobrett-I A3-A6A3, Cobrett-I A3-A6A4, Cobrett-I A3-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| Cobrett-I A3-A1A1 | Cobretty CR-1 Vector Main Drive | 4 | Cobretty Corporation | yard, LRU, 36 ч |
| Cobrett-I A3-A2A1 | Cobretty ER-5 Propulsion Reactor | 2 | Cobretty Corporation | yard, LRU, 48 ч |
| Cobrett-I A3-A2A2 | Cobretty SR-70 Service Reactor | 1 | Cobretty Corporation | station, LRU, 12 ч |
| Cobrett-I A3-A3A1 | Cobretty C-SiC radiator panel set (10 m2) | 38 | Cobretty Corporation | crew, LRU, 1.5 ч |
| Cobrett-I A3-A3A2 | Cobretty coolant loop + pump (180 MW) | 1 | Cobretty Corporation | station, LRU, 8 ч |
| Cobrett-I A3-A4A1 | Ядро «Нулевая точка» Я-0.5 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | factory |
| Cobrett-I A3-A4A2 | Бак заряда прыжка (1 т) | 3 | Agora Modular | station, LRU, 6 ч |
| Cobrett-I A3-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 2 | Agora Modular | station, LRU, 6 ч |
| Cobrett-I A3-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| Cobrett-I A3-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| Cobrett-I A3-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| Cobrett-I A3-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| Cobrett-I A3-A7A1 | Евро-Хабитат ЖО L-4 (4 чел.) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| Cobrett-I A3-A7A2 | Cobretty катапультируемая капсула кабины (2 места) | 1 | Cobretty Corporation | station, LRU, 6 ч |
| Cobrett-I A3-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 6 | Agora Modular | crew, LRU, 3 ч |
| Cobrett-I A3-A9A1 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |
| Cobrett-I A3-A11A1 | Cobretty PD-1 оборонительная турель | 1 | Cobretty Corporation | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for AssistedI (2 aft + 2 fore)
- OK `one_generator_per_group` — 2 propulsion reactors for 2 groups
- OK `generator_covers_group_demand` — 5000 MWe per group vs 4850 MW demand
- OK `equipment_fits_dry_mass` — hull structure 120 t = 46% of dry 260 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 46% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 19.12 MN: 5.05 g loaded, 7.34 g in service (class burn 1.0 g)
- WARN `thrust_near_class_target` — 19.1 MN vs class 9.56 MN
- OK `service_power_covers_hotel` — service 70 MWe vs hotel 4.3 MW (+15 %)
- OK `rcs_powered_from_propulsion_spare` — RCS 0 MW installed, 0 MW simultaneous, from propulsion spare 300 MW
- WARN `propulsion_power_near_class_target` — 10000 MWe vs class 5000 MWe
- OK `coolant_loops_carry_service_heat` — loops 180 MW vs service heat 9.6 MW
- OK `radiators_reject_service_heat` — 380 m2 at 1050 K = 236 MW vs 9.6 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 500 t vs gross 386 t
- OK `jump_charge_capacity` — 3 t charge vs class 3.0 t
- OK `life_support_capacity` — 4 places for 2 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 30 days
- OK `survival_seats_for_everyone_awake` — 2 survival seats for 2 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 6 C20 places vs 6
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 110 t vs class 120 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- OK `component_prices_near_class_model` — main_drive 0.76/0.47 M, reactor 0.98/0.57 M, jump_drive 0.63/0.76 M, electronics 0.20/0.24 M, life_support 0.07/0.09 M, rcs_thermal 0.17/0.17 M, weapons 0.12/0.15 M
- WARN `no_single_point_of_failure_in_critical_functions` — Cobrett-I A3-A2A2, Cobrett-I A3-A3A2, Cobrett-I A3-A6A1, Cobrett-I A3-A6A3, Cobrett-I A3-A6A4, Cobrett-I A3-A7A1

## OCS-4 (OCS-4)

- Класс: `ocs4_lastochka`.
- Производитель: Orbital Freight Union «Ural-Kourou» (sol_authority, блок sol, школа mass_production); верфь: Урал-Куру (орбитальные верфи ОФУ).
- Назначение: только орбита ↔ хабы и депо (без посадки, без прыжка).
- Корпус: box, 56.0 × 18.0 × 14.0 м; ферма-хребет с открытыми гнёздами C20; сталь-титановая ферма; силовой набор 314.9 т.
- Схема управления: Newtonian I (рантайм `NewtonianI`, generic).
- Экипаж 3, пассажиры 0; груз 409 т, C20 24; автономность 7 сут; ресурс 40 лет.
- Масса: сухая 380 т (оборудование 65.0 т), в рабочем снаряжении 382.396 т, полная 832.196 т (груз 409.0 т + тара C20 40.8 т).
- Тяга 3.3 МН: 0.404 g гружёный (полная масса), 0.88 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 1 × ЭР-2.6 = 2600 МВт; бытовая нагрузка 4.4 МВт от 20 МВт; площадка реактора 62.6 м².
- Охлаждение: 100 м² радиаторов при 1050 K = 62.0 МВт; контуры 300 МВт; излучатель двигателя ⌀ 2.02 м.
- Цена: детали 1.20 млн, корпус 0.52 млн, прейскурант 2.26 млн Cr.
- Единичные точки отказа: OCS-4-A1A1, OCS-4-A2A1, OCS-4-A2A2, OCS-4-A3A2, OCS-4-A6A1, OCS-4-A6A2, OCS-4-A6A3, OCS-4-A6A4, OCS-4-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| OCS-4-A1A1 | Урал-33 | 1 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| OCS-4-A2A1 | ОФУ ЭР-2.6 | 1 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 48 ч |
| OCS-4-A2A2 | ОФУ СР-20 | 1 | Orbital Freight Union «Ural-Kourou» | station, LRU, 12 ч |
| OCS-4-A3A1 | ЛМВ радиаторная секция (50 м2) | 2 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| OCS-4-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| OCS-4-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 2 | Agora Modular | station, LRU, 6 ч |
| OCS-4-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| OCS-4-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| OCS-4-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| OCS-4-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| OCS-4-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| OCS-4-A7A1 | ОФУ ЖО О-4 (челнок) | 1 | Orbital Freight Union «Ural-Kourou» | station, LRU, 10 ч |
| OCS-4-A7A2 | Спасательная капсула (4 места) | 1 | Agora Modular | station, LRU, 6 ч |
| OCS-4-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 24 | Agora Modular | crew, LRU, 3 ч |
| OCS-4-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 1 drives for NewtonianI (1 aft + 0 fore)
- OK `one_generator_per_group` — 1 propulsion reactors for 1 groups
- OK `generator_covers_group_demand` — 2600 MWe per group vs 2400 MW demand
- OK `equipment_fits_dry_mass` — hull structure 315 t = 83% of dry 380 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 83% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 3.30 MN: 0.40 g loaded, 0.88 g in service (class burn 0.4 g)
- OK `thrust_near_class_target` — 3.3 MN vs class 3.3 MN
- OK `service_power_covers_hotel` — service 20 MWe vs hotel 4.4 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 200 MW
- OK `propulsion_power_near_class_target` — 2600 MWe vs class 2600 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 9.8 MW
- OK `radiators_reject_service_heat` — 100 m2 at 1050 K = 62 MW vs 9.8 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 4 places for 3 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 14 days vs endurance 7 days
- OK `survival_seats_for_everyone_awake` — 4 survival seats for 3 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 24 C20 places vs 24
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 409 t vs class 409 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 0.26/0.22 M, reactor 0.33/0.26 M, electronics 0.20/0.09 M, life_support 0.05/0.05 M, rcs_thermal 0.25/0.14 M
- WARN `no_single_point_of_failure_in_critical_functions` — OCS-4-A1A1, OCS-4-A2A1, OCS-4-A2A2, OCS-4-A3A2, OCS-4-A6A1, OCS-4-A6A3, OCS-4-A6A4, OCS-4-A7A1

## SL-12 (SL-12)

- Класс: `surface_lifter`.
- Производитель: Orbital Freight Union «Ural-Kourou» (sol_authority, блок sol, школа mass_production); верфь: Урал-Куру.
- Назначение: космопорт на поверхности ↔ низкая орбита: 12 мест C20 или 12 модулей P20 (144 пассажира).
- Корпус: lifter, 48.0 × 16.0 × 16.0 м; цилиндрический корпус с теплозащитой и опорами; титан, абляционно-керамический экран; силовой набор 301.4 т.
- Схема управления: Newtonian Y (рантайм `NewtonianY`, generic).
- Экипаж 3, пассажиры 144; груз 240 т, C20 12; автономность 3 сут; ресурс 30 лет.
- Масса: сухая 620 т (оборудование 318.6 т), в рабочем снаряжении 625.916 т, полная 899.276 т (груз 252.96 т + тара C20 20.4 т).
- Тяга 21.99 МН: 2.494 g гружёный (полная масса), 3.583 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × SR-9 = 27000 МВт; бытовая нагрузка 12.8 МВт от 120 МВт; площадка реактора 216.7 м².
- Охлаждение: 200 м² радиаторов при 1050 K = 124.1 МВт; контуры 600 МВт; излучатель двигателя ⌀ 4.3 м.
- Цена: детали 4.48 млн, корпус 0.77 млн, прейскурант 6.96 млн Cr.
- Единичные точки отказа: SL-12-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| SL-12-A1A1 | Урал-73Л (подъёмный) | 3 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| SL-12-A2A1 | Sol Reactor Works SR-9 | 3 | Sol Reactor Works | yard, LRU, 48 ч |
| SL-12-A2A2 | Sol Reactor Works S-60 | 2 | Sol Reactor Works | station, LRU, 12 ч |
| SL-12-A3A1 | ЛМВ радиаторная секция (50 м2) | 4 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| SL-12-A3A2 | ЛМВ контур охлаждения (300 МВт) | 2 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| SL-12-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 3 | Agora Modular | station, LRU, 6 ч |
| SL-12-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| SL-12-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| SL-12-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| SL-12-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| SL-12-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| SL-12-A7A1 | ОФУ ЖО О-4 (челнок) | 2 | Orbital Freight Union «Ural-Kourou» | station, LRU, 10 ч |
| SL-12-A7A2 | Спасательная капсула (4 места) | 1 | Agora Modular | station, LRU, 6 ч |
| SL-12-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 12 | Agora Modular | crew, LRU, 3 ч |
| SL-12-A8A2 | Пассажирский модуль P20 (12 мест, своё ЖО на сутки, отстреливаемый) | 12 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 24 ч |
| SL-12-A9A1 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |
| SL-12-A10A1 | Теплозащитный экран подъёмника | 1 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 40 ч |
| SL-12-A10A2 | Посадочные опоры (4) | 1 | Orbital Freight Union «Ural-Kourou» | station, LRU, 12 ч |

Проверки:
- OK `drives_fill_layout_slots` — 3 drives for NewtonianY (3 aft + 0 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 9000 MWe per group vs 8670 MW demand
- OK `equipment_fits_dry_mass` — hull structure 301 t = 49% of dry 620 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 49% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 21.99 MN: 2.49 g loaded, 3.58 g in service (class burn 2.5 g)
- OK `thrust_near_class_target` — 22.0 MN vs class 22.0 MN
- OK `service_power_covers_hotel` — service 120 MWe vs hotel 12.8 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 990 MW
- OK `propulsion_power_near_class_target` — 27000 MWe vs class 27000 MWe
- OK `coolant_loops_carry_service_heat` — loops 600 MW vs service heat 28.4 MW
- OK `radiators_reject_service_heat` — 200 m2 at 1050 K = 124 MW vs 28.4 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 152 places for 147 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 14 days vs endurance 3 days
- OK `survival_seats_for_everyone_awake` — 148 survival seats for 147 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 12 C20 places vs 12
- OK `passenger_places` — 144 places vs 144 passengers (alternative load to cargo)
- OK `cargo_near_class_target` — 240 t vs class 240 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 1.20/1.02 M, reactor 1.64/1.27 M, electronics 0.34/0.17 M, life_support 0.08/0.19 M, rcs_thermal 0.34/0.34 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant

## C-120 «Вахак» (C-120)

- Класс: `medium_freighter`.
- Производитель: Бхарат Ассембли (hd20794_newindia, блок southern_circle, школа licensed_kit_assembly); верфь: industrial_hub.
- Назначение: вторичные межзвёздные трассы: контейнеры, машины, товары, медикаменты.
- Корпус: box, 95.0 × 30.0 × 26.0 м; ферма с 120 гнёздами C20, жилой модуль в носу; сталь, алюминий; силовой набор 1671.3 т.
- Схема управления: Newtonian Y (рантайм `NewtonianY`, generic).
- Экипаж 6, пассажиры 0; груз 2400 т, C20 120; автономность 40 сут; ресурс 36 лет.
- Масса: сухая 1900 т (оборудование 228.7 т), в рабочем снаряжении 1939.98 т, полная 4543.98 т (груз 2400.0 т + тара C20 204.0 т).
- Тяга 12.99 МН: 0.292 g гружёный (полная масса), 0.683 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × SR-3.3 = 9900 МВт; бытовая нагрузка 4.9 МВт от 60 МВт; площадка реактора 79.4 м².
- Охлаждение: 200 м² радиаторов при 1050 K = 124.1 МВт; контуры 300 МВт; излучатель двигателя ⌀ 2.26 м.
- Цена: детали 8.33 млн, корпус 2.88 млн, прейскурант 14.88 млн Cr.
- Единичные точки отказа: C-120-A2A2, C-120-A3A2, C-120-A4A1, C-120-A6A1, C-120-A6A2, C-120-A6A3, C-120-A6A4, C-120-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| C-120-A1A1 | Урал-43 | 3 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| C-120-A2A1 | Sol Reactor Works SR-3.3 | 3 | Sol Reactor Works | yard, LRU, 48 ч |
| C-120-A2A2 | Sol Reactor Works S-60 | 1 | Sol Reactor Works | station, LRU, 12 ч |
| C-120-A3A1 | ЛМВ радиаторная секция (50 м2) | 4 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| C-120-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| C-120-A4A1 | Дар аль-Мизан М-5 | 1 | Дар аль-Мизан | factory |
| C-120-A4A2 | Бак заряда прыжка (1 т) | 30 | Agora Modular | station, LRU, 6 ч |
| C-120-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 8 | Agora Modular | station, LRU, 6 ч |
| C-120-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| C-120-A6A1 | Мизан бортовой вычислитель МВ-1 | 1 | Дар аль-Мизан | station, LRU, 4 ч |
| C-120-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| C-120-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| C-120-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| C-120-A7A1 | Евро-Хабитат ЖО L-20 | 1 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| C-120-A7A2 | Спасательная капсула (4 места) | 2 | Agora Modular | station, LRU, 6 ч |
| C-120-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 120 | Agora Modular | crew, LRU, 3 ч |
| C-120-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 3 drives for NewtonianY (3 aft + 0 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 3300 MWe per group vs 3000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 1671 t = 88% of dry 1900 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 88% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 12.99 MN: 0.29 g loaded, 0.68 g in service (class burn 0.3 g)
- OK `thrust_near_class_target` — 13.0 MN vs class 13.0 MN
- OK `service_power_covers_hotel` — service 60 MWe vs hotel 4.9 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 900 MW
- OK `propulsion_power_near_class_target` — 9900 MWe vs class 10000 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 10.9 MW
- OK `radiators_reject_service_heat` — 200 m2 at 1050 K = 124 MW vs 10.9 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 5000 t vs gross 4544 t
- OK `jump_charge_capacity` — 30 t charge vs class 30.0 t
- OK `life_support_capacity` — 20 places for 6 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 90 days vs endurance 40 days
- OK `survival_seats_for_everyone_awake` — 8 survival seats for 6 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 120 C20 places vs 120
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 2400 t vs class 2400 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 1.59/1.44 M, reactor 1.97/1.71 M, jump_drive 3.80/3.24 M, electronics 0.18/0.38 M, life_support 0.16/0.14 M, rcs_thermal 0.28/0.54 M
- WARN `no_single_point_of_failure_in_critical_functions` — C-120-A2A2, C-120-A3A2, C-120-A6A1, C-120-A6A3, C-120-A6A4, C-120-A7A1

## Хэ-120 «Синча» (He-120)

- Класс: `medium_freighter` — вариант bharat.ship.c-120.konveyer: школа «простые однокорпусные»: один большой двигатель, тяжелее и дешевле; 74 % флота Небесного пакта — корпуса Гармонии (allied_fleets_uniform).
- Производитель: Верфи Гармонии (eps_eri_harmony, блок heavenly_pact, школа standard_monohull); верфь: orbital.
- Назначение: как C-120 для флотов Пакта; ремонт в любом порту блока.
- Корпус: box, 98.0 × 32.0 × 28.0 м; стандартный однокорпусный короб «Хэ» с внутренними гнёздами; сталь; силовой набор 1632.7 т.
- Схема управления: Newtonian I (рантайм `NewtonianI`, generic).
- Экипаж 7, пассажиры 0; груз 2400 т, C20 120; автономность 40 сут; ресурс 40 лет.
- Масса: сухая 1900 т (оборудование 267.4 т), в рабочем снаряжении 1940.31 т, полная 4544.31 т (груз 2400.0 т + тара C20 204.0 т).
- Тяга 13.0 МН: 0.292 g гружёный (полная масса), 0.683 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 1 × ТР-10 = 10000 МВт; бытовая нагрузка 7.6 МВт от 150 МВт; площадка реактора 267.8 м².
- Охлаждение: 200 м² радиаторов при 1000 K = 99.8 МВт; контуры 600 МВт; излучатель двигателя ⌀ 4.04 м.
- Цена: детали 7.87 млн, корпус 2.56 млн, прейскурант 13.85 млн Cr.
- Единичные точки отказа: He-120-A1A1, He-120-A2A1, He-120-A2A2, He-120-A3A2, He-120-A4A1, He-120-A6A1, He-120-A6A2, He-120-A6A3, He-120-A6A4, He-120-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| He-120-A1A1 | Тяньхэ ТЦ-13 | 1 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| He-120-A2A1 | Тяньхэ ТР-10 | 1 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| He-120-A2A2 | Тяньхэ СР-150 | 1 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| He-120-A3A1 | Тяньхэ радиаторная секция (50 м2) | 4 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| He-120-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 1 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| He-120-A4A1 | Гармония Х-5 (сборка из блоков Когниума) | 1 | Верфи Гармонии | factory |
| He-120-A4A2 | Бак заряда прыжка (1 т) | 30 | Agora Modular | station, LRU, 6 ч |
| He-120-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 8 | Agora Modular | station, LRU, 6 ч |
| He-120-A5A1 | Тяжёлый кластер RCS (4 сопла) | 8 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| He-120-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| He-120-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| He-120-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| He-120-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| He-120-A7A1 | Terra Habitat ЖО T-10 | 1 | Terra Habitat | station, LRU, 10 ч |
| He-120-A7A2 | Спасательная капсула (4 места) | 2 | Agora Modular | station, LRU, 6 ч |
| He-120-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 120 | Agora Modular | crew, LRU, 3 ч |
| He-120-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 1 drives for NewtonianI (1 aft + 0 fore)
- OK `one_generator_per_group` — 1 propulsion reactors for 1 groups
- OK `generator_covers_group_demand` — 10000 MWe per group vs 9000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 1633 t = 86% of dry 1900 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 86% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 13.00 MN: 0.29 g loaded, 0.68 g in service (class burn 0.3 g)
- OK `thrust_near_class_target` — 13.0 MN vs class 13.0 MN
- OK `service_power_covers_hotel` — service 150 MWe vs hotel 7.6 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 1200 MW installed, 360 MW simultaneous, from propulsion spare 1000 MW
- OK `propulsion_power_near_class_target` — 10000 MWe vs class 10000 MWe
- OK `coolant_loops_carry_service_heat` — loops 600 MW vs service heat 18.2 MW
- OK `radiators_reject_service_heat` — 200 m2 at 1000 K = 100 MW vs 18.2 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 5000 t vs gross 4544 t
- OK `jump_charge_capacity` — 30 t charge vs class 30.0 t
- OK `life_support_capacity` — 10 places for 7 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 40 days
- OK `survival_seats_for_everyone_awake` — 8 survival seats for 7 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 120 C20 places vs 120
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 2400 t vs class 2400 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- OK `component_prices_near_class_model` — main_drive 1.28/1.28 M, reactor 1.66/1.52 M, jump_drive 3.80/2.88 M, electronics 0.20/0.34 M, life_support 0.08/0.13 M, rcs_thermal 0.50/0.48 M
- WARN `no_single_point_of_failure_in_critical_functions` — He-120-A1A1, He-120-A2A1, He-120-A2A2, He-120-A3A2, He-120-A6A1, He-120-A6A3, He-120-A6A4, He-120-A7A1

## Хэ-T12 «Хэсе» (He-T12)

- Класс: `jump_train`.
- Производитель: Верфи Гармонии (eps_eri_harmony, блок heavenly_pact, школа standard_monohull); верфь: orbital.
- Назначение: магистрали: навал и контейнеры в модулях; тягач оставляет модули в депо и забирает готовые.
- Корпус: train, 160.0 × 40.0 × 40.0 м; тягач с ядром прыжка и сцепкой 12 модулей (42×20×20 м); сталь; силовой набор 4784.9 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 12, пассажиры 0; груз 57600 т, C20 2880; автономность 60 сут; ресурс 45 лет.
- Масса: сухая 10800 т (оборудование 6015.1 т), в рабочем снаряжении 11345.4 т, полная 73841.4 т (груз 57600.0 т + тара C20 4896.0 т).
- Тяга 90.0 МН: 0.124 g гружёный (полная масса), 0.809 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ТР-20 = 80000 МВт; бытовая нагрузка 26.4 МВт от 600 МВт; площадка реактора 535.6 м².
- Охлаждение: 1000 м² радиаторов при 1000 K = 499.0 МВт; контуры 1200 МВт; излучатель двигателя ⌀ 5.63 м.
- Цена: детали 56.13 млн, корпус 12.80 млн, прейскурант 89.89 млн Cr.
- Единичные точки отказа: He-T12-A2A2, He-T12-A4A1, He-T12-A6A1, He-T12-A6A2, He-T12-A6A3, He-T12-A6A4, He-T12-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| He-T12-A1A1 | Тяньхэ ТЦ-23 | 4 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| He-T12-A2A1 | Тяньхэ ТР-20 | 4 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| He-T12-A2A2 | Тяньхэ СР-600 | 1 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| He-T12-A3A1 | Тяньхэ радиаторная секция (50 м2) | 20 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| He-T12-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 2 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| He-T12-A4A1 | Гармония Х-80 (тягач поезда) | 1 | Верфи Гармонии | factory |
| He-T12-A4A2 | Бак заряда прыжка (1 т) | 420 | Agora Modular | station, LRU, 6 ч |
| He-T12-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 120 | Agora Modular | station, LRU, 6 ч |
| He-T12-A5A1 | Тяжёлый кластер RCS (4 сопла) | 16 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| He-T12-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| He-T12-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| He-T12-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| He-T12-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| He-T12-A7A1 | Terra Habitat ЖО T-60 (флот) | 1 | Terra Habitat | station, LRU, 10 ч |
| He-T12-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 1 | Agora Modular | station, LRU, 8 ч |
| He-T12-A8A1 | Модуль поезда грузовой (4 800 т, 240 C20) | 12 | Верфи Гармонии | station, LRU, 6 ч |
| He-T12-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |
| He-T12-A11A1 | Гармония турель ПРО ПТ-3 | 2 | Верфи Гармонии | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 20000 MWe per group vs 17500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 4785 t = 44% of dry 10800 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 44% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 90.00 MN: 0.12 g loaded, 0.81 g in service (class burn 0.12 g)
- OK `thrust_near_class_target` — 90.0 MN vs class 90.0 MN
- OK `service_power_covers_hotel` — service 600 MWe vs hotel 26.4 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 2400 MW installed, 720 MW simultaneous, from propulsion spare 10000 MW
- OK `propulsion_power_near_class_target` — 80000 MWe vs class 80000 MWe
- OK `coolant_loops_carry_service_heat` — loops 1200 MW vs service heat 60.0 MW
- OK `radiators_reject_service_heat` — 1000 m2 at 1000 K = 499 MW vs 60.0 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 80000 t vs gross 73841 t
- OK `jump_charge_capacity` — 420 t charge vs class 420.0 t
- OK `life_support_capacity` — 60 places for 12 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 60 days
- OK `survival_seats_for_everyone_awake` — 20 survival seats for 12 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 2880 C20 places vs 2880
- OK `passenger_places` — 0 places vs 0 passengers
- OK `module_payload_matches_cargo` — 57600 t in modules vs cargo 57600 t
- OK `cargo_near_class_target` — 57600 t vs class 57600 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 6.00/4.80 M, reactor 8.60/6.40 M, jump_drive 24.88/17.60 M, electronics 0.90/0.96 M, life_support 0.76/0.40 M, rcs_thermal 1.14/2.40 M
- WARN `no_single_point_of_failure_in_critical_functions` — He-T12-A2A2, He-T12-A6A1, He-T12-A6A3, He-T12-A6A4, He-T12-A7A1

## Хэ-T12G «Хэсе» (He-T12G)

- Класс: `jump_tanker`.
- Производитель: Верфи Гармонии (eps_eri_harmony, блок heavenly_pact, школа standard_monohull); верфь: orbital.
- Назначение: топливо реакторов и заряд прыжка к депо и импортёрам.
- Корпус: train, 160.0 × 40.0 × 40.0 м; тот же тягач «Локомотив» + 12 модулей-цистерн; сталь; силовой набор 4784.9 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 12, пассажиры 0; груз 50000 т, C20 0; автономность 60 сут; ресурс 45 лет.
- Масса: сухая 12000 т (оборудование 7215.1 т), в рабочем снаряжении 12545.4 т, полная 62545.4 т (груз 50000.0 т + тара C20 0.0 т).
- Тяга 90.0 МН: 0.147 g гружёный (полная масса), 0.732 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ТР-20 = 80000 МВт; бытовая нагрузка 26.4 МВт от 600 МВт; площадка реактора 535.6 м².
- Охлаждение: 1000 м² радиаторов при 1000 K = 499.0 МВт; контуры 1200 МВт; излучатель двигателя ⌀ 5.63 м.
- Цена: детали 57.33 млн, корпус 14.40 млн, прейскурант 93.54 млн Cr.
- Единичные точки отказа: He-T12G-A2A2, He-T12G-A4A1, He-T12G-A6A1, He-T12G-A6A2, He-T12G-A6A3, He-T12G-A6A4, He-T12G-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| He-T12G-A1A1 | Тяньхэ ТЦ-23 | 4 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| He-T12G-A2A1 | Тяньхэ ТР-20 | 4 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| He-T12G-A2A2 | Тяньхэ СР-600 | 1 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| He-T12G-A3A1 | Тяньхэ радиаторная секция (50 м2) | 20 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| He-T12G-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 2 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| He-T12G-A4A1 | Гармония Х-80 (тягач поезда) | 1 | Верфи Гармонии | factory |
| He-T12G-A4A2 | Бак заряда прыжка (1 т) | 420 | Agora Modular | station, LRU, 6 ч |
| He-T12G-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 120 | Agora Modular | station, LRU, 6 ч |
| He-T12G-A5A1 | Тяжёлый кластер RCS (4 сопла) | 16 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| He-T12G-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| He-T12G-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| He-T12G-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| He-T12G-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| He-T12G-A7A1 | Terra Habitat ЖО T-60 (флот) | 1 | Terra Habitat | station, LRU, 10 ч |
| He-T12G-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 1 | Agora Modular | station, LRU, 8 ч |
| He-T12G-A8A1 | Модуль поезда-цистерна (4 170 т) | 12 | Верфи Гармонии | station, LRU, 6 ч |
| He-T12G-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |
| He-T12G-A11A1 | Гармония турель ПРО ПТ-3 | 2 | Верфи Гармонии | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 20000 MWe per group vs 17500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 4785 t = 40% of dry 12000 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 40% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 90.00 MN: 0.15 g loaded, 0.73 g in service (class burn 0.13 g)
- OK `thrust_near_class_target` — 90.0 MN vs class 90.0 MN
- OK `service_power_covers_hotel` — service 600 MWe vs hotel 26.4 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 2400 MW installed, 720 MW simultaneous, from propulsion spare 10000 MW
- OK `propulsion_power_near_class_target` — 80000 MWe vs class 80000 MWe
- OK `coolant_loops_carry_service_heat` — loops 1200 MW vs service heat 60.0 MW
- OK `radiators_reject_service_heat` — 1000 m2 at 1000 K = 499 MW vs 60.0 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 80000 t vs gross 62545 t
- OK `jump_charge_capacity` — 420 t charge vs class 420.0 t
- OK `life_support_capacity` — 60 places for 12 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 60 days
- OK `survival_seats_for_everyone_awake` — 20 survival seats for 12 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `module_payload_matches_cargo` — 50040 t in modules vs cargo 50000 t
- OK `cargo_near_class_target` — 50000 t vs class 50000 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 6.00/4.80 M, reactor 8.60/6.40 M, jump_drive 24.88/17.60 M, electronics 0.90/0.96 M, life_support 0.76/0.40 M, rcs_thermal 1.14/2.40 M
- WARN `no_single_point_of_failure_in_critical_functions` — He-T12G-A2A2, He-T12G-A6A1, He-T12G-A6A3, He-T12G-A6A4, He-T12G-A7A1

## GT-3000 «Титан» (GT-3000)

- Класс: `gas_tanker` — вариант harmony.ship.he-t12c.tanker: отдельный экономический класс: внутрисистемный газовый танкер Сола (He-3/D с платформ газовых гигантов), без прыжка.
- Производитель: Лунно-Марсианские верфи (sol_authority, блок sol, школа mass_production); верфь: Лунно-Марсианские верфи.
- Назначение: He-3 и дейтерий с платформ Юпитера и Сатурна к депо и портам.
- Корпус: cylinder, 110.0 × 30.0 × 30.0 м; три криоцистерны на хребте; сталь, теплоизоляция; силовой набор 632.3 т.
- Схема управления: Newtonian Y (рантайм `NewtonianY`, generic).
- Экипаж 4, пассажиры 0; груз 3000 т, C20 0; автономность 60 сут; ресурс 40 лет.
- Масса: сухая 900 т (оборудование 267.7 т), в рабочем снаряжении 911.8 т, полная 3911.8 т (груз 3000.0 т + тара C20 0.0 т).
- Тяга 6.99 МН: 0.182 g гружёный (полная масса), 0.782 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × SR-2.2 = 6600 МВт; бытовая нагрузка 4.6 МВт от 60 МВт; площадка реактора 53.0 м².
- Охлаждение: 150 м² радиаторов при 1050 K = 93.0 МВт; контуры 300 МВт; излучатель двигателя ⌀ 1.85 м.
- Цена: детали 2.44 млн, корпус 1.10 млн, прейскурант 4.62 млн Cr.
- Единичные точки отказа: GT-3000-A2A2, GT-3000-A3A2, GT-3000-A6A1, GT-3000-A6A2, GT-3000-A6A3, GT-3000-A6A4, GT-3000-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| GT-3000-A1A1 | Урал-23 | 3 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| GT-3000-A2A1 | Sol Reactor Works SR-2.2 | 3 | Sol Reactor Works | yard, LRU, 48 ч |
| GT-3000-A2A2 | Sol Reactor Works S-60 | 1 | Sol Reactor Works | station, LRU, 12 ч |
| GT-3000-A3A1 | ЛМВ радиаторная секция (50 м2) | 3 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| GT-3000-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| GT-3000-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 10 | Agora Modular | station, LRU, 6 ч |
| GT-3000-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| GT-3000-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| GT-3000-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| GT-3000-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| GT-3000-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| GT-3000-A7A1 | Terra Habitat ЖО T-10 | 1 | Terra Habitat | station, LRU, 10 ч |
| GT-3000-A7A2 | Спасательная капсула (4 места) | 1 | Agora Modular | station, LRU, 6 ч |
| GT-3000-A8A1 | Криоцистерна He-3/D (1 000 т) | 3 | Лунно-Марсианские верфи | yard, LRU, 30 ч |
| GT-3000-A9A1 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 3 drives for NewtonianY (3 aft + 0 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 2200 MWe per group vs 2000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 632 t = 70% of dry 900 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 70% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 6.99 MN: 0.18 g loaded, 0.78 g in service (class burn 0.18 g)
- OK `thrust_near_class_target` — 7.0 MN vs class 7.0 MN
- OK `service_power_covers_hotel` — service 60 MWe vs hotel 4.6 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 600 MW
- OK `propulsion_power_near_class_target` — 6600 MWe vs class 6500 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 10.2 MW
- OK `radiators_reject_service_heat` — 150 m2 at 1050 K = 93 MW vs 10.2 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 10 places for 4 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 60 days
- OK `survival_seats_for_everyone_awake` — 4 survival seats for 4 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `module_payload_matches_cargo` — 3000 t in modules vs cargo 3000 t
- OK `cargo_near_class_target` — 3000 t vs class 3000 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- OK `component_prices_near_class_model` — main_drive 0.60/0.51 M, reactor 0.87/0.68 M, electronics 0.20/0.13 M, life_support 0.06/0.05 M, rcs_thermal 0.27/0.26 M
- WARN `no_single_point_of_failure_in_critical_functions` — GT-3000-A2A2, GT-3000-A3A2, GT-3000-A6A1, GT-3000-A6A3, GT-3000-A6A4, GT-3000-A7A1

## «Камень» (каменный танкер) (RT)

- Класс: `gas_tanker` — вариант sol.ship.gt-3000.titan: Вольные пояса: выдолбленный астероид как цистерна и щит, двигатель и реактор — восстановленные б/у ОФУ (лор Каменных доков).
- Производитель: Каменные доки Раздора (eps_eri_strife, блок free_belts, школа hollowed_asteroid_and_refit); верфь: mining_hub.
- Назначение: лёд и летучие внутри системы; каменная оболочка — защита от обломков; медленный и дешёвый.
- Корпус: asteroid, 60.0 × 45.0 × 40.0 м; выдолбленный каменный монолит, ферма двигателя; силикатная порода, стальная ферма; силовой набор 2338.6 т.
- Схема управления: Newtonian I (рантайм `NewtonianI`, generic).
- Экипаж 5, пассажиры 0; груз 6000 т, C20 0; автономность 90 сут; ресурс 21 лет.
- Масса: сухая 2400 т (оборудование 61.4 т), в рабочем снаряжении 2409.15 т, полная 8409.15 т (груз 6000.0 т + тара C20 0.0 т).
- Тяга 3.3 МН: 0.04 g гружёный (полная масса), 0.14 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 1 × ЭР-2.6 = 2600 МВт; бытовая нагрузка 6.2 МВт от 20 МВт; площадка реактора 62.6 м².
- Охлаждение: 100 м² радиаторов при 1050 K = 62.0 МВт; контуры 300 МВт; излучатель двигателя ⌀ 2.02 м.
- Цена: детали 1.38 млн, корпус 1.91 млн, прейскурант 4.29 млн Cr.
- Единичные точки отказа: RT-A1A1, RT-A2A1, RT-A2A2, RT-A3A2, RT-A6A1, RT-A6A2, RT-A6A3, RT-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| RT-A1A1 | Урал-33 | 1 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| RT-A2A1 | ОФУ ЭР-2.6 | 1 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 48 ч |
| RT-A2A2 | ОФУ СР-20 | 1 | Orbital Freight Union «Ural-Kourou» | station, LRU, 12 ч |
| RT-A3A1 | ЛМВ радиаторная секция (50 м2) | 2 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| RT-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| RT-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 6 | Agora Modular | station, LRU, 6 ч |
| RT-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| RT-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| RT-A6A2 | Kern радар обломков DR-5 (5 000 км) | 1 | Kern Heavy Mining | station, LRU, 6 ч |
| RT-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| RT-A7A1 | Евро-Хабитат ЖО L-20 | 1 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| RT-A7A2 | Спасательная капсула (4 места) | 2 | Agora Modular | station, LRU, 6 ч |
| RT-A9A1 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 1 drives for NewtonianI (1 aft + 0 fore)
- OK `one_generator_per_group` — 1 propulsion reactors for 1 groups
- OK `generator_covers_group_demand` — 2600 MWe per group vs 2400 MW demand
- OK `equipment_fits_dry_mass` — hull structure 2339 t = 97% of dry 2400 t (possible 50%-100%)
- OK `hull_structure_share_typical` — hull structure 97% of dry (typical 50%-100%)
- OK `thrust_meets_class_acceleration` — 3.30 MN: 0.04 g loaded, 0.14 g in service (class burn 0.03 g)
- WARN `thrust_near_class_target` — 3.3 MN vs class 7.0 MN
- OK `service_power_covers_hotel` — service 20 MWe vs hotel 6.2 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 200 MW
- WARN `propulsion_power_near_class_target` — 2600 MWe vs class 6500 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 13.7 MW
- OK `radiators_reject_service_heat` — 100 m2 at 1050 K = 62 MW vs 13.7 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 20 places for 5 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 90 days vs endurance 90 days
- OK `survival_seats_for_everyone_awake` — 8 survival seats for 5 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 6000 t vs class 3000 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- OK `component_prices_near_class_model` — main_drive 0.26/0.33 M, reactor 0.33/0.44 M, electronics 0.35/0.08 M, life_support 0.16/0.03 M, rcs_thermal 0.25/0.17 M
- WARN `no_single_point_of_failure_in_critical_functions` — RT-A1A1, RT-A2A1, RT-A2A2, RT-A3A2, RT-A6A1, RT-A6A3, RT-A7A1

## HT-20K (HT-20K)

- Класс: `ore_tug`.
- Производитель: Kern Heavy Mining (cyg61_kern, блок cognium_pool, школа modular_premium); верфь: mining_hub.
- Назначение: толкает плоты руды и льда (до 20 000 т) от рудников к заводам; буксирует оборудование и маяки.
- Корпус: tug, 60.0 × 25.0 × 25.0 м; открытая силовая рама, толкающая плита; сталь; силовой набор 563.5 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 3, пассажиры 0; груз 0 т, C20 0; автономность 90 сут; ресурс 46 лет.
- Масса: сухая 700 т (оборудование 136.5 т), в рабочем снаряжении 713.89 т, полная 713.89 т (груз 0.0 т + тара C20 0.0 т).
- Тяга 4.0 МН: 0.571 g гружёный (полная масса), 0.571 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ER-1 = 3320 МВт; бытовая нагрузка 6.2 МВт от 60 МВт; площадка реактора 20.0 м².
- Охлаждение: 150 м² радиаторов при 1050 K = 93.0 МВт; контуры 300 МВт; излучатель двигателя ⌀ 1.2 м.
- Цена: детали 1.87 млн, корпус 0.62 млн, прейскурант 3.29 млн Cr.
- Единичные точки отказа: HT-20K-A2A2, HT-20K-A3A2, HT-20K-A6A1, HT-20K-A6A2, HT-20K-A6A3, HT-20K-A6A4, HT-20K-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| HT-20K-A1A1 | Kern KT-1 Tug Drive | 4 | Kern Heavy Mining | yard, LRU, 36 ч |
| HT-20K-A2A1 | Cobretty ER-1 | 4 | Cobretty Corporation | yard, LRU, 48 ч |
| HT-20K-A2A2 | Sol Reactor Works S-60 | 1 | Sol Reactor Works | station, LRU, 12 ч |
| HT-20K-A3A1 | ЛМВ радиаторная секция (50 м2) | 3 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| HT-20K-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| HT-20K-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 12 | Agora Modular | station, LRU, 6 ч |
| HT-20K-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| HT-20K-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| HT-20K-A6A2 | Kern радар обломков DR-5 (5 000 км) | 1 | Kern Heavy Mining | station, LRU, 6 ч |
| HT-20K-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| HT-20K-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| HT-20K-A7A1 | Евро-Хабитат ЖО L-20 | 1 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| HT-20K-A7A2 | Спасательная капсула (4 места) | 1 | Agora Modular | station, LRU, 6 ч |
| HT-20K-A10A1 | Kern буксирная рама плота (20 000 т) | 1 | Kern Heavy Mining | station, LRU, 16 ч |
| HT-20K-A9A1 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 830 MWe per group vs 750 MW demand
- OK `equipment_fits_dry_mass` — hull structure 564 t = 80% of dry 700 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 80% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 4.00 MN: 0.57 g loaded, 0.57 g in service (class burn 0.02 g)
- OK `thrust_near_class_target` — 4.0 MN vs class 4.0 MN
- OK `service_power_covers_hotel` — service 60 MWe vs hotel 6.2 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 320 MW
- OK `propulsion_power_near_class_target` — 3320 MWe vs class 3300 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 13.7 MW
- OK `radiators_reject_service_heat` — 150 m2 at 1050 K = 93 MW vs 13.7 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 20 places for 3 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 90 days vs endurance 90 days
- OK `survival_seats_for_everyone_awake` — 4 survival seats for 3 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 0 t vs class 20000 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 0.40/0.50 M, reactor 0.49/0.53 M, electronics 0.36/0.10 M, life_support 0.14/0.05 M, rcs_thermal 0.27/0.20 M
- WARN `no_single_point_of_failure_in_critical_functions` — HT-20K-A2A2, HT-20K-A3A2, HT-20K-A6A1, HT-20K-A6A3, HT-20K-A6A4, HT-20K-A7A1

## ST-6 (ST-6)

- Класс: `service_tender`.
- Производитель: Лунно-Марсианские верфи (sol_authority, блок sol, школа mass_production); верфь: Лунно-Марсианские верфи.
- Назначение: ремонт рудников, станций, маяков и кораблей на месте; запчасти и мастерская.
- Корпус: box, 48.0 × 20.0 × 14.0 м; корпус с мастерской, складом запчастей и манипуляторами; алюминий, сталь; силовой набор 398.2 т.
- Схема управления: Assisted I (рантайм `AssistedI`, generic).
- Экипаж 8, пассажиры 0; груз 150 т, C20 6; автономность 30 сут; ресурс 35 лет.
- Масса: сухая 500 т (оборудование 101.8 т), в рабочем снаряжении 506.16 т, полная 666.36 т (груз 150.0 т + тара C20 10.2 т).
- Тяга 3.0 МН: 0.459 g гружёный (полная масса), 0.604 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 2 × SR-1.4 = 2800 МВт; бытовая нагрузка 5.5 МВт от 60 МВт; площадка реактора 33.7 м².
- Охлаждение: 150 м² радиаторов при 1050 K = 93.0 МВт; контуры 300 МВт; излучатель двигателя ⌀ 1.46 м.
- Цена: детали 1.53 млн, корпус 0.38 млн, прейскурант 2.58 млн Cr.
- Единичные точки отказа: ST-6-A2A2, ST-6-A3A2, ST-6-A6A1, ST-6-A6A2, ST-6-A6A3, ST-6-A6A4, ST-6-A7A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| ST-6-A1A1 | Урал-15 | 4 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| ST-6-A2A1 | Sol Reactor Works SR-1.4 | 2 | Sol Reactor Works | yard, LRU, 48 ч |
| ST-6-A2A2 | Sol Reactor Works S-60 | 1 | Sol Reactor Works | station, LRU, 12 ч |
| ST-6-A3A1 | ЛМВ радиаторная секция (50 м2) | 3 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| ST-6-A3A2 | ЛМВ контур охлаждения (300 МВт) | 1 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| ST-6-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 4 | Agora Modular | station, LRU, 6 ч |
| ST-6-A6A1 | Синьчжи MotherComputer MC-C1 (гражданский) | 1 | Синьчжи Электроникс | station, LRU, 4 ч |
| ST-6-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| ST-6-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 1 | Синьчжи Электроникс | crew, LRU, 2 ч |
| ST-6-A6A4 | Транспондер Управления Маяков Т-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| ST-6-A7A1 | Terra Habitat ЖО T-10 | 1 | Terra Habitat | station, LRU, 10 ч |
| ST-6-A7A2 | Спасательная капсула (4 места) | 2 | Agora Modular | station, LRU, 6 ч |
| ST-6-A7A3 | Медотсек (4 койки) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| ST-6-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 6 | Agora Modular | crew, LRU, 3 ч |
| ST-6-A9A1 | Отсек дронов (4 сервисных дрона) | 1 | Agora Modular | station, LRU, 8 ч |
| ST-6-A10A1 | Мастерская сервисного тендера (2 бригады) | 1 | Лунно-Марсианские верфи | station, LRU, 12 ч |
| ST-6-A9A2 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for AssistedI (2 aft + 2 fore)
- OK `one_generator_per_group` — 2 propulsion reactors for 2 groups
- OK `generator_covers_group_demand` — 1400 MWe per group vs 1250 MW demand
- OK `equipment_fits_dry_mass` — hull structure 398 t = 80% of dry 500 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 80% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 3.00 MN: 0.46 g loaded, 0.60 g in service (class burn 0.4 g)
- OK `thrust_near_class_target` — 3.0 MN vs class 3.0 MN
- OK `service_power_covers_hotel` — service 60 MWe vs hotel 5.5 MW (+15 %)
- OK `rcs_powered_from_propulsion_spare` — RCS 0 MW installed, 0 MW simultaneous, from propulsion spare 300 MW
- OK `propulsion_power_near_class_target` — 2800 MWe vs class 2800 MWe
- OK `coolant_loops_carry_service_heat` — loops 300 MW vs service heat 12.2 MW
- OK `radiators_reject_service_heat` — 150 m2 at 1050 K = 93 MW vs 12.2 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 10 places for 8 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 30 days
- OK `survival_seats_for_everyone_awake` — 8 survival seats for 8 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 6 C20 places vs 6
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 150 t vs class 150 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 0.26/0.22 M, reactor 0.37/0.26 M, electronics 0.20/0.14 M, life_support 0.16/0.08 M, rcs_thermal 0.10/0.12 M
- WARN `no_single_point_of_failure_in_critical_functions` — ST-6-A2A2, ST-6-A3A2, ST-6-A6A1, ST-6-A6A3, ST-6-A6A4, ST-6-A7A1

## PC-30 «Фарос» (PC-30)

- Класс: `patrol_cutter`.
- Производитель: Лунно-Марсианские верфи (sol_authority, блок sol, школа mass_production); верфь: Лунно-Марсианские верфи.
- Назначение: береговая охрана: таможня и досмотр, полиция, SAR, охрана маяков.
- Корпус: flat, 34.0 × 22.0 × 8.0 м; плоский корпус, ангар досмотрового катера; титан, керамокомпозитная броня; силовой набор 200.7 т.
- Схема управления: Assisted I (рантайм `AssistedI`, generic).
- Экипаж 9, пассажиры 6; груз 20 т, C20 0; автономность 30 сут; ресурс 30 лет.
- Масса: сухая 420 т (оборудование 219.3 т), в рабочем снаряжении 432.51 т, полная 453.05 т (груз 20.54 т + тара C20 0.0 т).
- Тяга 13.0 МН: 2.926 g гружёный (полная масса), 3.065 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 2 × SR-4.8 = 9600 МВт; бытовая нагрузка 27.6 МВт от 400 МВт; площадка реактора 115.6 м².
- Охлаждение: 200 м² радиаторов при 1050 K = 124.1 МВт; контуры 600 МВт; излучатель двигателя ⌀ 2.68 м.
- Цена: детали 10.62 млн, корпус 1.53 млн, прейскурант 15.75 млн Cr.
- Единичные точки отказа: PC-30-A4A1, PC-30-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| PC-30-A1A1 | Урал-65М (служебный) | 4 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| PC-30-A2A1 | Sol Reactor Works SR-4.8 | 2 | Sol Reactor Works | yard, LRU, 48 ч |
| PC-30-A2A2 | Sol Reactor Works S-200 | 2 | Sol Reactor Works | station, LRU, 12 ч |
| PC-30-A3A1 | ЛМВ радиаторная секция (50 м2) | 4 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| PC-30-A3A2 | ЛМВ контур охлаждения (300 МВт) | 2 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| PC-30-A4A1 | Ядро «Нулевая точка» Я-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | factory |
| PC-30-A4A2 | Бак заряда прыжка (1 т) | 5 | Agora Modular | station, LRU, 6 ч |
| PC-30-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 4 | Agora Modular | station, LRU, 6 ч |
| PC-30-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| PC-30-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| PC-30-A6A3 | Синьчжи связной комплекс C-3 (флот, защищённый) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| PC-30-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| PC-30-A7A1 | Terra Habitat ЖО T-10 | 2 | Terra Habitat | station, LRU, 10 ч |
| PC-30-A7A2 | Спасательная капсула (4 места) | 4 | Agora Modular | station, LRU, 6 ч |
| PC-30-A7A3 | Медотсек (4 койки) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| PC-30-A8A1 | Камера для задержанных и спасённых (6 мест) | 1 | Лунно-Марсианские верфи | yard, LRU, 24 ч |
| PC-30-A9A1 | Ангар досмотрового катера (1 катер) | 1 | Лунно-Марсианские верфи | yard |
| PC-30-A9A2 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |
| PC-30-A11A1 | Твердыня магнитная пушка КП-30 | 1 | Бронезавод Твердыни | station, LRU, 10 ч |
| PC-30-A11A2 | Твердыня турель ПРО ПТ-2 | 1 | Бронезавод Твердыни | station, LRU, 10 ч |
| PC-30-A12A1 | Броня керамокомпозитная (1 т) | 20 | Бронезавод Твердыни | yard, LRU, 2 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for AssistedI (2 aft + 2 fore)
- OK `one_generator_per_group` — 2 propulsion reactors for 2 groups
- OK `generator_covers_group_demand` — 4800 MWe per group vs 4500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 201 t = 48% of dry 420 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 48% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 13.00 MN: 2.93 g loaded, 3.06 g in service (class burn 1.5 g)
- OK `thrust_near_class_target` — 13.0 MN vs class 13.0 MN
- OK `service_power_covers_hotel` — service 400 MWe vs hotel 27.6 MW (+15 %)
- OK `rcs_powered_from_propulsion_spare` — RCS 0 MW installed, 0 MW simultaneous, from propulsion spare 600 MW
- OK `propulsion_power_near_class_target` — 9600 MWe vs class 9500 MWe
- OK `coolant_loops_carry_service_heat` — loops 600 MW vs service heat 61.4 MW
- OK `radiators_reject_service_heat` — 200 m2 at 1050 K = 124 MW vs 61.4 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 1000 t vs gross 453 t
- OK `jump_charge_capacity` — 5 t charge vs class 5.0 t
- OK `life_support_capacity` — 20.0 places for 15 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 30 days
- OK `survival_seats_for_everyone_awake` — 16.0 survival seats for 15 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 6 places vs 6 passengers
- OK `cargo_near_class_target` — 20 t vs class 20 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 1.40/1.19 M, reactor 2.14/1.45 M, jump_drive 1.84/1.53 M, electronics 2.47/1.53 M, life_support 0.24/0.20 M, rcs_thermal 0.18/0.51 M, weapons 1.90/1.19 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant
- OK `craft_bay_fit` — АК-1: 1 x boarding_boat -> bay 8.0 x 6.0 x 15.0 m in hull 22.0 x 8.0 x 34.0 m

## B-4 класса «Фальк» (B-4)

- Класс: `interceptor`.
- Производитель: Арсенал «Бастион» (wolf359_citadel, блок citadel_compact, школа heavy_military); верфь: military_hub.
- Назначение: быстрый ответ в системе: перехват пиратов и нарушителей, камней и обломков; короткая автономность.
- Корпус: dart, 18.0 × 12.0 × 4.0 м; плоская стрела, восемь гондол векторной тяги; титан, тяжёлая броня носа; силовой набор 26.5 т.
- Схема управления: Assisted X (рантайм `AssistedX`, generic).
- Экипаж 2, пассажиры 0; груз 2 т, C20 0; автономность 5 сут; ресурс 30 лет.
- Масса: сухая 140 т (оборудование 113.5 т), в рабочем снаряжении 142.24 т, полная 144.24 т (груз 2.0 т + тара C20 0.0 т).
- Тяга 6.2 МН: 4.383 g гружёный (полная масса), 4.445 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ТР-2.6В = 10000 МВт; бытовая нагрузка 27.4 МВт от 40 МВт; площадка реактора 60.2 м².
- Охлаждение: 300 м² радиаторов при 1050 K = 186.1 МВт; контуры 360 МВт; излучатель двигателя ⌀ 1.88 м.
- Цена: детали 14.75 млн, корпус 1.12 млн, прейскурант 20.56 млн Cr.
- Единичные точки отказа: B-4-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| B-4-A1A1 | Тяньхэ ТЦ-1.6В (военная поставка) | 8 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| B-4-A2A1 | Тяньхэ ТР-2.6В | 4 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| B-4-A2A2 | ОФУ СР-20 | 2 | Orbital Freight Union «Ural-Kourou» | station, LRU, 12 ч |
| B-4-A3A1 | Cobretty C-SiC radiator panel set (10 m2) | 30 | Cobretty Corporation | crew, LRU, 1.5 ч |
| B-4-A3A2 | Cobretty coolant loop + pump (180 MW) | 2 | Cobretty Corporation | station, LRU, 8 ч |
| B-4-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 2 | Agora Modular | station, LRU, 6 ч |
| B-4-A6A1 | Бастион боевой вычислитель БВ-1 | 2 | Арсенал «Бастион» | station, LRU, 4 ч |
| B-4-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| B-4-A6A3 | Бастион связь БС-1 | 2 | Арсенал «Бастион» | crew, LRU, 2 ч |
| B-4-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| B-4-A7A1 | ОФУ ЖО О-4 (челнок) | 2 | Orbital Freight Union «Ural-Kourou» | station, LRU, 10 ч |
| B-4-A7A2 | Cobretty катапультируемая капсула кабины (2 места) | 1 | Cobretty Corporation | station, LRU, 6 ч |
| B-4-A11A1 | Бастион скорострельная пушка БП-10 | 2 | Арсенал «Бастион» | station, LRU, 10 ч |
| B-4-A12A1 | Броня Бастиона тяжёлая (1 т) | 5 | Арсенал «Бастион» | yard, LRU, 2 ч |

Проверки:
- OK `drives_fill_layout_slots` — 8 drives for AssistedX (4 aft + 4 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 2500 MWe per group vs 2375 MW demand
- OK `equipment_fits_dry_mass` — hull structure 26 t = 19% of dry 140 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 19% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 6.20 MN: 4.38 g loaded, 4.44 g in service (class burn 4.0 g)
- OK `thrust_near_class_target` — 6.2 MN vs class 6.2 MN
- OK `service_power_covers_hotel` — service 40 MWe vs hotel 27.4 MW (+15 %)
- OK `rcs_powered_from_propulsion_spare` — RCS 0 MW installed, 0 MW simultaneous, from propulsion spare 500 MW
- OK `propulsion_power_near_class_target` — 10000 MWe vs class 10000 MWe
- OK `coolant_loops_carry_service_heat` — loops 360 MW vs service heat 60.9 MW
- OK `radiators_reject_service_heat` — 300 m2 at 1050 K = 186 MW vs 60.9 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 8 places for 2 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 14 days vs endurance 5 days
- OK `survival_seats_for_everyone_awake` — 2 survival seats for 2 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 2 t vs class 2 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- OK `grey_supply_declared` — N-10 (tau_ceti_cognium, dual_use black; реэкспорт)
- WARN `component_prices_near_class_model` — main_drive 1.84/1.82 M, reactor 1.65/2.24 M, electronics 8.87/1.68 M, life_support 0.10/0.11 M, rcs_thermal 0.18/0.73 M, weapons 2.10/2.52 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant

## Хэ-F22 класса «Шэнхэ» (He-F22)

- Класс: `escort_frigate`.
- Производитель: Верфи Гармонии (eps_eri_harmony, блок heavenly_pact, школа standard_monohull); верфь: orbital.
- Назначение: охрана конвоев: ПРО, сенсоры, автономность наравне с поездами.
- Корпус: cylinder, 85.0 × 30.0 × 18.0 м; стандартный корпус «Хэ» с бронепоясом; сталь, керамокомпозит; силовой набор 1337.1 т.
- Схема управления: Assisted Y (рантайм `AssistedY`, generic).
- Экипаж 40, пассажиры 0; груз 80 т, C20 0; автономность 90 сут; ресурс 35 лет.
- Масса: сухая 2200 т (оборудование 862.9 т), в рабочем снаряжении 2283.2 т, полная 2363.2 т (груз 80.0 т + тара C20 0.0 т).
- Тяга 32.1 МН: 1.385 g гружёный (полная масса), 1.434 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × ТР-11 = 32100 МВт; бытовая нагрузка 52.3 МВт от 1200 МВт; площадка реактора 286.5 м².
- Охлаждение: 500 м² радиаторов при 1000 K = 249.5 МВт; контуры 1200 МВт; излучатель двигателя ⌀ 4.0 м.
- Цена: детали 51.38 млн, корпус 8.00 млн, прейскурант 76.96 млн Cr.
- Единичные точки отказа: He-F22-A4A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| He-F22-A1A1 | Тяньхэ ТЦ-11 | 6 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| He-F22-A2A1 | Тяньхэ ТР-11 | 3 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| He-F22-A2A2 | Тяньхэ СР-600 | 2 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| He-F22-A3A1 | Тяньхэ радиаторная секция (50 м2) | 10 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| He-F22-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 2 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| He-F22-A4A1 | Гармония Х-5 (сборка из блоков Когниума) | 1 | Верфи Гармонии | factory |
| He-F22-A4A2 | Бак заряда прыжка (1 т) | 28 | Agora Modular | station, LRU, 6 ч |
| He-F22-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 30 | Agora Modular | station, LRU, 6 ч |
| He-F22-A5A1 | Тяжёлый кластер RCS (4 сопла) | 8 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| He-F22-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| He-F22-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 2 | Синьчжи Электроникс | station, LRU, 6 ч |
| He-F22-A6A3 | Синьчжи связной комплекс C-3 (флот, защищённый) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| He-F22-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| He-F22-A7A1 | Terra Habitat ЖО T-60 (флот) | 2 | Terra Habitat | station, LRU, 10 ч |
| He-F22-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 2 | Agora Modular | station, LRU, 8 ч |
| He-F22-A7A3 | Медотсек (4 койки) | 2 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| He-F22-A9A1 | Ангар досмотрового катера (1 катер) | 1 | Лунно-Марсианские верфи | yard |
| He-F22-A9A2 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |
| He-F22-A11A1 | Гармония пусковая ВПУ-8 (8 ракет) | 2 | Верфи Гармонии | station, LRU, 10 ч |
| He-F22-A11A2 | Гармония турель ПРО ПТ-3 | 6 | Верфи Гармонии | station, LRU, 10 ч |
| He-F22-A12A1 | Броня керамокомпозитная (1 т) | 220 | Бронезавод Твердыни | yard, LRU, 2 ч |

Проверки:
- OK `drives_fill_layout_slots` — 6 drives for AssistedY (3 aft + 3 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 10700 MWe per group vs 10000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 1337 t = 61% of dry 2200 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 61% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 32.10 MN: 1.39 g loaded, 1.43 g in service (class burn 1.4 g)
- OK `thrust_near_class_target` — 32.1 MN vs class 32.0 MN
- OK `service_power_covers_hotel` — service 1200 MWe vs hotel 52.3 MW (+15 %)
- OK `rcs_powered_from_propulsion_spare` — RCS 1200 MW installed, 360 MW simultaneous, from propulsion spare 2100 MW
- OK `propulsion_power_near_class_target` — 32100 MWe vs class 32000 MWe
- OK `coolant_loops_carry_service_heat` — loops 1200 MW vs service heat 119.0 MW
- OK `radiators_reject_service_heat` — 500 m2 at 1000 K = 249 MW vs 119.0 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 5000 t vs gross 2363 t
- OK `jump_charge_capacity` — 28 t charge vs class 28.0 t
- OK `life_support_capacity` — 120 places for 40 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 90 days
- OK `survival_seats_for_everyone_awake` — 40 survival seats for 40 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 80 t vs class 80 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 7.02/5.60 M, reactor 10.20/7.20 M, jump_drive 3.86/8.32 M, electronics 2.92/9.60 M, life_support 1.68/1.44 M, rcs_thermal 0.66/2.88 M, weapons 24.60/14.40 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant
- OK `craft_bay_fit` — АК-1: 1 x boarding_boat -> bay 8.0 x 6.0 x 15.0 m in hull 30.0 x 18.0 x 85.0 m

## B-90 класса «Ингрид Хауглан» (B-90)

- Класс: `line_combatant`.
- Производитель: Арсенал «Бастион» (wolf359_citadel, блок citadel_compact, школа heavy_military); верфь: military_hub.
- Назначение: сдерживание и война: дальние ракеты, сильные сенсоры, броня; главная боевая единица Цитадели.
- Корпус: cylinder, 180.0 × 45.0 × 35.0 м; бронированный корпус-цитадель, ВПУ по бортам; сталь, тяжёлая броня; силовой набор 5307.9 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 160, пассажиры 0; груз 300 т, C20 0; автономность 180 сут; ресурс 48 лет.
- Масса: сухая 9000 т (оборудование 3692.1 т), в рабочем снаряжении 9467.2 т, полная 9767.2 т (груз 300.0 т + тара C20 0.0 т).
- Тяга 110.0 МН: 1.148 g гружёный (полная масса), 1.185 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ТР-30В = 120000 МВт; бытовая нагрузка 184.2 МВт от 1200 МВт; площадка реактора 722.2 м².
- Охлаждение: 2000 м² радиаторов при 1000 K = 998.0 МВт; контуры 2400 МВт; излучатель двигателя ⌀ 6.4 м.
- Цена: детали 508.37 млн, корпус 109.20 млн, прейскурант 800.37 млн Cr.
- Единичные точки отказа: B-90-A4A1.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| B-90-A1A1 | Тяньхэ ТЦ-28В | 4 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| B-90-A2A1 | Тяньхэ ТР-30В | 4 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| B-90-A2A2 | Тяньхэ СР-600 | 2 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| B-90-A3A1 | Тяньхэ радиаторная секция (50 м2) | 40 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| B-90-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 4 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| B-90-A4A1 | Бастион Б-12 | 1 | Арсенал «Бастион» | factory |
| B-90-A4A2 | Бак заряда прыжка (1 т) | 120 | Agora Modular | station, LRU, 6 ч |
| B-90-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 160 | Agora Modular | station, LRU, 6 ч |
| B-90-A5A1 | Тяжёлый кластер RCS (4 сопла) | 16 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| B-90-A6A1 | Бастион боевой вычислитель БВ-1 | 2 | Арсенал «Бастион» | station, LRU, 4 ч |
| B-90-A6A2 | Бастион радар обзора С-50 (50 000 км) | 2 | Арсенал «Бастион» | station, LRU, 6 ч |
| B-90-A6A3 | Бастион связь БС-1 | 2 | Арсенал «Бастион» | crew, LRU, 2 ч |
| B-90-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| B-90-A7A1 | Terra Habitat ЖО T-200 (флот) | 2 | Terra Habitat | station, LRU, 10 ч |
| B-90-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 9 | Agora Modular | station, LRU, 8 ч |
| B-90-A7A3 | Медотсек (4 койки) | 3 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| B-90-A9A1 | Ангар перехватчиков (4 места) | 1 | Арсенал «Бастион» | yard |
| B-90-A9A2 | Отсек дронов (4 сервисных дрона) | 2 | Agora Modular | station, LRU, 8 ч |
| B-90-A9A3 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |
| B-90-A11A1 | Бастион ВПУ-16 дальних ракет | 6 | Арсенал «Бастион» | station, LRU, 10 ч |
| B-90-A11A2 | Бастион турель ПРО БТ-4 | 12 | Арсенал «Бастион» | station, LRU, 10 ч |
| B-90-A12A1 | Броня Бастиона тяжёлая (1 т) | 1600 | Арсенал «Бастион» | yard, LRU, 2 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 30000 MWe per group vs 27500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 5308 t = 59% of dry 9000 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 59% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 110.00 MN: 1.15 g loaded, 1.18 g in service (class burn 1.1 g)
- OK `thrust_near_class_target` — 110.0 MN vs class 110.0 MN
- OK `service_power_covers_hotel` — service 1200 MWe vs hotel 184.2 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 2400 MW installed, 720 MW simultaneous, from propulsion spare 10000 MW
- OK `propulsion_power_near_class_target` — 120000 MWe vs class 120000 MWe
- OK `coolant_loops_carry_service_heat` — loops 2400 MW vs service heat 418.6 MW
- OK `radiators_reject_service_heat` — 2000 m2 at 1000 K = 998 MW vs 418.6 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 12000 t vs gross 9767 t
- OK `jump_charge_capacity` — 120 t charge vs class 120.0 t
- OK `life_support_capacity` — 400 places for 160 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 180 days
- OK `survival_seats_for_everyone_awake` — 180 survival seats for 160 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 300 t vs class 300 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 42.00/58.80 M, reactor 58.20/79.80 M, jump_drive 67.20/92.40 M, electronics 44.42/117.60 M, life_support 6.78/16.80 M, rcs_thermal 1.56/33.60 M, weapons 285.00/189.00 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant
- OK `craft_bay_fit` — АП-4: 4 x bastion.ship.b-4.sokol -> bay 30.0 x 7.0 x 45.0 m in hull 45.0 x 35.0 x 180.0 m

## R-7 «Аргус» (R-7)

- Класс: `recon_picket`.
- Производитель: Верфи Гипериона (tau_ceti_cognium, блок cognium_pool, школа modular_premium); верфь: orbital.
- Назначение: сенсоры и раннее предупреждение у точек выхода, РЭБ, ретрансляция сети маяков в войну.
- Корпус: flat, 40.0 × 26.0 × 8.0 м; плоский малозаметный корпус, выдвижная мачта; композит, поглощающее покрытие; силовой набор 379.9 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 14, пассажиры 0; груз 10 т, C20 0; автономность 60 сут; ресурс 34 лет.
- Масса: сухая 600 т (оборудование 220.2 т), в рабочем снаряжении 623.3 т, полная 633.3 т (груз 10.0 т + тара C20 0.0 т).
- Тяга 12.0 МН: 1.932 g гружёный (полная масса), 1.963 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × ER-3 = 11000 МВт; бытовая нагрузка 68.5 МВт от 300 МВт; площадка реактора 66.2 м².
- Охлаждение: 600 м² радиаторов при 1050 K = 372.2 МВт; контуры 360 МВт; излучатель двигателя ⌀ 1.86 м.
- Цена: детали 25.55 млн, корпус 3.00 млн, прейскурант 37.01 млн Cr.
- Единичные точки отказа: R-7-A4A1, R-7-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| R-7-A1A1 | Hyperion HD-3 | 4 | Верфи Гипериона | yard, LRU, 36 ч |
| R-7-A2A1 | Cobretty ER-3 | 4 | Cobretty Corporation | yard, LRU, 48 ч |
| R-7-A2A2 | Cobretty SR-150 | 2 | Cobretty Corporation | station, LRU, 12 ч |
| R-7-A3A1 | Cobretty C-SiC radiator panel set (10 m2) | 60 | Cobretty Corporation | crew, LRU, 1.5 ч |
| R-7-A3A2 | Cobretty coolant loop + pump (180 MW) | 2 | Cobretty Corporation | station, LRU, 8 ч |
| R-7-A4A1 | Ядро «Нулевая точка» Я-1 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | factory |
| R-7-A4A2 | Бак заряда прыжка (1 т) | 7 | Agora Modular | station, LRU, 6 ч |
| R-7-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 10 | Agora Modular | station, LRU, 6 ч |
| R-7-A5A1 | Кластер RCS (4 сопла, поле) | 8 | Agora Modular | crew, LRU, 2 ч |
| R-7-A6A1 | Hyperion разведывательный вычислитель HR-1 | 2 | Верфи Гипериона | station, LRU, 4 ч |
| R-7-A6A2 | Hyperion пассивная решётка PA-100 + активный режим | 1 | Верфи Гипериона | station, LRU, 6 ч |
| R-7-A6A3 | Hyperion выдвижная сенсорная мачта | 1 | Верфи Гипериона | station, LRU, 10 ч |
| R-7-A6A4 | Hyperion комплекс РЭБ E-1 | 1 | Верфи Гипериона | station, LRU, 8 ч |
| R-7-A6A5 | Синьчжи связной комплекс C-3 (флот, защищённый) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| R-7-A6A6 | Hyperion полевой ретранслятор R-1 (сеть маяков в войну) | 1 | Верфи Гипериона | crew, LRU, 2 ч |
| R-7-A6A7 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| R-7-A7A1 | Евро-Хабитат ЖО L-20 | 2 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| R-7-A7A2 | Спасательная капсула (4 места) | 4 | Agora Modular | station, LRU, 6 ч |
| R-7-A7A3 | Медотсек (4 койки) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| R-7-A9A1 | Отсек дронов (4 сервисных дрона) | 1 | Agora Modular | station, LRU, 8 ч |
| R-7-A9A2 | Стыковочный узел (стандарт хабов) | 1 | Agora Modular | station, LRU, 8 ч |
| R-7-A11A1 | Твердыня турель ПРО ПТ-2 | 1 | Бронезавод Твердыни | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 2750 MWe per group vs 2500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 380 t = 63% of dry 600 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 63% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 12.00 MN: 1.93 g loaded, 1.96 g in service (class burn 2.0 g)
- OK `thrust_near_class_target` — 12.0 MN vs class 12.0 MN
- OK `service_power_covers_hotel` — service 300 MWe vs hotel 68.5 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 320 MW installed, 96 MW simultaneous, from propulsion spare 1000 MW
- OK `propulsion_power_near_class_target` — 11000 MWe vs class 11000 MWe
- OK `coolant_loops_carry_service_heat` — loops 360 MW vs service heat 152.2 MW
- OK `radiators_reject_service_heat` — 600 m2 at 1050 K = 372 MW vs 152.2 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 1000 t vs gross 633 t
- OK `jump_charge_capacity` — 7 t charge vs class 7.0 t
- OK `life_support_capacity` — 40 places for 14 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 90 days vs endurance 60 days
- OK `survival_seats_for_everyone_awake` — 16 survival seats for 14 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 10 t vs class 10 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 1.80/2.25 M, reactor 2.52/2.75 M, jump_drive 1.87/3.25 M, electronics 17.72/13.75 M, life_support 0.39/0.50 M, rcs_thermal 0.45/1.12 M, weapons 0.60/0.75 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant

## Хэ-B240 класса «Бутянь» (He-B240)

- Класс: `fleet_tender`.
- Производитель: Верфи Гармонии (eps_eri_harmony, блок heavenly_pact, школа standard_monohull); верфь: orbital.
- Назначение: автономность эскадры вдали от баз: топливо, заряд прыжка, боеприпасы, ремонт.
- Корпус: box, 150.0 × 40.0 × 35.0 м; стандартный корпус «Хэ», 240 гнёзд C20, ангар перехватчиков; сталь; силовой набор 3985.4 т.
- Схема управления: Newtonian Y (рантайм `NewtonianY`, generic).
- Экипаж 60, пассажиры 0; груз 6000 т, C20 240; автономность 180 сут; ресурс 40 лет.
- Масса: сухая 5000 т (оборудование 1014.6 т), в рабочем снаряжении 5220.2 т, полная 11628.2 т (груз 6000.0 т + тара C20 408.0 т).
- Тяга 39.0 МН: 0.342 g гружёный (полная масса), 0.762 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × ТР-11 = 32100 МВт; бытовая нагрузка 35.3 МВт от 1200 МВт; площадка реактора 286.5 м².
- Охлаждение: 600 м² радиаторов при 1000 K = 299.4 МВт; контуры 1200 МВт; излучатель двигателя ⌀ 4.04 м.
- Цена: детали 36.51 млн, корпус 9.60 млн, прейскурант 59.76 млн Cr.
- Единичные точки отказа: He-B240-A4A1, He-B240-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| He-B240-A1A1 | Тяньхэ ТЦ-13 | 3 | Тяньхэ Тяжмаш | yard, LRU, 36 ч |
| He-B240-A2A1 | Тяньхэ ТР-11 | 3 | Тяньхэ Тяжмаш | yard, LRU, 48 ч |
| He-B240-A2A2 | Тяньхэ СР-600 | 2 | Тяньхэ Тяжмаш | station, LRU, 12 ч |
| He-B240-A3A1 | Тяньхэ радиаторная секция (50 м2) | 12 | Тяньхэ Тяжмаш | crew, LRU, 1.5 ч |
| He-B240-A3A2 | Тяньхэ контур охлаждения (600 МВт) | 2 | Тяньхэ Тяжмаш | station, LRU, 8 ч |
| He-B240-A4A1 | Гармония Х-12 | 1 | Верфи Гармонии | factory |
| He-B240-A4A2 | Бак заряда прыжка (1 т) | 90 | Agora Modular | station, LRU, 6 ч |
| He-B240-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 60 | Agora Modular | station, LRU, 6 ч |
| He-B240-A5A1 | Тяжёлый кластер RCS (4 сопла) | 16 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| He-B240-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| He-B240-A6A2 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| He-B240-A6A3 | Синьчжи связной комплекс C-3 (флот, защищённый) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| He-B240-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| He-B240-A7A1 | Terra Habitat ЖО T-60 (флот) | 2 | Terra Habitat | station, LRU, 10 ч |
| He-B240-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 3 | Agora Modular | station, LRU, 8 ч |
| He-B240-A7A3 | Медотсек (4 койки) | 3 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| He-B240-A8A1 | Стойка C20 (замки и рельсы, 1 место) | 240 | Agora Modular | crew, LRU, 3 ч |
| He-B240-A9A1 | Ангар перехватчиков (2 места) | 1 | Верфи Гармонии | yard |
| He-B240-A9A2 | Отсек дронов (4 сервисных дрона) | 2 | Agora Modular | station, LRU, 8 ч |
| He-B240-A10A1 | Мастерская сервисного тендера (2 бригады) | 2 | Лунно-Марсианские верфи | station, LRU, 12 ч |
| He-B240-A9A3 | Стыковочный узел (стандарт хабов) | 4 | Agora Modular | station, LRU, 8 ч |
| He-B240-A11A1 | Гармония турель ПРО ПТ-3 | 2 | Верфи Гармонии | station, LRU, 10 ч |

Проверки:
- OK `drives_fill_layout_slots` — 3 drives for NewtonianY (3 aft + 0 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 10700 MWe per group vs 9000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 3985 t = 80% of dry 5000 t (possible 15%-90%)
- WARN `hull_structure_share_typical` — hull structure 80% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 39.00 MN: 0.34 g loaded, 0.76 g in service (class burn 0.35 g)
- OK `thrust_near_class_target` — 39.0 MN vs class 35.0 MN
- OK `service_power_covers_hotel` — service 1200 MWe vs hotel 35.3 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 2400 MW installed, 720 MW simultaneous, from propulsion spare 5100 MW
- OK `propulsion_power_near_class_target` — 32100 MWe vs class 33000 MWe
- OK `coolant_loops_carry_service_heat` — loops 1200 MW vs service heat 80.2 MW
- OK `radiators_reject_service_heat` — 600 m2 at 1000 K = 299 MW vs 80.2 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 12000 t vs gross 11628 t
- OK `jump_charge_capacity` — 90 t charge vs class 90.0 t
- OK `life_support_capacity` — 120 places for 60 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 180 days
- OK `survival_seats_for_everyone_awake` — 60 survival seats for 60 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 240 C20 places vs 240
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 6000 t vs class 6000 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 3.84/4.20 M, reactor 10.20/5.40 M, jump_drive 11.22/8.40 M, electronics 2.47/3.00 M, life_support 1.82/1.80 M, rcs_thermal 1.04/2.40 M, weapons 3.00/1.80 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant
- OK `craft_bay_fit` — АП-2: 2 x bastion.ship.b-4.sokol -> bay 30.0 x 7.0 x 24.0 m in hull 40.0 x 35.0 x 150.0 m

## PDT-7 «Тунгуска» (PDT-7)

- Класс: `impact_defence_tug`.
- Производитель: Лунно-Марсианские верфи (sol_authority, блок sol, школа mass_production); верфь: Лунно-Марсианские верфи.
- Назначение: защита планет и обитаемых станций: обнаружить, перехватить, увести, отклонить или раздробить камень; тяжёлое спасение.
- Корпус: tug, 70.0 × 30.0 × 30.0 м; силовая рама с тяжёлым захватом и пусковой ударников; сталь, броня носа; силовой набор 1007.3 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 16, пассажиры 0; груз 200 т, C20 0; автономность 45 сут; ресурс 35 лет.
- Масса: сухая 1600 т (оборудование 592.7 т), в рабочем снаряжении 1635.76 т, полная 1835.76 т (груз 200.0 т + тара C20 0.0 т).
- Тяга 28.0 МН: 1.555 g гружёный (полная масса), 1.745 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × SR-7 = 28000 МВт; бытовая нагрузка 141.7 МВт от 400 МВт; площадка реактора 168.5 м².
- Охлаждение: 700 м² радиаторов при 1050 K = 434.2 МВт; контуры 600 МВт; излучатель двигателя ⌀ 3.22 м.
- Цена: детали 22.21 млн, корпус 3.31 млн, прейскурант 33.08 млн Cr.
- Единичные точки отказа: нет.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| PDT-7-A1A1 | Урал-70 | 4 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| PDT-7-A2A1 | Sol Reactor Works SR-7 | 4 | Sol Reactor Works | yard, LRU, 48 ч |
| PDT-7-A2A2 | Sol Reactor Works S-200 | 2 | Sol Reactor Works | station, LRU, 12 ч |
| PDT-7-A3A1 | ЛМВ радиаторная секция (50 м2) | 14 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| PDT-7-A3A2 | ЛМВ контур охлаждения (300 МВт) | 2 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| PDT-7-A4A1 | Бункер топливных таблеток D/He-3 (1 т) | 30 | Agora Modular | station, LRU, 6 ч |
| PDT-7-A5A1 | Тяжёлый кластер RCS (4 сопла) | 8 | Тяньхэ Тяжмаш | crew, LRU, 2 ч |
| PDT-7-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| PDT-7-A6A2 | Kern радар обломков DR-5 (5 000 км) | 1 | Kern Heavy Mining | station, LRU, 6 ч |
| PDT-7-A6A3 | Синьчжи радар обзора N-10 (10 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| PDT-7-A6A4 | Синьчжи связной комплекс C-3 (флот, защищённый) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| PDT-7-A6A5 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| PDT-7-A7A1 | Terra Habitat ЖО T-60 (флот) | 2 | Terra Habitat | station, LRU, 10 ч |
| PDT-7-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 1 | Agora Modular | station, LRU, 8 ч |
| PDT-7-A7A3 | Медотсек (4 койки) | 1 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| PDT-7-A9A1 | Отсек дронов (4 сервисных дрона) | 2 | Agora Modular | station, LRU, 8 ч |
| PDT-7-A9A2 | Стыковочный узел (стандарт хабов) | 2 | Agora Modular | station, LRU, 8 ч |
| PDT-7-A10A1 | Kern тяжёлый захват (обломки, камни) | 1 | Kern Heavy Mining | station, LRU, 16 ч |
| PDT-7-A11A1 | Kern пусковая ударников МД-40 (масс-драйвер) | 1 | Kern Heavy Mining | station, LRU, 10 ч |
| PDT-7-A12A1 | Броня керамокомпозитная (1 т) | 60 | Бронезавод Твердыни | yard, LRU, 2 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 7000 MWe per group vs 6500 MW demand
- OK `equipment_fits_dry_mass` — hull structure 1007 t = 63% of dry 1600 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 63% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 28.00 MN: 1.56 g loaded, 1.75 g in service (class burn 1.6 g)
- OK `thrust_near_class_target` — 28.0 MN vs class 28.0 MN
- OK `service_power_covers_hotel` — service 400 MWe vs hotel 141.7 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 8 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 1200 MW installed, 360 MW simultaneous, from propulsion spare 2000 MW
- OK `propulsion_power_near_class_target` — 28000 MWe vs class 28000 MWe
- OK `coolant_loops_carry_service_heat` — loops 600 MW vs service heat 315.0 MW
- OK `radiators_reject_service_heat` — 700 m2 at 1050 K = 434 MW vs 315.0 MW
- OK `no_jump_core_on_insystem_class` — in-system class
- OK `life_support_capacity` — 120 places for 16 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 200 days vs endurance 45 days
- OK `survival_seats_for_everyone_awake` — 20 survival seats for 16 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 0 places vs 0 passengers
- OK `cargo_near_class_target` — 200 t vs class 200 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 4.50/3.83 M, reactor 6.14/4.84 M, electronics 2.69/2.29 M, life_support 1.54/0.51 M, rcs_thermal 0.69/1.53 M, weapons 5.55/3.19 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant

## L-900 «Эос» (L-900)

- Класс: `liner`.
- Производитель: Верфи Гипериона (tau_ceti_cognium, блок cognium_pool, школа modular_premium); верфь: orbital.
- Назначение: регулярные межзвёздные рейсы: 900 мест (60 каюты / 240 бизнес / 600 эконом), почта и 90 т срочного груза.
- Корпус: cylinder, 150.0 × 34.0 × 28.0 м; модульный корпус: 9 пассажирских секций Agora на хребте; алюминий, композит; силовой набор 1550.8 т.
- Схема управления: Newtonian Y (рантайм `NewtonianY`, generic).
- Экипаж 40, пассажиры 900; груз 90 т, C20 0; автономность 30 сут; ресурс 46 лет.
- Масса: сухая 2600 т (оборудование 1049.2 т), в рабочем снаряжении 2837.8 т, полная 3008.8 т (груз 171.0 т + тара C20 0.0 т).
- Тяга 12.0 МН: 0.407 g гружёный (полная масса), 0.431 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 3 × ER-5 = 15000 МВт; бытовая нагрузка 42.5 МВт от 300 МВт; площадка реактора 120.4 м².
- Охлаждение: 4000 м² радиаторов при 1050 K = 2481.3 МВт; контуры 360 МВт; излучатель двигателя ⌀ 2.35 м.
- Цена: детали 20.59 млн, корпус 6.50 млн, прейскурант 35.96 млн Cr.
- Единичные точки отказа: L-900-A4A1, L-900-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| L-900-A1A1 | Hyperion HD-4 | 3 | Верфи Гипериона | yard, LRU, 36 ч |
| L-900-A2A1 | Cobretty ER-5 Propulsion Reactor | 3 | Cobretty Corporation | yard, LRU, 48 ч |
| L-900-A2A2 | Cobretty SR-150 | 2 | Cobretty Corporation | station, LRU, 12 ч |
| L-900-A3A1 | Cobretty C-SiC radiator panel set (10 m2) | 400 | Cobretty Corporation | crew, LRU, 1.5 ч |
| L-900-A3A2 | Cobretty coolant loop + pump (180 MW) | 2 | Cobretty Corporation | station, LRU, 8 ч |
| L-900-A4A1 | Ядро «Нулевая точка» Я-5 | 1 | Фабрика ядер Управления Маяков «Нулевая точка» | factory |
| L-900-A4A2 | Бак заряда прыжка (1 т) | 45 | Agora Modular | station, LRU, 6 ч |
| L-900-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 20 | Agora Modular | station, LRU, 6 ч |
| L-900-A5A1 | Кластер RCS (4 сопла, поле) | 16 | Agora Modular | crew, LRU, 2 ч |
| L-900-A6A1 | Синьчжи MotherComputer MC-C3 (флот) | 2 | Синьчжи Электроникс | station, LRU, 4 ч |
| L-900-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| L-900-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| L-900-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| L-900-A7A1 | Евро-Хабитат ЖО L-200 (лайнер) | 5 | Консорциум «Евро-Хабитат» | station, LRU, 10 ч |
| L-900-A7A2 | Спасательная шлюпка (20 мест, 10 суток) | 47 | Agora Modular | station, LRU, 8 ч |
| L-900-A7A3 | Медотсек (4 койки) | 4 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| L-900-A8A1 | Пассажирская секция Agora (100 мест) | 9 | Agora Modular | yard, LRU, 24 ч |
| L-900-A8A2 | Грузовой отсек (100 т, 300 м3) | 1 | Agora Modular | yard |
| L-900-A9A1 | Стыковочный узел (стандарт хабов) | 4 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 3 drives for NewtonianY (3 aft + 0 fore)
- OK `one_generator_per_group` — 3 propulsion reactors for 3 groups
- OK `generator_covers_group_demand` — 5000 MWe per group vs 4000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 1551 t = 60% of dry 2600 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 60% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 12.00 MN: 0.41 g loaded, 0.43 g in service (class burn 0.3 g)
- OK `thrust_near_class_target` — 12.0 MN vs class 12.0 MN
- OK `service_power_covers_hotel` — service 300 MWe vs hotel 42.5 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 640 MW installed, 192 MW simultaneous, from propulsion spare 3000 MW
- OK `propulsion_power_near_class_target` — 15000 MWe vs class 14000 MWe
- OK `coolant_loops_carry_service_heat` — loops 360 MW vs service heat 94.5 MW
- OK `radiators_reject_service_heat` — 4000 m2 at 1050 K = 2481 MW vs 94.5 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 5000 t vs gross 3009 t
- OK `jump_charge_capacity` — 45 t charge vs class 45.0 t
- OK `life_support_capacity` — 1000.0 places for 940 awake persons (0 in cold sleep)
- OK `life_support_endurance` — 40 days vs endurance 30 days
- OK `survival_seats_for_everyone_awake` — 940.0 survival seats for 940 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 900 places vs 900 passengers
- OK `cargo_near_class_target` — 90 t vs class 90 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 1.90/2.38 M, reactor 1.70/3.00 M, jump_drive 5.13/6.00 M, electronics 0.96/1.12 M, life_support 6.64/4.50 M, rcs_thermal 1.80/1.00 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant

## T-5000 «Правас» (T-5000)

- Класс: `colonist_transport`.
- Производитель: Бхарат Ассембли (hd20794_newindia, блок southern_circle, школа licensed_kit_assembly); верфь: industrial_hub.
- Назначение: миграция и эвакуация: 5 000 мест (70 % койки, 30 % анабиоз), медленно и дёшево на человека; реквизируется при бедствиях.
- Корпус: cylinder, 220.0 × 50.0 × 40.0 м; хребет с 7 секциями коек и 15 блоками анабиоза; сталь; силовой набор 3825.5 т.
- Схема управления: Newtonian X (рантайм `NewtonianX`, generic).
- Экипаж 60, пассажиры 5000 (в анабиозе 1500); груз 600 т, C20 0; автономность 60 сут; ресурс 40 лет.
- Масса: сухая 6500 т (оборудование 2674.5 т), в рабочем снаряжении 7947.0 т, полная 8997.0 т (груз 1050.0 т + тара C20 0.0 т).
- Тяга 17.32 МН: 0.196 g гружёный (полная масса), 0.222 g в рабочем снаряжении; длительно допустимая доля тяги 1.0.
- Энергия: 4 × SR-3.3 = 13200 МВт; бытовая нагрузка 148.4 МВт от 400 МВт; площадка реактора 79.4 м².
- Охлаждение: 1000 м² радиаторов при 1050 K = 620.3 МВт; контуры 600 МВт; излучатель двигателя ⌀ 2.26 м.
- Цена: детали 35.91 млн, корпус 8.10 млн, прейскурант 58.43 млн Cr.
- Единичные точки отказа: T-5000-A4A1, T-5000-A6A2.

| Позиция | Изделие | Кол. | Производитель | Ремонт |
|---|---|---:|---|---|
| T-5000-A1A1 | Урал-43 | 4 | Orbital Freight Union «Ural-Kourou» | yard, LRU, 36 ч |
| T-5000-A2A1 | Sol Reactor Works SR-3.3 | 4 | Sol Reactor Works | yard, LRU, 48 ч |
| T-5000-A2A2 | Sol Reactor Works S-200 | 2 | Sol Reactor Works | station, LRU, 12 ч |
| T-5000-A3A1 | ЛМВ радиаторная секция (50 м2) | 20 | Лунно-Марсианские верфи | crew, LRU, 1.5 ч |
| T-5000-A3A2 | ЛМВ контур охлаждения (300 МВт) | 2 | Лунно-Марсианские верфи | station, LRU, 8 ч |
| T-5000-A4A1 | Дар аль-Мизан М-12 | 1 | Дар аль-Мизан | factory |
| T-5000-A4A2 | Бак заряда прыжка (1 т) | 120 | Agora Modular | station, LRU, 6 ч |
| T-5000-A4A3 | Бункер топливных таблеток D/He-3 (1 т) | 40 | Agora Modular | station, LRU, 6 ч |
| T-5000-A5A1 | Кластер RCS (4 сопла, поле) | 16 | Agora Modular | crew, LRU, 2 ч |
| T-5000-A6A1 | Мизан бортовой вычислитель МВ-1 | 2 | Дар аль-Мизан | station, LRU, 4 ч |
| T-5000-A6A2 | Синьчжи навигационный радар N-2 (2 000 км) | 1 | Синьчжи Электроникс | station, LRU, 6 ч |
| T-5000-A6A3 | Синьчжи связной комплекс C-1 (приём/передача, сеть маяков) | 2 | Синьчжи Электроникс | crew, LRU, 2 ч |
| T-5000-A6A4 | Транспондер Управления Маяков Т-1 | 2 | Фабрика ядер Управления Маяков «Нулевая точка» | crew, LRU, 0.5 ч |
| T-5000-A7A1 | Uhuru ЖО U-500 (биорегенерация) | 8 | Uhuru Bio-Systems | station, LRU, 10 ч |
| T-5000-A7A2 | Спасательный модуль-убежище (100 мест) | 51 | Бхарат Ассембли | station, LRU, 8 ч |
| T-5000-A7A3 | Медотсек (4 койки) | 6 | Консорциум «Евро-Хабитат» | station, LRU, 6 ч |
| T-5000-A8A1 | Секция переселенцев (500 коек) | 7 | Бхарат Ассембли | yard, LRU, 24 ч |
| T-5000-A8A2 | Блок анабиоза (100 капсул) | 15 | Uhuru Bio-Systems | yard, LRU, 24 ч |
| T-5000-A8A3 | Грузовой отсек (100 т, 300 м3) | 6 | Agora Modular | yard |
| T-5000-A9A1 | Стыковочный узел (стандарт хабов) | 4 | Agora Modular | station, LRU, 8 ч |

Проверки:
- OK `drives_fill_layout_slots` — 4 drives for NewtonianX (4 aft + 0 fore)
- OK `one_generator_per_group` — 4 propulsion reactors for 4 groups
- OK `generator_covers_group_demand` — 3300 MWe per group vs 3000 MW demand
- OK `equipment_fits_dry_mass` — hull structure 3826 t = 59% of dry 6500 t (possible 15%-90%)
- OK `hull_structure_share_typical` — hull structure 59% of dry (typical 30%-75%)
- OK `thrust_meets_class_acceleration` — 17.32 MN: 0.20 g loaded, 0.22 g in service (class burn 0.18 g)
- OK `thrust_near_class_target` — 17.3 MN vs class 18.0 MN
- OK `service_power_covers_hotel` — service 400 MWe vs hotel 148.4 MW (+15 %)
- OK `rcs_fills_newtonian_bank` — 16 RCS units on the 8-node Newtonian bank
- OK `rcs_powered_from_propulsion_spare` — RCS 640 MW installed, 192 MW simultaneous, from propulsion spare 1200 MW
- OK `propulsion_power_near_class_target` — 13200 MWe vs class 23000 MWe
- OK `coolant_loops_carry_service_heat` — loops 600 MW vs service heat 329.9 MW
- OK `radiators_reject_service_heat` — 1000 m2 at 1050 K = 620 MW vs 329.9 MW
- OK `jump_core_installed` — jump class needs a core
- OK `jump_core_rating_covers_gross` — core 12000 t vs gross 8997 t
- OK `jump_charge_capacity` — 120 t charge vs class 120.0 t
- OK `life_support_capacity` — 4000.0 places for 3560 awake persons (1500 in cold sleep)
- OK `life_support_endurance` — 60 days vs endurance 60 days
- OK `survival_seats_for_everyone_awake` — 5100.0 survival seats for 3560 awake persons (cold-sleep blocks are their own survival cells)
- OK `container_slots_match` — 0 C20 places vs 0
- OK `passenger_places` — 5000 places vs 5000 passengers
- OK `cargo_near_class_target` — 600 t vs class 600 t
- OK `suppliers_reachable_through_channels` — all suppliers inside open / licensed / restricted-civil channels
- WARN `component_prices_near_class_model` — main_drive 2.12/2.34 M, reactor 2.97/3.42 M, jump_drive 9.04/7.38 M, electronics 0.30/0.72 M, life_support 14.67/5.58 M, rcs_thermal 0.74/1.08 M
- OK `no_single_point_of_failure_in_critical_functions` — thrust / life / comms redundant
