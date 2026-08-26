# Elite Model Asset Editor — руководство пользователя

**Текущий этап:** Stage 2 — Native Geometry / Physics / Instancing  
**Формат результата:** `.elmodel`, binary format v2  
**Назначение:** подготовить модель полностью до состояния runtime asset, чтобы игра впоследствии не вычисляла на лету нормали, topology, видимые рёбра, collision, mass properties, joints и semantic attachment points.

> Главное правило редактора: **render geometry, topology, collision, physics и sockets — разные слои одного asset.** Они связаны общей иерархией Node, но не должны выводиться друг из друга в runtime.

---

## 1. Что представляет собой asset

Редактор не считает станцию или корабль одним большим mesh. Asset состоит из нескольких уровней.

### GeometryDefinition

`GeometryDefinition` — уникальная геометрия детали. В ней хранятся:

- indexed vertices/triangles;
- authored normals;
- UV;
- material assignment по треугольникам;
- исходные polygon IDs;
- topology/adjacency;
- список candidate edges;
- отдельные masks видимых рёбер для `Technical` и `Elite` renderer;
- LOD0/LOD1;
- surface mode.

Одна GeometryDefinition должна существовать **один раз**, даже если визуально одна и та же деталь повторена много раз.

### Node

`Node` — экземпляр детали в сборке. Он хранит:

- ссылку на GeometryDefinition (`G0`, `G1`, ...);
- parent node;
- local position;
- local rotation;
- transform pivot;
- joint/pivot/rotation axis;
- break metadata;
- rigid-body mass/inertia metadata.

Поэтому правильная станция должна стремиться к структуре вроде:

```text
Station
├─ Core                 -> Geometry G0
├─ Ring                 -> Geometry G1
├─ Habitat_A            -> Geometry G2
├─ Habitat_B            -> Geometry G2
└─ Habitat_C            -> Geometry G2
```

`Habitat_A/B/C` — три разных Node, но mesh `G2` хранится один раз.

### CollisionVolume

Collision отделена от render mesh. Поддерживаются compound-наборы из:

- Box / OBB;
- Sphere;
- Capsule (локальная ось капсулы `+Y`).

Один Node может иметь несколько collision primitives.

### Socket / attachment point

Socket — semantic точка относительно Node:

- camera;
- weapon muzzle;
- equipment mount;
- dock;
- main/RCS thruster;
- light;
- VFX;
- sensor;
- generic/custom point.

### Materials

Материалы имеют стабильные semantic IDs. Сейчас редактор их **показывает**, но полноценного material editing UI ещё нет.

---

# 2. Запуск и загрузка модели

Запуск:

```bash
./build/tools/model_asset_editor/EliteAssetEditor.exe
```

В верхнем левом списке выберите asset, например **Orbital Station**.

Редактор действует так:

1. если `src/assets/compiled/models/<asset>.elmodel` уже существует и версия поддерживается — читает binary;
2. если binary отсутствует — импортирует исходную assembly/OBJ;
3. если binary устарел — повторно импортирует source assembly;
4. после чтения строит viewport.

Во время операции поверх viewport отображается progress overlay:

```text
READING · READ / PARSE / TOPOLOGY
READING · ASSEMBLE
READING · LOAD VIEW
WRITING · SERIALIZE / WRITE
```

Нижний status bar сообщает итог:

- `READING` — чтение/импорт;
- `WRITING` — запись;
- `IDLE` — операция закончена;
- `ERROR` — операция завершилась ошибкой.

Также там отображаются путь и размер binary-файла.

---

# 3. Управление viewport

| Действие | Управление |
|---|---|
| Выбрать submodel/Node | левый клик по модели |
| Orbit | drag левой кнопкой |
| Pan | drag правой кнопкой |
| Zoom | колесо мыши |
| Подогнать камеру | `Fit` |
| Вернуть всё скрытое | `Show all` |

Выбранный Node подсвечивается голубым.

Сетка сейчас является только пространственным ориентиром.

---

# 4. Верхняя панель

## Asset selector

Выбор текущего объекта из существующего registry.

Точка `●` рядом с asset в списке означает, что для него уже существует compiled `.elmodel`.

## `Reimport source`

Полностью выбрасывает текущие in-memory/compiled edits и заново строит asset из source assembly/OBJ.

Используйте это как **жёсткий откат к исходнику**.

