# Celestial runtime renderer — contract

Code: `src/render/celestial/flight/` (library `EliteCelestialFlightRender`),
surface model `src/world/celestial/surface/` (`EliteCelestialSurface`, no GL).
Hosts: the game (`SceneRenderer`, pass 0.25) and Planet Lab
(`ElitePlanetLab --flight*`) — the same class, the same catalog.

## 1. One world, map and flight agree

- Frame source: `FlightFrameBuilder` takes the celestial snapshot the maps
  use (`ClientCatalogService`, render universe time, every frame) plus the
  atlas system definition. Positions = `CelestialBodyState::worldMeters`
  (system-local metres, the frame of the ship positions); radius from the
  catalog; orientation = `makePlanetTextureOrientation` (the maps' own
  function: tilt, node, spin phase, texture longitude offset).
- The renderer never moves or scales a body. The camera sits at the origin
  (camera-relative double maths, float only after subtraction); depth is
  logarithmic (written per fragment), far plane 1e13 m.
- Checked (`--flight-check`): silhouettes of the Moon, Mars and Jupiter at
  4 / 15 / 60 radii, on and 12° off axis, match asin(R/d) within 0.5 px;
  Earth moves with its snapshot velocity (11 m error over 60 s / 1816 km);
  Earth spins east by 15.04°/h.

## 2. Representation by distance (one continuous field per body)

| on screen | representation | data |
|---|---|---|
| < 1.5 px | point sprite (reflected-light flux, phase); stars: glow sprite | none |
| disc .. orbit | globe: cube-sphere mesh, relief displacement, albedo / normal / relief cube maps | `GlobeBake` of the field, face 32 .. 1024 by projected radius |
| low orbit .. ground | camera-centred local terrain over the globe (stencil) | `LocalTerrain` clipmap levels |
| inside a belt | asteroid meshes (SmallBodyField) within ±2 cells, sprites further | reservoir lattice |

- **Globe cache.** Each texel is `SurfaceField::sample(dir, 2 × texel)`;
  a larger face resolves more of the same geography. Tiers: new tier when
  the projected radius needs it, a smaller one when 4× too big. Uploads in
  slices (2 faces per frame); the old tier stays visible until the new one
  is complete.
- **Local terrain (no patches).** Nested square grids (128 cells per side by
  default) on the equal-angle chart of the cube face under the camera; the
  chart stays planet-fixed (face hysteresis 1.3), level k has lattice step
  2^k × 2^-30. A vertex is the field at its lattice point (band 2 × spacing)
  — rebuilding at another centre reproduces it bit for bit. Each vertex
  carries its morph target on level k+1; the vertex shader blends to it in
  0.70–0.86 of the half extent (measured from the camera), so outer rings
  equal the coarser surface (no cracks, no skirts). Recentre hysteresis
  1/16 of a level. A face change builds the whole new set and swaps at once.
  Finest spacing = max(policy, altitude × pixel angle × 4); coarsest covers
  the horizon (+15 %), at most 14 levels; active below 0.6 R of altitude.
- **Coastline per pixel** from the interpolated coast value (LOD-independent).
- **Near pass:** `renderTerrainDepth` writes the local terrain into the
  ordinary depth buffer so ships and hubs are hidden by the ground.
- **Clouds:** `CloudWeatherField` coverage cube (256) at world time, new
  state every 20 min of world time, cross-faded. Gas giants have none (the
  bands are the clouds).
- **Atmosphere:** shell at 8 scale heights, 16-step single scattering with
  game-grade extinction; colour class from composition (N2/O2 Rayleigh,
  CO2 dust, thick CO2, tholin haze, H2 giants); surface aerial haze with
  optical depth reduced by camera altitude; optical depth capped for
  readability (0.9 for N2 worlds).
- **Rings:** annulus, radial profile from the ring bands, planet shadow.
- **Stars:** limb-darkened disc + glow sprite (size from flux).

## 3. Styles (`CelestialRenderStyle`)

One shading function (`SurfaceShading` in `CelestialFlightShaders.h`) for
globe and terrain, so both meet seamlessly in every style.

- **EliteStyle:** opaque black bodies; phosphor-green lines: silhouette
  (always bright), coastlines, height contours (relief / 6), lattice
  wireframe on the local terrain (every 8 cells, sparser with distance),
  atmosphere limb outline (outside only), cloud outlines, ring band edges,
  star outline + rays, asteroids black + wireframe.
- **Modern:** linear albedo × light, relief + band-limited micro normals,
  water specular, night emission, cloud shadows, scattering sky, aerial
  haze, gamma-encoded output.
- **Anime:** posterised palette (5 levels), three hard light bands with
  hue-shifted shadows, ink silhouette and coastline, toon water highlight,
  flat stepped clouds and sky, 4-point star flares.

Selection: `ClientModeState::celestialRenderStyleId` ("elite" / "modern" /
"anime"), persisted in `ClientPreferences` (`celestial_render_style`); main
menu → SETTINGS → PLANET RENDER STYLE; in flight Ctrl+F7 cycles.
DEV: `ELITE_DEV_CELESTIAL_STYLE` overrides (not persisted).

## 4. Memory and streaming

