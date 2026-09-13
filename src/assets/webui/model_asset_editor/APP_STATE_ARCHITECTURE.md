# Model Asset Editor application-state architecture

## Scope

This layer controls the **editor application**, not runtime game entities and not the authored `ModelAsset` payload itself.

The Model Asset Editor prepares renderer/asset data, including SOURCE, LOD, GEOMETRY, SURFACES, SEMANTICS, PHYSICS, DAMAGE and final validation/build metadata. Runtime simulation of a concrete ship/station instance belongs to the game and is outside this state machine.

## Dependency direction

```text
UI / backend / viewport event
            |
            v
          action
            |
            v
 application controller
            |
            v
          store
            |
            v
         reducer
            |
            v
  authoritative app state
            |
            +--> selectors / view models --> render
            |
            +--> effect adapters (DOM / THREE / send / filesystem)
```

Reducers are pure. They do not own DOM, THREE, RPC, filesystem, timers, rendering or asset serialization.

## Top-level workflow

The canonical authoring sequence is defined in `app/workflow.js`:

```text
SOURCE
  -> LODS
  -> GEOMETRY
  -> SURFACES
  -> SEMANTICS
  -> PHYSICS
  -> DAMAGE
  -> VALIDATE
  -> BUILD
```

`PHYSICS` here means authored physical/collision/rigid-body metadata for the asset. It does not mean runtime world simulation.

The reducer owns the current stage, previous stage and transition revision. A stage change is an application action, not an ad-hoc mutation of navigation state.

## State boundaries

### Application reducer owns

- workflow stage and transition revision;
- session control flags such as dirty/busy/locale;
- scalar editor-control/selection/mode values that drive feature behavior.

### `EditorViewState` remains authoritative for

- active/rendered LOD;
- RenderNode/mesh/semantic selection;
- per-LOD visibility and isolation;
- loaded/resident LOD state.

The root application snapshot composes the reducer state with `EditorViewState`; it does not duplicate viewport ownership.

### Intentionally outside the reducer

- authored `ModelAsset` domain data;
- THREE scenes, groups, meshes and material/runtime handles;
- geometry caches and preview caches;
- timers / RAF handles;
- WebSocket/backend transport;
- filesystem and persistence effects.

Those are domain data or effect/runtime objects, not application control state.

## Compatibility projection

The current editor shell is still physically located in `model_asset_editor.html`. To avoid a dangerous all-at-once rewrite, `app/state.js` installs accessors over the legacy `state` object. Existing code can temporarily keep expressions such as:

```js
state.wizardStage = 'physics';
state.busy = true;
```

but those writes no longer own the value. They dispatch through `ApplicationController` into the reducer/store, and reads project the authoritative value back out.

This compatibility layer is deliberate and temporary. It lets feature/effect extraction proceed independently while making the state authority explicit now.

`effects/i18n.js` currently installs the projection because its factory is the first stable effect bootstrap invoked immediately after the legacy state object is constructed. This is a migration hook, **not** the desired final ownership boundary. When the HTML shell is reduced to bootstrap-only code, `installApplicationState()` must move into that bootstrap.

## Remaining shell decomposition

State authority and physical file ownership are separate problems. After this state-control pass, the remaining application decomposition is:

1. move application bootstrap out of `effects/i18n.js` into a dedicated shell/bootstrap module;
2. move stage transition side effects out of `model_asset_editor.html` into application/effect adapters;
3. replace the remaining stage-view `if` dispatcher with a declarative renderer registry;
4. split backend/session, persistence and THREE viewport orchestration into dedicated adapters;
5. continue shrinking `model_asset_editor.html` until it contains only static shell markup plus bootstrap imports.

The important invariant is already established: **feature/effect code may request a change; the application state layer owns the resulting control state.**
