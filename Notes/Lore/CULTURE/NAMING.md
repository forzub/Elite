# Имена 3126: эндонимы, экзонимы, сленг

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

<!-- GENERATED:naming BEGIN -->
Источник: `tools/colonization_lab/canon/naming.json` → `naming.py` (резолвер, зеркала, проверки) → `src/assets/data/naming/names_3126.json` (runtime: stable ID → имя). Поля имён в `world.json`, `political_order.json`, `settlements.json` и легаси `actors.json` — зеркала, их пишет `naming.py --write`.

**Правило.** Русский — язык разработки и локализации, не язык галактики. Эндоним идёт от тех, кто реально назвал объект; русская форма — экзоним. Каталожные имена звёзд (proper / Bayer / Flamsteed / HD / GJ / HIP) не уничтожаются: имя эпохи колоний ложится поверх.

Статус: архитектура и решения автора — `CANON`; этимологии, цепочки именования и геральдика — `PROPOSAL`, если не сказано иначе.

### Основные акторы

| ID | Основатели | Языки (полития / столица) | Звезда (каталог) | Местное имя системы | Эндоним политии | Столица (эндоним) | RU (полития / столица) | Вердикт | Что было не так |
|---|---|---|---|---|---|---|---|---|---|
| `tau_ceti_cognium` | MAYFLOWER-2, GENESIS-1, TRADERS-GUILD-1 | en / zh-Hans | Tau Ceti | 天仓 | Cognium | 新上海 | Когниум / Синь-Шанхай | KEEP / NORMALIZE | город назвали те, кто ушёл и от Пекина, и от Вашингтона; столица не равна политии |
| `proxima_outpost` | MERU-1 | sa / hi | Proxima Centauri | Ulugʻbek | मेरु | लाल कोट | Меру / Лал-Кот | RENAME / RENAME | имя корабля: Меру — мировая гора индийской и буддийской космологии, общая для индийцев и Средней Азии; «Красная крепость»: так звали первую крепость Дели; колония выросла вокруг корабля-крепости — смысл прежнего имени сохранён, связь с Россией снята (автор 2026-11-12) |
| `gj832_tianhe` | TIANHE-1 | zh-Hans / zh-Hans | Gliese 832 | 鹊桥 | 天河 | 天河 | Тяньхэ / Тяньхэ | KEEP / KEEP | — |
| `gj832_gaia_zone` | GREEN-ARK-1 | en / en | Gliese 832 | 鹊桥 | Gaia Prime remnant | Philomorphosis Zone | остатки Гайи-Прайм / Зона «Филоморфоз» | RENAME / KEEP | прежнее имя было псевдонемецким гибридом (werden + Salbe) — у англоязычных основателей нет причины так называться |
| `lalande_hellas` | HERACLES-1 | el / el | Lalande 21185 | Ηρακλής | Νέα Ελλάδα | Αγορά | Новая Эллада / Агора | RENAME / NORMALIZE | прежнее имя смешивало греческое «агора» с латинским primus; грек так государство не назовёт; кольца первой стоянки стали рынком задолго до города |
| `lalande_mecca` | VAKA-MOANA-1 | mi / mi | Lalande 21185 | Ηρακλής | Te Kāpehu | Tauranga | Капеху / Тауранга | RENAME / RENAME | kāpehu whetū — звёздный компас (современная маорийская форма компаса Мау Пиайлуга и Нааинои Томпсона); государство назвало себя инструментом, которым его предки нашли дорогу; tauranga (маори) — стоянка, якорная, место высадки каноэ; так назвали точку швартовки корабля, город вырос вокруг неё |
| `eps_eri_harmony` | CELESTIAL-DESTINY-1, HOMEWARD-1 | zh-Hans / zh-Hans | Epsilon Eridani | 天苑 | 新和 | 盛和 | Синьхэ / Цветущая Гармония | NORMALIZE / EXONYM | 新 = новый, 和 = гармония (эхо 和谐社会); прежняя этимология «ядро / река» отброшена: 河 «река» — другой иероглиф. Русское написание слитно по Палладию. Совпадение с реальным уездом 新和县 (сокращение 新疆和平) — VERIFY |
| `eps_eri_strife` | FREE-VENTURE-1, HOMEWARD-1 | en / en | Epsilon Eridani | 天苑 | Warpstanis | Strife Belt | Варпстанис / Пояс Раздора | SLANG / NORMALIZE | кривой пилотский английский (warp + -stan) — в вольном поясе это нормально |
| `wolf359_citadel` | IRON-SHIELD-1 | nb / nb | Wolf 359 | Ulven | Skjoldborg | Skjoldborg | Скьольдборг / Цитадель | NORMALIZE / EXONYM | прежняя русская форма смешивала древнесканд. Skjöldr и немецкое -burg; правильная современная норвежская форма — skjoldborg. Русская практическая транскрипция по правилам — «Шолльборг»; принята распространённая побуквенная «Скьольдборг» (ср. «Скьольд») |
| `gj581_europa` | EUROPA-1 | fr / fr | Gliese 581 | Marianne | Union de la Nouvelle-Europe | Nouvelle-Europe | Союз Новой Европы / Новая Европа | RENAME / KEEP | прежнее имя (Re + Tauros) — не греческое и не латинское слово; доктрина реставрации естественно называется по-французски (ср. Реставрация Бурбонов) |
| `gj581_freeclans` | FREE-WORLD-1 | en / en | Gliese 581 | Marianne | Free World clans | Free Settlements | кланы Свободного мира / Свободные поселения | KEEP / KEEP | — |
| `hd20794_hellas2` | HOMESTEAD-1 | en / en | HD 20794 (82 G. Eridani) | Lantern | Homestead | Homestead | Хомстед / Хомстед | RENAME / RENAME | прежнее «эллинское» имя у североамериканского консорциума без греков ничем не объяснялось; homestead — надел для опоздавших (Homestead Act 1862, Dominion Lands Act 1872), ровно мотив миссии «мы опоздали к Мэйфлауэру» |
| `hd20794_newindia` | DHARMA-2 | sa / hi | HD 20794 (82 G. Eridani) | Lantern | दक्षिण धर्ममण्डल | नया भारत | Дхарма-Мандала (южная ветвь) / Новая Индия | NORMALIZE / EXONYM | धर्म — закон, долг; मण्डल — круг, область; санскритский составной корректен |
| `hd40307_maland` | UHURU-2 | sw / sw | HD 40307 | Nuru | Jumuiya ya Ujamaa | Maland Mpya | Содружество Уджамаа / Новая Маланд | NORMALIZE / VERIFY | ujamaa (суах.) — братство, общинность (политика Ньерере); jumuiya — содружество (ср. Jumuiya ya Afrika Mashariki). «Jahi» в словарях суахили не засвидетельствовано (это англоязычное «африканское» имя для детей) — снято. Суахили — рабочий язык АС с 2022; «Маланд» — имя автора, происхождение не задано; в суахили такого слова нет — не выдумываем; mpya = «новая» |
| `hd40307_firmament` | CELESTIAL-DESTINY-2 | zh-Hans / zh-Hans | HD 40307 | Nuru | 天垒省 | 天垒 | Синьхэ (провинция Тяньлэй) / Небесная Твердыня | NORMALIZE / EXONYM | — |
| `hd69830_verde` | AMAZONIA-1 | pt / pt | HD 69830 | Guaraci | Selva Verde | Selva Verde | Сельва-Верде / Сельва-Верде | NORMALIZE / NORMALIZE | «Сельта» — опечатка; selva (порт.) — сельва, тропический лес |
| `delta_pav_karma` | DHARMA-1 | sa / hi | Delta Pavonis | मयूर | उत्तर धर्ममण्डल | कर्मभूमि | Дхарма-Мандала (северная ветвь) / Новая Карма | NORMALIZE / EXONYM | — |
| `delta_pav_freekarma` | DHARMA-1 | hi / hi | Delta Pavonis | मयूर | स्वतंत्र कर्मभूमि | स्वतंत्र कर्मभूमि | Свободная Карма / Свободная Карма | EXONYM / EXONYM | — |
| `gj876_next` | NEXT-1 | en / en | Gliese 876 | Primum Mobile | Providence | Providence | Провидение / Провидение | KEEP / KEEP | — |
| `cyg61_kern` | RESOURCER-1 | en / en | 61 Cygni | Bessel | Kern Prospectorate | Mine Alpha | Проспекторат Керн / Шахта Альфа | KEEP / KEEP | — |
| `hd114783_hellas3` | ODYSSEY-1 | el / el | HD 114783 | Όμηρος | Τρίτη Ελλάδα | Τρίτη Ελλάδα | Третья Эллада / Третья Эллада | KEEP / KEEP | — |
| `leo83_zion` | NEW-JERUSALEM-1 | en / en | 83 Leonis | Θηβαΐς | New Thebaid | New Thebaid | Ново-Фиваида / Ново-Фиваида | RENAME / RENAME | решение автора 2026-11-05: Фиваида — пустыня ранних отшельников; рабочий язык ковчега английский, литургия греческая и латинская |
| `sol_authority` | — | en | Sol (Солнце) | — | Sol Authority | — | Сол | KEEP | — |

