"""Naming series 3126: the names the discoverers / first settlers gave to the stars and bodies of every
inhabited system (PROPOSAL, author request 2026-11-11: "write the names the discoverers already gave,
not Tau Ceti alpha, beta"). One series per system, in the language and myth of the people who named it,
the way Sol has Roman gods and Fortuna has the circle of Fortune (CANON 2026-11-06).

Data only. `python naming_series_3126.py --write` merges the series into canon/naming.json as entities
(system:<id> name, body:<runtime id> names, status PROPOSAL, existing CANON entries never touched);
then `python naming.py --write` mirrors them into the runtime table names_3126.json.

Row: designation -> (native text, ru, en, translit or None, meaning in Russian).
Captured moonlets below 1e-5 Earth masses keep their catalogue designation (no one names a 1 km rock).
"""

import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
NAMING = os.path.join(HERE, "canon", "naming.json")
SYSTEMS = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "systems")
MIN_MASS_EARTH = 1e-5
DATE = "2026-11-11"

S = {}

# ------------------------------------------------------------------ 2 Barnard's Star: Skjoldborg listening post
S[2] = dict(lang="nb", named_by="Скьольдборг (Цитадель Wolf 359), пост прослушивания у Барнарда, 2860",
            theme="вороны Одина и огненный юг скандинавского мифа: пост, который смотрит на Сол",
            star=("Huginn", "Хугин", "Huginn", None, "ворон Одина «Мысль», облетающий мир и приносящий вести: пост прослушивания Сола"),
            bodies={
    "Barnard's Star f": ("Surt", "Сурт", "Surt", None, "огненный великан Муспелля: самая горячая планета"),
    "Barnard's Star g": ("Muspell", "Муспелль", "Muspell", None, "огненный мир юга"),
    "Barnard d": ("Loge", "Логе", "Loge", None, "пламя, соперник Локи в поедании"),
    "Barnard b": ("Brokk", "Брокк", "Brokk", None, "карлик-кузнец, ковавший Мьёльнир"),
    "Barnard c": ("Sindre", "Синдри", "Sindre", None, "брат Брокка, кузнец у горна"),
    "Barnard e": ("Hlidskjalv", "Хлидскьяльв", "Hlidskjalf", None, "престол Одина, с которого видно все миры"),
    "Barnard's Star h": ("Valaskjalv", "Валаскьяльв", "Valaskjalf", None, "чертог Одина с серебряной кровлей"),
    "Barnard's Star i": ("Gjøll", "Гьёлль", "Gjoll", None, "ледяная река у врат Хель"),
    "Barnard's Star j": ("Muninn", "Мунин", "Muninn", None, "второй ворон Одина, «Память»: архив и столица поста"),
    "Barnard's Star k": ("Fimbul", "Фимбуль", "Fimbul", None, "Фимбульвинтер, великая зима: самая холодная"),
})

# ------------------------------------------------------------------ 3 Wolf 359 (Ulven): Norwegian Citadel
S[3] = dict(lang="nb", named_by="норвежские поселенцы IRON-SHIELD-1 (Скьольдборг)",
            theme="волки скандинавского мифа: звезда Ulven («Волк»), планеты — её стая",
            star=None,
            bodies={
    "Wolf 359 b": ("Sköll", "Скёлль", "Skoll", None, "волк, гонящийся за Солнцем: ближайшая к звезде"),
    "Wolf 359 c": ("Hati", "Хати", "Hati", None, "волк, гонящийся за Луной: поселение Нюхамн"),
    "Wolf 359 d": ("Geri", "Гери", "Geri", None, "«жадный», волк Одина"),
    "Wolf 359 e": ("Garm", "Гарм", "Garm", None, "пёс-страж у врат Хель: мир Цитадели и Мьёльнера"),
    "Wolf 359 f": ("Freki", "Фреки", "Freki", None, "«прожорливый», второй волк Одина: поселение Ярнвик"),
    "Wolf 359 g": ("Fenrir", "Фенрир", "Fenrir", None, "скованный волк на краю мира: внешняя ледяная планета"),
})

# ------------------------------------------------------------------ 4 Lalande 21185: Heracles mission + Kapehu
S[4] = dict(lang="el", named_by="HERACLES-1 (Новая Эллада); капеху зовут звезду Te Rā Hou",
            theme="двенадцать подвигов Геракла — двенадцать планет, по порядку от звезды",
            star=("Ηρακλής", "Геракл", "Heracles", "Iraklís", "миссия HERACLES-1 назвала звезду именем своего героя"),
            bodies={
    "Lalande 21185 b": ("Νεμέα", "Немея", "Nemea", "Neméa", "1-й подвиг: немейский лев"),
    "Lalande 21185 c": ("Λέρνη", "Лерна", "Lerna", "Lérni", "2-й подвиг: лернейская гидра"),
    "Lalande 21185 d": ("Κερύνεια", "Керинея", "Ceryneia", "Kerýneia", "3-й подвиг: керинейская лань"),
    "Lalande 21185 e": ("Ερύμανθος", "Эриманф", "Erymanthos", "Erýmanthos", "4-й подвиг: эримантский вепрь"),
    "Lalande 21185 f": ("Αυγείας", "Авгий", "Augeas", "Avgeías", "5-й подвиг: авгиевы конюшни"),
    "Lalande 21185 g": ("Στυμφαλία", "Стимфалия", "Stymphalia", "Stymfalía", "6-й подвиг: стимфалийские птицы"),
    "Lalande 21185 h": ("Κρήτη", "Крит", "Crete", "Kríti", "7-й подвиг: критский бык; водный мир общих поселений и огородов капеху"),
    "Lalande 21185 i": ("Θράκη", "Фракия", "Thrace", "Thráki", "8-й подвиг: кони Диомеда"),
    "Lalande 21185 j": ("Θεμίσκυρα", "Фемискира", "Themiscyra", "Themískyra", "9-й подвиг: пояс Ипполиты; газовые платформы обеих общин"),
    "Lalande 21185 k": ("Ερύθεια", "Эрифия", "Erytheia", "Erýtheia", "10-й подвиг: коровы Гериона"),
    "Lalande 21185 k I": ("Όρθρος", "Орф", "Orthrus", "Órthros", "двуглавый пёс Гериона, страж его стад"),
    "Lalande 21185 l": ("Κέρβερος", "Кербер", "Cerberus", "Kérveros", "12-й подвиг: пёс Аида, самая тёмная из ближних"),
    "Lalande 21185 m": ("Εσπερίδες", "Геспериды", "Hesperides", "Esperídes", "11-й подвиг: сад Гесперид — мир Агоры и Тауранги"),
})

# ------------------------------------------------------------------ 8 Tau Ceti (Tiancang): Cognium
S[8] = dict(lang="zh-Hans", named_by="Когниум (Синь-Шанхай), по китайскому имени звезды 天仓 «Небесная житница»",
            theme="звезда — Небесная житница, раскалённые планеты — зёрна в её закромах",
            star=None,
            bodies={
    "Tau Ceti b": ("稻", "Дао", "Dao", "Dào", "рис"),
    "Tau Ceti c": ("麦", "Май", "Mai", "Mài", "пшеница"),
    "Tau Ceti d": ("黍", "Шу", "Shu", "Shǔ", "просо клейкое"),
    "Tau Ceti e": ("高粱", "Гаолян", "Gaoliang", "Gāoliang", "гаолян, сорго"),
    "Tau Ceti i": ("豆", "Доу", "Dou", "Dòu", "бобы"),
    "Tau Ceti j": ("粟", "Су", "Su", "Sù", "чумиза"),
    "Tau Ceti k": ("粱", "Лян", "Liang", "Liáng", "отборное просо"),
    "Tau Ceti l": ("麻", "Ма", "Ma", "Má", "конопля"),
    "Tau Ceti m": ("谷", "Гу", "Gu", "Gǔ", "зерно в колосе: самая дальняя"),
})

