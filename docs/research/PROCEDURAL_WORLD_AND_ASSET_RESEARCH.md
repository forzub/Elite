# Elite — Procedural Worlds & Procedural Assets Research

**Дата:** 2026-09-27  
**Статус:** research / architecture input  
**Цель:** собрать решения, которые реально пригодятся Elite для процедурных планет, поверхности, модульных кораблей и будущей транспортной инфраструктуры.

> Каталог third_party/procedural_reference содержит только исследовательский reference-код. Production-код Elite не должен зависеть от него напрямую: идеи и формулы переносим в собственную реализацию и покрываем своими тестами.

---

## 1. Главный вывод

Технически реально одновременно получить:

- одну и ту же планету при каждом посещении;
- спуск от орбиты до земли без загрузки готовой гигантской планеты;
- локальную детализацию только вокруг наблюдателя;
- поверхность без миллионов уникальных текстур;
- небольшой набор исходных материалов и моделей;
- большое визуальное разнообразие;
- воспроизводимость для save/load, multiplayer и regression tests.

Базовое представление мира:

~~~text
Planet =
    GeneratorVersion
  + PlanetSeed
  + PhysicalParameters
  + PersistentDeltas
~~~

Планета не обязана храниться как готовый mesh и огромная texture map. Она может быть вычислимой функцией:

~~~text
QueryPlanet(seed, position, lod) ->
    height/density
    biome
    material weights
    local feature candidates
    population candidates
~~~

При одинаковых seed, координате, параметрах и версии алгоритма результат должен быть одинаковым.

---

## 2. Главные технические источники

### 2.1 Innes McKendrick — Continuous World Generation in No Man's Sky

GDC 2017  
https://www.gdcvault.com/play/1024265/

Официальное описание доклада: voxel-based world generation -> polygonization -> texturing -> population -> simulation.

**Полезно Elite:**

- генерация мира вокруг игрока, а не хранение всей планеты;
- разное представление одного мира на разных расстояниях;
- LOD как часть world generation;
- physics/collision только там, где они нужны;
- population отдельным слоем поверх terrain;
- streaming как штатная часть генератора.

Предлагаемый контракт:

~~~cpp
struct PlanetSample {
    double density;
    double elevationMeters;
    BiomeId biome;
    MaterialWeights materials;
};

PlanetSample samplePlanet(
    const PlanetDescriptor& planet,
    const glm::dvec3& planetLocalPosition,
    PlanetLod lod);
~~~

Критическое свойство: samplePlanet не зависит от порядка загрузки чанков.

---

### 2.2 Sean Murray — Building Worlds Using Math(s)

GDC 2017  
https://www.gdcvault.com/play/1024514/Building-Worlds-Using

Доклад посвящён математической генерации terrain и тестированию фактически бесконечной среды маленькой командой.

Один из прямо упомянутых источников влияния:

**Ian Parberry — Modeling Real-World Terrain with Exponentially Distributed Noise**  
https://ianparberry.com/research/tobler/  
Source: https://github.com/Ian-Parberry/Tobler

Parberry показывает, что обычный Perlin/fBm часто даёт слишком равномерную "шероховатость". Модификация распределения градиентов позволяет получить больше спокойной поверхности и редкие сильные склоны.

Это хорошо совпадает с нашим визуальным направлением: крупные читаемые массы вместо равномерной игровой "наждачки".

Reference-код сохранён:

~~~text
third_party/procedural_reference/tobler/
~~~

Pinned upstream:

~~~text
Ian-Parberry/Tobler
bee7c4d77992db31a427e406a5f201246227cf04
~~~

Лицензия: GNU All-Permissive License; copyright notices сохранены.

---

### 2.3 Ian Parberry — Designer Worlds

**Designer Worlds: Procedural Generation of Infinite Terrain from Real-World Elevation Data**  
https://ianparberry.com/research/valuenoise/  
Source: https://github.com/Ian-Parberry/DesignerWorlds

Идея: статистику настоящего DEM можно превратить в профиль генератора и получать новые бесконечные участки с похожим географическим характером.

