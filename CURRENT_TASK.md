# Elite — CURRENT TASK

**Updated:** 2026-09-13  
**Branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`

## Goal

Reduce Model Asset Binary blast radius to the minimum practical unit: package orchestration, validation, storage policy, manifest I/O, LOD I/O, mesh encoding and each logical manifest domain must be independently owned. A fix in one domain should not require editing unrelated domains or the package controller.

## Active work order

1. **Accept the binary layer split candidate**
   - `ModelAssetBinary.cpp` is facade-only;
   - `binary::controller` owns only sequencing and migration routing;
   - validation, storage, manifest I/O and LOD I/O are separate layers;
   - mesh payload encoding is separate from LOD file framing;
   - FourCC dispatch is separate from codec implementations;
   - manifest codecs are split into metadata, semantics/state, collision, sockets, damage/openings/repair, structural, LOD metadata and legacy compatibility files;
   - production format remains v4.

2. **Verify the candidate**
   - run `python tests/architecture_contracts/check_model_asset_binary_layers.py`;
   - run existing model-asset architecture contracts;
   - build `EliteModelAsset` / `EliteAssetEditor` with the normal MinGW+CMake toolchain;
   - perform v4 save/load smoke before any v5 implementation work.

3. **Next architecture gate — independent translation units**
   - add each `src/model_asset/binary/*.cpp` and `binary/chunks/*.cpp` file directly to `EliteModelAsset` in CMake;
   - remove temporary `.cpp` aggregation from `ModelAssetBinary.cpp`;
   - keep all public/internal APIs unchanged during this move;
   - add a contract that rejects `.cpp` includes in the facade after the CMake split.

4. **Then implement v5 under the separated architecture**
   - explicit LE primitives and v5 common header/directory;
   - package-id matching;
   - v5 manifest container and independent LOD container;
   - per-domain v5 chunk codecs without cross-domain ownership;
   - keep v4 reader/writer production-active until acceptance.

5. **SEMANTICS UI remains queued, not discarded**
   - visually enforce `STRUCTURE -> VISUAL BINDINGS -> KINEMATICS -> STRUCTURAL LINKS -> CHECK`;
   - move engineering diagnostics/tests under `?`;
   - expose one obvious task/action at a time;
   - slightly lighten the active top-level wizard tab.

## Definition of done for binary architecture

The architecture phase is complete only when:

- public facade contains delegation only;
- controller contains process sequencing only;
- each domain codec can be edited without touching unrelated codec files;
- validation and filesystem policy have no codec knowledge;
- manifest and LOD I/O have no migration/process orchestration;
- legacy compatibility is isolated;
- all binary layer sources compile as independent translation units;
- architecture contracts and existing v4 save/load tests pass.

Until the independent-CMake-source gate is complete, call the current state **layer split candidate**, not final binary architecture.