# ------------------------------------------------------------------ 9 Epsilon Eridani (Tianyuan): Harmony + Strife Belt
S[9] = dict(lang="zh-Hans", named_by="CELESTIAL-DESTINY-1 (Цветущая Гармония), по китайскому имени звезды 天苑 «Небесные луга»",
            theme="Небесные луга: планеты — цветы «Цветущей Гармонии», гигант — небесный конь и его скакуны, ледяные тела — хозяйство луга",
            star=None,
            bodies={
    "Epsilon Eridani c": ("梅", "Мэй", "Mei", "Méi", "цветущая слива"),
    "Epsilon Eridani d": ("兰", "Лань", "Lan", "Lán", "орхидея"),
    "Epsilon Eridani e": ("竹", "Чжу", "Zhu", "Zhú", "бамбук"),
    "Epsilon Eridani f": ("菊", "Цзюй", "Ju", "Jú", "хризантема"),
    "Epsilon Eridani g": ("牡丹", "Мудань", "Mudan", "Mǔdān", "пион"),
    "Epsilon Eridani h": ("荷", "Хэ", "He", "Hé", "лотос"),
    "Epsilon Eridani i": ("桂", "Гуй", "Gui", "Guì", "османтус"),
    "Epsilon Eridani j": ("桃", "Тао", "Tao", "Táo", "персик"),
    "eps Eri b": ("天马", "Тяньма", "Tianma", "Tiānmǎ", "Небесный конь: газовый гигант с кольцами"),
    "eps Eri b I": ("骐", "Ци", "Qi", "Qí", "вороной скакун"),
    "eps Eri b II": ("骥", "Цзи", "Ji", "Jì", "скакун на тысячу ли"),
    "eps Eri b III": ("骅", "Хуа", "Hua", "Huá", "рыжий скакун"),
    "Epsilon Eridani A1": ("栅", "Чжа", "Zha", "Zhà", "изгородь луга: пояс астероидов"),
    "Epsilon Eridani K2": ("牧场", "Мучан", "Muchang", "Mùchǎng", "пастбище"),
    "Epsilon Eridani K3": ("泉", "Цюань", "Quan", "Quán", "родник"),
    "Epsilon Eridani K4": ("园", "Юань", "Yuan", "Yuán", "сад: мир Гармонии и Пояса Раздора"),
})

# ------------------------------------------------------------------ 10 Gliese 581: Nouvelle-Europe
S[10] = dict(lang="fr", named_by="EUROPA-1 (Новая Европа)",
             theme="реки Европы — планеты, озёра — ледяные карлики; звезда — Марианна",
             star=("Marianne", "Марианна", "Marianne", None, "символ Республики: колонисты EUROPA-1 назвали звезду по-французски"),
             bodies={
    "Gliese 581 d": ("Rhône", "Рона", "Rhone", None, "самая быстрая река: ближайшая к звезде"),
    "Gliese 581 f": ("Garonne", "Гаронна", "Garonne", None, "река Аквитании"),
    "Gliese 581 g": ("Loire", "Луара", "Loire", None, "река замков"),
    "GJ 581 e": ("Tage", "Тахо", "Tagus", None, "Тежу / Тахо, река Иберии"),
    "GJ 581 b": ("Danube", "Дунай", "Danube", None, "великая река: тяжёлая суперземля"),
    "GJ 581 c": ("Rhin", "Рейн", "Rhine", None, "пограничная река"),
    "Gliese 581 h": ("Seine", "Сена", "Seine", None, "река Парижа: мир Новой Европы и Свободных поселений"),
    "Gliese 581 i": ("Vistule", "Висла", "Vistula", None, "река с кольцами газовых платформ"),
    "Gliese 581 K2": ("Léman", "Леман", "Leman", None, "Женевское озеро"),
    "Gliese 581 K3": ("Constance", "Констанц", "Constance", None, "Боденское озеро"),
    "Gliese 581 K1": ("Ladoga", "Ладога", "Ladoga", None, "самое большое озеро Европы: агромодули"),
})

# ------------------------------------------------------------------ 11 Gliese 667: Ubuntu node
S[11] = dict(lang="zu", named_by="Узел Убунту (Когниум, связисты из Южной Африки), 2800",
             theme="слова зулу: у звезды A — огонь, металлы и земля, у B — погода и вода, у C — время суток, у пояса — общее дело",
             star=("Ubuntu", "Убунту", "Ubuntu", None, "«человек человеку — человек»: узел связи, который держит три колонии вместе"),
             bodies={
    "Gliese 667 A d": ("Umlilo", "Умлило", "Umlilo", None, "огонь"),
    "Gliese 667 A h": ("Ilangabi", "Илангаби", "Ilangabi", None, "пламя"),
    "Gliese 667 A i": ("Amalahle", "Амалахле", "Amalahle", None, "угли"),
    "Gliese 667 A j": ("Umlotha", "Умлота", "Umlotha", None, "пепел"),
    "Gliese 667 A k": ("Ukushisa", "Укушиса", "Ukushisa", None, "жар"),
    "Gliese 667 A l": ("Ilitshe", "Илитше", "Ilitshe", None, "камень"),
    "Gliese 667 A m": ("Insimbi", "Инсимби", "Insimbi", None, "железо"),
    "Gliese 667 A n": ("Ithusi", "Итуси", "Ithusi", None, "медь"),
    "Gliese 667 A o": ("Igolide", "Иголиде", "Igolide", None, "золото"),
    "Gliese 667 A p": ("Isihlabathi", "Исихлабати", "Isihlabathi", None, "песок"),
    "Gliese 667 A q": ("Intaba", "Интаба", "Intaba", None, "гора"),
    "Gliese 667 A r": ("Umhlabathi", "Умхлабати", "Umhlabathi", None, "почва"),
    "Gliese 667 A s": ("Umfula", "Умфула", "Umfula", None, "река: поселение"),
    "Gliese 667 A s II": ("Amanzi", "Аманзи", "Amanzi", None, "вода"),
    "Gliese 667 A t": ("Indaba", "Индаба", "Indaba", None, "совет, разговор: столица узла связи"),
    "Gliese 667 A u": ("Ithemba", "Итемба", "Ithemba", None, "надежда: поселение"),
    "Gliese 667 A u II": ("Isibane", "Исибане", "Isibane", None, "светильник"),
    "Gliese 667 A v": ("Isiphepho", "Исипепо", "Isiphepho", None, "смерч: плотная горячая атмосфера"),
    "Gliese 667 B d": ("Umbane", "Умбане", "Umbane", None, "молния"),
    "Gliese 667 B h": ("Ukuduma", "Укудума", "Ukuduma", None, "гром"),
    "Gliese 667 B i": ("Imvula", "Имвула", "Imvula", None, "дождь"),
    "Gliese 667 B j": ("Ifu", "Ифу", "Ifu", None, "облако"),
    "Gliese 667 B k": ("Inkungu", "Инкунгу", "Inkungu", None, "туман"),
    "Gliese 667 B l": ("Umoya", "Умоя", "Umoya", None, "ветер, дух"),
    "Gliese 667 B m": ("Ulwandle", "Улвандле", "Ulwandle", None, "море"),
    "Gliese 667 B n": ("Isiqhingi", "Исикхинги", "Isiqhingi", None, "остров"),
    "Gliese 667 B o": ("Iqhwa", "Икхва", "Iqhwa", None, "лёд"),
    "Gliese 667 B p": ("Ubusika", "Убусика", "Ubusika", None, "зима"),
    "Gliese 667 C d": ("Ukusa", "Укуса", "Ukusa", None, "рассвет"),
    "Gliese 667 C h": ("Imini", "Имини", "Imini", None, "полдень"),
    "Gliese 667 C i": ("Ntambama", "Нтамбама", "Ntambama", None, "после полудня"),
    "GJ 667 C b": ("Kusihlwa", "Кусихлва", "Kusihlwa", None, "вечер"),
    "GJ 667 C c": ("Ubusuku", "Убусуку", "Ubusuku", None, "ночь"),
    "Gliese 667 C j": ("Inkanyezi", "Инканьези", "Inkanyezi", None, "звезда"),
    "Gliese 667 (BA) K2": ("Ubumbano", "Убумбано", "Ubumbano", None, "единство"),
    "Gliese 667 (BA) K1": ("Ukuthula", "Укутула", "Ukuthula", None, "мир, покой"),
    "Gliese 667 (BA) K3": ("Inkululeko", "Инкулулеко", "Inkululeko", None, "свобода"),
})

