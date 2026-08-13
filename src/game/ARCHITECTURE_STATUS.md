# Runtime architecture status

This file records the migration boundary that must remain true while the game
is being split into an authoritative server runtime and a client presentation
runtime.

## Independent progress tracks

Do not use one stage number for all architectural work. Three tracks currently
exist:

1. **Authoritative runtime/time contracts** — server/client ownership, clocks,
   snapshots and debug-time isolation.
2. **Map subsystem decomposition** — Views, interactions, presentation builders,
   render backends and resource ownership.
3. **Server -> client presentation migration** — moving deterministic map and
   celestial work off the server while preserving server authority for gameplay.

A change in one track does not imply progress in the other two.

## Runtime/time contracts: transactional diagnostic branch

Accelerated universe-time diagnostics are an alternate presentation/simulation
branch, not a fast-forward command applied to the production world.

Required invariants:

- `ServerTimelineClock` is monotonic and never rewinds.
- `GameServer` exposes `UniverseClock` read-only; timeline changes must pass
  through the server debug-session API so revision/session bookkeeping cannot be
  bypassed.
- `UniverseClock` may jump when a debug timeline is entered, rescaled or left.
- Every such timeline contract change increments `universeTimelineRevision`.
- `SnapshotMetadata`, simulation snapshots and every map response carry that
  revision.
- No interpolation buffer may use endpoints from different revisions.
- Gameplay ship transforms, motion state, controls, sensors, repair state and
  detached-fragment state remain frozen while the diagnostic branch is active.
- `UniverseDiagnosticTrajectorySession` owns alternate trajectories for every
  live ship that can be published to a client (player, production NPC, and
  diagnostic motion probes). Entry is all-or-nothing: a partial branch is
  rejected rather than mixing accelerated and frozen production ships in one
  snapshot/timeline revision. Leaving the mode discards that session; it never
  commits diagnostic coordinates into production state.
- Celestial bodies, orbital hubs and orbital static infrastructure are derived
  from absolute universe time. They do not require a checkpoint: after a
  timeline rewind they are recomputed from the new canonical time. Their
  position and velocity caches must be derived from the same epoch.
- Client synchronization that can switch timeline revision is consumed before
  map preparation and input. A branch change must never land between map
  picking and rendering in the same application frame.
- Active diagnostic mode is runtime state and must never be persisted as a
  startup Debug Control default.

## Simulation / replication publication boundary

Authoritative fixed-step simulation and client replication now have separate
lifecycles. `GameSimulation::update()` advances world state; it does not build a
throw-away `SimulationSnapshot` every fixed tick. `GameServer` explicitly
materializes a replication snapshot only on its publication cadence (or an
explicit forced publication).

Required invariants:

- snapshot DTO construction must not return to the fixed simulation step;
- replication dirty flags are consumed only by a snapshot that is actually
  published;
- ship structural-graph resend lifetime is measured in publications, not
  simulation ticks;
- this is the first seam toward a dedicated server runtime. The snapshot builder
  still lives on `GameSimulation` temporarily and can be extracted behind a
  server-side replication component after the headless compile boundary is in
  place.

## Headless server compile boundary

Shared assembly geometry is now split at the CPU/GPU ownership boundary.
`ObjectAssembly` and `AssemblyMeshLibrary` contain only local CPU-side definition
data (mesh topology, bounds, module hierarchy and logical transforms) that may be
loaded independently by authoritative/headless code and by the client. OpenGL
objects are owned by the render-side `AssemblyGpuLibrary` as a presentation-only
sidecar derived from the same CPU assembly.

Required invariants:

- authoritative/server headers must be includable without glad/OpenGL include
  paths or libraries;
- `ObjectAssembly` must never own `MeshGPU`, `GLuint` or GPU-ready state;
- `AssemblyMeshLibrary` must never allocate/upload GPU resources;
- render code may derive/cache GPU resources from the shared CPU definition, but
  the shared assembly/simulation dependency may not point back into GPU/render
  resource ownership;
- server and client may each load the same static assembly/descriptor library
  locally. Static mesh/definition payloads are not replication state.

This establishes the first compile-time headless seam. A separate
`EliteServer` executable target is still pending, but local authoritative
execution now owns `ServerRuntime`/`GameServer` on a dedicated worker thread.

## Map subsystem decomposition

Detail/Hub have completed the Stage-6D ownership split: their backends and
shared celestial render services no longer depend on privileged facade access.
Galaxy/System still use the shared facade/low-level `.inl` backend pipeline, so
the map decomposition is not finished.

