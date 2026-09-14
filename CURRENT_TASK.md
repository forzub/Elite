# Elite — CURRENT TASK

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Track:** OpenGL 4.3 renderer modernization
**Stage:** migrate all currently working client presentation paths to OpenGL 4.3 Core Profile before CPU -> GPU offload

## Decision

Do **not** start the previously planned System Map sphere offload, compute culling or other GPU optimization waves yet.

First complete the render API migration:

```text
4.3 Compatibility scaffold
    -> remove all compatibility-only rendering
    -> 4.3 Core Profile accepted
    -> measure
    -> offload only proven CPU hot paths
```

Detailed plan: `src/render/GL43_MODERNIZATION_PLAN.md`.

## Immediate gate GL43-A — accept scaffold and complete inventory

Local commands:

```bash
cmake --build build --target EliteGame
./build/EliteGame.exe
```

Verify the existing Compatibility candidate first:

- actual OpenGL >= 4.3;
- `compute=1`;
- `ssbo=1`;
- plausible vendor/renderer/GLSL/limits;
- ordinary flight renders;
- cockpit/rear view renders;
- Galaxy/System/Detail/Hub maps render;
- close-navigation HUD/labels render.

Then perform a repository-wide inventory of compatibility-only production calls. The final Core gate must reject at least these classes of legacy API:

- `glBegin/glEnd` and immediate `glVertex*`/`glColor*`/`glTexCoord*`/`glNormal*`;
- fixed-function matrix stack (`glMatrixMode`, push/pop/load/mult, `glOrtho`);
- `GL_CURRENT_COLOR`, `GL_MODELVIEW`, `GL_PROJECTION`, `GL_MATRIX_MODE`;
- fixed-function texture enable state;
- legacy client arrays if any remain.

Add a static/architecture contract so the forbidden surface cannot silently return later.

## Confirmed first migration seam

`src/game/system_map/LocalMapPrimitiveRenderer.cpp` is a high-leverage first seam because Detail/Hub map code calls it and it is entirely immediate mode today.

Replace it with a shader + VAO/VBO path with explicit projection/color inputs. Do not change map semantics or visual design while doing this.

Confirmed dependent legacy areas to migrate afterward:

1. `DetailMapGeometryPass`;
2. `DetailMapPlanetPass`;
3. `HubMapBackend` fixed-function background/projection setup;
4. `HubMapGeometryPass` compatibility fallbacks;
5. `MapObjectOverlayRenderer`;
6. every remaining render/HUD/debug/cockpit compatibility call found by the full inventory.

Existing shader/VBO/VAO code that is already Core-compatible should not be rewritten without a concrete reason.

## Core cutover gate

After the forbidden-call scan is clean:

- switch/regenerate bundled GLAD to `gl:core=4.3`;
- request `GLFW_OPENGL_CORE_PROFILE`;
- build and launch `EliteGame`;
- confirm runtime Core 4.3+;
- smoke all currently working visual modes again;
- accept only if no working feature was removed or visually broken merely to satisfy Core Profile.

## Explicitly deferred until after Core acceptance

- System Map textured-sphere CPU tessellation removal;
- System/Galaxy primitive optimization beyond what Core migration itself requires;
- `SceneRenderer` compute culling/LOD/compaction;
- instance-streaming optimization;
- starfield compute migration;
- any other CPU -> GPU algorithmic offload.

The existing offload audit and priorities are preserved in `src/render/CLIENT_GPU_OFFLOAD_AUDIT.md` and `src/render/GPU_OFFLOAD_PLAN.md`.

## Keep CPU boundary

The earlier ownership decision is unchanged: authoritative physics, route/path/docking decisions, replication, `ClientWorldState` gameplay/prediction state and CPU interaction semantics do not move to the client GPU merely because the renderer becomes modern.

## Deferred runtime-model task

The first read-only `RuntimeModelAssetLibrary` consumer migration remains queued until the render modernization reaches a stable checkpoint.

## State discipline

Every accepted GL43 migration wave updates `CURRENT_STATE.md`, `CURRENT_TASK.md`, `src/render/GL43_MODERNIZATION_PLAN.md` and `src/game/GAME_RUNTIME_DECOMPOSITION.md`. `GPU_OFFLOAD_PLAN.md` changes only if offload sequencing or audit conclusions change. Runtime-model ingress documentation changes only when that boundary itself changes.