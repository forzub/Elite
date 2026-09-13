# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Editor candidate:** v0.10.79
**Whole-editor separation:** ~90%

## Verified baseline entering this pass

User reported the complete v0.10.78 architecture test set PASS and local `EliteAssetEditor` build completed successfully.

## v0.10.79 viewport-core candidate

This pass moves the renderer-owned core out of `model_asset_editor.html`:

- scene/bootstrap lifecycle, resize/render loop, world axes, raycaster and camera fit -> `viewport/runtime.js`;
- geometry cache, BufferGeometry construction, surface preview materials and raw/working geometry selection -> `viewport/geometry.js`;
- `rebuildScene` and render-node visibility orchestration -> `viewport/scene.js`;
- cyclic legacy dependencies between LOD/SEMANTICS effects and scene rebuild are contained by a narrow late-bound `viewport/scene_bridge.js` instead of putting renderer implementation back into the shell.

Existing transport, session/persistence and application-state boundaries remain unchanged.

## Remaining renderer work

The shell still owns specialized viewport adapters that are coupled to feature UI:

- edge and normal overlays;
- collision / structural proxy / socket THREE materialization;
- socket camera preview;
- picking decision/effect wiring;
- some semantic gizmo integration remains in the existing semantics effect layer.

These are the next extraction target. After that, physically move `EditorViewState` and the visibility adapters out of the HTML shell, then perform final shell cleanup.

## Acceptance

GitHub candidate validation must pass:

- `git diff --check`;
- application-state architecture;
- orchestration architecture;
- transport architecture;
- session/persistence architecture;
- viewport architecture;
- SEMANTICS workspace layout;
- JS syntax for new viewport modules.

Local acceptance remains the authoritative C++/pack/runtime gate.
