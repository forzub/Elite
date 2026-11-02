# FREE-VENTURE-1 — Фри-Венчур-1

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

## Общие данные

<!-- GENERATED:summary BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| ID / имена | FREE-VENTURE-1, Фри-Венчур-1, Пояс Раздора | PROPOSAL · PUBLIC |
| Волна | III | PROPOSAL · PUBLIC |
| Спонсор | First anarcho-capitalist ark (small, crowdfunded) | PROPOSAL · PUBLIC |
| Старт | 2335, Sol | PROPOSAL · PUBLIC |
| Цель | Epsilon Eridani | PROPOSAL · PUBLIC |
| Прибытие | 2405 | PROPOSAL · PUBLIC |
| Длительность полёта, лет | 70 | PROPOSAL · PUBLIC |
| Колония | [Пояс Раздора / Вольные астероиды](../COLONIES/eps_eri_strife.md) | PROPOSAL · PUBLIC |
| Источники | generation_ships.json FREE-VENTURE-1; generation ships.md (A); 29. HOMEWARD-1.md (обломки FREE-VENTURE-1) | — |
<!-- GENERATED:summary END -->

## Причины полёта

<!-- GENERATED:motives BEGIN -->
Машинная разметка досье (у этих полей нет собственного статуса; статус унаследован от миссии: `PROPOSAL · PUBLIC`).

| Мотив | Вес |
|---|---|
| ideology | 0.40 |
| elite_capital_flight | 0.20 |
| frontier_myth | 0.20 |
| stupid | 0.20 |

Культуры набора: crypto_libertarian 0.80, american_frontier 0.20.

Чёрная комедия миссии: Модули крепились на смарт-контрактах; первый же спор о воде был решён отстыковкой.
<!-- GENERATED:motives END -->

### Почему полетели (рассказ)

<!-- NARRATIVE:causes -->
_TODO: кто и зачем снарядил корабль, общественный фон эпохи старта, что обещали людям; опираться на мотивы выше и на досье из «Источники»._

## Корабль

<!-- GENERATED:ship BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Заявленная масса, Мт (полная / сухая) | 9 / 6 | PROPOSAL · PUBLIC (досье) |
| Эра двигателя | III | PROPOSAL · PUBLIC (модель) |
| Крейсерская скорость, c | 0.162 | PROPOSAL · PUBLIC (модель) |
| Расчётная масса, Мт (полная / сухая) | 39.7 / 2.2 | PROPOSAL · PUBLIC (модель) |
| Отношение масс | 18.1 | PROPOSAL · PUBLIC (модель) |
| Δv, c | 0.174 | PROPOSAL · PUBLIC (модель) |
| Топливо He-3, Мт | 12.3 | PROPOSAL · PUBLIC (модель) |
| Ускорение разгона, g | 0.0131 | PROPOSAL · PUBLIC (модель) |
| Мощность струи, ПВт | 11.9 | PROPOSAL · PUBLIC (модель) |
| Мощность на крейсерском участке, ГВт | 0.32 | PROPOSAL · PUBLIC (модель) |
| Радиаторы разгона / крейсера, км² | 40.0 / 0.38 | PROPOSAL · PUBLIC (модель) |
| Вердикт physics.py | fix: полная масса 40 Мт против заявленных 9 Мт | PROPOSAL · PUBLIC (модель) |
| Доля крио | 0.50 | PROPOSAL · PUBLIC |
| Размеры, двигатель, груз | нет полей в машинном каноне — см. досье | — |
<!-- GENERATED:ship END -->

### Как выглядит корабль

<!-- NARRATIVE:ship_description -->
_TODO: облик, размеры, модули, двигатель, груз и запасы по досье; не противоречить расчёту physics.py (если досье расходится — это помечено в вердикте)._

## Население

<!-- GENERATED:population BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Старт: людей | 2 800 | PROPOSAL · PUBLIC |
| Бодрствующие / крио | 1 400 / 1 400 | PROPOSAL · PUBLIC |
| Вахта (минимум функций по модели) | 1 400 (135) | PROPOSAL · PUBLIC |
| Лет вахты / безопасных циклов | 20 / 3 | PROPOSAL · PUBLIC (досье) |
| Политика рождений | contract_licence | PROPOSAL · PUBLIC (досье) |
| Наследственная вахта / хранители | да / 22 | PROPOSAL · PUBLIC (модель) |
| Сон владельцев, лет | 0 | PROPOSAL · PUBLIC (модель) |
| Неучтённые потери | 285 | PROPOSAL · PUBLIC (модель) |

Когорты (доля / доля бодрствующих):

| Когорта | Доля | Бодрствует | Культура |
|---|---|---|---|
| engineers | 0.40 | 0.60 | как у миссии |
| patricians | 0.30 | 0.30 | как у миссии |
| workers | 0.30 | 0.60 | как у миссии |
<!-- GENERATED:population END -->

## Хроника полёта

<!-- GENERATED:flight BEGIN -->
Значимые события: якоря канона, смена режима, потери от 1 % людей; 1 мелких заполняющих событий модели (seed-зависимых) опущены — полный список в `out/lab_report.md` §2.

| Год | Событие | Население | Awake | Cryo | Власть/режим | Техсостояние | Статус |
|---|---|---|---|---|---|---|---|
| 2335 | launch: старт: 2800 чел., крио 50 % | 2 800 | 1 400 | 1 400 | олигархия | 1.00 | PROPOSAL · PUBLIC |
| 2405 | arrival: Эпсилон Эридана, где уже 190 лет правит Гармония; поселение в поясе | 2 500 | 2 500 | 0 | технократия | 0.96 | PROPOSAL · PUBLIC |
| 2440 | clan_fragmentation: Гражданская война модулей: «Пояс Раздора» | 2 500 | 2 500 | 0 | технократия | 0.96 | PROPOSAL · PUBLIC |
<!-- GENERATED:flight END -->

### Жизнь на борту (рассказ)

<!-- NARRATIVE:flight_story -->
_TODO: быт, конфликты и смена власти между событиями таблицы; числа брать только из таблицы._

## Итог

<!-- GENERATED:outcome BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| Итог | arrived_fragmented | PROPOSAL · PUBLIC |
| Корабль сейчас | scattered_modules | PROPOSAL · PUBLIC |
| Примечание | модули рассеяны по поясу Эпсилон Эридана | PROPOSAL · PUBLIC |
| Можно найти в игре | да | PROPOSAL · PUBLIC |
<!-- GENERATED:outcome END -->

## Прибытие и высадка

<!-- NARRATIVE:arrival -->
_TODO: что увидели у цели, первая высадка/стройка, кто взял власть, что стало с кораблём; для не долетевших — «нет»/последний контакт._