## Render-style boundary

Render style is a client-side presentation policy, not world state. The planned
wireframe/technical renderer and anime/cel-shaded renderer are mutually exclusive
ways to visualize the same `ClientWorldState`, map presentation state and
authoritative entity identities.

Required invariant:

- the server, snapshots and persistent world records never branch on render style;
- physics, activation, map membership and entity identity are identical for every
  style;
- a visual asset may expose semantic material/surface slots, but each client
  renderer decides how those semantics look;
- switching style must not require rebuilding or re-authoring the authoritative
  world.

## Runtime entity policy foundation

The first policy layer for the next migration stages is now explicit and
independent of existing production object code:

- `EntityType` identifies what a persistent/runtime entity is.
- `MotionModel` identifies the mathematical law currently owning its motion.
- `SimulationMode` identifies how much server work is currently allocated to it.
- `TimelineDomain` and `AuthorityPolicy` identify the authoritative clock/owner.
- client `PresentationPolicy` is resolved per observer; it is not stored as a
  permanent property of the entity. The same dynamic player ship is locally
  predicted for its controller and snapshot-interpolated for remote clients.
- scheduled materialization is fenced: an entity cannot jump directly from
  `Scheduled` to `Active`; it must enter `Prewarm`, establish a valid dynamic
  state, and only then become active. Collapse back to scheduled/coarse motion
  is similarly explicit.

These contracts are currently infrastructure and regression guards. Existing
production ships/hubs/modules have **not yet been migrated** wholesale to the new
policy objects.

The Hub Motion Lab presentation investigation is now accepted as a stable
server-to-render baseline:

- `ClientPresentationClock` owns the single delayed render playhead derived from
  authoritative server time and received snapshot history;
- `SnapshotPresentationWindow` selects one adjacent snapshot pair and one alpha
  shared by remote dynamic objects and interpolated reference frames in a frame;
- mature snapshot history must not silently degrade into newest-snapshot hold;
- the local controlled ship keeps fixed-step prediction/reconciliation, while a
  presentation-only copy is advanced by the remaining fixed-step accumulator so
  the camera is not fed a 50 Hz staircase;
- the fixed predicted state is never overwritten by the fractional presentation
  sample;
- deterministic analytic presentation remains time-derived rather than
  snapshot-stepped.

The accepted end-to-end capture after this change showed no oldest/newest clamps,
an interpolation bracket on every captured frame, sub-millimeter SLOW remote
error, ~1.4 mm maximum FAST remote error, and micrometer-scale MATCH error when
compared on the same delayed timeline. These runtime measurements are acceptance
evidence, not hard-coded gameplay constants; automated tests use looser
architecture-safe thresholds.

## Server -> client presentation migration

Functional migration is currently at **Migration Stage 3F in progress**:

- Stage 0: client-facing state stopped depending directly on `GameServer`;
  `IGameSession`/`ITransport` and the local host own the server boundary.
- Stage 0B: the in-process loopback transport no longer owns a `GameServer&`.
  Client and server now see distinct `ITransport` / `IServerTransport` protocol
  surfaces, while `ServerRunner` is the only runtime bridge that consumes client
  messages, advances authority and publishes replicated values. The local
  loopback queues are mutex-protected and `ServerRuntime` is constructed,
  advanced and destroyed on a dedicated `ServerWorker` thread.
- Stage 1: the client owns `StarAtlasDatabase` and
  `CelestialRuntimeRegistry` and can resolve deterministic celestial state from
  synchronized universe time.
- Stage 2: predictable celestial presentation fields (time/orientation) for
  Detail/Hub are reconstructed client-side.
- Stage 3A: the System-map celestial layer is client-owned. `GameServer` no
  longer serializes `SystemMapBody` rows or resolves celestial state for a
  normal System-map request. `ClientMapService` combines the authoritative
  dynamic-object response with its local `StarAtlasDatabase` and
  `CelestialRuntimeRegistry` at the **same response universe-time epoch**. The
  optional server motion CSV may still resolve celestial state explicitly when
  that diagnostic is enabled.
