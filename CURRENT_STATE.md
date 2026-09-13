# Elite — CURRENT STATE

**Updated:** 2026-09-14
**Authoritative working branch:** `chatgpt/mae-v01075-semantic-workflow-motion-v5`
**Model Asset Editor candidate:** v0.10.78

## Accepted / frozen baseline

- SOURCE / LODS / GEOMETRY / SURFACES remain accepted unless a regression forces a targeted repair.
- SEMANTICS compact workflow remains accepted.
- MODEL ROOT is implicit identity root and is not serialized as a semantic node.
- PHYSICS is Model Asset authoring metadata; game-world runtime simulation is out of scope here.
- production Model Asset binary remains v4; v5 remains draft.

## User-local verification before v0.10.78

On 2026-09-14 the user verified v0.10.77 locally:

```text
MODEL ASSET EDITOR APPLICATION STATE: PASS
MODEL ASSET EDITOR ORCHESTRATION LAYERS: PASS
MODEL ASSET EDITOR TRANSPORT LAYERS: PASS
MODEL ASSET SEMANTICS WORKSPACE LAYOUT: PASS
```

The local CMake build reconfigured for the new transport JS files, regenerated the UI resource pack, linked `EliteAssetEditor.exe`, and staged the MinGW runtime without an error in the supplied log. A manual runtime smoke was not explicitly reported.

## Whole-editor decomposition

Current completed boundaries:

1. authoritative application state / reducer / controller;
2. dedicated application bootstrap;
3. workflow transition effect adapter;
4. declarative workflow stage renderer registry;
5. WebSocket lifecycle and reconnect;
6. JSON command + diagnostic transport;
7. ELWIR001 binary codec and binary transfer bookkeeping;
8. declarative backend JSON message router;
9. asset metadata/full-payload acceptance effect;
10. WORKING SAVE persistence acknowledgement;
11. settings persistence acknowledgement/error handling.

Dependency direction:

```text
UI / feature command
        ↓
application / workflow
        ↓
command transport
        ↓
WebSocket/backend

backend JSON / binary
        ↓
transport decode
        ↓
session message router
        ↓
asset / persistence / feature effects
        ↓
view + authored asset state
```

Transport does not own backend message semantics. Session/persistence effects do not construct WebSocket or THREE runtime objects.

## Progress

Estimated whole-editor physical separation after v0.10.78: **~83% by architectural layers**.

Remaining large surfaces:

- THREE / viewport runtime and scene lifecycle;
- physical extraction of `EditorViewState` plus remaining view adapters;
- final HTML shell cleanup / architecture closure.

## Binary subsystem parallel debt

The binary subsystem is logically split but still needs its independent-CMake-translation-unit closure: direct source registration, removal of `.cpp` aggregation, architecture-contract update, then v4 save/load build smoke.
