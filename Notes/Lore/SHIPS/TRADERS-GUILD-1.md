# TRADERS-GUILD-1 — TRADER'S-GUILD-1

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

## Общие данные

<!-- GENERATED:summary BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| ID / имена | TRADERS-GUILD-1, TRADER'S-GUILD-1, Первый гипердоговор (legacy name) | PROPOSAL · PUBLIC |
| Волна | III | PROPOSAL · PUBLIC |
| Спонсор | Lalande colonies (New Hellas + New Mecca): first inter-colony sublight courier | PROPOSAL · PUBLIC |
| Старт | 2385, Lalande 21185 | PROPOSAL · PUBLIC |
| Цель | Tau Ceti | PROPOSAL · PUBLIC |
| Прибытие | 2512 | PROPOSAL · PUBLIC |
| Длительность полёта, лет | 127 | PROPOSAL · PUBLIC |
| Колония | [Новый Шанхай](../COLONIES/tau_ceti_cognium.md) | CANON · PUBLIC |
| Источники | generation_ships.json TRADER'S-GUILD-1; generation ships.md (A) | — |
<!-- GENERATED:summary END -->

## Причины полёта

<!-- GENERATED:motives BEGIN -->
Машинная разметка досье (у этих полей нет собственного статуса; статус унаследован от миссии: `PROPOSAL · PUBLIC`).

| Мотив | Вес |
|---|---|
| prestige | 0.30 |
| scientific_vanity | 0.30 |
| frontier_myth | 0.20 |
| name_in_history | 0.20 |

Культуры набора: greek 0.50, gulf_arab 0.50.

Чёрная комедия миссии: Везли «самое ценное в Лаланде»: семена, патентные архивы и рецепт кофе по-гречески, который в Новом Шанхае объявили турецким.
<!-- GENERATED:motives END -->

### Почему полетели (рассказ)

<!-- NARRATIVE:causes -->
_TODO: кто и зачем снарядил корабль, общественный фон эпохи старта, что обещали людям; опираться на мотивы выше и на досье из «Источники»._

## Корабль

<!-- GENERATED:ship BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Заявленная масса, Мт (полная / сухая) | 5 / 3 | PROPOSAL · PUBLIC (досье) |
| Эра двигателя | III | PROPOSAL · PUBLIC (модель) |
| Крейсерская скорость, c | 0.161 | PROPOSAL · PUBLIC (модель) |
| Расчётная масса, Мт (полная / сухая) | 29.1 / 1.6 | PROPOSAL · PUBLIC (модель) |
| Отношение масс | 18.0 | PROPOSAL · PUBLIC (модель) |
| Δv, c | 0.173 | PROPOSAL · PUBLIC (модель) |
| Топливо He-3, Мт | 9.0 | PROPOSAL · PUBLIC (модель) |
| Ускорение разгона, g | 0.0130 | PROPOSAL · PUBLIC (модель) |
| Мощность струи, ПВт | 8.7 | PROPOSAL · PUBLIC (модель) |
| Мощность на крейсерском участке, ГВт | 0.30 | PROPOSAL · PUBLIC (модель) |
| Радиаторы разгона / крейсера, км² | 29.3 / 0.36 | PROPOSAL · PUBLIC (модель) |
| Вердикт physics.py | fix: полная масса 29 Мт против заявленных 5 Мт | PROPOSAL · PUBLIC (модель) |
| Доля крио | 0.80 | PROPOSAL · PUBLIC |
| Размеры, двигатель, груз | нет полей в машинном каноне — см. досье | — |
<!-- GENERATED:ship END -->

### Как выглядит корабль

<!-- NARRATIVE:ship_description -->
_TODO: облик, размеры, модули, двигатель, груз и запасы по досье; не противоречить расчёту physics.py (если досье расходится — это помечено в вердикте)._

## Население

<!-- GENERATED:population BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Старт: людей | 800 | PROPOSAL · PUBLIC |
| Бодрствующие / крио | 160 / 640 | PROPOSAL · PUBLIC |
| Вахта (минимум функций по модели) | 60 (66) | PROPOSAL · PUBLIC |
| Лет вахты / безопасных циклов | 20 / 4 | PROPOSAL · PUBLIC (досье) |
| Политика рождений | forbidden | PROPOSAL · PUBLIC (досье) |
| Наследственная вахта / хранители | нет / 0 | PROPOSAL · PUBLIC (модель) |
| Сон владельцев, лет | 0 | PROPOSAL · PUBLIC (модель) |
| Неучтённые потери | 16 | PROPOSAL · PUBLIC (модель) |

Когорты (доля / доля бодрствующих):

| Когорта | Доля | Бодрствует | Культура |
|---|---|---|---|
| engineers | 0.50 | 0.10 | как у миссии |
| scientists | 0.50 | 0.05 | как у миссии |

Флаги модели: бодрствующая вахта 60 меньше минимума функций 66
<!-- GENERATED:population END -->

## Хроника полёта

<!-- GENERATED:flight BEGIN -->
Значимые события: якоря канона, смена режима, потери от 1 % людей; 4 мелких заполняющих событий модели (seed-зависимых) опущены — полный список в `out/lab_report.md` §2.

| Год | Событие | Население | Awake | Cryo | Власть/режим | Техсостояние | Статус |
|---|---|---|---|---|---|---|---|
| 2385 | launch: старт: 800 чел., крио 80 % | 800 | 160 | 640 | технократия | 1.00 | PROPOSAL · PUBLIC |
| 2512 | arrival: Тау Кита; капитан Лира Вэнс — легенда | 760 | 760 | 0 | технократия | 0.89 | PROPOSAL · PUBLIC |
<!-- GENERATED:flight END -->

### Жизнь на борту (рассказ)

<!-- NARRATIVE:flight_story -->
_TODO: быт, конфликты и смена власти между событиями таблицы; числа брать только из таблицы._

## Итог

<!-- GENERATED:outcome BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| Итог | arrived | PROPOSAL · PUBLIC |
| Корабль сейчас | museum | PROPOSAL · PUBLIC |
| Примечание | музей в Новом Шанхае | PROPOSAL · PUBLIC |
| Можно найти в игре | да | PROPOSAL · PUBLIC |
<!-- GENERATED:outcome END -->

## Прибытие и высадка

<!-- NARRATIVE:arrival -->
_TODO: что увидели у цели, первая высадка/стройка, кто взял власть, что стало с кораблём; для не долетевших — «нет»/последний контакт._