# ------------------------------------------------------------------ 12 Gliese 832: Tianhe
S[12] = dict(lang="zh-Hans", named_by="TIANHE-1 (Тяньхэ, «Небесная река» — Млечный Путь)",
             theme="легенда о Пастухе и Ткачихе, разлучённых Небесной рекой: звезда — Сорочий мост",
             star=("鹊桥", "Цюэцяо", "Queqiao", "Quèqiáo", "Сорочий мост, по которому Пастух и Ткачиха встречаются раз в год"),
             bodies={
    "Gliese 832 c": ("王母", "Ванму", "Wangmu", "Wángmǔ", "Царица-Мать Запада, разделившая влюблённых"),
    "Gliese 832 d": ("玉簪", "Юйцзань", "Yuzan", "Yùzān", "нефритовая шпилька, которой она провела Небесную реку"),
    "Gliese 832 e": ("银汉", "Иньхань", "Yinhan", "Yínhàn", "Серебряная река"),
    "Gliese 832 f": ("织机", "Чжицзи", "Zhiji", "Zhījī", "ткацкий станок: железная планета"),
    "Gliese 832 g": ("七夕", "Цыси", "Qixi", "Qīxī", "ночь седьмого дня седьмой луны, ночь встречи"),
    "GJ 832 b": ("牛郎", "Нюлан", "Niulang", "Niúláng", "Пастух: газовый гигант"),
    "GJ 832 b I": ("喜鹊", "Сицюэ", "Xique", "Xǐquè", "сорока, строительница моста; поселение и комбинат"),
    "GJ 832 b II": ("金梭", "Цзиньсо", "Jinsuo", "Jīnsuō", "золотой челнок Ткачихи; поселение"),
    "GJ 832 b III": ("织女", "Чжинюй", "Zhinü", "Zhīnǚ", "Ткачиха: столица Тяньхэ и Зона «Филоморфоз»"),
    "Gliese 832 A1": ("扁担", "Бяньдань", "Biandan", "Biǎndan", "коромысло, на котором Пастух нёс детей через небо"),
    "Gliese 832 K2": ("老牛", "Лаоню", "Laoniu", "Lǎoniú", "старый вол, научивший Пастуха"),
})

# ------------------------------------------------------------------ 13 Gliese 876: Providence (Cult of the Machine)
S[13] = dict(lang="en", named_by="NEXT-1 (Провидение, Культ Машины)",
             theme="святые Машины: учёные, придумавшие вычисление",
             star=("Primum Mobile", "Перводвигатель", "Primum Mobile", None, "первая движущая сфера: Культ Машины называет звезду источником всякого счёта"),
             bodies={
    "GJ 876 d": ("Pascal", "Паскаль", "Pascal", None, "первая суммирующая машина"),
    "GJ 876 c": ("Leibniz", "Лейбниц", "Leibniz", None, "двоичное исчисление"),
    "GJ 876 b": ("Babbage", "Бэббидж", "Babbage", None, "Аналитическая машина: главный гигант концессий"),
    "GJ 876 e": ("Lovelace", "Лавлейс", "Lovelace", None, "первая программа"),
    "Gliese 876 f": ("Turing", "Тьюринг", "Turing", None, "машина Тьюринга: поселение"),
    "Gliese 876 g": ("Von Neumann", "Фон Нейман", "Von Neumann", None, "архитектура вычислителя: столица и Ядро «Провидца»"),
    "Gliese 876 K1": ("Boole", "Буль", "Boole", None, "алгебра логики"),
    "Gliese 876 K2": ("Shannon", "Шеннон", "Shannon", None, "теория информации: поселение"),
    "Gliese 876 K3": ("Hopper", "Хоппер", "Hopper", None, "первый компилятор"),
})

# ------------------------------------------------------------------ 14 HD 20794: Homestead + New India
S[14] = dict(lang="en", named_by="HOMESTEAD-1 (Хомстед); Новая Индия приняла эти имена",
             theme="хозяйство поселенца: от кузни у звезды до усадьбы на льду",
             star=("Lantern", "Фонарь", "Lantern", None, "фонарь над усадьбой: тусклая звезда, которую видно из каждого купола"),
             bodies={
    "HD 20794 c": ("Cinder", "Окалина", "Cinder", None, "окалина: раскалённая ближняя"),
    "HD 20794 g": ("Ember", "Уголёк", "Ember", None, "тлеющий уголь"),
    "HD 20794 h": ("Kiln", "Обжиг", "Kiln", None, "печь для обжига"),
    "HD 20794 i": ("Forge", "Кузня", "Forge", None, "кузница"),
    "HD 20794 j": ("Anvil", "Наковальня", "Anvil", None, "наковальня"),
    "HD 20794 k": ("Furnace", "Горн", "Furnace", None, "горн"),
    "HD 20794 l": ("Smithy", "Мастерская", "Smithy", None, "мастерская кузнеца"),
    "HD 20794 b": ("Millstone", "Жёрнов", "Millstone", None, "жёрнов: тяжёлая суперземля"),
    "HD 20794 d": ("Windmill", "Ветряк", "Windmill", None, "ветряная мельница: газовые платформы обеих общин"),
    "HD 20794 f": ("Silo", "Силос", "Silo", None, "зерновая башня"),
    "HD 20794 f I": ("Scarecrow", "Пугало", "Scarecrow", None, "пугало у силоса"),
    "HD 20794 m": ("Frost", "Иней", "Frost", None, "первый заморозок"),
    "HD 20794 K1": ("Paddock", "Загон", "Paddock", None, "загон: поселение"),
    "HD 20794 K2": ("Orchard", "Сад", "Orchard", None, "фруктовый сад: поселение"),
    "HD 20794 K3": ("Hearth", "Очаг", "Hearth", None, "очаг: столица Хомстеда и Новой Индии"),
})