- Stage 3B: ordinary player/NPC System-map ships are reconstructed from exact-epoch normal replication history instead of a second map-specific ship channel.
- Stage 3C: Galaxy systems/objects are reconstructed from the client-local `StarAtlasDatabase`; `GameServer` now emits only authoritative Galaxy world overlays (currently jurisdiction) plus universe epoch/date.
- Stage 3D: `StarAtlasDatabase` is now genuinely endpoint-local static data. Client and server load the packaged/source catalog independently; the obsolete `PresentationDataMessage` / `StarAtlasRequest` transport path is removed. `SessionWelcome` carries only a catalog schema version + deterministic content fingerprint, and the client refuses synchronization when its local physical catalog is incompatible with the server.
- Stage 3E: production System-map infrastructure and orbital hubs no longer come from `GameServer::buildSystemMapSnapshot()`. `SimulationSnapshot` publishes ordinary authoritative hub state plus static-object instance/navigation facts. `ClientWorldState` retains that history and `ClientMapService` samples hubs/static infrastructure at the exact `SystemMapResponse::metadata.serverTimeSeconds`, then converts them to `SystemMapObject` rows locally. Static System-map name/galactic placement are also rehydrated from the endpoint-local StarAtlas. The System-map response is now an epoch anchor plus explicit diagnostic probes, not a second production world-state channel.
- Stage 3F: Details presentation is client-composed. `DetailMapResponse` now carries only the semantic `DetailTarget` plus authoritative server/universe epoch metadata. The client resolves deterministic body/ring/environment state from its endpoint-local StarAtlas/CelestialRuntime at that universe epoch and samples ships, hubs and complete local static infrastructure from retained ordinary `SimulationSnapshot` history at the exact response server epoch. Static-object and hub world velocities are first-class replication facts so Details no longer needs `GameServer::buildDetailMapSnapshot()` or its dynamic refresh path.


The clock/revision work that followed is infrastructure for this migration.
First-class entity system membership and the response-epoch rule are now the
main safety seams for continuing Stage 3 without mixing coordinate domains or
map epochs.

### Next functional migration stage

**Migration Stage 3 is in progress:** deterministic System-map celestial bodies,
ordinary real ships, production static infrastructure and orbital hubs are now
client-composed from local catalogs plus exact-epoch ordinary replication.
Details has joined the same model: the server only acknowledges target + epoch,
and the client composes the scene from local celestial state plus exact-epoch
replicated entities. The complete Hub DTO is still server-built.

The server should eventually publish compact authoritative dynamic entity state
(ships, hubs/infrastructure identities and gameplay-owned transforms/bindings),
while the client combines that stream with its local catalog/celestial runtime
to construct Galaxy/System/Detail/Hub presentation snapshots.

During the partial migration, client-derived celestial translation for System
and Details is evaluated at the exact universe-time epoch carried by the server
response, while dynamic entities are sampled from normal replication at the same
response server-time epoch. Hub remains on the narrower Stage-2 bridge until its
scene composition migrates to the same model.

## Authoritative world bootstrap

The initial dynamic world is data-driven through `initial_world_state.json`.
Player start, physical-system map facts and orbital hubs are validated before
scene construction. Production startup must fail loudly on invalid authored world
data; it must not fabricate a Sol/Earth promo fallback. Political/map labels are
server-owned world facts and must never be inferred on the client from numeric
physical-system IDs. Diagnostic scenarios may keep explicit Sol/Earth constants,
but those constants must stay inside diagnostic/promo code paths.

## Known migration blockers / debt

- `GameServer` still builds the authoritative Galaxy overlay plus the complete
  Hub snapshot. System-map celestial bodies, ordinary real ships, static
  infrastructure and orbital hubs, and the complete Details scene have migrated
  to client-side composition.
  Production dynamic System-map rows are sampled from retained
  `SimulationSnapshot` history at the exact
  `SystemMapResponse::metadata.serverTimeSeconds`, so System-map requests no
  longer form a second replication channel for those world entities. Only an
  explicit Hub Motion Lab analytic probe remains as presentation-only diagnostic
  payload. The remaining complete Hub DTO still contains deterministic/
  presentation data to remove in the next Stage-3 slice.
- The legacy duplicate Sol/Earth/Moon gameplay pass has been removed from
  `SceneRenderer`; gameplay and maps must consume canonical client presentation
  state instead of maintaining a second hard-coded celestial world.
- Runtime system membership is now first-class for ships, hub reference frames,
  static objects and sensor-space sources. Client interpolation/prediction and
  gameplay scene preparation are fenced by the same system domain: no render
  smoother may blend system-local coordinates across a system change, and
  presentation-only traffic/drones carry explicit system membership.
  The current dynamic simulation remains
  deliberately **single-active-system**: its celestial/gravity/reference caches
  carry an explicit active system id, foreign-system runtime creation is rejected,
  and foreign-system entities are frozen rather than advanced by the active
  system's AI/physics/repair/gravity/corridor/radar/signal domain. This is a
  safety contract, not multi-system
  simulation. A real inter-system transfer transaction and simultaneous per-system
  dynamic contexts remain pending.