Для Elite это интереснее, чем одна формула "rocky planet". Возможны data-driven профили:

~~~text
OldErodedHighlands
YoungVolcanic
DryPlateau
Glacial
BrokenBadlands
~~~

Каждый профиль должен определять статистику высот, склонов, спектра масштабов и частоту крупных особенностей, а не готовую карту.

Reference-код:

~~~text
third_party/procedural_reference/designer_worlds/
~~~

Pinned upstream:

~~~text
Ian-Parberry/DesignerWorlds
f175201ad9cebd6c1153c9a51feb3c327724b432
~~~

---

### 2.4 Giliam de Carpentier — Scape

Overview:  
https://www.decarpentier.nl/scape

Procedural basics:  
https://www.decarpentier.nl/scape-procedural-basics

Procedural extensions:  
https://www.decarpentier.nl/scape-procedural-extensions

Scape полезен именно математикой terrain:

- fBm;
- billowy turbulence;
- ridged turbulence;
- noise derivatives;
- derivative-aware подавление мелких деталей;
- erosion-like octave coupling;
- Swiss turbulence;
- GPU-friendly local evaluation.

В Elite сохранены ключевые reference-файлы:

~~~text
third_party/procedural_reference/scape/noise.cgh
third_party/procedural_reference/scape/HeightfieldOperationGPUNoise.cpp
third_party/procedural_reference/scape/ProceduralLookupTextures.cpp
third_party/procedural_reference/scape/LICENSE
~~~

Pinned upstream port/fork:

~~~text
OGRECave/scape
9c95ec7b6d8372a7a2a4eb6b96dcb5f2219e2bf2
~~~

Оригинальный Scape 0.1.1 публиковался de Carpentier под Simplified BSD.

Что брать: не старый editor/render stack, а noise math, производные, композицию октав и GPU-подход.

---

### 2.5 Grant Duncan — How I Learned to Love Procedural Art

GDC 2015  
https://www.gdcvault.com/play/1021805/Art-Direction-Bootcamp-How-I

Hello Games announcement:  
https://www.nomanssky.com/2015/02/no-mans-sky-at-gdc/

Ключевой принцип: художник создаёт не каждый финальный объект, а **пространство допустимых вариантов** — части, правила, пропорции, материалы, палитры.

Для Elite генератор корабля должен быть constraint-driven:

~~~text
ShipArchetype
  hull
  nose
  bridge
  engine cluster
  cargo/service modules
  radiators
  antennas/sensors
  docking/landing equipment
  detail groups
~~~

Но наш модуль — не просто mesh:

~~~cpp
struct ShipModuleVariant {
    AssetId visual;
    CollisionDescriptor collision;
    double massKg;
    glm::dvec3 localCenterOfMass;
    std::vector<Socket> sockets;
    std::vector<ThrusterDescriptor> thrusters;
    std::vector<HardpointDescriptor> hardpoints;
    PowerDescriptor power;
    ThermalDescriptor thermal;
    CompatibilityRules compatibility;
};
~~~

Следствие: визуальный вид судна становится результатом его назначения и инженерии.

---

### 2.6 NoMansTerrain — community reverse engineering

https://github.com/gistya/NoMansTerrain

Pinned research commit:

~~~text
gistya/NoMansTerrain
b7042f1fdbe50184cbe023a484248748b81e2d6b
~~~

Полезен как карта гипотез: layers, Min/Max ranges, slope/altitude/ridge erosion, domain warping и другие параметры VoxelGeneratorSettings.

**Ограничение:** это не официальная спецификация Hello Games. Использовать как исследовательскую подсказку, не как контракт нашей архитектуры.

---

### 2.7 Space Engineers 2 — гибридная планета

Полезные официальные материалы:

- https://2.spaceengineersgame.com/mareks-dev-diary-august-21-2025/
- https://2.spaceengineersgame.com/mareks-dev-diary-october-9-2025/
- https://2.spaceengineersgame.com/space-engineers-2-vs2-planets-survival-foundations-live-now/

Особенно полезная идея: **не заставлять одну формулу решать всё**.

Базовая поверхность может оставаться heightfield/cube projection, а локальные объёмные особенности добавляются отдельно:

- overhangs;
- boulders;
- caves;
- local voxel formations.

Пещеры могут быть сделаны как качественные archetypes в Houdini, voxelized и затем процедурно размещены/вычтены из terrain по distribution rules.

Для первого Elite PlanetGenerator это сильный практический вариант:

~~~text
macro planet surface
    cube-sphere height field
+
local volumetric feature field near player
=
perceived full 3D planet
~~~

Это намного дешевле и проще, чем сразу делать всю планету универсальным 3D SDF.

---

## 3. Детерминизм: одна планета раз за разом

### 3.1 Не использовать один последовательный RNG

Плохо:

~~~cpp
rng.seed(planetSeed);
for (...) value = rng.next();
~~~

Любой новый вызов RNG изменит всю последующую генерацию.

Лучше coordinate-addressed hashing:

~~~text
systemSeed = Hash(universeSeed, systemCoordinate)
planetSeed = Hash(systemSeed, planetIndex)
cellSeed   = Hash(planetSeed, face, level, cellX, cellY, layerId)
objectSeed = Hash(cellSeed, objectClass, candidateIndex)
~~~

Тогда участок не зависит от того:

- какой patch загрузился первым;
- сколько потоков используется;
- куда игрок летал раньше;
- сколько соседних чанков уже в cache.

### 3.2 GeneratorVersion обязателен

~~~cpp
struct PlanetKey {
    uint32_t generatorVersion;
    uint64_t universeSeed;
    uint64_t systemId;
    uint32_t planetIndex;
};
~~~

Seed без версии недостаточен: изменение формулы terrain иначе изменит старые savegames.

### 3.3 Сохранять delta, а не всю планету

~~~text
procedural base
+
persistent simulation/player delta
=
current world
~~~

В delta попадают:

- базы;
- дороги;
- города;
- вырытые тоннели, если они постоянны;
- разрушенные уникальные объекты;
- экономическое состояние;
- долговременные изменения инфраструктуры.

---

## 4. Можно ли сесть на планету без миллионов текстур?

**Да. Это нормальная архитектура procedural terrain.**

Нужна небольшая библиотека повторяемых материалов:

~~~text
rock_01
rock_02
sand_01
soil_01
snow_01
ice_01
basalt_01
regolith_01
...
~~~

Shader вычисляет веса материалов:

~~~text
MaterialWeights = f(
    biome,
    altitude,
    slope,
    curvature,
    moisture,
    temperature,
    geologicalMask,
    localNoise)
~~~

### 4.1 Проекция

Для скал и произвольных уклонов практичны:

- triplanar / multi-projection;
- planet-local/world-space coordinates;
- несколько масштабов texture frequency;
- macro variation отдельно от micro detail.

### 4.2 Разные масштабы информации

~~~text
orbit:
    biome/albedo macro field

mountain distance:
    rock/snow/soil material groups

ground:
    tileable detail materials

near camera:
    normal/detail/procedural micro variation
~~~

То есть вместо гигантской уникальной surface texture хранятся формулы и небольшой набор material sets.

Материал в одной точке тоже должен быть детерминирован:

~~~cpp
MaterialWeights materialAt(PlanetKey key, glm::dvec3 position);
~~~

---

## 5. Предлагаемый PlanetGenerator v1

~~~text
PlanetDescriptor
  |
  +-- physical parameters
  |     radius
  |     gravity
  |     temperature
  |     atmosphere
  |     hydrology
  |     geology
  |
  +-- deterministic seeds
        |
        v
MacroShape
  continents / basins
        |
        v
ElevationField
  low-frequency structure
        |
        +--> MountainField
        |      ridged / exponential-gradient / derivative-aware
        |
        +--> Plateau / erosion profile
        |
        +--> Crater / tectonic features
        |
        v
BiomeField
  latitude + altitude + moisture + temperature + geology
        |
        v
MaterialField
  procedural weights
        |
        v
Local3DFeatures
  caves / arches / overhangs / boulders
        |
        v
PopulationLayer
  flora / rocks / debris / resources
        |
        v
