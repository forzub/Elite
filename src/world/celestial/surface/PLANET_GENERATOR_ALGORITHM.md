# Procedural surface generator — contract

Code: `src/world/celestial/surface/` (library `EliteCelestialSurface`, no GL).
Proof and checks: `tools/planet_lab/` (`ElitePlanetLab`).

## 1. One surface model

```
generated physical descriptor (elite.generated_body.v1 / generated_reservoir.v1)
  -> BodySurfaceDescriptor / SmallBodyDescriptor      (BodySurfaceDescriptor.h)
  -> SurfaceField::sample(dir, minWavelengthM)        (SurfaceField.h)
       macro fields   continents, ranges, uplands, craters, volcanoes, lakes, ice caps
       climate        temperature, moisture, biome (from the MACRO height)
       landmarks      stable feature class + id per direction
  -> SurfaceField::appearance(sample, dir, slope)     materials -> albedo / emission
  -> runtime renderer (src/render/celestial/flight, CELESTIAL_RUNTIME_RENDERER.md):
       far globe      GlobeBake: cube-map cache of the field (face size from screen size)
       near surface   LocalTerrain: camera-centred clipmap on a planet-fixed lattice
  -> baker equirect maps (CelestialTextureBaker, ProceduralFromBodyDescriptor)
```

The planet is ONE continuous function of planet-fixed direction and band
limit. There is no patch identity: neither the globe cache nor the local
terrain owns a piece of the world; both are view-dependent samplings of the
same field and are rebuilt / dropped freely. The cube-sphere patch pyramid
(`tools/planet_lab/experimental/SurfacePatch.*`) is a Planet Lab experiment
and reference for the old checks, not a production path.

Implementations: `PlanetSurfaceField` (rocky / terrestrial / dry / icy /
volcanic), `GasGiantField` (no ground: cloud deck), `SmallBodyField`
(asteroids), `ImportedSurfaceField` (curated maps of Sol bodies as the macro
field + procedural detail below the source texel). `CloudWeatherField` is a
separate layer over the surface.

Sources: a body is either `ImportedRealData` (curated maps, Sol) or
procedural from its descriptor. Both end in the same render-ready texture
set (`CelestialTextureBaker` → `bakeRenderReadyLodSet`); the procedural
provider can be run on any descriptor, including Sol bodies (Planet Lab).

## 2. Identity and determinism

- `surfaceSeed = deriveSeed(bodySeed, "planet_surface", GeneratorVersion)`,
  one tag per layer (`continents`, `ranges`, `uplands`, `hills`,
  `impact_craters`, `volcanoes`, `lakes`, `climate`, `weather` …).
  A new layer = a new tag: existing layers never move.
- Everything is coordinate-addressed: noise lattice points and landmark
  cells are hashed (`latticeHash`), never drawn from a sequential RNG.
- Asteroids of a reservoir: `deriveSeed(latticeHash(deriveSeed(beltSeed,
  "asteroid"), cell), "candidate", index)` — never materialised in bulk.
- `GeneratorVersion` (now 1) changes only when an existing layer's output
  changes on purpose; it is part of every checksum.
- Equal (version, body seed, descriptor, direction, band limit) → equal
  sample, bit for bit, in any order, on any thread, after a restart.

## 3. LOD never changes geography

- Each octave series is split at `macroCutoffM` (≈ R/5000, 100 m–2 km):
  `macro` = all octaves above it, always evaluated in full; `total` = all
  octaves down to the band limit (the last one fades in).
- Cross-octave state (ridged weight, derivative damping) runs on unfaded
  octaves: a finer LOD only appends terms.
- LOD-independent: `macroHeightM`, `water`, `coastM`, climate, biome,
  `featureClass/featureId`. Only `heightM` (displayed) gains detail.
- Coastline = zero set of `coastM` (macro height above the applicable
  water level). Land is never displayed below the water it borders; the
  renderer decides water/land per pixel from the interpolated `coastM`.
- A crater size class that can contain a macro crater is evaluated at
  every LOD.
- Local terrain: a level-k vertex is the field at a fixed lattice point of
  the chart (face, k, i, j) with band limit 2 × spacing_k, so rebuilding a
  level at another centre reproduces it bit for bit; vertices morph to the
  next coarser level (linear along its triangles) near the level's outer
  ring, so levels meet without cracks or skirts.