- Static-object spatial authority is split from map metadata: `systemId` is fixed
  at spawn/transfer, `orbitalParentBodyId` owns physical orbital parenting, and
  `mapParentBodyId` is presentation/index metadata only.
- System-map player/NPC ships are reconstructed on the client from ordinary
  authoritative replication history and filtered by explicit `systemId` before
  conversion to map coordinates. Cross-system `WorldPosition` values are never
  interpolated. The current System-map interaction path still has explicit
  picking and selection only for hubs/bodies, so ship selection/Details
  navigation remains an unfinished functional contract.
- `ServerRuntime` is now the sole in-process production owner of `GameServer`.
  `LocalGameHost` composes the gameplay transport, a separate local debug/control
  channel, and `ServerRuntime`; bootstrap world configuration and the initial
  authoritative publication happen inside the server runtime. Client identity is
  also server-owned: a one-time `SessionWelcome` publishes `controlledEntityId`,
  while recurring simulation snapshots do not repeat that stable metadata and
  client command packets carry intent only—they cannot select an `EntityId` to
  control.
- Debug tools no longer expose `ServerRuntime` or read `GameServer` memory.
  `LocalDebugSessionControl` provides separate `IDebugSessionControl` (tool side)
  and `IServerDebugChannel` (server side) endpoints. Mutating debug operations are
  queued requests; structural snapshots and universe-time status return as copied
  value state with revisions. `SpaceState` waits for a newer revision before
  refreshing the HTML debug panels, so the UI no longer assumes that an
  authoritative mutation completed synchronously inside its command handler.
  Gameplay and debug local channels are mutex-protected because their two
  endpoints now execute on different OS threads.
- `ServerWorker` is the execution owner of `ServerRuntime`. The runtime itself is
  a local variable of the worker thread, so `GameServer` is constructed,
  advanced and destroyed on that same thread. Local play now uses a bounded
  one-batch asynchronous pipeline: `LocalGameHost::advance()` waits only for the
  previous authoritative batch when necessary, then submits the current elapsed
  time and returns the previous completed result. At most one server batch can
  be in flight, so server work overlaps client/render work without an unbounded
  backlog or silent fixed-step/input loss. If the server exceeds the available
  frame budget, the next submission applies back-pressure until that one batch
  completes; `ServerRunner` remains the sole owner of fixed-step debt/catch-up
  policy.
- Procedural cloud morphology is presentation-only wall-time work. Its timing is
  intentionally separate from universe time, but texture generation still runs
  synchronously on the render thread and remains a performance/LOD concern.

- Signal reception still reads gameplay reception thresholds from the legacy
  `render/VisualTuning.h` location. That header currently carries no OpenGL/GPU
  dependency, so it does not block the headless compile seam, but its ownership
  is mislabeled. Keep signal/radar behavior untouched during the server split;
  move only the neutral shared tuning contract in a later isolated cleanup.

## Fixed-step player prediction/reconciliation contract

Client prediction and authoritative player motion share one fixed-step input
stream. `ShipControlState::controlTick` is a prediction-step sequence, not merely
a version number for a coalescible state packet. In normal gameplay the server
consumes at most one queued sample per authoritative fixed step and publishes
`acknowledgedControlTick` from the sample actually consumed. The client may then
remove and replay pending predicted steps using that acknowledgement without
inventing or deleting simulation time.

Several control samples must never be collapsed into the newest sample while
advancing the acknowledgement across all of them. Doing so makes the
acknowledgement claim that client-predicted fixed steps exist in the
Authoritative snapshot when they were never simulated, which can force the
predicted/render target backwards on snapshot arrival. Accelerated diagnostic
branches are the explicit exception: gameplay prediction is disabled there, so
pending production controls are discarded and acknowledged without being
applied to production motion. A fixed-step stream is not silently truncated on queue
overflow; a future remote-network implementation must use an explicit
reconciliation reset/disconnect contract rather than skip predicted steps.

## Simulation activation foundation — Stage 3A

The first server-side activation primitive is now explicit and tested:

- `SpatialBounds` derives a conservative interaction radius from the existing
  logical object dimensions instead of inventing a second size contract.
- `InteractionHorizon` evaluates relative motion using closest point of
  approach over a bounded look-ahead interval.
- object size, physical safety padding and gameplay effect range are separate
  inputs.
