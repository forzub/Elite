# Elite — Stellar System & Celestial Body Procedural Generator

**Дата:** 2026-09-27  
**Статус:** architecture / research input  
**Связанный документ:** docs/research/PROCEDURAL_WORLD_AND_ASSET_RESEARCH.md

## 0. Цель

Нужны два независимых, но связанных генератора.

1. **StellarSystemGenerator** — создаёт правдоподобную звёздную систему: звёзды, кратность, барицентры, планеты, луны, пояса, кольца, орбитальные элементы и возраст.
2. **CelestialBodyGenerator** — превращает каждую звезду/планету/луну/астероид в физически определённое тело: масса, радиус, состав, гравитация, температура, атмосфера, вращение, внутреннее тепло, геология и параметры будущей поверхности.

Ключевое правило:

> Runtime-генерация не должна при каждом посещении симулировать миллиарды лет формирования системы. Система строится из наблюдательно и физически мотивированных распределений, затем проходит дешёвые физические ограничения и stability gates. Полная N-body/эволюционная симуляция используется offline для калибровки и тестов.

Детерминированная идентичность:

~~~text
UniverseSeed
  -> GalacticCellSeed
    -> StellarSystemSeed
      -> StarSeed
      -> PlanetSeed
      -> MoonSeed
      -> BeltSeed
~~~

При одинаковых seed + generatorVersion + galactic context система должна воспроизводиться.

---

# 1. Полный причинный pipeline

~~~text
GALACTIC CONTEXT
 position in galaxy
 stellar population
 age
 metallicity
 local density
        |
        v
STELLAR SYSTEM GENERATOR
        |
        +-- stellar initial mass
        +-- stellar age/evolution
        +-- multiplicity hierarchy
        +-- present-day stellar state
        +-- protoplanetary disk history
        +-- planetary architecture
        +-- belts/comets
        +-- moons/rings
        +-- stability validation
        |
        v
CELESTIAL BODY GENERATOR
        |
        +-- bulk composition
        +-- radius/density
        +-- gravity/escape velocity
        +-- thermal state
        +-- atmosphere/volatile retention
        +-- rotation/obliquity/tides
        +-- radiogenic/tidal heating
        +-- geology class
        +-- surface-generation parameters
        |
        v
PLANET SURFACE GENERATOR
 terrain / caves / materials / ocean / ice / clouds
        |
        v
INFRASTRUCTURE GENERATOR
 resources -> settlements -> transport routes
~~~

Каждый слой имеет причину. Никаких независимых random generators, не знающих друг о друге.

---

# 2. StellarSystemGenerator API

~~~cpp
struct StellarSystemKey {
    uint32_t generatorVersion;
    uint64_t universeSeed;
    uint64_t systemId;
};

struct GalacticContext {
    glm::dvec3 galacticPosition;
    double formationTimeGyr;
    double ageGyr;
    double metallicityFeH;
    GalacticPopulation population;
    double localStellarDensity;
};

StellarSystemDescriptor generateStellarSystem(
    const StellarSystemKey& key,
    const GalacticContext& context);
~~~

GalacticContext обязателен: тонкий диск, толстый диск, балдж и гало не должны давать статистически одинаковые системы.

---

# 3. Звёздная масса и IMF

Основной источник:

**Pavel Kroupa — The Initial Mass Function and its Variation (2001)**  
https://arxiv.org/abs/astro-ph/0102155

IMF задаёт распределение начальных масс.

Но Elite должен генерировать нынешнее состояние, а не только рождение:

~~~text
initial stellar mass
 + age
 + metallicity
 -> stellar evolution
 -> present-day mass/radius/luminosity/temperature/state
~~~

Старая система не может содержать массивную голубую звезду главной последовательности, если её lifetime давно истёк.

---

# 4. Быстрая эволюция звезды

Основной источник:

**Hurley, Pols & Tout — Comprehensive analytic formulae for stellar evolution as a function of mass and metallicity (2000)**  
https://arxiv.org/abs/astro-ph/0001295  
https://astronomy.swin.edu.au/~jhurley/stellar.html