InfrastructureLayer
  settlements / roads / ports / traffic nodes
~~~

Каждый слой независимо тестируется и не обязан знать renderer.

---

## 6. LOD и streaming

Первый практичный кандидат — cube-sphere + quadtree на каждой из шести граней.

~~~text
sphere
 <- cube projection
    6 faces
      -> quadtree patches
         -> adaptive LOD
~~~

Patch address:

~~~text
(face, level, x, y)
~~~

Преимущества:

- простой детерминированный PatchKey;
- нет полярной сингулярности lat/lon;
- хороший horizon/frustum culling;
- удобная загрузка соседей;
- patch можно удалить из cache и воспроизвести.

Runtime patch хранит только cache:

~~~cpp
struct PlanetPatchRuntime {
    PatchKey key;
    MeshHandle mesh;
    CollisionHandle collision;
    GpuMaterialData material;
    RuntimeState state;
};
~~~

---

## 7. Что проверять в сохранённом reference-коде

### Tobler / Perlin

~~~text
third_party/procedural_reference/tobler/perlin/perlin.cpp
~~~

Сделать сравнение:

~~~text
A: standard fBm
B: Parberry-inspired exponential-gradient noise
C: derivative-aware ridged terrain
~~~

Метрики:

- histogram slope;
- доля почти плоской поверхности;
- 95/99 percentile slope;
- число экстремумов;
- visual readability orbit/mid/ground;
- CPU ns/sample.

### Tobler / Amortized

~~~text
third_party/procedural_reference/tobler/amortized/
~~~

Особенно проверить:

- координатно воспроизводимый hash;
- infinite/local evaluation;
- стоимость sample;
- генерацию patch без предварительной генерации соседей.

### Designer Worlds

~~~text
third_party/procedural_reference/designer_worlds/valuenoise.cpp
~~~

Задача: превратить географический характер в data profile.

Будущий контракт:

~~~cpp
struct TerrainStatisticsProfile {
    Distribution elevation;
    Distribution slope;
    SpectrumProfile spectrum;
    FeatureDensity features;
};
~~~

### Scape

~~~text
third_party/procedural_reference/scape/noise.cgh
~~~

Изучить:

- derivative noise;
- ridged/billowy transforms;
- octave coupling;
- erosion-like shaping;
- перенос общей математики в современный C++ + GLSL.

---

## 8. Процедурные модели кораблей

Не “рандомно приклеить детали”, а идти от функции судна:

~~~text
role
 -> payload
 -> mass
 -> power
 -> propulsion
 -> required modules
 -> compatible hulls
 -> visual variants
~~~

Socket:

~~~cpp
struct ModuleSocket {
    SocketType type;
    glm::dmat4 transform;
    SizeClass sizeClass;
    std::vector<Tag> requiredTags;
    std::vector<Tag> forbiddenTags;
};
~~~

Алгоритм:

~~~text
1. выбрать archetype;
2. сформировать functional requirements;
3. выбрать core hull;
4. раскрыть sockets;
5. подобрать совместимые modules;
6. пересчитать mass/CoM/power/thermal/thrust;
7. отклонить физически плохой вариант;
8. выбрать cosmetic variants и palette;
9. построить collision + render instance.
~~~

Небольшая библиотека уже даёт огромное пространство комбинаций:

~~~text
8 hulls
5 noses
6 engine groups
8 cargo/service modules
5 bridges
8 detail groups
6 palettes
~~~

---

## 9. Самая интересная связь: terrain -> инфраструктура

Это может стать более важной особенностью Elite, чем сама procedural planet.

~~~text
terrain
 -> slope/water/hazards/resources
 -> settlement suitability
 -> industry/resource nodes
 -> route cost field
 -> road/rail/air/space transport graph
 -> settlements/ports/stations
 -> traffic demand
~~~

Причинная цепочка:

~~~text
geology
 -> resources
 -> industry
 -> settlements
 -> infrastructure
 -> traffic
 -> trade/conflict/military value
~~~

То есть дороги и потоки транспорта появляются по причине, а не как декоративный noise layer.

---

## 10. Что не делать