# ------------------------------------------------------------------ 15 HD 40307: Maland (Ujamaa) + Firmament
S[15] = dict(lang="sw", named_by="UHURU-2 (Новая Маланд, Содружество Уджамаа)",
             theme="слова суахили: стихии и хозяйство общины",
             star=("Nuru", "Нуру", "Nuru", None, "свет"),
             bodies={
    "HD 40307 e": ("Moto", "Мото", "Moto", None, "огонь"),
    "HD 40307 h": ("Chuma", "Чума", "Chuma", None, "железо: железная планета"),
    "HD 40307 b": ("Mlima", "Млима", "Mlima", None, "гора: суперземля"),
    "HD 40307 c": ("Simba", "Симба", "Simba", None, "лев: самая массивная из твёрдых"),
    "HD 40307 i": ("Jiwe", "Дживе", "Jiwe", None, "камень"),
    "HD 40307 d": ("Kimbunga", "Кимбунга", "Kimbunga", None, "ураган: атмосфера в 940 бар"),
    "HD 40307 j": ("Upepo", "Упепо", "Upepo", None, "ветер"),
    "HD 40307 f": ("Mvua", "Мвуа", "Mvua", None, "дождь: газовые платформы обеих общин"),
    "HD 40307 k": ("Uhuru", "Ухуру", "Uhuru", None, "свобода: столица Маланд и Небесной Твердыни"),
    "HD 40307 K1": ("Theluji", "Телуджи", "Theluji", None, "снег"),
    "HD 40307 K2": ("Shamba", "Шамба", "Shamba", None, "поле: агрокольца «Житница»"),
    "HD 40307 K3": ("Kisima", "Кисима", "Kisima", None, "колодец: поселение"),
})

# ------------------------------------------------------------------ 16 HD 69830: Selva Verde
S[16] = dict(lang="pt", named_by="AMAZONIA-1 (Сельва-Верде)",
             theme="Амазония: реки бассейна — планеты, боги тупи — гигант и его луны, лесные духи — ледяные карлики",
             star=("Guaraci", "Гуараси", "Guaraci", None, "солнце-бог тупи-гуарани"),
             bodies={
    "HD 69830 e": ("Xingu", "Шингу", "Xingu", None, "река Шингу"),
    "HD 69830 f": ("Tapajós", "Тапажос", "Tapajos", None, "река Тапажос"),
    "HD 69830 g": ("Madeira", "Мадейра", "Madeira", None, "река Мадейра"),
    "HD 69830 b": ("Boitatá", "Боитата", "Boitata", None, "огненный змей: мир в 3600 K"),
    "HD 69830 h": ("Purus", "Пурус", "Purus", None, "река Пурус"),
    "HD 69830 c": ("Juruá", "Журуа", "Jurua", None, "река Журуа: газовые платформы"),
    "HD 69830 i": ("Japurá", "Жапура", "Japura", None, "река Жапура"),
    "HD 69830 j": ("Tocantins", "Токантинс", "Tocantins", None, "река Токантинс"),
    "HD 69830 d": ("Rio Negro", "Риу-Негру", "Rio Negro", None, "Чёрная река"),
    "HD 69830 k": ("Tupã", "Тупан", "Tupa", None, "бог грома: газовый гигант с кольцами"),
    "HD 69830 k I": ("Jaci", "Жаси", "Jaci", None, "луна, жена Гуараси"),
    "HD 69830 k II": ("Rudá", "Руда", "Ruda", None, "бог любви"),
    "HD 69830 k III": ("Iara", "Иара", "Iara", None, "водная дева"),
    "HD 69830 l": ("Amazonas", "Амазонас", "Amazonas", None, "Амазонка: столица Сельва-Верде"),
    "HD 69830 l I": ("Boto", "Бото", "Boto", None, "речной дельфин-оборотень"),
    "HD 69830 A1": ("Saci", "Саси", "Saci", None, "одноногий проказник"),
    "HD 69830 K2": ("Mapinguari", "Мапингуари", "Mapinguari", None, "лесной великан: поселение"),
    "HD 69830 K3": ("Caipora", "Кайпора", "Caipora", None, "хранительница зверей"),
    "HD 69830 K4": ("Curupira", "Курупира", "Curupira", None, "страж леса с вывернутыми ступнями: поселение"),
})

# ------------------------------------------------------------------ 22 Delta Pavonis: Karma
S[22] = dict(lang="hi", named_by="DHARMA-1 (Новая Карма)",
             theme="звезда созвездия Павлина — Маюра, павлин; планеты — боги ведийского пантеона, ледяные тела — их ваханы",
             star=("मयूर", "Маюра", "Mayura", "Mayūr", "павлин: колонисты перевели созвездие Павлина"),
             bodies={
    "Delta Pavonis b": ("अग्नि", "Агни", "Agni", "Agni", "огонь"),
    "Delta Pavonis c": ("ज्वाला", "Джвала", "Jvala", "Jvālā", "пламя"),
    "Delta Pavonis d": ("तेजस्", "Теджас", "Tejas", "Tejas", "сияние, жар"),
    "Delta Pavonis e": ("कुण्डल", "Кундала", "Kundala", "Kuṇḍala", "серьга-кольцо: планета с кольцами"),
    "Delta Pavonis f": ("मरुत्", "Марут", "Marut", "Marut", "буревые божества"),
    "Delta Pavonis g": ("मित्र", "Митра", "Mitra", "Mitra", "бог договора"),
    "Delta Pavonis h": ("पृथ्वी", "Притхви", "Prithvi", "Pṛthvī", "земля-мать: умеренная малая планета"),
    "Delta Pavonis i": ("इन्द्र", "Индра", "Indra", "Indra", "громовержец: суперземля в бурях 160 бар"),
    "Delta Pavonis j": ("सोम", "Сома", "Soma", "Soma", "напиток бессмертия, луна: поселение"),
    "Delta Pavonis k": ("कैलास", "Кайласа", "Kailasa", "Kailāsa", "священная гора: столица Новой и Свободной Кармы"),
    "Delta Pavonis K1": ("नन्दी", "Нанди", "Nandi", "Nandī", "бык Шивы: поселение"),
    "Delta Pavonis K2": ("गरुड", "Гаруда", "Garuda", "Garuḍa", "птица Вишну"),
    "Delta Pavonis K3": ("हंस", "Хамса", "Hamsa", "Haṃsa", "лебедь Сарасвати"),
})

# ------------------------------------------------------------------ 23 Gliese 892 (HD 219134): Kern mines
S[23] = dict(lang="en", named_by="Проспекторат Керн, стратегические рудники, 2880",
             theme="горняцкий словарь Керна: плавка у звезды, жилы и забои у гиганта",
             star=("Bonanza", "Бонанза", "Bonanza", None, "богатая жила: здесь Керн нашёл платиноиды"),
             bodies={
    "Gliese 892 e": ("Slag", "Шлак", "Slag", None, "шлак"),
    "Gliese 892 i": ("Crucible", "Тигель", "Crucible", None, "тигель"),
    "HD 219134 b": ("Bessemer", "Бессемер", "Bessemer", None, "конвертер Бессемера: суперземля"),
    "Gliese 892 j": ("Smelter", "Плавильня", "Smelter", None, "плавильня"),
    "HD 219134 c": ("Ingot", "Слиток", "Ingot", None, "слиток"),
    "Gliese 892 k": ("Assay", "Проба", "Assay", None, "проба руды"),
    "Gliese 892 l": ("Lode", "Жила", "Lode", None, "рудная жила"),
    "HD 219134 d": ("Bellows", "Мехи", "Bellows", None, "кузнечные мехи: газовый субнептун"),
    "Gliese 892 A1": ("Nugget", "Самородок", "Nugget", None, "самородок: поселение"),
    "HD 219134 h": ("Motherlode", "Главная Жила", "Motherlode", None, "главная жила: газовый гигант с кольцами"),
    "HD 219134 h I": ("Pickaxe", "Кайло", "Pickaxe", None, "кайло: поселение"),
    "HD 219134 h II": ("Bedrock", "Коренная", "Bedrock", None, "коренная порода: столица рудников"),
})