SSE предназначен именно для быстрого population synthesis. По initial mass, metallicity и age можно получать luminosity, radius, core mass и evolutionary stage от ZAMS до remnant.

Для Elite нужен аналогичный быстрый интерфейс:

~~~cpp
StellarState evolveStar(
    double initialMassSolar,
    double metallicityZ,
    double ageGyr);
~~~

Результат:

~~~cpp
struct StellarState {
    StellarEvolutionStage stage;
    double currentMassSolar;
    double radiusSolar;
    double luminositySolar;
    double effectiveTemperatureK;
    double metallicityZ;
    double surfaceGravity;
    double rotationPeriodDays;
    RemnantType remnant;
};
~~~

Для проверки main-sequence параметров полезна таблица Eric Mamajek:

https://github.com/emamajek/SpectralType

Она содержит Teff, luminosity, mass и фотометрические свойства по spectral type.

---

# 5. Двойные и кратные звёзды

Источники:

**Duchêne & Kraus — Stellar Multiplicity (2013)**  
https://arxiv.org/abs/1303.3028

**Moe & Di Stefano — Mind Your Ps and Qs (2017)**  
DOI: 10.3847/1538-4365/aa6fb6

Наблюдательные зависимости:

- multiplicity fraction зависит от primary mass;
- period/separation distribution зависит от primary mass;
- mass-ratio distribution зависит от класса системы;
- eccentricity связана с period;
- высокомассивные звёзды чаще кратные.

Плохой алгоритм:

~~~text
binaryChance = 0.2
companionMass = random
orbit = random
~~~

Правильная структура:

~~~text
primary mass
 -> multiplicity model
 -> companion count
 -> period distribution
 -> conditional mass ratio
 -> conditional eccentricity
 -> hierarchy stability gate
~~~

---

# 6. Барицентрическое дерево

Кратная система должна быть деревом орбит, а не списком объектов вокруг условного центра.

~~~text
SystemBarycenter
 |
 +-- Star A
 |
 +-- BinaryBarycenter BC
      |
      +-- Star B
      +-- Star C
~~~

Circumbinary planets естественно вращаются вокруг BinaryBarycenter.

~~~cpp
struct OrbitalNode {
    BodyId id;
    BodyKind kind;
    std::optional<BodyId> parent;
    KeplerianElements orbit;
    double gravitatingMassKg;
};
~~~

---

# 7. Stability gate для triple systems

Для hierarchical triple использовать Mardling-Aarseth-type criterion.

Useful modern reference:

https://academic.oup.com/mnras/article/516/3/4146/6694254

Проверяются:

- inner/outer semimajor axis;
- outer eccentricity;
- mass ratio;
- mutual inclination.

Runtime:

~~~text
generate hierarchy
 -> analytic stability gate
 -> fail => resample
~~~

Полный N-body каждого triple не нужен.

---

# 8. Протопланетный диск как скрытая причина

Планеты не генерируются независимо.

~~~cpp
struct ProtoplanetaryDiskDescriptor {
    double initialGasMassSolar;
    double solidMassEarth;
    double metallicity;
    double innerEdgeAu;
    double characteristicRadiusAu;
    double lifetimeMyr;
    double snowLineAu;
    double turbulenceAlpha;
};
~~~

Он может не существовать сегодня как игровой объект, но определяет planet architecture.

~~~text
stellar mass + metallicity + formation environment
 -> disk mass
 -> solid budget
 -> gas budget
 -> snow line
 -> migration regime
 -> disk lifetime
 -> planetary system
~~~

---

# 9. Bern Model как карта причин

**Emsenhuber et al. — The New Generation Planetary Population Synthesis, Bern global model (2021)**  
https://www.aanda.org/articles/aa/pdf/2021/12/aa38553-20.pdf

Модель связывает:

- gas disk evolution;
- planetesimals/solids;
- embryo growth;
- gas accretion;
- planetary internal structure;
- migration;
- N-body interaction;
- long-term evolution.

Elite не должен выполнять весь Bern Model в runtime.

Мы используем его causal graph и калибруем дешёвую генерацию по synthetic populations.

---

# 10. Наблюдательная калибровка

**NASA Exoplanet Archive**  
https://exoplanetarchive.ipac.caltech.edu/

Полезны:

- planetary systems;
- stellar hosts;
- periods;
- masses/radii;
- multiplicity;
- Kepler completeness/reliability products.

Нельзя просто брать histogram confirmed planets: он искажён selection effects.

Нужны intrinsic population models / forward models.

---

# 11. SysSim — reference для correlated systems

**ExoplanetsSysSim.jl**  
https://github.com/ExoJulia/ExoplanetsSysSim.jl

**SysSimExClusters**  
https://github.com/ExoJulia/SysSimExClusters

Clustered-model paper:  
https://academic.oup.com/mnras/article/490/4/4575/5613397

Очень полезная идея: планеты одной системы correlated.

~~~text
system
 -> number of planet clusters
 -> planets per cluster
 -> correlated periods
 -> correlated sizes/masses
 -> mutual inclination distribution
 -> eccentricity distribution
~~~

Pinned research commits:

~~~text
ExoJulia/ExoplanetsSysSim.jl
9a0793a9ababbc606b192e4063ba567c2f20d36c

ExoJulia/SysSimExClusters
c329140458499521ed2cb0900a83d6f7085eaacb
~~~

---

# 12. P-pop

**P-pop — Monte-Carlo synthetic exoplanet populations**  
https://github.com/kammerje/P-pop

Pinned:

~~~text
b2179f93952cf6a1e8c5845fdb040e8c98cf918b
~~~

License: MIT.

Полезен как пример быстрой population generation, в отличие от полного planet-formation solver.

---

# 13. Runtime planet architecture

Предлагаемая последовательность:

### A. System formation profile

~~~text
host mass
metallicity
age
stellar multiplicity
disk mass
disk lifetime
snow line
~~~

### B. Statistical architecture mode

Не игровые жёсткие классы, а latent statistical modes:

~~~text
compact rocky
compact mixed
cold giant + inner small planets
hot/warm giant disturbed system
wide giant system
M-dwarf compact system
binary-truncated system
circumbinary system
sparse/planet-poor system
~~~

### C. Correlated planet candidates

~~~text
cluster center in log(period)
planet count
mass/size scale
intra-cluster dispersion
mutual inclination
eccentricity
~~~

### D. Mass/radius correlation

Reference:

**Chen & Kipping — Probabilistic Forecasting of the Masses and Radii of Other Worlds**  
https://arxiv.org/abs/1603.08614

Использовать как empirical/probabilistic calibration, не как фундаментальный закон.

### E. Stability gates

- non-crossing orbits;
- pericenter/apocenter clearance;
- mutual Hill stability;
- binary forbidden zones;
- Roche/Hill constraints for moons.

---

# 14. Hill stability

**Gladman — Dynamics of Systems of Two Close Planets (1993)**  
https://www.sciencedirect.com/science/article/pii/S0019103583711693

Для близких почти circular coplanar пар есть аналитический Hill-stability condition.

Нам нужен API:

~~~cpp
bool passesPairStabilityGate(
    double hostMassKg,
    const PlanetOrbit& inner,
    const PlanetOrbit& outer);
~~~

Для multi-planet system использовать safety margin выше голого theoretical limit. Этот margin калибровать offline через N-body.

---

# 15. Планеты в binary systems

**Holman & Wiegert — Long-Term Stability of Planets in Binary Systems**  
https://arxiv.org/abs/astro-ph/9809315

Есть empirical critical boundaries для:

- S-type planets вокруг одной компоненты;
- P-type circumbinary planets.

Зависят от binary semimajor axis, eccentricity и mass ratio.

Pipeline:

~~~text
binary
 -> circumstellar stable zones
 -> circumbinary inner stable boundary
 -> disk truncation / planet zones
 -> planet architecture
~~~

---

# 16. REBOUND как offline truth model

**REBOUND**  
https://github.com/hannorein/rebound

Pinned:

~~~text
5a93ed17a90cd90a0ba87e095621e6dba3daba0a
~~~

License: GPL-3.0.

REBOUND поддерживает stars, planets, moons/rings/particles и integrators WHFast, IAS15, MERCURIUS и др.

Использование для Elite:

~~~text
generate 10000 Elite systems
 -> choose representative/edge cases
 -> integrate offline in REBOUND
 -> classify unstable systems
 -> tune cheap Elite stability gates
~~~

REBOUND не должен автоматически становиться runtime dependency из-за GPL и из-за лишней вычислительной стоимости.

---

# 17. CelestialBodyGenerator API

После SystemGenerator уже известно:

~~~text
planet mass
orbit
host star
age
formation region
migration class
~~~

Контракт:

~~~cpp
struct BodyFormationContext {
    double ageGyr;
    double hostLuminositySolar;
    double hostEffectiveTemperatureK;
    double incidentFluxEarth;
    double formationDistanceAu;
    double snowLineAtFormationAu;
    double systemMetallicity;
    bool migrated;
    MigrationHistoryClass migrationClass;
};

CelestialBodyDescriptor generateCelestialBody(
    const CelestialBodyKey& key,
    const BodySeedProperties& seed,
    const BodyFormationContext& context);
~~~

---

# 18. Bulk composition first, class later

Не начинать с random type = lava/ice/earth.

Сначала:

~~~cpp
struct BulkComposition {
    double ironCore;
    double silicateMantle;
    double waterIce;
    double otherVolatiles;
    double hHeEnvelope;
};
~~~

Formation distance + migration + mass + disk state определяют composition.

Потом class становится производной:

~~~text
rocky
iron-rich
water-rich
sub-Neptune
ice giant
gas giant
dwarf icy body
~~~

---

# 19. Mass -> radius -> density

Radius зависит от:

~~~text
mass
+ bulk composition
+ H/He envelope
+ age/thermal state
~~~

Derived:

~~~text
surface gravity = G M / R^2
escape velocity = sqrt(2 G M / R)
mean density = M / volume
~~~

Никаких независимых random mass и radius.

---

# 20. Thermal state

Минимальная причинная модель:

~~~text
stellar luminosity
orbital distance
albedo estimate
internal heat
atmosphere greenhouse
 -> equilibrium temperature
 -> surface temperature regime
~~~

Важно:

~~~text
equilibrium temperature != actual surface temperature
~~~

---

# 21. Habitable zone

Reference:

**Kopparapu et al. — Habitable Zones Around Main-Sequence Stars**  
https://arxiv.org/abs/1301.6674

HZ хранить как context, не как bool habitable.

~~~cpp
struct InsolationContext {
    double fluxEarthUnits;
    double conservativeHzPosition;
    double optimisticHzPosition;
};
~~~

Life/habitability — отдельный будущий слой.

---

# 22. Atmosphere model

Inputs:

- mass / escape velocity;
- temperature;
- volatile inventory;
- stellar XUV history;
- age;
- outgassing;
- loss/impact history;
- condensation/freeze-out.

Causal model:

~~~text
initial volatiles
 + retention
 - thermal/XUV escape
 + outgassing
 - sequestration/condensation
 = current atmosphere
~~~

~~~cpp
struct AtmosphereDescriptor {
    double surfacePressurePa;
    std::vector<GasFraction> composition;
    double greenhouseStrength;
    double scaleHeightMeters;
    double cloudPotential;
    AtmosphereOrigin origin;
};
~~~

---

# 23. VPLanet — reference для эволюции

**VPLanet**  
https://github.com/VirtualPlanetaryLaboratory/vplanet

Pinned:

~~~text
dd55da7e1ff063f0ea7048f91c9d2d97d6ba9a5d
~~~

License: MIT.

Модули VPLanet включают:

- stellar luminosity/XUV evolution;
- radiogenic heating;
- thermal interior;
- tidal evolution;
- spin/orbit evolution;
- atmospheric escape;
- N-body/dynamical physics.

Для Elite VPLanet — causal/reference model, а не runtime requirement.

Пример связи:

~~~text
close orbit
 -> tides
 -> spin evolution / tidal lock
 -> tidal heating
 -> interior/geology
 -> atmosphere/surface
~~~

---

# 24. Spin / obliquity / tides

Хранить:

~~~cpp
rotationPeriod
obliquity
spinAxis
tidalLockState
tidalHeatingFlux
~~~

Initial spin sample, затем evolutionary corrections.

Close planets/moons могут lock; eccentric close orbit может давать сильное tidal heating.

---

# 25. Moons

Moon generator запускается после planet descriptor.

Constraints:

~~~text
inside stable fraction of Hill sphere
outside Roche-disruption region
satellite budget conditioned on planet class
~~~

Latent origin modes:

~~~text
regular co-formed
giant-impact
captured irregular
fragment/ring-derived
~~~

Для giants: regular inner moons + possible irregular outer population.

---

# 26. Rings

Не random decoration.

Причины:

~~~text
Roche region
moon disruption/collision history
age
material
planet gravity
~~~

~~~cpp
struct RingSystemDescriptor {
    double innerRadiusMeters;
    double outerRadiusMeters;
    double opticalDepth;
    double iceFraction;
    double rockFraction;
    double planeInclination;
};
~~~

---

# 27. Asteroid/comet reservoirs

Не materialize миллиард объектов.

~~~cpp
struct SmallBodyReservoir {
    ReservoirType type;
    double totalMassEarth;
    RadialDistribution radial;
    SizeDistribution sizeDistribution;
    CompositionProfile composition;
};
~~~

Связи:

- snow line;
- giant planet migration;
- resonance gaps;
- system age;
- scattering history.

Конкретные minor bodies создаются только при необходимости.

---

# 28. Граница BodyGenerator -> SurfaceGenerator

CelestialBodyGenerator **не делает terrain mesh**.

Он выдаёт ограничения поверхности:

~~~text
radius
gravity
bulk composition
water inventory
atmosphere
thermal state
tectonic/volcanic state
impact history
glaciation potential
erosion potential
~~~

PlanetSurfaceGenerator уже строит:

~~~text
continents
mountains
craters
caves
materials
ice
rivers/oceans
~~~

Картинка — следствие physics descriptor.

---

# 29. Независимые deterministic streams

~~~text
Hash(SystemSeed, stars)
Hash(SystemSeed, multiplicity)
Hash(SystemSeed, disk)
Hash(SystemSeed, planet_architecture)

Hash(PlanetSeed, bulk)
Hash(PlanetSeed, atmosphere)
Hash(PlanetSeed, spin)
Hash(PlanetSeed, moons)
Hash(PlanetSeed, rings)
~~~

Добавление нового random draw в atmosphere не должно изменять moons.

---

# 30. Generator versioning

~~~cpp
struct UniverseGenerationVersions {
    uint32_t stellarSystem;
    uint32_t celestialBody;
    uint32_t planetSurface;
    uint32_t proceduralAssets;
};
~~~

Можно обновить surface generator, не меняя орбиты старой системы.

---

# 31. Runtime vs research validation

Runtime:

~~~text
seed
 -> empirical distributions
 -> analytic relations
 -> cheap stability gates
 -> descriptors
~~~

Research:

~~~text
large generated population
 -> REBOUND / VPLanet / analysis
 -> compare to NASA Archive / SysSim
 -> tune distributions and margins
~~~

---

# 32. Confidence/provenance

Полезно явно знать происхождение параметра:

~~~cpp
enum class ModelConfidence {
    PhysicalLaw,
    EmpiricalFit,
    PopulationModel,
    Heuristic
};
~~~

Примеры:

- Kepler orbit: PhysicalLaw
- mass-radius relation: EmpiricalFit
- planet multiplicity: PopulationModel
- speculative tectonic regime: Heuristic/Model.

---

# 33. Кодовая декомпозиция

~~~text
src/world/generation/
    SeedDerivation
    GeneratorVersion

    stellar/
        StellarPopulationModel
        StellarMassSampler
        StellarEvolutionModel
        StellarMultiplicityModel
        StellarHierarchyBuilder
        StellarSystemGenerator

    planetary/
        DiskPopulationModel
        PlanetArchitectureModel
        OrbitalStabilityGate
        BinaryPlanetStabilityGate
        SmallBodyReservoirGenerator

    body/
        BulkCompositionModel
        MassRadiusModel
        ThermalStateModel
        AtmosphereModel
        SpinTideModel
        MoonSystemGenerator
        RingSystemGenerator
        CelestialBodyGenerator
~~~

Scientific tools отдельно:

~~~text
tools/research/
    stellar_system_lab/
    body_population_lab/
    rebound_validation/
~~~

---

# 34. Stellar System Lab v1

Первый executable research tool:

~~~text
tools/stellar_system_lab/
~~~

Input:

~~~text
seed
galactic population
age
metallicity
count
~~~

Output:

- deterministic JSON;
- human-readable report;
- batch histograms/statistics;
- generation timing.

---

# 35. Acceptance tests StellarSystemGenerator

Determinism:

~~~text
same input -> same checksum
thread/order independent
~~~

Population:

- mass histogram соответствует выбранному population model;
- old systems не содержат impossible massive unevolved stars;
- multiplicity depends on primary mass;
- stellar hierarchy stable.

Planets:

- no orbit intersects star;
- binary forbidden regions respected;
- adjacent planets pass stability gate;
- correlated system architectures survive;
- generated distributions stay inside calibration envelopes.

Performance:

- benchmark 100k systems;
- measure systems/sec and allocations.

---

# 36. Acceptance tests CelestialBodyGenerator

Для каждого body:

~~~text
mass > 0
radius > 0
density finite/physically bounded
g consistent with GM/R^2
escape velocity consistent
bulk fractions sum to 1
atmospheric fractions sum to 1
moons inside stable region
rings ordered
temperatures finite
~~~

Population sanity:

- gas giants do not have terrestrial density distribution;
- hot small planets preferentially lose light envelopes;
- volatile-rich bodies are linked to formation outside snow line or migration;
- tidal heating appears where orbital configuration supports it;
- age changes thermal/geological state.

---

# 37. Source lock

As of 2026-09-27:

~~~text
hannorein/rebound
5a93ed17a90cd90a0ba87e095621e6dba3daba0a
GPL-3.0
https://github.com/hannorein/rebound

VirtualPlanetaryLaboratory/vplanet
dd55da7e1ff063f0ea7048f91c9d2d97d6ba9a5d
MIT
https://github.com/VirtualPlanetaryLaboratory/vplanet

kammerje/P-pop
b2179f93952cf6a1e8c5845fdb040e8c98cf918b
MIT
https://github.com/kammerje/P-pop

ExoJulia/ExoplanetsSysSim.jl
9a0793a9ababbc606b192e4063ba567c2f20d36c
https://github.com/ExoJulia/ExoplanetsSysSim.jl

ExoJulia/SysSimExClusters
c329140458499521ed2cb0900a83d6f7085eaacb
https://github.com/ExoJulia/SysSimExClusters

synthpop-galaxy/synthpop
bc170e053231e19965f6085e397d029e683b1ed5
GPL-3.0
https://github.com/synthpop-galaxy/synthpop
~~~

---

# 38. Galactic-scale reference: Synthpop

**Synthpop**  
https://github.com/synthpop-galaxy/synthpop

Модульный Galactic population synthesis framework:

