# Celestial bodies: resource ownership audit (2026-11-11)

Source audit only (no runtime measurement yet). Question from the author: who loads what for a planet / moon,
how much memory it holds, when it is released, and how to reduce it to one owner per body.

## 1. Who holds a body today

| # | Owner (process) | What | Size per body | Released |
|---|---|---|---|---|
| A | `CelestialFlightRenderer::Impl::bodies` (client) | `SurfaceField` (+ `CloudWeatherField`), loaded once the disc is > 1 px | procedural: ~0; curated Sol maps: albedo `glm::vec3` float (12 B/texel) + height float + water byte = 17 B/texel: 1024x512 = 8.9 MB, 2048x1024 (Earth, Moon) = 36 MB, Jupiter 4096x2048 albedo = 100 MB | **never** (map entries are erased only at shutdown, also across jumps) |
| B | same, `GlobeTier` + `CloudTier` | cube-map albedo / normal / relief + 2 cloud cubes | by screen size | LRU under `textureBudgetBytes` = 512 MB - the one budgeted cache |
| C | same, `GpuLevel` | local terrain rings of the nearest body | small | rebuilt / dropped by distance |
| D | same, asteroids | small-body meshes | small | after 600 unused frames |
| E | `BodyTerrainProbe` static cache (client: `SpaceStateBodyAutopilot`; server: `GameServer` contact) | a second / third `loadBodySurface` of the same body: albedo maps included although only heights are read | as A | **never** |
| F | `MapCelestialRenderResources` (client) | cached generated textures: global albedo 2048x1024, preview 512, normal (Hub) | 2048x1024 RGBA + mips = 11 MB each map | **never** |
| G | `ProceduralBodyTextureCache` (client, new) | procedural albedo 1024x512 | 2.8 MB with mips | **never** |
| H | `CelestialShapeMeshLibrary` (client) | OBJ shape + its albedo (Phobos, Deimos) | small + 11 MB texture | never |
| I | `ProceduralCloudLayer` (client, map) | cloud textures per style | per style | on map session change (the one map cache that is cleared) |
| J | disk `assets/generated/celestial` | baker output: albedo / normal / material / preview / flat TGA + `lod/global` + `lod/map` + `lod/tiles` | Sol 4.9 GB, Tau Ceti 778 MB; Earth 297 MB of which tiles 211 MB | n/a |

## 2. Findings

1. **No owner, no lifetime for CPU fields (A, E).** A body's field lives until the process ends. In Sol the curated
   maps alone are several hundred MB of float data once the bodies have been on screen; every jump adds the next
   system's bodies (procedural fields are tiny, curated ones are not). The probe (E) loads the same body again in the
   client and again in the server - up to three copies on one machine.
2. **Curated maps are kept as float.** `ImportedSurfaceMaps::albedo` is `glm::vec3` (12 B) per texel where the source
   is 3 bytes: 4x the memory for no precision gain. The server and the probe never read albedo at all.
3. **Map GPU caches are unbounded (F, G, H).** Every body ever opened on the map keeps its textures. Sol alone can
   reach ~0.8 GB (albedo + preview + normal per body); procedural bodies add 2.8 MB each with no ceiling over ~900
   bodies.
4. **Two sources of truth for how Sol looks.** Flight draws the curated sources (`assets/source/.../albedo_source.*`)
   through the field; the map draws the baker's cache (`assets/generated`), which is restyled (Jupiter: saturation
   0.08, tint) and partly hidden by environment profiles (Jupiter, Saturn, Uranus, Neptune, Venus surfaces hidden,
   replaced by a procedural cloud deck). The same body looks different in flight and on the map.
5. **Most of the disk cache is dead weight.** `lod/tiles` (~70 % of the Sol cache) is only validated by
   `CelestialGeneratedAssetLibrary`, never read for drawing. Each body is also stored as several uncompressed TGA
   copies of the same albedo (`albedo.tga`, `lod/global/albedo_2048.tga`, `preview*.tga`, `lod/map/*`).
6. **Load policy differs per consumer.** Flight loads on screen size, the probe on first query, the map on first
   draw - three triggers, three caches, three invalidation stories (none for A, E, F, G).

## 3. Target: one owner per body

1. **`BodySurfaceRegistry` (one per process, in `EliteCelestialSurface`).** Key = body id + descriptor path.
   Returns `shared_ptr<const LoadedBodySurface>`; holds `weak_ptr`s plus a small LRU of strong refs under a byte
   budget. Flight (A), the probe (E) and the map bake (G) all ask it; nobody calls `loadBodySurface` directly.
   Request flags: `heights only` (server / probe: no albedo decode) vs `full`.
2. **Compact curated maps.** Albedo RGB8 (decoded to linear on sample), height int16 or half: ~4x less, no visible
   change (the source is 8-bit).
3. **One map texture cache with a budget.** F + G + H merged into `MapBodyTextureCache`: LRU, byte budget (e.g.
   256 MB), drop the previous system's textures on a system change.
4. **One source of truth for the look.** The map texture is a bake of the SAME field the flight renderer draws
   (`ProceduralBodyTextureCache` already does this for procedural bodies; it can bake an `ImportedSurfaceField` the
   same way). Then the map no longer needs `assets/generated` at all; environment profiles keep only atmosphere /
   clouds and stop hiding surfaces.
5. **Disk.** Stop baking `lod/tiles` and the duplicate TGA copies; once (4) is accepted, the whole
   `assets/generated/celestial` cache is optional (offline previews only), Tau Ceti included.
6. **Flight lifetime.** Bodies of the previous system are released on a jump; within a system, fields of bodies that
   stayed a point for N seconds are released (the registry keeps them only while the budget allows).

Order (each step alone, checked in the game): 1+2 (registry, compact maps, heights-only for server / probe) ->
3 (map cache with budget) -> 4 (map bakes the flight field; Sol look unified; author judges Sol bodies) -> 5 (disk) ->
6 (flight release policy). Measurement first: a DEV memory line per cache (bytes, entries) so every step has numbers.