# ------------------------------------------------------------------ 25 Gliese 357: Cognium frontier (falcons)
S[25] = dict(lang="en", named_by="фронтир Когниума у маяка (2860): Управление маяков «Сычёвник» называет свои миры птицами",
             theme="соколы и ястребы: открытый фронтир хищных птиц",
             star=("Kestrel", "Пустельга", "Kestrel", None, "пустельга, зависающая над полем"),
             bodies={
    "Gliese 357 e": ("Merlin", "Дербник", "Merlin", None, "дербник"),
    "Gliese 357 f": ("Hobby", "Чеглок", "Hobby", None, "чеглок"),
    "Gliese 357 g": ("Peregrine", "Сапсан", "Peregrine", None, "сапсан"),
    "Gliese 357 h": ("Saker", "Балобан", "Saker", None, "балобан"),
    "GJ 357 b": ("Goshawk", "Ястреб", "Goshawk", None, "ястреб-тетеревятник"),
    "Gliese 357 i": ("Lanner", "Ланнер", "Lanner", None, "средиземноморский сокол"),
    "GJ 357 c": ("Harrier", "Лунь", "Harrier", None, "лунь: плотная атмосфера"),
    "GJ 357 d": ("Osprey", "Скопа", "Osprey", None, "скопа: газовый субнептун"),
    "Gliese 357 j": ("Buzzard", "Канюк", "Buzzard", None, "канюк"),
    "Gliese 357 k": ("Gyrfalcon", "Кречет", "Gyrfalcon", None, "кречет: столица фронтира"),
    "Gliese 357 l": ("Kite", "Коршун", "Kite", None, "коршун"),
    "Gliese 357 m": ("Eagle", "Беркут", "Eagle", None, "беркут: поселение"),
    "Gliese 357 m I": ("Eaglet", "Орлёнок", "Eaglet", None, "птенец беркута"),
    "Gliese 357 n": ("Sparrowhawk", "Перепелятник", "Sparrowhawk", None, "перепелятник: поселение"),
})

# ------------------------------------------------------------------ 30 Epsilon Indi: contested helium field
S[30] = dict(lang="zh-Hans", named_by="Гелиевое поле (Гармония, Тяньхэ, Когниум, 2778); имена дали первые операторы — Цветущая Гармония",
             theme="звезда — Гелиевое поле; гигант и его луна — Небо и Земля, тела двух коричневых карликов — триграммы, пояса — пять стихий, Инь и Ян",
             star=("氦田", "Хайтянь", "Haitian", "Hàitián", "Гелиевое поле"),
             bodies={
    "eps Ind A b": ("乾", "Цянь", "Qian", "Qián", "Небо: сверхюпитер"),
    "eps Ind A b I": ("坤", "Кунь", "Kun", "Kūn", "Земля: водная луна"),
    "Epsilon Indi Ba c": ("震", "Чжэнь", "Zhen", "Zhèn", "Гром"),
    "Epsilon Indi Ba d": ("巽", "Сюнь", "Xun", "Xùn", "Ветер: поселение"),
    "Epsilon Indi Ba e": ("坎", "Кань", "Kan", "Kǎn", "Вода"),
    "Epsilon Indi Bb c": ("离", "Ли", "Li", "Lí", "Огонь, сияние: столица Гелиевого поля"),
    "Epsilon Indi Bb d": ("艮", "Гэнь", "Gen", "Gèn", "Гора"),
    "Epsilon Indi Bb e": ("兑", "Дуй", "Dui", "Duì", "Озеро: поселение"),
    "Epsilon Indi Bb f": ("水", "Шуй", "Shui", "Shuǐ", "вода"),
    "Epsilon Indi A A1": ("金", "Цзинь", "Jin", "Jīn", "металл"),
    "Epsilon Indi A K2": ("木", "Му", "Mu", "Mù", "дерево"),
    "Epsilon Indi A K3": ("火", "Хо", "Huo", "Huǒ", "огонь"),
    "Epsilon Indi A K4": ("土", "Ту", "Tu", "Tǔ", "земля"),
    "Epsilon Indi Bb (BAB) K1": ("阴", "Инь", "Yin", "Yīn", "тёмное начало"),
    "Epsilon Indi Bb (BAB) K2": ("阳", "Ян", "Yang", "Yáng", "светлое начало"),
})

# ------------------------------------------------------------------ 31 61 Cygni: Kern prospectorate
S[31] = dict(lang="en", named_by="RESOURCER-1 (Шахта Альфа, Проспекторат Керн)",
             theme="звезда — Бессель (первый измеренный параллакс, 1838); планеты — минералы, за которыми пришёл Керн",
             star=("Bessel", "Бессель", "Bessel", None, "в 1838 г. Бессель измерил по 61 Лебедя первое расстояние до звезды"),
             bodies={
    "61 Cygni A b": ("Cinnabar", "Киноварь", "Cinnabar", None, "киноварь"),
    "61 Cygni A c": ("Pyrite", "Пирит", "Pyrite", None, "пирит"),
    "61 Cygni A d": ("Hematite", "Гематит", "Hematite", None, "гематит"),
    "61 Cygni A e": ("Magnetite", "Магнетит", "Magnetite", None, "магнетит"),
    "61 Cygni A f": ("Galena", "Галенит", "Galena", None, "галенит: поселение"),
    "61 Cygni B b": ("Sulphur", "Сера", "Sulphur", None, "сера"),
    "61 Cygni B c": ("Realgar", "Реальгар", "Realgar", None, "реальгар"),
    "61 Cygni B d": ("Cuprite", "Куприт", "Cuprite", None, "куприт"),
    "61 Cygni B e": ("Bauxite", "Боксит", "Bauxite", None, "боксит"),
    "61 Cygni B f": ("Malachite", "Малахит", "Malachite", None, "малахит"),
    "61 Cygni B g": ("Bornite", "Борнит", "Bornite", None, "борнит"),
    "61 Cygni B h": ("Cassiterite", "Касситерит", "Cassiterite", None, "касситерит"),
    "61 Cygni B i": ("Wolframite", "Вольфрамит", "Wolframite", None, "вольфрамит: газовые платформы"),
    "61 Cygni B j": ("Quartz", "Кварц", "Quartz", None, "кварц: столица Керна, Шахта Альфа"),
    "61 Cygni B k": ("Mica", "Слюда", "Mica", None, "слюда: поселение"),
})