### Языковой слой

- `non_is_not_modern` — Old Norse (non) допустим только для kind = ceremonial / motto / historical / mythological; современные эндонимы норвежской ветви — nb или nn
- `nordic_endonym_not_russian` — у эндонима nb / nn / non не может быть кириллицы
- `pacific_lingua_franca` — te reo Māori — государственный и топонимический язык Капеху; английский — второй (торговля, сеть); самоанский, тонганский, гавайский — общинные и топонимические вкрапления
- `nordic_standards` — административный язык Скьольдборга вырос из букмола; старые поселения вахтовиков из Вестланна держат нюнорск (hamn, jarn, ikkje); флот пишет сокращениями (nb)

### Места и площадки

| ID | Эндоним | Язык | Произношение | RU | EN | Сленг и другие формы | Почему так | Вердикт |
|---|---|---|---|---|---|---|---|---|
| `place:wolf359.asteroid_2` | Toeren | nb | [ˈtuːərən] | Тоэрен | Toeren | — | «Двойка»: код «астероид № 2» стал именем; причина потерь IRON-SHIELD-2 остаётся UNKNOWN | SLANG |
| `site:beacon.BCN-003-BR` | Bifrost-varden | nb | — | Биврёст — мост-маяк BCN-003-BR | Bifrost Beacon BCN-003-BR | — | мост богов — мост-маяк; varde — каменный знак и сигнальный костёр на берегу | RENAME |
| `site:delta_pav_karma.gene_temple` | बीज मन्दिर | hi | — | Храм Семени (криобанк генов) | Seed Temple (gene cryobank) | — | «Храм Массива» — бессмысленная калька; बीज — семя, मन्दिर — храм | RENAME |
| `site:gj581_europa.registry` | Registre des traités et Arbitrage « Nouveau-Bruxelles » | fr | — | Реестр договоров и Арбитраж «Новый Брюссель» | Treaty Registry and Arbitration “New Brussels” | — | — | EXONYM |
| `site:lalande_hellas.agora` | Σταυροδρόμι | el | [stavroˈðromi] | Ставродроми — нейтральный порт и биржа | Stavrodromi (Crossroads) free port | Перекрёсток (translation) | σταυροδρόμι — перекрёсток; прежнее «Агора» оставлено столице | NORMALIZE |
| `site:lalande_mecca.lalande_j_gas_m` | Ngā Papa o Lalande 21185 j | mi | — | Платформы Lalande 21185 j (доля Капеху) | Lalande 21185 j platforms (Kāpehu share) | — | — | RENAME |
| `site:lalande_mecca.mecca_farms` | Ngā Māra Kai | mi | — | Огороды Мара-Каи (Lalande 21185 g) | Māra Kai gardens | — | māra kai — огород, сад, где растёт еда | RENAME |
| `site:lalande_mecca.rasad` | Te Whare Tātai Arorangi | mi | [tɛ ˈfaɾɛ ˈtaːtai ˈaɾɔɾaŋi] | Обсерватория-вычислитель Фаре-Татаи | House of Star Lore (observatory-computer) | Фаре (slang), the Whare (slang) | whare — дом; tātai arorangi — астрономия (kōkōrangi само по себе — сорт кумары, «tohunga kōkōrangi» — астроном). Здесь сформулирована геометрия этак-фреймов | RENAME |
| `site:leo83_golden_city.capital` | Golden City | en | — | «Золотой Город» | Golden City | Golden Debt (slang), Золотой Долг (slang) | — | KEEP |
| `site:leo83_zion.capital` | New Thebaid (orbital ark) | en | — | Ново-Фиваида (ковчег на орбите) | New Thebaid (orbital ark) | — | — | RENAME |
| `site:site.003.wolf359_citadel.military_hub01` | Heimdal vaktstasjon | nb | — | Хеймдаль — сторожевая станция | Heimdal Watch Station | — | страж Биврёста, который слышит, как растёт трава: радар и наблюдение | RENAME |
| `site:tau_ceti_cognium.beacon_authority` | the Owlery | en | — | Управление Маяков и фабрика ядер прыжка «Сычёвник» | the Owlery | — | русское авторское «Сычёвник» — экзоним английского Owlery | EXONYM |
| `site:wolf359_citadel.archive_vault` | Arkivhvelvet | nb | — | Хранилище LAST-LIGHT | LAST-LIGHT Vault | — | «архивный свод», по образцу Svalbard globale frøhvelv; русское имя автора — экзоним | EXONYM |
| `site:wolf359_citadel.bastion` | Mjølner | nb | [ˈmjœ̂lnər] | Мьёльнер — арсенал и военная верфь | Mjølner Arsenal and Naval Yard | — | молот Тора — оружейный завод; функция совпадает с мифом | RENAME |
| `site:wolf359_citadel.habitat1` | Nyhamn | nn | [ˈnŷːhɑmn] | Нюхамн | Nyhamn | — | «новая гавань»; hamn — нюнорск, havn — букмол | RENAME |
| `site:wolf359_citadel.habitat2` | Jarnvik | nn | — | Ярнвик | Jarnvik | — | «железная бухта» (jarn — нюнорск, jern — букмол): у мира f нет моря, бухтой назвали кратер с рудой | RENAME |
| `site:wolf359_citadel.wolf_mines` | Gruve 7 | nb | — | Груве-7 (рудники пылевого диска) | Gruve 7 | — | рабочий номер шахты, как Gruve 7 на Шпицбергене; номер пережил первоначальную нумерацию | RENAME |
| `system:1` | अयोध्या | sa | — | Айодхья | Ayodhya | — | — | KEEP |
| `system:10` | Marianne | fr | — | Марианна | Marianne | — | — | KEEP |
| `system:11` | Ubuntu | zu | — | Убунту | Ubuntu | — | — | KEEP |
| `system:12` | 鹊桥 | zh-Hans | — | Цюэцяо | Queqiao | — | — | KEEP |
| `system:13` | Primum Mobile | en | — | Перводвигатель | Primum Mobile | — | — | KEEP |
| `system:14` | Lantern | en | — | Фонарь | Lantern | — | — | KEEP |
| `system:15` | Nuru | sw | — | Нуру | Nuru | — | — | KEEP |
| `system:16` | Guaraci | pt | — | Гуараси | Guaraci | — | — | KEEP |
| `system:17` | Antikythera | en | — | Антикитера | Antikythera | — | — | KEEP |
| `system:19` | 月门 | zh-Hans | — | Жопа мира | World's Ass | — | — | KEEP |
| `system:2` | Huginn | nb | — | Хугин | Huginn | — | — | KEEP |
| `system:20` | 零维 | zh-Hans | — | Линвэй | Lingwei | — | — | KEEP |
| `system:21` | Lumière | fr | — | Люмьер | Lumiere | — | — | KEEP |
| `system:22` | मयूर | hi | — | Маюра | Mayura | — | — | KEEP |
| `system:23` | Bonanza | en | — | Бонанза | Bonanza | — | — | KEEP |
| `system:24` | Παρνασσός | el | — | Парнас | Parnassus | — | — | KEEP |
| `system:25` | Kestrel | en | — | Пустельга | Kestrel | — | — | KEEP |
| `system:26` | 甲子 | zh-Hans | — | Цзяцзы | Jiazi | — | — | KEEP |
| `system:27` | Yggdrasil | nb | — | Иггдрасиль | Yggdrasil | — | — | KEEP |
| `system:28` | Plymouth | en | — | Плимут | Plymouth | — | — | KEEP |
| `system:29` | 四时 | zh-Hans | — | Сыши | Sishi | — | — | KEEP |
| `system:3` | Ulven | nb | — | Ульвен | Ulven | — | народная этимология: каталожный Wolf стал ulv — «волк»; Ulven — «Волк» | KEEP |
| `system:30` | 氦田 | zh-Hans | — | Хайтянь | Haitian | — | — | KEEP |
| `system:31` | Bessel | en | — | Бессель | Bessel | — | — | KEEP |
| `system:32` | Red Eye | en | — | Красный Глаз | Red Eye | — | — | KEEP |
| `system:33` | Lovelock | en | — | Лавлок | Lovelock | — | — | KEEP |
| `system:34` | 龙宫 | zh-Hans | — | Лунгун | Longgong | — | — | KEEP |
| `system:35` | ज्योतिष | sa | — | Джйотиша | Jyotisha | — | — | KEEP |
| `system:36` | 鲸箱 | zh-Hans | — | Китовый Сундук | Whale Chest | — | — | KEEP |
| `system:37` | Όμηρος | el | — | Гомер | Homer | — | — | KEEP |
| `system:38` | Eureka | en | — | Эврика | Eureka | — | — | KEEP |
| `system:39` | Libertalia | en | — | Либерталия | Libertalia | — | — | KEEP |
| `system:4` | Ηρακλής | el | — | Геракл | Heracles | — | звезда 7.5m — у неё нет традиционного имени; две культуры одной системы называют её по-своему | KEEP |
| `system:40` | त्रिवेणी | hi | — | Тривени | Triveni | — | — | KEEP |
| `system:41` | Lark | en | — | Жаворонок | Lark | — | — | KEEP |
| `system:42` | Prospect | en | — | Проспект | Prospect | — | — | KEEP |
| `system:43` | Giverny | fr | — | Живерни | Giverny | — | — | KEEP |
| `system:44` | Ακαδημία | el | — | Академия | Academy | — | — | KEEP |
| `system:45` | Θηβαΐς | el | — | Фиваида | Thebaid | — | — | KEEP |
| `system:46` | संगीत | hi | — | Сангит | Sangeet | — | — | KEEP |
| `system:47` | Prairie | en | — | Прерия | Prairie | — | — | KEEP |
| `system:48` | Concordia | la | — | Конкордия | Concordia | — | — | KEEP |
| `system:49` | 天工 | zh-Hans | — | Тяньгун | Tiangong | — | — | KEEP |
| `system:5` | Βηθλεέμ | el | — | Вифлеем | Bethlehem | — | — | KEEP |
| `system:50` | 孙子 | zh-Hans | — | Сунь-цзы | Sunzi | — | — | KEEP |
| `system:51` | Ergo | en | — | Эрго | Ergo | — | — | KEEP |
| `system:52` | Saudade | pt | — | Саудади | Saudade | — | — | KEEP |
| `system:53` | Strix | en | — | Стрикс | Strix | — | — | KEEP |
| `system:54` | Heron | en | — | Цапля | Heron | — | — | KEEP |
| `system:55` | मसाला | hi | — | Масала | Masala | — | — | KEEP |
| `system:56` | Te Ara | mi | — | Те-Ара | Te Ara | — | — | KEEP |
| `system:57` | Daraja | sw | — | Дараджа | Daraja | — | — | KEEP |
| `system:58` | Αργώ | el | — | Арго | Argo | — | — | KEEP |
| `system:59` | Paydirt | en | — | Россыпь | Paydirt | — | — | KEEP |
| `system:6` | 和声 | zh-Hans | — | Хэшэн | Hesheng | — | — | KEEP |
| `system:60` | Ulugʻbek | uz | — | Улугбек | Ulugh Beg | — | — | KEEP |
| `system:7` | Mzee | sw | — | Мзее | Mzee | — | — | KEEP |
| `system:8` | 天仓 | zh-Hans | — | Тяньцан | Tiancang | — | — | KEEP |
| `system:9` | 天苑 | zh-Hans | — | Тяньюань | Tianyuan | — | — | KEEP |

