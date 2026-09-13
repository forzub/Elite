# Model Asset Editor application-state architecture

## Scope

This architecture controls the **Model Asset Editor application**. It does not own runtime game entities and it does not turn authored `ModelAsset` data into game-instance state.

The editor workflow is SOURCE -> LODS -> GEOMETRY -> SURFACES -> SEMANTICS -> PHYSICS -> DAMAGE -> VALIDATE -> BUILD. `PHYSICS` is authored asset-side collision/rigid-body metadata, not runtime simulation.

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
       reducer/store
            |
            v
 authoritative app control state
            |
            +--> selectors / view models
            +--> effect adapters

EditorViewState ---------------------> viewport selection / LOD / visibility
Transport bridge -------------------> command/WebSocket transport
Session router ---------------------> asset/session effects
Viewport adapters -----------------> THREE runtime
```

Reducers remain pure: no DOM, THREE, WebSocket, filesystem, timers or rendering.

## State ownership

### Application reducer

Owns workflow state, dirty/busy/locale and serializable editor-control fields.

### `app/editor_view_state.js`

Owns active/rendered LOD, RenderNode/mesh/semantic selection, per-LOD visibility/isolation and loaded/resident LOD sets. Compatibility projections onto the runtime `state` object are installed by `installEditorViewProjection()`; the implementation no longer lives in the HTML shell.

### `app/runtime_state.js`

Creates the non-reducer runtime container: authored asset reference, THREE groups/handles, caches and feature effect state. It is intentionally not the application reducer.

### `app/view_invariants.js`

Owns transition snapshots and invariant enforcement between active LOD, rendered scene LOD and resident payload state.

## Effect boundaries

- workflow orchestration: `effects/workflow.js`;
- transport: `transport/*`;
- backend message/session acceptance: `session/*`;
- persistence acknowledgements: `persistence/*`;
- THREE scene/runtime/geometry/overlays/attachments/picking: `viewport/*`;
- feature calculations and HTML builders: feature/domain modules.

`transport/bridge.js` is a narrow late-bound bootstrap bridge. It gives early-created feature/effect adapters stable command/diagnostic functions before the concrete WebSocket transport is composed, avoiding temporal-dead-zone bootstrap coupling without moving transport ownership into those features.

## Shell rule

`model_asset_editor.html` remains the composition host and still contains residual feature-specific DOM bindings where a separate module would add little isolation value. It must not re-acquire implementations for application state, workflow routing, backend message routing, persistence, WebSocket/binary transport, EditorViewState, or THREE viewport core/adapters.

That boundary is enforced by architecture contracts.
