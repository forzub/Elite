# Флоты и классы кораблей 3126

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

<!-- GENERATED:fleets BEGIN -->
### Грузоподъёмность Cobrett (единая модель)

6 мест под контейнер C20 (до 20 т полезной массы, 23.3 м³, тара 1.7 т): 140 м³, предел конструкции 120 т. Обычный смешанный груз упирается в объём (~0.4 т/м³): типичная загрузка 56 т; плотный груз (металл, боеприпасы) — до 120 т. Источник для рантайма: `src/game/catalog/ships/ShipCargoPassports.h`; трафик больше не держит свою грузоподъёмность (`PROPOSAL`).

### Классы

| Класс | Роль | Назначение | Сухая масса, т | Груз, т | м³ | C20 | Ускорение, g | Экипаж | Цена, Cr | Срок, лет | Прыжков на бак (пуст/пол/полн) | Статус |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Cobrett-I N1 | средний груз | fast flexible freight: high-value cargo, urgent parts, courier, small charters; expensive per tonne | 260 | 120 | 140 | 6 | 1.0 | 2 | 3 393 621 | 35 | 3.0/2.73/2.5 | `PROPOSAL` (модель) |
| фрейтер «Конвейер» | средний груз | secondary routes: containers, machinery, consumer goods, medical; 120 C20 places | 1 900 | 2 400 | 2 800 | 120 | 0.3 | 6 | 15 241 078 | 40 | 3.96/3.13/2.59 | `PROPOSAL` (модель) |
| прыжковый поезд | навал / контейнеры (поезда) | trunk routes: bulk and containers in modules; the tug can drop and pick up modules at depots, so the expensive jump core is not idle while modules load | 10 800 | 57 600 | 140 000 | 2880 | 0.12 | 12 | 73 942 470 | 45 | 10.49/4.77/3.09 | `PROPOSAL` (модель) |
| поезд-танкер | танкер | reactor fuel and jump charge to depots and importers; the same tug as the jump train, tank modules | 12 000 | 50 000 | 300 000 | 0 | 0.13 | 12 | 76 550 670 | 45 | 9.58/5.1/3.48 | `PROPOSAL` (модель) |
| OCS-4 «Ласточка» | челнок | in-system cargo ORBIT ONLY: orbital hubs, depots, moons without atmosphere; 0.4 g, no landing gear / heat shield (cannot lift off Earth); 450 t gross = 409 t net + 24 C20 tare | 380 | 409 | 2 600 | 24 | 0.4 | 3 | 1 973 160 | 40 | — | `PROPOSAL` (модель) |
| газовый танкер | танкер | He-3 / deuterium from gas-giant platforms to depots and ports | 900 | 3 000 | 19 000 | 0 | 0.18 | 4 | 4 186 161 | 40 | — | `PROPOSAL` (модель) |
| рудный буксир | буксир | pushes ore / ice / construction rafts from asteroid mines to refineries; tows equipment and beacons; slow and strong | 700 | 20 000 | 0 | 0 | 0.02 | 3 | 2 104 704 | 40 | — | `PROPOSAL` (модель) |
| сервисный тендер | сервис | repairs of mines, stations, beacons and ships in place; carries spare parts and a workshop | 500 | 150 | 600 | 6 | 0.4 | 8 | 1 890 714 | 35 | — | `PROPOSAL` (модель) |
| патрульный катер | патруль | unified coast guard: customs and inspections, law enforcement, SAR, beacon watch, traffic control backup | 420 | 20 | 60 | 0 | 1.5 | 9 | 13 919 040 | 30 | 3.21/3.18/3.15 | `PROPOSAL` (модель) |
| перехватчик | перехватчик | fast response inside a system: time-to-intercept of hostile ships, pirates, rogue fragments; short endurance | 140 | 2 | 6 | 0 | 4.0 | 2 | 9 460 800 | 25 | — | `PROPOSAL` (модель) |
| эскортный фрегат | эскорт | convoy protection on routes: point defence, sensors, endurance to stay with trains; matches train acceleration with a large margin | 2 200 | 80 | 250 | 0 | 1.4 | 40 | 93 052 800 | 35 | 4.05/4.01/3.98 | `PROPOSAL` (модель) |
| линейный ракетный корабль | военный | deterrence and war: long-range missiles, strong sensors, armour; the polity's main combat unit | 9 000 | 300 | 900 | 0 | 1.1 | 160 | 645 408 000 | 40 | 4.6/4.56/4.52 | `PROPOSAL` (модель) |
| разведчик / РЭБ-пикет | военный | sensors, early warning at jump points, electronic warfare, beacon-network relay in war | 600 | 10 | 40 | 0 | 2.0 | 14 | 28 382 400 | 30 | 3.3/3.29/3.28 | `PROPOSAL` (модель) |
| тендер флота | военный | endurance of a squadron away from bases: fuel, jump charge, munitions, repair | 5 000 | 6 000 | 9 000 | 240 | 0.35 | 60 | 59 292 000 | 40 | 5.18/4.02/3.31 | `PROPOSAL` (модель) |
| буксир ПАО | противоастероидная оборона | planetary / habitat impact defence: detect, intercept, tow, divert or break dangerous rocks and debris; dual use (heavy rescue, beacon towing) | 1 600 | 200 | 500 | 0 | 1.6 | 16 | 29 743 200 | 35 | — | `PROPOSAL` (модель) |
| surface_lifter | surface_shuttle | surface spaceport <-> low orbit: cargo in 12 C20 places or 12 passenger modules (P20); heat shield, landing legs, 2.5 g at lift-off | 620 | 240 | 280 | 12 | 2.5 | 3 | 5 868 080 | 30 | — | `PROPOSAL` (модель) |
| liner | passenger | scheduled interstellar passenger service: 900 seats (60 cabin / 240 business / 600 economy), mail and 90 t of priority cargo | 2 600 | 90 | 300 | 0 | 0.3 | 40 | 26 021 352 | 40 | 3.81/3.78/3.76 | `PROPOSAL` (модель) |
| colonist_transport | passenger | migration and evacuation: 5 000 berths (bunks, 30 % cold-sleep pods), slow and cheap per head; requisitioned in disasters | 6 500 | 600 | 2 000 | 0 | 0.18 | 60 | 42 218 316 | 45 | 4.5/4.42/4.35 | `PROPOSAL` (модель) |