> В текущей версии **Undo/Redo нет**. Перед рискованными массовыми операциями разумно сохранить рабочее состояние или иметь копию `.elmodel` вне редактора.

## `Save binary`

Записывает текущий asset в:

```text
src/assets/compiled/models/<asset>.elmodel
```

После успешной записи status bar показывает `Saved ...`.

## `Fit`

Подгоняет камеру по размеру всей видимой сборки.

## `Show all`

Снимает viewport-only Hide/Isolate.

## `Delete unused geometry`

Удаляет GeometryDefinition, на которые больше не ссылается ни один Node.

Типичный сценарий:

1. заменить три дублирующихся geometry одним shared geometry;
2. визуально проверить результат;
3. только потом выполнить `Delete unused geometry`.

До проверки нажимать эту кнопку не стоит.

## `Blender axes → Game`

Однократное преобразование source basis:

```text
Blender model:
    +X right
    +Z up
    -Y forward

Game canonical:
    +X right
    +Y up
    -Z forward
```

Преобразуются:

- geometry;
- normals;
- winding;
- nodes;
- pivots;
- joints;
- collision;
- sockets;
- inertia tensors.

### Важно

**Для уже существующих игровых assembly, включая нынешнюю Orbital Station, кнопку обычно нажимать НЕ надо.** Они импортируются как `game_current` и уже считаются находящимися в game basis.

Используйте `Blender axes → Game` только для source asset, который действительно был импортирован как Blender-oriented model и ещё не был canonicalized.

Повторная basis conversion запрещена; для отмены нужен `Reimport source`.

---

# 5. Дерево Assembly

Правая панель `Assembly` показывает все Nodes.

Строка имеет вид примерно:

```text
● ▰ habitat_s1       G4     ISO
```

где:

- `●/○` — видимость Node в viewport;
- `▰` — Node имеет geometry;
- `G4` — индекс GeometryDefinition;
- `ISO` — изолировать Node.

Иерархическая вложенность отображается отступом.

## Hide

Скрывает Node только в viewport. Asset не меняется.

## Isolate / `ISO`

Оставляет выбранную ветку отдельно для осмотра.

Повторный `ISO` снимает изоляцию.

`Show all` снимает все viewport-only скрытия/изоляцию.

---

# 6. Что делать со станцией в первую очередь

Для станции рекомендована следующая последовательность.

## Шаг 1 — ничего не редактировать, осмотреть структуру

1. Нажмите `Fit`.
2. По очереди кликайте крупные части станции.
3. Смотрите `ID / module` и badge `G#`.
4. Используйте `ISO`, чтобы понять, что относится к каждому Node.
5. Отдельно найдите:
   - центральный корпус;
   - вращающийся/кольцевой элемент;
   - три визуально одинаковых повторяющихся сегмента;
   - solar panels и прочие вспомогательные детали.

Цель первого прохода — понять **assembly topology**, а не сразу что-то удалить.

## Шаг 2 — найти реальные повторяющиеся geometry

Если три сегмента визуально одинаковы, выберите первый и запомните его GeometryDefinition, например `G4`.

Затем выберите второй сегмент и в поле:

```text
Geometry definition (shared by instances)
```

назначьте ему `G4`.

Повторите для третьего.

### Проверка обязательна

После каждой замены:

1. `Show all`;
2. осмотрите станцию со всех сторон;
3. убедитесь, что Node сохранил правильное положение и orientation;
4. отдельно `ISO` каждый instance.

Если при подмене geometry деталь сместилась/повернулась неправильно, **не удаляйте старую geometry**. Значит исходные OBJ не являются простой копией в локальной системе координат и сначала нужно исправить Node transform/pivot.

## Шаг 3 — удалить действительно лишние geometry

Только когда все instances визуально проверены:

```text
Delete unused geometry
```

Удалит старые geometry definitions, на которые Node больше не ссылаются.

---

# 7. Node Inspector

После выбора Node справа появляется редактор.

## Position

Local position относительно parent.

Изменение применяется кнопкой:

```text
Apply transform
```

## Rotation XYZ deg

Local Euler rotation в градусах.

## Transform pivot

Pivot, вокруг которого применяется transform rotation этого Node.

Не путать с `Joint pivot` ниже.

- **Transform pivot** — часть transform Node;
- **Joint pivot** — геометрия механического сочленения/вращения.

## Geometry definition

Позволяет перепривязать Node к существующей GeometryDefinition.