### Миссии

| Код | Собственное имя | Язык | RU | Почему | Вердикт |
|---|---|---|---|---|---|
| `CELESTIAL-DESTINY-1` | 天命一号 | zh-Hans | Небесная Судьба | 天命 — Мандат Неба; англ. код CELESTIAL-DESTINY — вольный экспортный перевод, китайские госпроекты называют себя пиньинем (ср. Tianwen, Shenzhou) | NORMALIZE |
| `CELESTIAL-DESTINY-2` | 天命二号 | zh-Hans | Небесная Судьба-2 | — | NORMALIZE |
| `HOMESTEAD-1` | Homestead | en | Хомстед | — | RENAME |
| `IRON-SHIELD-1` | Jernskjold | nb | Айрон Шилд-1 | — | EXONYM |
| `MERU-1` | मेरु-1 | sa | Меру-1 | корабль-крепость назван по мировой горе Меру | RENAME |
| `TIANHE-1` | 天河一号 | zh-Hans | Тяньхэ-1 | — | NORMALIZE |
| `VAKA-MOANA-1` | Vaka Moana | to | Вака-Моана | общеполинезийское «океанский корабль» вместо маорийского waka: имя не должно было принадлежать одной стране консорциума | RENAME |

### Реликты массовой культуры

| Форма | Вид | Язык | Откуда | Что так зовут | Статус |
|---|---|---|---|---|---|
| Ватман | spacer_slang | ru | Batman | ночной патруль Скьольдборга; так зовут перехватчики, которые выходят только во тьме у Ульвена | PROPOSAL |
| Хулк | miners_nickname | ru | Hulk | тяжёлый буксир-крушитель астероидов на Груве-7 | PROPOSAL |
| Саперман | children_nickname | ru | Superman | детское прозвище пилота спасательной службы, который вытащил людей из пожара на Миналфе | PROPOSAL |
| Око Саурона / Sauron's Eye | pilot_nickname | en | Tolkien, Eye of Sauron | красная звезда HD 102195 сквозь искажение у горячего железа; у русских пилотов — «Саурово око» | PROPOSAL |
| Солярис | pilot_nickname | ru | Lem, Solaris | ядро «Провидца» у Gliese 876: машину, которая отвечает не на те вопросы | PROPOSAL |
| Stainless Steel Rat | smugglers_nickname | en | Harrison | ремонтный причал Чёрного рынка Пояса Раздора; русские — «Крыса» | PROPOSAL |
| Amber | cultural_relic | en | Zelazny | архивный уровень Хранилища LAST-LIGHT, где лежит «единственная настоящая копия» Земли; остальные — тени | PROPOSAL |

