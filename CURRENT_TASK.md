# Elite — CURRENT TASK

**Updated:** 2026-09-15  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`  
**Track:** client CPU -> GPU migration  
**Stage:** GPU-P0 — corrected System Map static textured sphere visual acceptance

## Accepted prerequisite

OpenGL 4.3 Core is **accepted locally**. The compatibility-removal track is closed.

The station-adjacent freezes predate the renderer migration. They remain deferred and must not be investigated as part of this task.

## Regression just found

The first static-sphere candidate (`e9a9cfdef1331f67259d019fd9024054ce30779e`) passed static/compile checks but failed local runtime acceptance: textured planets and moons disappeared while rings, labels and other System Map presentation remained visible.

An isolated OpenGL 4.3 Core raster diagnostic proved that the indexed topology, EBO/VAO submission, `GL_BACK + GL_CCW` culling and shader pair can rasterize correctly with no GL error. Therefore the correction avoids the newly introduced full-runtime shader ABI rather than reverting the GPU optimization.

## Corrected candidate

Correction commit: `d9f1db5fdd6e72b68fa59e887bfc88535cb3f279`.

The two resident indexed meshes remain:

- low: 24 x 48;
- high: 64 x 128.

The runtime-proven body shader ABI is restored:

```text
aPos + aUv + aColor + uMVP
```

Per body, CPU work is now only compact parameter work:

```text
center/radius/basis/phase
    -> bodyModel
    -> bodyMvp = frameMvp * bodyModel
    -> constant color attribute
    -> glDrawElements(static sphere)
```

The old hot work remains removed:

- no per-frame latitude/longitude tessellation;
- no tens of thousands of sphere vertex `sin/cos` evaluations;
- no per-frame `TexturedVertex` arrays;
- no full-sphere `glBufferData(GL_DYNAMIC_DRAW)` upload.

## Automated validation

PASS after the correction:

```text
python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
Windows/MSYS2 MinGW64 g++ syntax compile of SystemMapRenderer.cpp
```

The temporary diagnostic/CI helpers have been removed from the working branch after validation.

## Run now

```bash
git fetch origin
git switch chatgpt/mae-v01075-semantic-workflow-motion-v5
git pull --ff-only

python tests/architecture_contracts/check_gl43_modernization_boundary.py
python tests/architecture_contracts/check_system_map_static_sphere.py
cmake --build build --target EliteGame
./build/EliteGame.exe
```

## Focused visual acceptance

Open System Map and verify:

- planets and moons have returned;
- Saturn/Jupiter/Uranus/Neptune body is visible inside its rings;
- textures are not mirrored or rotated incorrectly;
- texture seam remains correct;
- zooming between small/large body ranges does not make the globe disappear;
- ring order remains back -> body -> front.

If this passes, mark GPU-P0 accepted immediately.

## After P0

Next wave: repeated System Map primitive modernization — orbit circles, marker/selection rings, billboard balls/halos and related CPU `sin/cos` topology. Prefer resident unit geometry + compact parameters/instancing, not compute.

`SceneRenderer` visibility/LOD compute remains profile-gated. Do not use the known station freeze as justification to start it without a dedicated profile wave.

Keep navigation, picking, gameplay authority, replication and route/docking decisions CPU-owned.