Это основной механизм превращения дублирующихся meshes в instances.

---

# 8. Surface mode: тонкая и объёмная геометрия

Для выбранной GeometryDefinition доступны:

### `Closed volume`

Обычная замкнутая объёмная модель.

### `Thin one-sided`

Листовая поверхность, рассчитанная на отображение с одной стороны.

### `Thin two-sided`

Листовая поверхность без физической геометрической толщины, отображаемая с обеих сторон.

Используйте для:

- обшивки;
- панелей;
- тонких экранов;
- солнечных панелей;
- пластин, где двойная shell geometry не нужна.

### Важно для instances

Surface mode принадлежит **GeometryDefinition**, а не отдельному Node.

Если `Habitat_A`, `Habitat_B`, `Habitat_C` используют одну `G4`, изменение Surface mode у одного instance меняет общую geometry для всех трёх. Это ожидаемое поведение.

---

# 9. Instances

## `Duplicate instance`

Создаёт новый Node с:

- той же GeometryDefinition;
- тем же parent;
- тем же transform;
- тем же Node metadata.

После создания задайте ему другой transform.

## `Break instance`

Создаёт **копию GeometryDefinition** и перепривязывает выбранный Node к ней.

Используйте, если один из ранее одинаковых элементов должен получить собственную geometry/topology/edge masks.

Пример:

```text
Habitat_A -> G4
Habitat_B -> G4
Habitat_C -> G4
```

после `Break instance` для C:

```text
Habitat_A -> G4
Habitat_B -> G4
Habitat_C -> G9_unique
```

## `Radial array`

Создаёт несколько Nodes вокруг оси `X/Y/Z`.

Параметры:

- количество **включая уже выбранный Node**;
- axis;
- total angle;
- pivot берётся из текущего Transform pivot Node.

Например:

```text
Count = 3
Axis = Y
Total angle = 360
```

создаёт instances с шагом `120°`.

### Текущее ограничение Stage 2

`Duplicate instance` и `Radial array` сейчас копируют **Node**, но не создают автоматически новые node-local записи `CollisionVolume` и `Socket`.

Поэтому:

- визуальную структуру станции уже можно правильно перевести в instances;
- **не надо пока вручную дублировать полный набор collision/sockets на каждый повторяющийся instance**;
- inheritance/instance expansion для этих payloads должен быть исправлен до production-конверсии каталога.

Это известное ограничение текущей версии, а не требование ручной работы.

---

# 10. Видимые рёбра

Рёбра редактируются **на GeometryDefinition**, поэтому все её instances получают один и тот же edge graph/mask.

Это именно то, что нужно для повторяющихся деталей.

## Вход в режим

1. выберите Node;
2. нажмите `Edit edges`;
3. выберите target:
   - `Elite edges`;
   - `Technical edges`.

На выбранной geometry появятся candidate edges.

Цвета:

- голубой — edge включён для выбранного style;
- тёмно-красный — edge выключен.

Левый клик по edge переключает его mask.

## Что оставлять в Technical

Обычно:

- silhouette-relevant конструктивные грани;
- сильные crease edges;
- важные стыки панелей;
- границы, необходимые для чтения формы.

Убирать:

- случайные triangulation diagonals;
- мелкую сеточную грязь;
- линии, которые не читаются на рабочей дистанции.

## Что оставлять в Elite

Elite mask может быть более художественным.

Цель — получить читаемую каркасную модель, а не вывести всю математическую триангуляцию.

Разрешено сознательно:

- оставить конструктивную линию ради силуэта;
- убрать реальное ребро, если оно засоряет wireframe.

## `Normals`

Показывает выборку normals выбранной geometry зелёными линиями.

Используйте для обнаружения:

- перевёрнутых нормалей;
- странного сглаживания;
- hard/smooth проблем после импорта.

Полноценного normal editing UI пока нет.

---

# 11. Collision / Hit volumes

Переключатель `Hit volumes` показывает/скрывает collision primitives.

Выбрать collision можно:

- кликом по wireframe volume в viewport;
- кликом по записи в списке `Hit volumes`.

## `+ SHAPE`

Сначала выберите Node.

Редактор спросит:

1. ID;
2. тип `box / sphere / capsule`.

Новый volume создаётся в локальной системе выбранного Node.

## Collision inspector

### Box / OBB

Использует:

- Center;
- Rotation XYZ;
- Box half size.