- population density;
- IMF;
- age distribution;
- metallicity distribution;
- kinematics.

Это хороший reference для будущего слоя:

~~~text
GalaxyGenerator
 -> GalacticContext
 -> StellarSystemGenerator
~~~

---

# 39. Финальная архитектура

~~~text
GalaxyGenerator
 |
 v
GalacticContext
 |
 v
StellarSystemGenerator
 |
 v
CelestialBodyGenerator
 |
 v
PlanetSurfaceGenerator
 |
 v
InfrastructureGenerator
 |
 v
Simulation
~~~

Главная идея: **не генерировать картинку, а генерировать причинную историю объекта настолько глубоко, насколько это полезно игре.**

---

# 40. Рекомендуемый порядок реализации

1. SeedDerivation + versioning.
2. Single-star system.
3. Kroupa-like stellar initial mass.
4. Age/metallicity -> present stellar state.
5. ProtoplanetaryDiskDescriptor.
6. Correlated planet architecture.
7. Hill/non-crossing stability.
8. Bulk composition + mass/radius.
9. JSON/batch statistics.
10. Binary/triple stars.
11. Holman-Wiegert planetary zones.
12. Moons/rings/belts.
13. Atmosphere/spin/tides.
14. Planet surface integration.

Сначала доказываем статистически нормальную систему. Только после этого выбранная планета получает полноценную поверхность.


---

# 41. Solar System and Earth ground-truth fixtures

The reference set contains explicit Solar System/Earth configurations, which are useful as validation targets rather than as procedural-surface generators.

## REBOUND

REBOUND has a built-in \`solarsystem\` dataset sourced from NASA Horizons for testing, and can also query named bodies such as Sun, Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus and Neptune from Horizons. This gives us high-quality orbital initial conditions for dynamical validation.

Important distinction:

~~~text
REBOUND Solar System
= orbital/dynamical initial conditions + N-body integration
!= procedural Earth terrain generator
~~~

## VPLanet

VPLanet contains a full \`examples/SS_NBody\` Solar System setup with Sun + Mercury through Neptune. We mirrored the input fixtures under:

~~~text
third_party/stellar_system_reference/vplanet/SS_NBody/
~~~

It also contains several Earth-specific physical examples now mirrored under:

~~~text
third_party/stellar_system_reference/vplanet/EarthClimate/earth.in
third_party/stellar_system_reference/vplanet/EarthInterior/earth.in
third_party/stellar_system_reference/vplanet/MagmOc_Earth/Earth.in
~~~

These provide distinct Earth benchmarks:

- **EarthClimate** — orbital forcing, obliquity, latitudinal energy balance, ice sheets and climate parameters;
- **EarthInterior** — radiogenic heating and thermal interior evolution;
- **MagmOc_Earth** — primordial magma-ocean solidification, water inventory, atmospheric escape and oxygen/water partitioning;
- **SS_NBody/Earth** — mass, radius, rotation, obliquity and orbital elements in a Solar-System dynamical fixture.

This is extremely valuable for Elite's CelestialBodyGenerator: Earth can be a canonical regression fixture.

Suggested acceptance concept:

~~~text
Generate body descriptor from an explicit "Earth calibration profile"
        |
        v
compare against known Earth targets:
 mass
 radius
 density
 surface gravity
 escape velocity
 semi-major axis
 eccentricity
 obliquity
 rotation period
 equilibrium/climate regime
 atmosphere class
 interior heat regime
        |
        v
only then trust synthetic Earth-like worlds
~~~

The same pattern should be applied to Venus, Mars, Jupiter and the other Solar System bodies to force the generator to span very different physical regimes.

Crucially, none of these projects provides the missing final layer we still need:

~~~text
physical Earth descriptor
        |
        X
actual procedural continents / mountains / oceans / terrain
~~~

That surface layer remains our own PlanetSurfaceGenerator problem. Solar System data gives us the physical constraints; our procedural surface generator must create a plausible realization consistent with those constraints.
