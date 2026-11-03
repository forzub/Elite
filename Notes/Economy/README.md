# Notes/Economy — экономика, логистика, политика, флоты

Постоянный раздел проектных решений политико-экономического слоя мира (2026-11-03). Исследовательские черновики остались в `D:/tmp/` (`WORLD_ECONOMY_ARCHITECTURE_REVIEW.md`, `REFERENCE_GAME_ECONOMIES.md`, `elite_research_games_aid.md`, `WORLD_DATA_GRAPH.md`, `PLAYER_ECONOMY_BALANCE.md`); здесь — очищенные рабочие выводы. Статус всего, что не сказано автором, — **PROPOSAL**; численность населения 3126 — **AUTHOR_TARGET**.

## Одна истина — три слоя

| Слой | Где | Что |
|---|---|---|
| Проектные решения (как устроено) | `Notes/Economy/*.md` (этот раздел) | архитектура, правила, формулы, контракты данных |
| Машинные данные (что есть) | `tools/colonization_lab/canon/*.json` → `tools/colonization_lab/out/world_3126/*.json` | вход и выход модели; источник истины для чисел |
| Справочник мира (читаемый вид) | `Notes/Lore/ECONOMY/`, `Notes/Lore/COLONIES/`, `Notes/Lore/SETTLEMENTS/`, `Notes/Lore/POLITICS/EARTH_3126.md` | таблицы генерирует `lore_md.py`; рассказ вне блоков пишут руками |

Канонический дизайн трафика и монетизированной цивилизации — `WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md` (корень репозитория, 2026-09-17). Этот раздел его детализирует и не заменяет.

## Документы

- [WORLD_ECONOMY_ARCHITECTURE.md](WORLD_ECONOMY_ARCHITECTURE.md) — запасы / потоки / решения, уровни симуляции, минимальные первичные данные, конвейер модели, порядок реализации, сквозной пример «Гармония».
- [WORLD_DATA_GRAPH.md](WORLD_DATA_GRAPH.md) — сущности и связи, что персистентно, что статистика.
- [POLITICAL_ECONOMY.md](POLITICAL_ECONOMY.md) — мотивы акторов, SupportProgram, каналы торговли в войне, санкции, военная экономика, бюджет безопасности, доктрины.
- [LOGISTICS_AND_NAVIGATION.md](LOGISTICS_AND_NAVIGATION.md) — грузовая модель (C20, Cobrett 120 т), правило прыжка, внутрисистемная тяга, стоимость доставки, флот из потока, маяки, транспондеры, ошибка прыжка, коридор и траектория.
- [FLEETS_AND_SHIPYARDS.md](FLEETS_AND_SHIPYARDS.md) — классы кораблей, военные роли, тактика, реестр кораблей и агрегатов, замена флота, верфи.
- [PLAYER_ECONOMY.md](PLAYER_ECONOMY.md) — финансовое давление на игрока, стоянка и собственность, астероидная база игрока, внимание государства.
- [BANKING.md](BANKING.md) — одна валюта, счета, кредиты, кредитный риск, залог корабля, дефолт.
- [INDUSTRY_AND_RESCUE.md](INDUSTRY_AND_RESCUE.md) — блоки-союзы, школы кораблестроения, кто строит корабли / оборудование / станционные модули, каналы торговли, однотипность флотов союзников, спасательные службы.
- [WORLD_SITES_AND_INFORMATION.md](WORLD_SITES_AND_INFORMATION.md) — пространственный слой: хабы и поселения, глубокий космос с мировыми линиями, фронтиры и «Жопа мира», общество и администраторы, право, маяки как сеть связи, WorldEvent → KnowledgeRecord, пассажиры, возраст флота, пираты, кладбища, карты.

## Команды

```bash
cd tools/colonization_lab
python calibrate_population.py --check   # население 3126 в авторских диапазонах
python lab.py                            # демография (эпохи), история, рынок lab -> out/
python logistics.py                      # корабли, прыжок, стоимость рейса -> out/logistics.*
python world_economy.py                  # баланс мира 3126 -> out/world_3126/
python world_sites.py                    # хабы, фронтиры, право, связь, пассажиры -> out/world_3126/ + копии для рантайма
python industry.py                       # промышленность и спасательные службы -> out/world_3126/
python lore_md.py --write                # справочник Notes/Lore
python lore_html.py                      # HTML-зеркало Notes/html (после любого изменения Notes/Lore)
python world_economy.py --check && python world_sites.py --check && python industry.py --check && python logistics.py --check && python lore_md.py --check && python lore_html.py --check
```