# ------------------------------------------------------------------ 37 HD 114783: Third Hellas (Odyssey)
S[37] = dict(lang="el", named_by="ODYSSEY-1 (Третья Эллада)",
             theme="«Одиссея»: звезда — Гомер, планеты — остановки Одиссея, второй гигант — Схерия, откуда его довезли домой, столица — Итака",
             star=("Όμηρος", "Гомер", "Homer", "Ómiros", "ODYSSEY-1 назвала звезду именем того, кто рассказал о её герое"),
             bodies={
    "HD 114783 d": ("Τροία", "Троя", "Troy", "Troía", "начало пути"),
    "HD 114783 e": ("Ίσμαρος", "Исмар", "Ismaros", "Ísmaros", "город киконов"),
    "HD 114783 f": ("Λωτοφάγοι", "Лотофаги", "Lotophagi", "Lotofágoi", "страна поедателей лотоса"),
    "HD 114783 g": ("Κύκλωψ", "Киклоп", "Cyclops", "Kýklops", "остров Полифема"),
    "HD 114783 h": ("Αιολία", "Эолия", "Aeolia", "Aiolía", "остров ветров"),
    "HD 114783 i": ("Λαιστρυγόνες", "Лестригония", "Laestrygonia", "Laistrygónes", "страна великанов-людоедов"),
    "HD 114783 j": ("Αιαία", "Ээя", "Aeaea", "Aiaía", "остров Кирки: душная атмосфера"),
    "HD 114783 b": ("Ποσειδών", "Посейдон", "Poseidon", "Poseidón", "бог, мешавший Одиссею вернуться: газовый гигант"),
    "HD 114783 b I": ("Σειρήνες", "Сирены", "Sirens", "Seirínes", "сирены"),
    "HD 114783 c": ("Σχερία", "Схерия", "Scheria", "Schería", "остров феаков, откуда Одиссея доставили домой: гигант Третьей Эллады"),
    "HD 114783 c I": ("Σκύλλα", "Сцилла", "Scylla", "Skýlla", "Сцилла"),
    "HD 114783 c II": ("Χάρυβδις", "Харибда", "Charybdis", "Cháryvdis", "Харибда"),
    "HD 114783 c III": ("Ιθάκη", "Итака", "Ithaca", "Itháki", "дом: столица Третьей Эллады и лагерь «Кларион»"),
    "HD 114783 c IV": ("Πηνελόπη", "Пенелопа", "Penelope", "Pinelópi", "та, что ждала"),
    "HD 114783 A1": ("Θρινακία", "Тринакия", "Thrinacia", "Thrinakía", "остров быков Гелиоса"),
    "HD 114783 K2": ("Τηλέμαχος", "Телемах", "Telemachus", "Tilémachos", "сын Одиссея: поселение"),
    "HD 114783 K3": ("Μέντωρ", "Ментор", "Mentor", "Méntor", "наставник Телемаха"),
    "HD 114783 K4": ("Εύμαιος", "Эвмей", "Eumaeus", "Évmaios", "верный свинопас: поселение"),
})

# ------------------------------------------------------------------ 38 Sigma2 UMa: Kern expedition
S[38] = dict(lang="en", named_by="экспедиция Керна (3060)",
             theme="звезда — «Эврика!», крик старателя; тела — золотые лихорадки Земли",
             star=("Eureka", "Эврика", "Eureka", None, "«нашёл!»: лучшие металлы каталога"),
             bodies={
    "Sigma² UMa A b": ("Deadwood", "Дедвуд", "Deadwood", None, "Дедвуд, Чёрные холмы"),
    "Sigma² UMa A c": ("Tombstone", "Тумстон", "Tombstone", None, "Тумстон, Аризона"),
    "Sigma² UMa A d": ("Bodie", "Боди", "Bodie", None, "Боди, Калифорния"),
    "Sigma² UMa A e": ("Comstock", "Комсток", "Comstock", None, "Комстокская жила, Невада"),
    "Sigma² UMa A f": ("Cripple Creek", "Крипл-Крик", "Cripple Creek", None, "Крипл-Крик, Колорадо: атмосфера 3300 бар"),
    "Sigma² UMa A g": ("Sutter", "Саттер", "Sutter", None, "лесопилка Саттера, начало калифорнийской лихорадки"),
    "Sigma² UMa A h": ("Klondike", "Клондайк", "Klondike", None, "Клондайк: сверхюпитер с кольцами"),
    "Sigma² UMa A h I": ("Dawson", "Доусон", "Dawson", None, "Доусон-Сити"),
    "Sigma² UMa A h II": ("Bendigo", "Бендиго", "Bendigo", None, "Бендиго, Австралия: поселение"),
    "Sigma² UMa A h III": ("Ballarat", "Балларат", "Ballarat", None, "Балларат, Австралия: поселение"),
    "Sigma² UMa A h IV": ("Eldorado", "Эльдорадо", "Eldorado", None, "Эльдорадо: база экспедиции"),
    "Sigma² UMa A A1": ("Nome", "Ном", "Nome", None, "Ном, Аляска"),
    "Sigma² UMa C b": ("Kalgoorlie", "Калгурли", "Kalgoorlie", None, "Калгурли, Западная Австралия"),
    "Sigma² UMa C c": ("Coolgardie", "Кулгарди", "Coolgardie", None, "Кулгарди"),
    "Sigma² UMa C d": ("Yukon", "Юкон", "Yukon", None, "река Юкон"),
})

# ------------------------------------------------------------------ 41 HD 104067: Cognium frontier (songbirds)
S[41] = dict(lang="en", named_by="фронтир Когниума у экспедиционного маяка (3050), птичья серия «Сычёвника»",
             theme="певчие птицы: лучшая агро-земля глубокого куста",
             star=("Lark", "Жаворонок", "Lark", None, "жаворонок над полем"),
             bodies={
    "HD 104067 d": ("Firecrest", "Королёк", "Firecrest", None, "красноголовый королёк"),
    "HD 104067 e": ("Wren", "Крапивник", "Wren", None, "крапивник"),
    "HD 104067 f": ("Finch", "Зяблик", "Finch", None, "зяблик"),
    "HD 104067 c": ("Starling", "Скворец", "Starling", None, "скворец: суперземля в 3600 K"),
    "HD 104067 b": ("Nightingale", "Соловей", "Nightingale", None, "соловей: газовый гигант с кольцами"),
    "HD 104067 g": ("Robin", "Малиновка", "Robin", None, "малиновка: столица фронтира"),
    "HD 104067 h": ("Thrush", "Дрозд", "Thrush", None, "дрозд"),
    "HD 104067 i": ("Swallow", "Ласточка", "Swallow", None, "ласточка: поселение"),
    "HD 104067 j": ("Bunting", "Овсянка", "Bunting", None, "овсянка: поселение"),
    "HD 104067 K1": ("Siskin", "Чиж", "Siskin", None, "чиж"),
    "HD 104067 K2": ("Waxwing", "Свиристель", "Waxwing", None, "свиристель"),
    "HD 104067 K3": ("Redpoll", "Чечётка", "Redpoll", None, "чечётка"),
})