### Люди XXIII–XXIX веков в именах

| Имя | Язык | Эпоха | Роль | Статус |
|---|---|---|---|---|
| Sione Leota | sm | 2470-е | математик Фаре-Татаи; первая формализация этак-фреймов: корабль неподвижен, звёзды движутся | PROPOSAL |
| Ingrid Haugland | nb | 2610-е | командир охраны, при ней Скьольдборг перестал быть охраной и стал протекторатом | PROPOSAL |
| Kofi Mensah-Ruto | sw | 2560-е | агроном агроколец Маланд; «житница» названа его шуткой | PROPOSAL |

### VERIFY

- 盛和 как китайское имя столицы «Цветущая Гармония» — стилистика
- 新和: совпадает с реальным уездом 新和县 (сокращение 新疆和平); оставлено как «новая гармония»
- шанхайское произношение 上海 «Заанхе»
- Maland: происхождение у автора не задано; Maland Mpya — грамматика суахили
- Skjaldborg heldr — грамматика древнескандинавского девиза
- skjoldborgere — норвежский демоним
- Toeren — разговорное norsk «двойка»
- Fomalhaut в каталоге полёта (StarAtlas id 61, 2026-11-05): промежуточный пояс JWST (~78 а. е.) не моделируется отдельным резервуаром — генератор создал один диск обломков, ему присвоено главное кольцо ALMA 136–150 а. е.
<!-- GENERATED:naming END -->