- radar/sensor visibility is intentionally excluded from simulation
  activation; sensor knowledge and network relevancy remain separate layers.

Stage 3B consumes this math as a physical shadow demand on real ships while
keeping every production ship fully simulated. Stage 3C adds a persistent,
stabilized activation plan: gameplay systems may raise demand explicitly for
combat/projectile/docking interactions, promotions are immediate, and
demotions use `Active -> Prewarm -> Coarse` hysteresis. Radar/sensor visibility
remains a separate perception domain and cannot raise activation directly.

Stage 3D replaces the temporary all-pairs planner with a conservative spatial
broad-phase. Candidate pruning uses a per-system co-moving velocity origin so
shared orbital bulk velocity does not inflate the search radius; exact CPA remains
the authority for the final interaction decision. The planner remains 5 Hz.

Stage 3E is the first production execution gate, deliberately limited to NPC
tactical AI think cadence. `Active` NPC AI keeps the existing fixed-tick cadence,
`Prewarm` runs at roughly 10 Hz and `Coarse` at roughly 1 Hz. The last computed
control continues to be applied while dynamic physics, HubTactical integration,
signals and snapshots remain full-rate. Physics must not consume planned mode
until coarse/scheduled motion and materialization/dematerialization semantics are
explicit.

### Static definition / runtime replication boundary

- `ObjectModuleSnapshot` is runtime-only: module id, state, health and live support count.
- Static module definition data (hierarchy, subsystem, policies, mesh-part ids, support topology, health limits) lives in the local descriptor catalogs on both server and client and is not repeated in ordinary snapshots.
- `ShipSnapshot::typeId` / `ObjectSnapshot::type` are the compact catalog keys used to join authoritative runtime state to those local definitions.
- Rich debug/presentation views are rehydrated client-side from `ModuleDescriptor + ObjectModuleSnapshot`; debug tooling is not a reason to widen the gameplay replication DTO.

## Client localization and sky-culture presentation contract

Player-facing localization is a client presentation concern and must not widen
server/protocol authority. `Application` owns one global UI locale and
`LocalizationService` resolves player-facing strings and catalog display names
with the mandatory fallback chain `exact locale -> base locale -> English`.
Authoritative server/simulation code continues to use stable ids and does not
load translation tables.

Current F12 service chords are protected player-facing behavior:

- plain `F12`: current Local/Hub navigation;
- `Ctrl+F12`: constellation overlay visibility;
- `Alt+F12`: active sky-culture topology;
- `Ctrl+Alt+F12`: global player-facing UI language, including menus/maps and
  constellation labels. This chord is global and does not require `SpaceState`.

Sky culture and UI language are deliberately independent state. Sky-culture
**topology only** remains under `assets/data/galaxy/sky_cultures`; all
player-facing culture/constellation names live under `assets/localization/sky`.
The culture never owns a second language selector. Missing labels fall back to
English. The current topology contains the accepted IAU/Western set, the curated
Chinese 28-lunar-mansion + surrounding asterisms set, and Hawaiian star lines.
Topology-only support stars remain separate from the visible top-3000 catalog.

`assets/localization` is the single editable localization root.
`LocalizationService` discovers every `*.json` recursively in deterministic path
order. Malformed/unsupported files are logged and skipped at runtime; duplicate
UI keys or catalog stable IDs are rejected with first-valid-definition wins.
`languages.json` is the only locale enable/order registry. UI text is divided by
category (`ui/maps`, `ui/cockpit`, `ui/common`), while world/game display names
use stable IDs. English remains mandatory for every player-facing record.

Gameplay star systems are deliberately isolated one file per system under
`assets/localization/world/star_systems`. Each file owns only that system's
display name plus its celestial-body/hub names, and the loader rejects body IDs
whose system prefix does not match the containing `system_id`. Filenames are
human-readable and have no identity meaning. Interstellar objects, navigation
regions, manufacturers, ship/station/beacon types and equipment have separate
localization categories and may grow without a manifest edit.

WebUI does not own a second translation file. `Application` loads the unified
localization tree once and exposes an in-memory `runtime_ui.json` bundle through
`HtmlUiServer`; native OpenGL UI and WebUI therefore share the same source and
fallback rules. Release packaging may later compile the development JSON tree
into a single binary/obfuscated localization package without changing callers.

Manufacturer-specific cockpit legends remain a distinct future presentation
language domain from the global UI locale. The current global cockpit/service
overlays use `assets/localization/ui/cockpit`; manufacturer-native instrument
legends can be layered separately when ship definitions begin owning cockpit
language.