# ------------------------------------------------------------------ 45 83 Leonis: New Thebaid (+ Golden City)
S[45] = dict(lang="el", named_by="NEW-JERUSALEM-1 (Ново-Фиваида)",
             theme="Фиваида — пустыня первых монахов: звезда — Фиваида, планеты — отцы и матери пустыни, столица — Скит",
             star=("Θηβαΐς", "Фиваида", "Thebaid", "Thivaḯs", "египетская пустыня, где родилось монашество"),
             bodies={
    "83 Leonis A d": ("Αντώνιος", "Антоний", "Antony", "Antónios", "Антоний Великий"),
    "83 Leonis A e": ("Παχώμιος", "Пахомий", "Pachomius", "Pachómios", "Пахомий Великий"),
    "83 Leonis A f": ("Μακάριος", "Макарий", "Macarius", "Makários", "Макарий Египетский"),
    "83 Leonis A g": ("Αρσένιος", "Арсений", "Arsenius", "Arsénios", "Арсений Великий"),
    "83 Leonis A h": ("Ποιμήν", "Пимен", "Poemen", "Poimín", "Пимен Великий"),
    "83 Leonis A i": ("Ευάγριος", "Евагрий", "Evagrius", "Evágrios", "Евагрий Понтийский"),
    "83 Leonis A j": ("Μωυσής", "Моисей", "Moses", "Moysís", "Моисей Мурин"),
    "83 Leonis A k": ("Σισώης", "Сисой", "Sisoes", "Sisóis", "Сисой Великий"),
    "83 Leonis A l": ("Αμμούν", "Аммун", "Amoun", "Ammoún", "Аммун Нитрийский: суперземля в 64 бар"),
    "83 Leonis A m": ("Παύλος", "Павел", "Paul", "Pávlos", "Павел Фивейский, первый пустынник: газовый субнептун с кольцами"),
    "83 Leonis A n": ("Ιλαρίων", "Иларион", "Hilarion", "Ilaríon", "Иларион Великий"),
    "83 Leonis B d": ("Μαρία", "Мария", "Mary", "María", "Мария Египетская"),
    "83 Leonis B e": ("Συγκλητική", "Синклитикия", "Syncletica", "Synklitikí", "Синклитикия Александрийская"),
    "83 Leonis B f": ("Κασσιανός", "Кассиан", "Cassian", "Kassianós", "Иоанн Кассиан"),
    "83 Leonis B g": ("Εφραίμ", "Ефрем", "Ephrem", "Efraím", "Ефрем Сирин"),
    "83 Leonis B h": ("Σκήτις", "Скит", "Scetis", "Skítis", "пустыня Скит: ковчег Ново-Фиваиды и «Золотой Город»"),
    "83 Leonis B h I": ("Κελλία", "Келлии", "Kellia", "Kellía", "Келлии, поселение отшельников"),
    "83 Leonis B i": ("Νιτρία", "Нитрия", "Nitria", "Nitría", "Нитрийская гора: поселение"),
    "83 Leonis B i I": ("Ισαάκ", "Исаак", "Isaac", "Isaák", "Исаак Сирин"),
    "83 Leonis B j": ("Αθανάσιος", "Афанасий", "Athanasius", "Athanásios", "Афанасий Великий: сверхюпитер с кольцами"),
    "83 Leonis B j I": ("Ιωάννης", "Иоанн", "John", "Ioánnis", "Иоанн Лествичник"),
    "83 Leonis B j II": ("Σιμεών", "Симеон", "Simeon", "Simeón", "Симеон Столпник: поселение"),
    "83 Leonis B j III": ("Δωρόθεος", "Дорофей", "Dorotheus", "Dorótheos", "авва Дорофей"),
    "83 Leonis B A1": ("Βαρσανούφιος", "Варсонофий", "Barsanuphius", "Varsanoúfios", "Варсонофий Великий"),
    "HD 99492 b": ("Σεραπίων", "Серапион", "Serapion", "Serapíon", "Серапион Синдонит"),
    "HD 99492 c": ("Ζωσιμάς", "Зосима", "Zosimas", "Zosimás", "Зосима Палестинский"),
})

# ------------------------------------------------------------------ 53 Gliese 674: Cognium frontier (owls)
S[53] = dict(lang="en", named_by="фронтир Когниума у маяка (2810), птичья серия «Сычёвника»",
             theme="совы: первый фронтир «Сычёвника»",
             star=("Strix", "Стрикс", "Strix", None, "сова-неясыть по-латыни: родовое имя сов"),
             bodies={
    "Gliese 674 c": ("Scops", "Сплюшка", "Scops", None, "сплюшка"),
    "Gliese 674 d": ("Pygmy", "Сычик", "Pygmy", None, "воробьиный сычик"),
    "GJ 674 b": ("Eagle-owl", "Филин", "Eagle-owl", None, "филин: суперземля в 530 бар"),
    "Gliese 674 e": ("Little Owl", "Домовый сыч", "Little Owl", None, "домовый сыч"),
    "Gliese 674 f": ("Barn Owl", "Сипуха", "Barn Owl", None, "сипуха"),
    "Gliese 674 g": ("Tawny", "Неясыть", "Tawny", None, "серая неясыть"),
    "Gliese 674 h": ("Snowy", "Белая сова", "Snowy", None, "белая сова: столица фронтира"),
    "Gliese 674 i": ("Boreal", "Мохноногий сыч", "Boreal", None, "мохноногий сыч"),
    "Gliese 674 j": ("Hawk-owl", "Ястребиная сова", "Hawk-owl", None, "ястребиная сова: поселение"),
    "Gliese 674 K2": ("Long-eared", "Ушастая сова", "Long-eared", None, "ушастая сова"),
    "Gliese 674 K1": ("Great Grey", "Бородатая неясыть", "Great Grey", None, "бородатая неясыть"),
    "Gliese 674 K3": ("Short-eared", "Болотная сова", "Short-eared", None, "болотная сова: поселение"),
})

# ------------------------------------------------------------------ 54 HD 85512: Cognium frontier (waterbirds)
S[54] = dict(lang="en", named_by="фронтир Когниума у маяка (2860), птичья серия «Сычёвника»",
             theme="водоплавающие и болотные птицы: фронтир на ледяных карликах",
             star=("Heron", "Цапля", "Heron", None, "цапля"),
             bodies={
    "HD 85512 b": ("Flamingo", "Фламинго", "Flamingo", None, "фламинго: самая горячая"),
    "HD 85512 c": ("Pelican", "Пеликан", "Pelican", None, "пеликан"),
    "HD 85512 d": ("Ibis", "Ибис", "Ibis", None, "ибис"),
    "HD 85512 e": ("Stork", "Аист", "Stork", None, "аист"),
    "HD 85512 f": ("Crane", "Журавль", "Crane", None, "журавль"),
    "HD 85512 g": ("Albatross", "Альбатрос", "Albatross", None, "альбатрос: суперземля в 54 бар"),
    "HD 85512 h": ("Petrel", "Буревестник", "Petrel", None, "буревестник"),
    "HD 85512 K3": ("Tern", "Крачка", "Tern", None, "крачка: поселение"),
    "HD 85512 K2": ("Loon", "Гагара", "Loon", None, "гагара: поселение"),
    "HD 85512 K1": ("Eider", "Гага", "Eider", None, "гага: столица фронтира"),
})

# ------------------------------------------------------------------ 57 HD 10360 / p Eridani: the relay bridge
S[57] = dict(lang="sw", named_by="Реле HD 10360 (Хомстед и Маланд, 2840): имена дали связисты Маланд",
             theme="звезда — Мост; у p Eridani A — стихии и гавани, у HD 10360 — время суток",
             star=("Daraja", "Дараджа", "Daraja", None, "мост: реле, связывающее Хомстед с HD 40307"),
             bodies={
    "p Eridani A b": ("Joto", "Джото", "Joto", None, "жара"),
    "p Eridani A c": ("Kaa", "Каа", "Kaa", None, "уголь"),
    "p Eridani A d": ("Jivu", "Дживу", "Jivu", None, "пепел"),
    "p Eridani A e": ("Mwako", "Мвако", "Mwako", None, "пыл, зарево"),
    "p Eridani A f": ("Radi", "Ради", "Radi", None, "раскат грома"),
    "p Eridani A g": ("Umeme", "Умеме", "Umeme", None, "молния"),
    "p Eridani A h": ("Ngurumo", "Нгурумо", "Ngurumo", None, "гром"),
    "p Eridani A i": ("Mchanga", "Мчанга", "Mchanga", None, "песок"),
    "p Eridani A j": ("Tanuri", "Танури", "Tanuri", None, "печь: атмосфера в 140 бар"),
    "p Eridani A k": ("Mnara", "Мнара", "Mnara", None, "маяк-башня: столица реле"),
    "p Eridani A l": ("Tufani", "Туфани", "Tufani", None, "буря: атмосфера в 520 бар"),
    "p Eridani A m": ("Wingu", "Вингу", "Wingu", None, "облако"),
    "p Eridani A n": ("Kivuko", "Кивуко", "Kivuko", None, "переправа: поселение"),
    "p Eridani A o": ("Bandari", "Бандари", "Bandari", None, "гавань: поселение"),
    "p Eridani (A) K1": ("Barafu", "Барафу", "Barafu", None, "лёд"),
    "p Eridani (A) K2": ("Sauti", "Сауте", "Sauti", None, "голос"),
    "p Eridani (A) K3": ("Ujumbe", "Уджумбе", "Ujumbe", None, "послание"),
    "HD 10360 b": ("Mwangaza", "Мвангаза", "Mwangaza", None, "сияние"),
    "HD 10360 c": ("Mchana", "Мчана", "Mchana", None, "день"),
    "HD 10360 d": ("Alfajiri", "Альфаджири", "Alfajiri", None, "рассвет"),
    "HD 10360 e": ("Jioni", "Джиони", "Jioni", None, "вечер"),
    "HD 10360 f": ("Machweo", "Мачвео", "Machweo", None, "закат"),
    "HD 10360 g": ("Usiku", "Усику", "Usiku", None, "ночь"),
    "HD 10360 K1": ("Kesho", "Кешо", "Kesho", None, "завтра"),
})

