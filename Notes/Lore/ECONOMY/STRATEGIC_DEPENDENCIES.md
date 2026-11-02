# Стратегические зависимости

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

<!-- GENERATED:dependencies BEGIN -->
Нет своего источника (импорт по необходимости):

| Покупатель | Товар | Поставщик | Класс | Узкие места | Статус |
|---|---|---|---|---|---|
| [Свободная Карма](../COLONIES/delta_pav_freekarma.md) | He-3 | Тяньхэ (Небесная Река) | medium | — | PROPOSAL · PUBLIC (модель) |
| [Свободная Карма](../COLONIES/delta_pav_freekarma.md) | He-3 | Сельта-Верде | medium | Gliese 357, Proxima Centauri | PROPOSAL · PUBLIC (модель) |
| [Свободная Карма](../COLONIES/delta_pav_freekarma.md) | He-3 | Пояс Раздора / Вольные астероиды | medium | — | PROPOSAL · PUBLIC (модель) |
| [Свободная Карма](../COLONIES/delta_pav_freekarma.md) | He-3 | Вторая Эллада | medium | — | PROPOSAL · PUBLIC (модель) |
| [Свободная Карма](../COLONIES/delta_pav_freekarma.md) | He-3 | Новая Индия | medium | — | PROPOSAL · PUBLIC (модель) |
| [Новая Карма](../COLONIES/delta_pav_karma.md) | He-3 | Пояс Раздора / Вольные астероиды | major | — | PROPOSAL · PUBLIC (модель) |
| [Новая Карма](../COLONIES/delta_pav_karma.md) | He-3 | Сельта-Верде | major | Gliese 357, Proxima Centauri | PROPOSAL · PUBLIC (модель) |
| [Новая Карма](../COLONIES/delta_pav_karma.md) | He-3 | Вторая Эллада | major | — | PROPOSAL · PUBLIC (модель) |
| [Новая Карма](../COLONIES/delta_pav_karma.md) | He-3 | Новая Индия | major | — | PROPOSAL · PUBLIC (модель) |
| [Новая Карма](../COLONIES/delta_pav_karma.md) | He-3 | Тяньхэ (Небесная Река) | medium | — | PROPOSAL · PUBLIC (модель) |
| [Новый Шанхай](../COLONIES/tau_ceti_cognium.md) | He-3 | Пояс Раздора / Вольные астероиды | minor | — | PROPOSAL · PUBLIC (модель) |
| [Новый Шанхай](../COLONIES/tau_ceti_cognium.md) | He-3 | Цветущая Гармония | minor | — | PROPOSAL · PUBLIC (модель) |
| [Новый Шанхай](../COLONIES/tau_ceti_cognium.md) | He-3 | Гелиевое поле Эпсилон Индейца | minor | — | PROPOSAL · PUBLIC (модель) |
| [Цитадель](../COLONIES/wolf359_citadel.md) | He-3 | Цветущая Гармония | major | — | PROPOSAL · PUBLIC (модель) |
| [Цитадель](../COLONIES/wolf359_citadel.md) | He-3 | Пояс Раздора / Вольные астероиды | major | — | PROPOSAL · PUBLIC (модель) |
| [Цитадель](../COLONIES/wolf359_citadel.md) | He-3 | Сельта-Верде | major | Gliese 357 | PROPOSAL · PUBLIC (модель) |

Узкие места сети (системы-перевалки на маршрутах):

| Система | Коридоров | Стоимость через узел | Контролёр | Товары | Статус |
|---|---|---|---|---|---|
| Gliese 357 | 26 | 345.8 | ?, Новый Шанхай, Цитадель | компоненты, ядра прыжка, электроника, He-3, корпуса, изотопы, боеприпасы, точная механика | PROPOSAL · PUBLIC (модель) |
| Proxima Centauri | 14 | 169.7 | ?, Новый Шанхай | компоненты, ядра прыжка, электроника, He-3, корпуса, боеприпасы, точная механика | PROPOSAL · PUBLIC (модель) |
| HD 20794 (82 G. Eridani) | 17 | 120.5 | Новый Шанхай | биотех, компоненты, ядра прыжка, электроника, He-3, корпуса, металлы, боеприпасы, точная механика | PROPOSAL · PUBLIC (модель) |
| HD 10360 | 9 | 86.8 | Вторая Эллада, Новый Шанхай | биотех, ядра прыжка, электроника, корпуса, боеприпасы, точная механика | PROPOSAL · PUBLIC (модель) |
| Wolf 359 | 5 | 29.6 | Новый Шанхай, Цитадель | компоненты, электроника, He-3, изотопы | PROPOSAL · PUBLIC (модель) |
| HD 85512 | 4 | 7.6 | Новый Шанхай | компоненты, электроника, He-3, корпуса | PROPOSAL · PUBLIC (модель) |
| Epsilon Indi | 1 | 1.7 | Новый Шанхай | точная механика | PROPOSAL · PUBLIC (модель) |
| Gliese 674 | 1 | 1.2 | Вторая Эллада | электроника | PROPOSAL · PUBLIC (модель) |
| Tau Ceti | 1 | 0.4 | Новый Шанхай | компоненты | PROPOSAL · PUBLIC (модель) |