Тяжёлый перевозчик — не монолит, а **прыжковый поезд**: тягач-«Локомотив» с ядром прыжка + до 12 модулей по 4 800 т. Тягач оставляет модули на погрузке в депо и забирает готовые, поэтому дорогое ядро не простаивает; поле прыжка строится по оболочке, и длинный поезд платит в основном за массу (дёшево на тонну).

### Флоты по политиям

| Полития | Гражданских | Безопасность | Военных | Новых/год | Потерь/год | Статус |
|---|---|---|---|---|---|---|
| Шахта Альфа | 2 449 | 142 | 84 | 117 | 8 | `PROPOSAL` (модель) |
| Свободная Карма | 2 028 | 167 | 99 | 90 | 9 | `PROPOSAL` (модель) |
| Новая Карма | 5 651 | 819 | 295 | 310 | 30 | `PROPOSAL` (модель) |
| Цветущая Гармония | 18 091 | 1 106 | 1 315 | 679 | 74 | `PROPOSAL` (модель) |
| Пояс Раздора / Вольные астероиды | 6 615 | 308 | 460 | 296 | 25 | `PROPOSAL` (модель) |
| Новая Европа | 1 903 | 223 | 126 | 82 | 9 | `PROPOSAL` (модель) |
| Свободные поселения | 2 185 | 175 | 97 | 118 | 10 | `PROPOSAL` (модель) |
| Зона «Филоморфоз» | 6 | 0 | 0 | 1 | 0 | `PROPOSAL` (модель) |
| Тяньхэ (Небесная Река) | 6 495 | 855 | 438 | 265 | 35 | `PROPOSAL` (модель) |
| Провидение | 33 | 3 | 2 | 2 | 0 | `PROPOSAL` (модель) |
| Третья Эллада | 6 | 2 | 4 | 1 | 0 | `PROPOSAL` (модель) |
| Вторая Эллада | 2 488 | 206 | 136 | 97 | 11 | `PROPOSAL` (модель) |
| Новая Индия | 8 333 | 711 | 353 | 400 | 38 | `PROPOSAL` (модель) |
| Небесная Твердыня | 1 835 | 162 | 129 | 97 | 9 | `PROPOSAL` (модель) |
| Новая Маланд | 4 110 | 358 | 200 | 205 | 19 | `PROPOSAL` (модель) |
| Сельта-Верде | 1 306 | 170 | 73 | 77 | 7 | `PROPOSAL` (модель) |
| Новая Эллада / Агора | 2 933 | 266 | 143 | 115 | 13 | `PROPOSAL` (модель) |
| Новая Мекка | 7 668 | 629 | 310 | 344 | 33 | `PROPOSAL` (модель) |
| Ново-Фиваида | 6 | 0 | 0 | 1 | 0 | `PROPOSAL` (модель) |
| Красный Форпост | 5 444 | 813 | 288 | 252 | 28 | `PROPOSAL` (модель) |
| Солнечная Администрация (Sol Authority) | 112 644 | 8 007 | 694 | 3 886 | 506 | `PROPOSAL` (модель) |
| Новый Шанхай | 15 503 | 1 120 | 532 | 558 | 56 | `PROPOSAL` (модель) |
| Цитадель | 6 457 | 441 | 292 | 299 | 26 | `PROPOSAL` (модель) |

Флот выводится из потоков: судов = т/год ÷ (полезная загрузка × гружёных рейсов в год), + резерв + ремонт; новых в год = списание (флот / срок) + потери + рост. Прыжковые двигатели для новых кораблей: 3184 в год; изготовители ядер: Новая Мекка (own), Новый Шанхай (state_secret), Цитадель (own).
<!-- GENERATED:fleets END -->

## Как выглядят флоты

<!-- NARRATIVE:fleet_story -->
_TODO: окраска, традиции экипажей, где стоят поезда, чем пахнет челнок; классы и числа — из таблиц выше._