- Globe cache: texel = field at the texel direction with band limit
  2 × texel; a larger face only resolves more of the same geography.

## 4. Relief

- Continents: exponential-gradient fBm (Tobler magnitudes, lacunarity
  1.92), domain-warped; land threshold = quantile giving the descriptor's
  ocean coverage (dry bodies: highland / basin dichotomy).
- Ranges: belts along zero crossings of a separate low-frequency field,
  ridged multifractal with octave coupling; strength from tectonic regime.
- Uplands: regional field scaling hill relief (rugged country vs plains).
- Hills: derivative-damped fBm from 40 km down (damping from erosion).
  Airless rocky / icy bodies: low, smooth hills (impact gardening: damping
  0.45, steeper fall-off, strength 0.035-0.085); craters carry the relief.
- Landmarks from 3D lattice cells (shell acceptance = uniform per area):
  impact craters (classes from R·0.3 to 160 m; simple bowls / complex
  floors with central peaks; density × (1 − resurfacing) × (1 − erosion);
  ocean worlds keep few, degraded ones), volcanoes (volcanism; taller at
  low gravity), lakes (climate, glaciation; level = macro terrain at the
  centre; memoised per thread), ice caps (descriptor ice coverage).
- Ocean reaches only what it connects to: the effective sea level falls
  away inland (continuous in the continent signal).
- Boulders: local rock field below 8 m on rocky ground.

## 5. Climate and materials

- Temperature: mean + equator–pole contrast (descriptor min/max, damped
  by pressure, reduced by obliquity) on a latitude deformed by a warp
  field; lapse rate with altitude when there is an atmosphere.
- Moisture: Hadley/Ferrel bands (wet ITCZ, dry subtropics, wet storm
  tracks), drier inland, rain shadow behind ranges, regional noise,
  scaled by weather potential.
- Vegetation only for a stated biosphere (`geophysics.biosphere` =
  complex; generated bodies carry `none` - no greening by assumption).
- Biomes from temperature + moisture + ice; materials (rock, basalt,
  regolith, sand, soil, vegetation, ice, snow, salt, lava) from
  composition (`surface_materials`), biome, temperature, moisture,
  altitude, slope, fresh ejecta and deterministic variation. A material
  is decided by the macro climate: snow stays snow at every LOD.

## 6. Clouds and weather

`CloudWeatherField`: layers (kind, height, opacity) from pressure and
cloud potential; coverage = climate tendency + pattern advected by zonal
winds and turning slowly in world time + cyclones per 6-h weather epoch
(blended between epochs). A function of (body seed, world time) only.
The game's map clouds use identity seeds; the former
`random_on_session` / `random_on_map_open` policies read as `world_time`.

## 7. Detail policy (streaming, not identity)

`surfaceDetailPolicy`: solid + reachable → down to 0.25 m vertex spacing;
solid but unreachable (>100 bar, >30 m/s², molten) → 250 m (overflight);
no solid surface → cloud deck only (no local terrain). The local terrain
takes the finest spacing from it.

## 7a. Ocean coverage (world generator)

`BodyPhysics`: ocean coverage = 1 - exp(-4.6 D / H) from the global
equivalent water layer D (observed water fraction = hydrosphere; generated
bulk fraction keeps most of a small inventory in the mantle, storage scale
0.02) against basin relief H >= 6 km. Earth gives 0.75; generated temperate
planets range 0.42 .. 1.0 (water worlds stay oceans).

## 8. Checks

`ElitePlanetLab --flight-check` (production renderer + real catalog):
silhouette size and position on screen vs asin(R/d) for the Moon, Mars and
Jupiter at 4 / 15 / 60 radii, on and off axis; ephemeris motion (60 s) and
spin direction / rate; local terrain lattice determinism (rebuild at
another centre) and coastline identity between levels.

`ElitePlanetLab --check [--record F] [--verify F]` (field + experimental
patches):

restart reproducibility (record / verify in a separate process), request
order and threads, parent/child consistency, coastline and landmark
stability across band limits, seams (same face, across cube faces,
T-junctions vs skirts), climate structure, deterministic clouds, policy,
asteroid silhouette.