Rotation делает обычный box ориентированным OBB.

### Sphere

Использует:

- Center;
- Radius.

### Capsule

Использует:

- Center;
- Rotation XYZ;
- Radius;
- Half Height.

Локальная продольная ось capsule — `+Y`.

## `Duplicate`

Копирует выбранный volume.

После этого меняйте position/rotation/size.

## `Delete`

Удаляет collision primitive.

## `+ RING`

Генератор radial capsule chain для колец/торов.

Параметры:

- число capsules;
- радиус кольца;
- радиус capsule;
- axis (`x/y/z`).

Используйте его для station ring вместо одного чудовищного AABB, который закрывает пустую дыру в центре станции.

### Практический порядок для кольца станции

1. `ISO` Node кольца.
2. Оставьте `Hit volumes` включённым.
3. Удалите/не используйте огромный ложный narrow-phase volume, если он действительно покрывает пустой центр.
4. `+ RING`.
5. Начните примерно с 12–24 capsules.
6. Подберите ring radius и capsule radius так, чтобы chain покрывал реальную конструкцию, а не воздух.
7. При необходимости отдельные capsules можно править индивидуально.

Большой asset AABB при этом может оставаться: он нужен как broad-phase bounds и не обязан повторять дырку станции.

---

# 12. Joint / ось вращения / отрыв

Каждый Node имеет `Joint / break attachment`.

## Type

### `Fixed`

Неподвижно относительно parent.

### `Revolute`

Вращается вокруг заданной локальной оси.

## Joint pivot

Точка механического вращения/сочленения.

## Rotation axis

Локальная нормализованная ось вращения.

Например:

```text
0 1 0
```

означает `+Y`.

## Default rate

Authoring default в градусах в секунду.

## Min / Max angle

Пределы вращения.

Для полного непрерывного кольца можно оставить широкий диапазон; runtime semantics непрерывного rotation будут уточняться позже.

## Breakable

Если включено, Node хранит authored structural limits:

- break force, N;
- break torque, Nm.

Это будущий контракт отрыва детали.

### Для станции

Для вращающегося кольца/секции сейчас важно прежде всего правильно задать:

- `Revolute`;
- joint pivot;
- rotation axis;
- default rate.

Числа разрушения можно пока не выдумывать без физической модели объекта.

---

# 13. Detached rigid body / масса и инерция

Эти данные нужны только если Node потенциально становится самостоятельным физическим телом после разрушения/отрыва.

## Mass mode

### `Disabled`

Не считать Node отдельным detachable rigid body.

Нормальный выбор для большинства декоративных/неотрываемых узлов на текущем этапе.

### `Auto from local collision`

Mass properties вычисляются из **collision volumes, принадлежащих этому Node**, а не из render mesh.

### `Manual`

Масса, COM и tensor задаются вручную.

Нужно для деталей, у которых реальная внутренняя масса сильно отличается от формы оболочки: реактор, заполненный оборудованием модуль и т.п.

## Density

Используется при `Estimate`.

## `Estimate`

Аналитически рассчитывает по local compound collision:

- mass;
- center of mass;
- inertia tensor;
- parallel-axis contributions.

Если у Node нет собственных enabled collision volumes, операция вернёт ошибку.

## Center of mass

Локальный центр масс Node.

## Inertia diagonal

```text
Ixx / Iyy / Izz
```

## Products

```text
Ixy / Ixz / Iyz
```

### Рекомендуемый порядок

Collision сначала, physics потом:

```text
geometry
    ↓
collision
    ↓
Estimate mass properties
    ↓
при необходимости Manual correction
```

Не рассчитывайте mass properties до того, как collision приблизительно соответствует реальной форме детали.

---

# 14. Sockets / attachment points

Переключатель `Sockets` показывает/скрывает точки как зелёные сферы.

### Текущее управление

Существующие sockets выбираются **из списка справа**. Click-selection socket прямо во viewport в текущей версии ещё не реализован.

## `+ SOCKET`

Сначала выберите Node.

Введите:

- semantic ID;
- kind.

Примеры kind:

```text
camera
weapon_muzzle
equipment_mount
dock
main_thruster
rcs_thruster
light_point
light_spot
generic
```

Socket получает parent Node и локальный transform.

## Position / Rotation

Задаются численно в локальной системе parent Node.

Transform gizmos пока не реализованы.

## Light payload

Любой socket может иметь:

- `None`;
- `Point`;
- `Spot`.

Для света задаются:

- linear RGB;
- intensity;
- range;
- outer cone для Spot.

Не путать:

- emissive material — поверхность визуально светится;
- Light socket — реальный источник освещения.

Один светильник в будущем обычно использует оба механизма.

---

# 15. Materials

Панель `Materials` сейчас является **информационной**.

Она показывает:

```text
semantic material id      M#
```

и помечает emissive material как `emit`.

Native OBJ importer уже сохраняет:

- material assignment;
- stable semantic id;
- base/emissive properties;
- texture names;
- material seams.

Но полноценное редактирование этих свойств из UI ещё не реализовано.

Не тратьте сейчас время на ручную правку MTL ради финального Technical/Anime appearance: renderer и material authoring UI ещё впереди.

---

# 16. Что сейчас делать с Orbital Station — конкретный рабочий проход

Ниже — рекомендуемый проход **именно сейчас**, с учётом текущих возможностей и ограничений Stage 2.

## Pass A — структура

- [ ] Открыть `Orbital Station` и дождаться `IDLE`.
- [ ] `Fit`.
- [ ] Обойти все Nodes кликом/ISO.
- [ ] Записать для себя, какие `G#` соответствуют корпусу, кольцу и повторяющимся секциям.
- [ ] Найти три одинаковые визуальные секции.

## Pass B — instances

- [ ] Выбрать одну секцию как эталонную GeometryDefinition.
- [ ] Перепривязать вторую секцию к geometry первой.
- [ ] Проверить позицию/rotation со всех сторон.
- [ ] Повторить для третьей.
- [ ] Только после визуальной проверки удалить unused geometry.

**Не создавайте пока вручную копии collision/socket для каждого instance:** текущая версия ещё не умеет наследовать их автоматически.

## Pass C — surface semantics

- [ ] Для каждого уникального `G#` решить: `Closed`, `Thin one-sided`, `Thin two-sided`.
- [ ] Солнечные панели/листовую обшивку проверить как кандидатов `Thin`.
- [ ] Не делать двойную толщину только ради renderer.

## Pass D — topology / edges

Для каждой **уникальной GeometryDefinition**, а не каждого instance:

- [ ] `ISO` один Node.
- [ ] включить `Normals`, визуально проверить normals;
- [ ] `Edit edges` → `Technical edges`;
- [ ] убрать мусорные/триангуляционные линии;
- [ ] затем `Elite edges`;
- [ ] оставить художественно читаемый wireframe.

У трёх instances общей geometry это делается **один раз**.

## Pass E — station ring collision

- [ ] `ISO` кольцо.
- [ ] Проверить существующий collision.
- [ ] Если narrow-phase volume закрывает пустой центр, заменить его compound chain.
- [ ] `+ RING` → подобрать count/radius/capsule radius.
- [ ] Осмотреть сверху, сбоку, изнутри кольца.

Цель: корабль, пролетающий через пустой центр, не должен сталкиваться с невидимым кубом.

## Pass F — остальные collision volumes

- [ ] Корпус: OBB/несколько OBB.
- [ ] Длинные трубчатые элементы: Capsules.
- [ ] Круглые агрегаты: Sphere/Capsule.
- [ ] Не пытаться аппроксимировать каждый болт — collision должна быть достаточно точной для navigation/physical contact, но дешёвой.

## Pass G — joints

- [ ] Найти вращающийся Node.
- [ ] `Revolute`.
- [ ] Проверить pivot.
- [ ] Проверить axis.
- [ ] Проверить default rotation rate из текущего описания станции.
- [ ] Остальные неподвижные узлы оставить `Fixed`.

Motion preview пока отсутствует, поэтому значения проверяются численно/по известной геометрии. Это один из следующих editor features.

## Pass H — sockets

Сейчас:

- [ ] проверить, какие attachment points уже импортировались;
- [ ] убедиться, что они принадлежат правильным Nodes;
- [ ] проверить docks, если они уже представлены в imported data;
- [ ] проверить Light sockets, если есть.

**Не выполнять пока массовую ручную раскладку одинаковых sockets на повторяющиеся instances.** Сначала надо реализовать instance inheritance.

## Pass I — rigid-body physics

Только для реально потенциально detachable крупных частей:

- [ ] построить их collision;
- [ ] выбрать `Auto from local collision`;
- [ ] задать разумную authoring density;
- [ ] `Estimate`;
- [ ] визуально/логически оценить COM;
- [ ] для реакторов/тяжёлых внутренних агрегатов перейти в `Manual`, если shell approximation очевидно неверна.

Для Node, который никогда не будет отдельным телом, оставить `Disabled`.

## Pass J — сохранить

- [ ] `Save binary`.
- [ ] дождаться `WRITING → IDLE`.
- [ ] закрыть/снова открыть Orbital Station.
- [ ] убедиться, что теперь грузится compiled `.elmodel` (`●` в catalog).
- [ ] повторно проверить assembly/edges/collision/joints.

Это round-trip sanity check.

---

# 17. Что пока НЕ надо делать

До следующих editor stages не надо массово конвертировать весь парк моделей.

Причины:

1. нет Undo/Redo;
2. нет viewport transform gizmos;
3. нет automatic geometry duplicate hash detection;
4. instances пока не наследуют автоматически node-local collision/sockets;
5. нет motion preview для joints;
6. нет полного validation report;
7. materials пока в основном read-only;
8. нет полноценного LOD/proxy authoring;
9. нет arbitrary source file browser — catalog пока основан на существующем registry;
10. game runtime ещё не использует `.elmodel`.

**Orbital Station сейчас является acceptance/stress asset.** На ней надо найти проблемы самого editor/data model до того, как прогонять все корабли.

---

# 18. Что безопасно редактировать уже сейчас

Production-safe по модели данных:

- source basis metadata/canonical conversion;
- unique GeometryDefinitions;
- Node hierarchy/transforms/pivots;
- shared geometry instancing;
- surface semantics;
- authored Technical/Elite edge masks;
- compound Box/Sphere/Capsule collision;
- joints/rotation axes/pivots;
- rigid-body mass contract;
- semantic sockets;
- binary round-trip.

Требует следующего улучшения UI/semantics перед массовой работой:

- instance-local/inherited sockets and collision;
- transforms gizmos;
- automatic duplicate consolidation;
- materials authoring;
- validation;
- LOD authoring.

---

# 19. Как не потерять работу

Текущая версия не имеет Undo.

Практическое правило:

1. делайте один логический блок изменений;
2. проверяйте модель визуально;
3. `Save binary`;
4. перед экспериментом, который может массово изменить структуру, при необходимости скопируйте `.elmodel` как внешний backup;
5. `Reimport source` используйте только если действительно хотите выбросить текущую authored версию и начать от исходников.

Особенно осторожно с:

- `Blender axes → Game`;
- `Delete unused geometry`;
- `Delete node`;
- массовым `Radial array`;
- изменением shared GeometryDefinition — оно влияет на все instances.

---

# 20. Признак правильно подготовленной станции

Когда editor будет доведён до production-ready состояния, Orbital Station должна удовлетворять следующему контракту:

- одна повторяющаяся геометрия не хранится трижды;
- instances отличаются transform, а не копиями vertex data;
- source axes один раз приведены к game canonical basis;
- thin panels не имеют искусственной двойной толщины;
- Technical и Elite edge masks осмыслены вручную/автоматикой и не содержат triangulation noise;
- пустой центр кольца реально пуст для narrow-phase collision;
- rotating section имеет authored pivot + axis;
- docking/camera/light/equipment points являются sockets asset-а, а не координатами из позднего object descriptor;
- detachable крупные модули имеют collision + mass + COM + inertia;
- materials имеют стабильные semantic IDs;
- `.elmodel` сохраняется и загружается обратно без изменения authored структуры.

После этого такой asset можно считать готовым к будущему GPU renderer и runtime loader.

---

# 21. Следующие необходимые функции редактора после проверки станции

По текущему состоянию Stage 2 следующий приоритет должен быть таким:

1. **Instance payload inheritance** — collision/sockets/joints semantics для repeated geometry/nodes без ручного размножения.
2. **Transform gizmos** — Node / Collision / Socket / Joint pivot/axis.
3. **Automatic duplicate geometry detection** по canonical mesh hash.
4. **Full validation report**.
5. **Joint motion preview**.
6. **Material authoring UI**.
7. **LOD/proxy authoring**.
8. **Batch compiler** для всего asset catalog.

Только после этих пунктов имеет смысл массово перегонять все модели, а затем переключать `EliteGame` с OBJ/runtime preprocessing на `.elmodel`.