# ------------------------------------------------------------------ 60 Proxima Centauri: Svarog (Red Outpost)
S[60] = dict(lang="ru", named_by="SVAROG-1 (Красный Форпост)",
             theme="славянский пантеон: звезда — Сварог, небесный кузнец, планеты — его род",
             star=("Сварог", "Сварог", "Svarog", None, "небесный кузнец, отец огня: имя миссии SVAROG-1"),
             bodies={
    "Proxima Centauri c": ("Семаргл", "Семаргл", "Semargl", None, "огненный крылатый пёс"),
    "Proxima Centauri e": ("Хорс", "Хорс", "Khors", None, "солнечный диск"),
    "Proxima Centauri f": ("Ярило", "Ярило", "Yarilo", None, "весеннее солнце"),
    "Proxima Cen d": ("Дажьбог", "Дажьбог", "Dazhbog", None, "податель света, сын Сварога"),
    "Proxima Cen b": ("Мокошь", "Мокошь", "Mokosh", None, "мать-земля: поселение"),
    "Proxima Centauri g": ("Велес", "Велес", "Veles", None, "хозяин скота и подземных вод"),
    "Proxima Centauri h": ("Морозко", "Морозко", "Morozko", None, "мороз"),
    "Proxima Centauri i": ("Марена", "Марена", "Marena", None, "зима и смерть"),
    "Proxima Centauri j": ("Китеж", "Китеж", "Kitezh", None, "невидимый град: столица Красного Форпоста"),
    "Proxima Centauri k": ("Лада", "Лада", "Lada", None, "богиня лада и урожая: поселение и фермы"),
    "Proxima Centauri l": ("Стрибог", "Стрибог", "Stribog", None, "владыка ветров: газовые платформы"),
})

# system names that already exist in canon (local name given; exonyms completed here)
SYSTEM_EXONYMS = {
    3: dict(en="Ulven"),
    8: dict(ru="Тяньцан", en="Tiancang"),
    9: dict(ru="Тяньюань", en="Tianyuan"),
}

KIND = {"planet": "planet", "moon": "moon", "dwarf_planet": "dwarf_planet"}


def system_dir(sid):
    hits = glob.glob(os.path.join(SYSTEMS, f"{sid:03d}_*"))
    return hits[0] if hits else None


def bodies_of(sid):
    out = {}
    for p in glob.glob(os.path.join(system_dir(sid), "generated", "bodies", "*.json")):
        with open(p, encoding="utf-8") as f:
            b = json.load(f)
        out[b["designation"]] = b
    return out


def build():
    """-> (entities to set, problems)."""
    ents, bad = {}, []
    for sid, ser in sorted(S.items()):
        bodies = bodies_of(sid)
        seen_ru = {}
        for des, (native, ru, en, tr, meaning) in ser["bodies"].items():
            b = bodies.get(des)
            if not b:
                bad.append(f"system {sid}: no body '{des}'")
                continue
            if ru in seen_ru:
                bad.append(f"system {sid}: duplicate ru '{ru}' ({seen_ru[ru]}, {des})")
            seen_ru[ru] = des
            intl = dict(text=native, lang=ser["lang"])
            if tr:
                intl["translit"] = tr
            ents["body:" + b["id"]] = dict(
                type=KIND.get(b["kind"], b["kind"]), system=f"system:{sid}", status="PROPOSAL",
                catalog=des,
                names=dict(international=intl, exonyms=dict(ru=ru, en=en)),
                naming_history=dict(status="PROPOSAL", named_by=ser["named_by"],
                                    origin=f"серия системы ({DATE}): {ser['theme']}. {meaning}"))
        # every named-worthy body must be covered
        for des, b in bodies.items():
            if b["kind"] in KIND and b["physical"]["mass_earth"] >= MIN_MASS_EARTH and des not in ser["bodies"]:
                bad.append(f"system {sid}: unnamed body '{des}'")
        if ser.get("star"):
            native, ru, en, tr, meaning = ser["star"]
            loc = dict(text=native, lang=ser["lang"])
            if tr:
                loc["translit"] = tr
            ents[f"system:{sid}"] = dict(local=loc, exonyms=dict(ru=ru, en=en), meaning=meaning,
                                         named_by=ser["named_by"], theme=ser["theme"])
        else:
            ents[f"system:{sid}"] = dict(named_by=ser["named_by"], theme=ser["theme"])
    return ents, bad


def merge(N, ents):
    E = N["entities"]
    added = changed = 0
    for eid, e in ents.items():
        if eid.startswith("body:"):
            cur = E.get(eid)
            if cur and cur.get("status") == "CANON":
                continue
            added += cur is None
            changed += cur is not None and cur != e
            E[eid] = e
            continue
        sid = int(eid.split(":")[1])
        cur = E.get(eid)
        if cur is None:
            atlas = json.load(open(os.path.join(system_dir(sid), "system.json"), encoding="utf-8"))
            cur = E[eid] = dict(type="system", catalog=atlas["name"].split(" (")[0], verdict="KEEP")
            added += 1
        if cur.get("status") == "CANON":
            continue
        if "local" in e:
            if cur.get("local_by") and "local" not in cur:
                cur["local"] = e["local"]          # Lalande: the Greek name leads, Kapehu's Te Rā Hou stays in local_by
            elif "local" not in cur:
                cur["local"] = e["local"]
            cur.setdefault("exonyms", {}).update(e["exonyms"])
            if e.get("meaning"):
                cur["meaning"] = e["meaning"]
        ex = SYSTEM_EXONYMS.get(sid)
        if ex:
            cur.setdefault("exonyms", {}).update(ex)
        nh = cur.setdefault("naming_history", {})
        nh.setdefault("status", "PROPOSAL")
        nh.setdefault("named_by", e["named_by"])
        nh["body_series"] = f"{e['theme']} (PROPOSAL {DATE})"
        changed += 1
    return added, changed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    ents, bad = build()
    nb = sum(1 for k in ents if k.startswith("body:"))
    print(f"series: {len(S)} systems, {nb} bodies")
    for b in bad:
        print("  !", b)
    if bad:
        return 1
    if a.write:
        with open(NAMING, encoding="utf-8") as f:
            N = json.load(f)
        added, changed = merge(N, ents)
        with open(NAMING, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(N, ensure_ascii=False, indent=1) + "\n")
        print(f"naming.json: {added} added, {changed} updated")
    return 0


if __name__ == "__main__":
    sys.exit(main())