1. Не строить всё одной giant noise formula.
2. Не хранить планету одной огромной texture/mesh.
3. Не использовать global sequential RNG для identity мира.
4. Не забивать всю поверхность одинаковым microdetail.
5. Не начинать сразу с full NMS/Star Citizen stack.
6. Не смешивать procedural base и persistent delta.
7. Не делать reference-код runtime dependency.
8. Не начинать города/флору/пещеры до доказанного orbital-to-ground LOD.

---

## 11. Первый эксперимент — Planet Lab

Предлагаемый каталог:

~~~text
tools/planet_lab/
~~~

### P0 — deterministic sphere

- cube-sphere;
- PlanetKey/PatchKey;
- adaptive quadtree;
- stable reproduction tests.

Acceptance:

~~~text
same PlanetKey + same PatchKey
=> same samples / same mesh checksum
~~~

### P1 — macro terrain

- continents/basins;
- 2-3 mountain algorithms;
- Parberry-inspired gradient profile;
- только debug coloring.

### P2 — orbital -> ground LOD

Камера:

~~~text
10000 km
1000 km
100 km
10 km
1 km
10 m
ground
~~~

Acceptance:

- нет дыр;
- нет явных seams;
- bounded CPU/GPU cost;
- bounded memory;
- deterministic patch regeneration.

### P3 — material system

Только:

~~~text
rock / soil / sand / snow / ice / basalt
~~~

Использовать:

- triplanar/multi-projection;
- altitude/slope/biome weights;
- macro/micro separation.

Acceptance: с орбиты и на земле поверхность читается без уникальной planet texture.

### P4 — local 3D features

- одна семья caves;
- одна семья overhangs;
- одна семья boulders;
- deterministic distribution.

### P5 — procedural ship prototype

Сгенерировать 1000 транспортных судов и проверить:

- разнообразие;
- валидность module compatibility;
- физические параметры;
- instancing/batching;
- GPU memory.

---

## 12. Наше железо как полезный лимит

Текущий dev GPU: Quadro RTX 3000 6 GB.

Для Planet Lab сразу писать telemetry:

~~~text
CPU generation ms/patch
GPU upload ms/patch
resident patch count
mesh memory
material/texture memory
collision memory
peak VRAM estimate
LOD transitions/sec
cache hit ratio
~~~

Ставка должна быть на:

- deterministic generation;
- instancing;
- shared materials;
- LOD;
- spatial streaming;
- procedural variation;
- controlled animation/NPR-oriented rendering.

Не на огромный набор уникальных 4K/8K assets.

---

## 13. Source lock

На дату исследования:

~~~text
Ian-Parberry/Tobler
bee7c4d77992db31a427e406a5f201246227cf04
https://github.com/Ian-Parberry/Tobler

Ian-Parberry/DesignerWorlds
f175201ad9cebd6c1153c9a51feb3c327724b432
https://github.com/Ian-Parberry/DesignerWorlds

OGRECave/scape
9c95ec7b6d8372a7a2a4eb6b96dcb5f2219e2bf2
https://github.com/OGRECave/scape

gistya/NoMansTerrain
b7042f1fdbe50184cbe023a484248748b81e2d6b
https://github.com/gistya/NoMansTerrain
~~~

Полные upstream checkout восстанавливаются скриптом:

~~~text
tools/research/fetch_procedural_references.ps1
~~~

---

## 14. Следующий инженерный документ

Перед кодом основного мира нужен:

~~~text
docs/research/PLANET_GENERATOR_ALGORITHM.md
~~~

С точными контрактами:

~~~text
PlanetDescriptor
PlanetKey
PatchKey
TerrainSample
BiomeSample
MaterialSample
PlanetPatchGenerator
PlanetLodPolicy
PlanetPatchCache
PersistentPlanetDelta
~~~

После этого — отдельный tools/planet_lab для сравнения алгоритмов на одном seed.

**Цель первого прототипа:**

> один seed -> одна стабильная планета -> орбита до земли -> небольшой memory/texture budget -> воспроизводимость -> возможность поверх неё строить биомы, инфраструктуру и симуляцию.