- Worker pool (hardware − 2, 2..12 threads), priority queue: field loads,
  terrain levels (finest first), globe tiers, asteroid meshes, cloud states.
- Texture budget 512 MB (`FlightRenderQuality::textureBudgetBytes`): globe
  tiers of bodies whose disc was not on screen this frame are evicted
  least-recently-used first. Bodies seen as dots keep no textures.
- Local terrain exists only for the solid body with the lowest relative
  altitude and only while active; leaving drops all levels.
- Asteroid meshes unused for 600 frames are deleted.
- Nothing is persisted; everything is rebuilt from the field on demand.

## 5. Quality knobs (`FlightRenderQuality`)

`maxGlobeFace`, `cloudFace`, `terrainCells`, `maxTerrainLevels`,
`terrainCellPixels`, `atmosphereSteps`, `clouds`, `atmosphere`,
`asteroids`, `rings`, `detailNormals`, `textureBudgetBytes`,
`workerThreads`. Performance and recommendations: section 6 (measured).

## 6. Performance (measured 2026-10-31, `ElitePlanetLab --flight-bench`)

Machine: Intel UHD Graphics 630 (integrated GPU), GL 3.3, 1600×900, Debug
game build (surface library -O2). Average of 90 frames after warm-up, GPU
time with `glFinish`; per-pass GPU from timestamp queries.

| view | Elite ms | Modern ms | Anime ms | main GPU cost |
|---|---|---|---|---|
| far space (Earth, Saturn, procedural) | 1.2–2.0 | 0.9–1.6 | 1.0–1.7 | globe 0.05–0.12 |
| near orbit Earth (155k terrain tris) | 9.5 | 11.3 | 11.3 | terrain 3.3–5.4, atmosphere 4.5–4.8 (Modern/Anime) |
| approach Earth (205k tris) | 4.7 | 15.9 | 16.0 | atmosphere 9.0–9.2 |
| near surface Earth (279k tris) | 5.4 | 15.4 | 16.2 | atmosphere 8.5–8.6, terrain 3.1–3.8 |
| Saturn near orbit / approach | 3.4–4.9 | 11.0–14.0 | 10.3–14.0 | atmosphere 6–9, rings 0.5–0.8 |

- CPU per frame: 0.2–1.1 ms (frame build + draw submission); GPU uploads
  ≤ 2 ms per frame (sliced).
- Memory: globe textures stay at the 512 MB budget (≈480 MB in use with
  all Sol bodies visited); terrain meshes 8–15 MB; clouds 1.6 MB per body.
- Streaming: 320k field samples per busy worker-second; cold first approach
  0.1–0.5 s per level set for Sol bodies, up to 24 s for a procedural
  terrestrial at near orbit (all levels from scratch, Debug); the first
  frame of the session loads curated maps (≈30 s in the Lab, all visible
  bodies).
- Worst frames: up to 50–64 ms in Modern / Anime (single frames; Elite
  ≤ 16 ms near the ground) — not the uploads (measured < 2 ms); suspected
  driver-side deferred work on this iGPU (open, needs a GPU profiler).

Bottlenecks in order: (1) atmosphere ray-march from inside the shell
(full screen, 16 steps); (2) local terrain fill (278k triangles + micro
normals); (3) first-time field evaluation on workers (CPU); (4) texture
memory for 1024 globe tiers (75–100 MB each).

## 7. Recommendations (what to turn down when FPS drops)

1. **Atmosphere** (biggest win, Modern / Anime): `atmosphereSteps` 16 → 8
   halves the pass (≈ −4 ms near the ground); render the sky shell at half
   resolution and upsample; skip the shell over the ground entirely below
   ~5 km (the terrain haze already covers it); on low settings replace the
   ray-march with a 2D lookup (altitude × view-sun angle) baked once per
   body.
2. **Local terrain**: `terrainCells` 128 → 96 (−44 % triangles),
   `terrainCellPixels` 4 → 6 (finest level one step coarser),
   `maxTerrainLevels` 14 → 10 (the globe covers the far ring);
   `detailNormals = false` drops the per-pixel noise (≈ −0.5 ms).
3. **Globe textures**: `maxGlobeFace` 1024 → 512 cuts globe memory 4× and
   upload time 4×; `textureBudgetBytes` 512 → 256 MB on 4 GB machines.
   Above 10 % of the screen the local terrain carries the detail anyway.
4. **Clouds**: `cloudFace` 256 → 128; `clouds = false` on low; the cloud
   state interval (20 min world time) can grow to 60 min.
5. **Rear monitor / mini views**: draw the celestial pass there with
   `atmosphere = false, clouds = false` (or reuse the main frame's sky);
   today it is a full second pass.
6. **Elite style** is the cheapest path (no atmosphere ray-march, no
   haze): a natural "performance" style.
7. **Streaming**: incremental (toroidal) level updates instead of full
   rebuilds would cut worker load ≈ 8× during low fast flight; a disk cache
   of baked globe tiers (keyed by GeneratorVersion + seed + face size) would
   remove the cold first-approach cost.
8. **Expose as user settings**: atmosphere quality (off / 8 / 16 steps),
   cloud quality, terrain detail (cells per level), texture budget.