Контроль сети маяков (сколько кратчайших маршрутов между акторами идёт через узлы оператора):

| Оператор | Маяки (системы) | Маршрутов | Статус |
|---|---|---|---|
| Новый Шанхай | Tau Ceti, Gliese 667 (A+B+C), Gliese 876, HD 20794 (82 G. Eridani), TRAPPIST-1, Gliese 357, Epsilon Indi, HD 104067, Gliese 674, HD 85512 | 26 | PROPOSAL · PUBLIC (модель) |
| Вторая Эллада | HD 10360 | 6 | PROPOSAL · PUBLIC (модель) |
| Цитадель | Wolf 359 | 3 | PROPOSAL · PUBLIC (модель) |
| Шахта Альфа | Gliese 892, 61 Cygni, HD 114783, 83 Leonis | 2 | PROPOSAL · PUBLIC (модель) |
| Новая Эллада / Агора | Lalande 21185 | 1 | PROPOSAL · PUBLIC (модель) |
| Свободная Карма | Delta Pavonis | 0 | PROPOSAL · PUBLIC (модель) |
| Цветущая Гармония | Epsilon Eridani | 0 | PROPOSAL · PUBLIC (модель) |
| Новая Европа | Gliese 581 | 0 | PROPOSAL · PUBLIC (модель) |
| Тяньхэ (Небесная Река) | Gliese 832 | 0 | PROPOSAL · PUBLIC (модель) |
| Новая Маланд | HD 40307 | 0 | PROPOSAL · PUBLIC (модель) |
| Сельта-Верде | HD 69830 | 0 | PROPOSAL · PUBLIC (модель) |
| Солнечная Администрация (Sol Authority) | Sol | 0 | PROPOSAL · PUBLIC (модель) |

Единственный межзвёздный поставщик (класс major/medium):

| Покупатель | Товар | Поставщик | Класс | Статус |
|---|---|---|---|---|
| Новая Карма | ядра прыжка | Новая Мекка | major | PROPOSAL · PUBLIC (модель) |
| Цветущая Гармония | точная механика | Новая Карма | medium | PROPOSAL · PUBLIC (модель) |
| Пояс Раздора / Вольные астероиды | точная механика | Новая Карма | major | PROPOSAL · PUBLIC (модель) |
| Тяньхэ (Небесная Река) | ядра прыжка | Цитадель | medium | PROPOSAL · PUBLIC (модель) |
| Вторая Эллада | ядра прыжка | Цитадель | medium | PROPOSAL · PUBLIC (модель) |
| Вторая Эллада | пища | Цветущая Гармония | major | PROPOSAL · PUBLIC (модель) |
| Новая Индия | ядра прыжка | Цитадель | medium | PROPOSAL · PUBLIC (модель) |
| Новая Маланд | ядра прыжка | Цитадель | medium | PROPOSAL · PUBLIC (модель) |
| Новая Маланд | точная механика | Новая Карма | major | PROPOSAL · PUBLIC (модель) |
| Сельта-Верде | точная механика | Новая Карма | medium | PROPOSAL · PUBLIC (модель) |
| Новая Эллада / Агора | вода/летучие | Красный Форпост | medium | PROPOSAL · PUBLIC (модель) |
| Новая Мекка | точная механика | Новая Карма | major | PROPOSAL · PUBLIC (модель) |
| Красный Форпост | корпуса | Цветущая Гармония | medium | PROPOSAL · PUBLIC (модель) |
| Новый Шанхай | точная механика | Новая Карма | medium | PROPOSAL · PUBLIC (модель) |
| Цитадель | корпуса | Цветущая Гармония | medium | PROPOSAL · PUBLIC (модель) |
<!-- GENERATED:dependencies END -->

## Рычаги давления

<!-- NARRATIVE:leverage -->
_TODO: кто кого может задушить и почему этого ещё не сделал._
