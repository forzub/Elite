# Elite client GPU modernization plan

**Baseline decision:** OpenGL 4.3+ is the minimum graphical-client API.  
**Implementation status:** GL43 compatibility baseline candidate implemented in `19db31eca5aa2c12475d87f2dfa322f794990c6e`; local runtime acceptance pending.

## Transitional profile

The client currently requests **OpenGL 4.3 Compatibility Profile**, not Core Profile. This is intentional: legacy presentation code still contains fixed-function calls such as `glBegin`, `glColor*` and `GL_TEXTURE_2D`. Compatibility 4.3 gives the project compute shaders, shader-storage buffers and modern synchronization now, while allowing legacy passes to be removed incrementally. Moving to Core Profile is a later cleanup gate after those calls are gone.

The bundled GLAD loader must be generated for `gl:compatibility=4.3`. A 4.3 context with a 3.3 loader is not accepted because it would hide the APIs needed for GPU offload.

`render::gpu::GlRuntimeCapabilities` is the single runtime capability gate. Startup must fail below 4.3 or when compute/SSBO entry points are unavailable, and must log the exact GPU/driver/GLSL and compute limits.

## Ownership rule

GPU offload is for **client presentation and derived data**. Authoritative gameplay/simulation remains CPU-owned and deterministic unless a future design explicitly introduces a server-side accelerator path. Do not move authoritative navigation, damage, economy, replication or ship-state decisions into a client GPU merely because they are expensive.

A workload is a GPU candidate when it has enough parallel work, its inputs can stay resident or upload infrequently, and its result is consumed by rendering without a blocking CPU readback. Avoid `dispatch -> glMemoryBarrier -> readback -> CPU decision` loops in the frame path.

## Current acceptance gate

Before any compute workload migration is accepted locally:

```bash
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify startup reports OpenGL >=4.3, `compute=1`, `ssbo=1`, credible compute limits, and visually smoke both ordinary gameplay rendering and Hub map rendering. Compatibility-profile rendering must remain visually equivalent before P0 compute work begins.

## Initial audit / priority

| Priority | Client workload | Decision | Reason |
| --- | --- | --- | --- |
| P0 | `CloudAppearanceTextureGenerator` | MOVE to compute | Large per-pixel loops with FBM/Worley/domain-warp work; output is a texture consumed by rendering. |
| P0 | `PlanetaryWeatherMapGenerator` | MOVE to compute with CPU reference during parity | Per-pixel procedural climate/noise generation is highly parallel and naturally texture/SSBO based. |
| P1 | `GalaxyStarfieldRenderer` dynamic rebuild/projection/culling | MOVE derived visibility/appearance work to GPU | Catalog can upload once; observer-relative transforms, magnitude appearance, filtering and draw preparation should avoid repeated CPU VBO rebuilds. |
| P1 | large instance/frustum visibility paths | PROFILE, then GPU culling/indirect draw where counts justify it | Good SSBO/compute workload only when object counts are large enough. |
| P2 | procedural celestial mesh/detail generation | PROFILE first | If generation is one-time/offline, prebuilding/caching beats compute migration; if regenerated often, GPU becomes attractive. |
| P2 | `PlanetWireRenderer` CPU geometry work | PROFILE first | Potentially parallel but benefit depends on rebuild cadence and line topology. |
| KEEP | Hub-map GPU geometry and near-navigation labels | keep active; optimize only from measurements | These are current required close-navigation presentation paths. |
| DISABLED | general `WorldLabelRenderer` world-signal labels | do not optimize now | Presentation contract is intentionally undecided. Renderer infrastructure remains because navigation markers still use it. |
| CPU | route/path planning, trajectory decisions, authoritative physics | keep CPU by default | Branchy algorithms, determinism/authority requirements and CPU consumers make GPU readback counterproductive. GPU visualization derivatives may be separate. |

## Audit rule for the rest of the client

Do not equate "CPU work" with "GPU candidate". For every suspected hot path record: execution frequency, item/pixel/vertex count, CPU wall time, allocation/buffer-upload cost, whether inputs can remain GPU-resident, whether the output is render-only, and whether any synchronous readback would be required. Only then promote it into a GPU migration wave.

High-value patterns to keep looking for are repeated per-pixel procedural generation, repeated observer-relative transforms over large stable catalogs, repeated CPU culling/compaction before rendering, dynamic VBO rebuilds whose source data is otherwise static, and large independent per-instance calculations. Low-value candidates are small branchy planners, one-time/offline generation, string/UI work and any result immediately needed by authoritative CPU logic.

## Migration protocol

For each GPU wave: first measure CPU wall time and update frequency; preserve a deterministic CPU/reference implementation while establishing parity; upload stable inputs once; dispatch without synchronous readback; consume GPU-owned output directly in render; measure CPU frame time, GPU time and transfer stalls; only then remove the old hot path. Every wave gets an architecture/regression contract so presentation code cannot silently fall back to a large per-frame CPU rebuild.

## Current label policy

`game::runtime::WorldSignalLabelsEnabled` is false. The general world-signal label feed and draw are gated off. Hub-map labels (`NavigationCellLabelLayer`, Hub-map renderers) and explicit close-navigation HUD markers remain active. Do not delete `WorldLabelRenderer` yet because the navigation marker rendering path currently shares that implementation.
