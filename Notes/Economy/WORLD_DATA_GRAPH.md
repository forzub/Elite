# Граф данных мира — сущности и связи

Статус: PROPOSAL (2026-11-03). Принцип: **персистентная идентичность** только у того, что может стать причиной цены, маршрута, войны или квеста; остальное — статистика. `[P]` — сущность с id, `[S]` — агрегат.

## Физика и сеть

```text
StarSystem [P]   id, позиция, тела, юрисдикция
Body [P]         система, тип, орбита (эфемериды), гравитация, атмосфера
Deposit [P]      тело, ресурс, запас (индекс каталога), качество, стоимость добычи
Beacon [P]       id (BCN-<sys>-<kind>), система, вид {bridge, trade_hub, station, relay, military, remote_science},
                 оператор, движение (естественная орбита / станция / буксируется), передаёт {position, velocity,
                 precise_time, network_id, health}, покрытие (св. сек), треков транспондеров, персонал, охрана,
                 содержание, состояние, история перемещений (буксировка регистрируется сетью)
JumpEdge [S]     (Beacon, Beacon), длина, правило эпохи, пошлина, риск; флаги unbeaconed / expedition
Corridor [S]     статистический путь между экономическими узлами: тонны, стоимость, класс trunk/secondary/feeder,
                 опорные маяки, кто охраняет
Trajectory [P]   конкретный рассчитанный путь корабля по текущим эфемеридам: отправление → перехват →
                 прыжковое плечо → предсказанное положение назначения (не хранить как линию в пространстве)
```

## Общество и акторы

```text
Settlement [P]   тело / хабитат, ярус {surface, underground, burrowed, asteroid, cylinder, ring, station}, население,
                 роль {capital, city, industrial, mining, port, military, research}
Polity [P]       территории, режим, юрисдикция, казна [S], доктрина безопасности, бюджет обороны [S]
Actor [P]        Polity / Faction / Corporation / Bank / MilitaryCommand / LocalGovernment / PlayerGroup / Player;
                 мотивы {security, trade_dependence, resource_dependence, alliance_obligation, prestige_ideology,
                 domestic_legitimacy, elite_interest, industrial_interest}, обязательства, внимание к другим
Treaty [P]       стороны, тип (союз, транзит, критический контракт), срок, условия
Jurisdiction [P] пошлины, легальность по категориям, доля инспекций, качество надзора
```

## Экономика

```text
CommodityClass [P]  15 категорий (canon/commodities.json): стоимость, плотность, опасность, легальность, рецепт
Asset [P|S]         тип (рудник, ледовый промысел, металлургия, химия, стройматериалы, верфь хабитатов, машиностроение,
                    электроника, агрокомплекс, биотех, электростанция, газовая платформа, завод заряда прыжка,
                    топливное депо, склад, торговый узел, орбитальный порт, ремонтная / гражданская / военная верфь,
                    научная станция, военная / патрульная база); у агрегата — count / output / workers / power;
                    у именованного — id, владелец, место, мощность, износ, персонал
Mine [P|S]          ресурс, запас, выпуск, качество, мощность, работники, автоматизация, оборудование (т), износ/год,
                    запчасти, топливо, склад, транспорт, охрана, выработанный объём (м³), места выработки, модули базы
Market [P]          узел, юрисдикция, запасы и цены по категориям [S]
PartClass [P]       реактор, главный двигатель, прыжковый двигатель, RCS, радиатор, насосы, радар, MotherComputer,
                    крупное оружие, жизнеобеспечение (отдельный рынок обслуживания)
```

## Потоки

```text
Shipment [P]       груз (категория или Container), масса, отправитель, получатель, перевозчик (ShipInstance или пул класса),
                   маршрут, канал {official, sanctioned, re_export, smuggling, covert_state}, статус
                   {planned, loading, in_transit, delayed, intercepted, lost, delivered}, ETA, причина
Container [P]      CAX-xxxx, класс C20 (≤ 20 т, 23.3 м³, тара 1.7 т), масса, класс опасности, легальный статус,
                   пломба {sealed, broken(time, by)}, заявленное и фактическое содержимое
Contract [P]       заказчик, исполнитель, груз / услуга, срок, оплата, штрафы, статус, источник (дефицит / программа)
SupportProgram [P] donor, recipient, motive, budget, goods, conditions, route, delivery_capacity, loss_rate,
                   статусы {promised/pledged, allocated, shipped, arrived, absorbed}
TradeChannel [S]   пара рынков × канал: стоимость, задержка, риск, кто «шлюз»
```

## Флот

```text
ShipClass [P]            физика + экономика (canon/ship_economics.json)
ShipInstance [P]         serial, класс, builder, build_year, owner, registration, condition, status, location
                         lifecycle {built, sold, leased, damaged, repaired, captured, lost, scrapped, destroyed}
SerializedComponent [P]  serial, модель, производитель, дата, установлен в, наработка/износ, законность
                         {clean, reported_stolen, salvage_unregistered, reflagged}, последняя проверка
FleetCensus [S]          по классу и владельцу: active, reserve, repair, new/year, lost/year, retired/year
Loan [P]                 счёт, сумма, ставка, залог (ShipInstance / база), график, просрочка, статус
```

## Риск и события

```text
RiskState [S]   по ребру и системе: пиратство, война, блокада, инспекции, история потерь → страховые ставки
Conflict [P]    стороны, причины (мотивы), театр, фаза, мобилизация
Event [P]       тип, время, место, причины → Event/Decision, следствия → Shipment/Market/Asset
```

## Связи с runtime

`ShipInstanceId` (`src/game/identity`), `ShipInstanceRegistry`, `ShipOwnershipRegistry` (`src/game/server`) — пока в памяти; нужна персистентность и поля выше. `CargoManifest` трафика дока — частный случай Shipment. Грузовая модель кораблей — `src/game/catalog/ships/ShipCargoPassports.h` (`ShipStorageCaps.cargoMass` в тоннах, `containerSlots`, `containerPayloadTonnes`).
