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
  -> generateSurfacePatch(field, PatchKey, n)         cube-sphere patch (SurfacePatch.h)
  -> SurfacePatchStreamer                             threaded cache
  -> renderer (Planet Lab) / baker equirect maps (CelestialTextureBaker,
     ProceduralFromBodyDescriptor) / collision later
```

Implementations: `PlanetSurfaceField` (rocky / terrestrial / dry / icy /
volcanic), `GasGiantField` (no ground: cloud deck), `SmallBodyField`
(asteroids). `CloudWeatherField` is a separate layer over the surface.

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
- Patches: band limit = 2 × vertex spacing; normals from a border ring of
  the same field; skirts (1.5 × spacing + bounded detail) hide T-junctions.

## 4. Relief

- Continents: exponential-gradient fBm (Tobler magnitudes, lacunarity
  1.92), domain-warped; land threshold = quantile giving the descriptor's
  ocean coverage (dry bodies: highland / basin dichotomy).
- Ranges: belts along zero crossings of a separate low-frequency field,
  ridged multifractal with octave coupling; strength from tectonic regime.
- Uplands: regional field scaling hill relief (rugged country vs plains).
- Hills: derivative-damped fBm from 40 km down (damping from erosion).
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

`surfaceDetailPolicy`: solid + reachable → down to 0.25 m vertices;
solid but unreachable (>100 bar, >30 m/s², molten) → 250 m (overflight);
no solid surface → cloud deck only (km-scale). `maximumPatchLevel` turns
it into the deepest quadtree level a client may request.

## 8. Checks (`ElitePlanetLab --check [--record F] [--verify F]`)

restart reproducibility (record / verify in a separate process), request
order and threads, parent/child consistency, coastline and landmark
stability across band limits, seams (same face, across cube faces,
T-junctions vs skirts), climate structure, deterministic clouds, policy,
asteroid silhouette.
