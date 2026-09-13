# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Immediate goal

Accept the cleaned SEMANTICS workspace in the real editor without changing the semantic data model or breaking existing controls.

## Current candidate — v0.10.75

### TREE

- keep `LOD` selection and title;
- one compact workflow row:
  - `TREE · ASSEMBLY / KINEMATICS`;
  - `GRAPH · STRUCTURAL LINKS`;
  - `CHECK` at the far right;
- remove the long MODEL ROOT explanation from the visible workspace;
- remove upper summary/counter/warning banners;
- keep legacy cleanup as a compact secondary button;
- condense 3D transform preview controls;
- move long contextual help behind `?`;
- preserve tree selection, reparenting, motion, bindings and all existing command IDs.

### GRAPH

- use the same workflow row and final `CHECK` placement;
- remove the long structural-graph lead and cleanup explanation;
- move new-link instructions behind `?`;
- keep mesh selection, ROOT/A/B assignment, link creation/editing, proxy editing and explode controls unchanged functionally.

## Verification

Run:

```bash
python tests/architecture_contracts/check_model_asset_semantics_workspace_layout.py
cmake --build build/tools/model_asset_editor --target EliteAssetEditor -j 8
./build/tools/model_asset_editor/bin/EliteAssetEditor.exe
```

Manual smoke in both TREE and GRAPH:

- switch LODs;
- switch TREE <-> GRAPH radios;
- verify `CHECK` remains the far-right final action;
- verify legacy cleanup control;
- toggle transform links/markers;
- move explode slider and reset to 0%;
- select semantic parts and use tree bulk actions;
- edit a motion connection;
- inspect/repair visual bindings;
- in GRAPH select a mesh, set ROOT/A/B, create/select/edit a structural link.

The old aggregate `tests/architecture_contracts/check_model_asset_editor.py` has a known stale assertion for `ManifestMagicV4` in the former binary monolith. Do not interpret that specific failure as a SEMANTICS regression; update that contract when closing the binary translation-unit gate.

## After visual acceptance

1. **Close binary data architecture**
   - compile every binary layer as an independent CMake translation unit;
   - remove `.cpp` aggregation from the facade;
   - update the stale aggregate architecture contract;
   - run v4 save/load smoke.

2. **Return to whole-editor decomposition**
   - map the application-level dependency graph;
   - isolate controller/orchestration from state and views;
   - isolate pure domain logic from DOM/THREE/send effects;
   - isolate runtime/3D adapters and persistence/transport;
   - add contracts so a defect in one logical layer affects the minimum practical area.

3. **Only then continue production v5 implementation** under the separated binary boundaries.
