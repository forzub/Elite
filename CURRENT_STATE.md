# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor baseline:** v0.10.86 accepted
**Model Asset Editor architecture:** closed at the current target boundary
**ModelAsset binary v4 architecture:** independent translation units closed
**Game runtime decomposition:** started — Wave R0 candidate (navigation + CPU assembly seams)

## Accepted baseline

The Model Asset Editor decomposition, localization architecture, binary-v4 independent-TU closure and uniform bottom stage footer are accepted as the current editor baseline. New work is now on the game runtime; editor architecture is not to be reopened unless a concrete regression requires it.

## Game runtime decomposition track

The game already has strong logical seams (headless server, client/server transport boundary, deterministic navigation/policy code), but most runtime `.cpp` files are still compiled directly into `EliteGame` and many are duplicated in `EliteServer` source lists. The new track converts those logical seams into CMake/link-time boundaries without changing gameplay behavior.

Wave R0 establishes the first physical library seam: `EliteNavigationGeometry`, owning deterministic obstacle geometry and geometric path planning. Both `EliteGame` and `EliteServer` must link it; neither executable may compile those implementation files directly.

Authoritative plan and decomposition rules: `src/game/GAME_RUNTIME_DECOMPOSITION.md`.

## R0 dependency finding

The first hosted headless link exposed a pre-existing CMake ownership gap: authoritative code consumes `AssemblyMeshLibrary`, but `EliteServer` did not own/link its implementation. R0 fixes the architecture instead of duplicating missing `.cpp` files: `EliteAssemblyGeometry` now owns CPU OBJ hydration, authored assembly definitions and the CPU assembly cache, and both runtime executables link the same implementation. It remains separate from render/GPU ownership.
