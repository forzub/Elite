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

This establishes the shared compile-time headless seam. A standalone
`EliteServer` executable now also builds this same authoritative runtime with
client/render/UI dependencies disabled; local play still owns
`ServerRuntime`/`GameServer` on a dedicated worker thread.

## Map subsystem decomposition

Detail/Hub have completed the Stage-6D ownership split: their backends and
shared celestial render services no longer depend on privileged facade access.
Galaxy/System still use the shared facade/low-level `.inl` backend pipeline, so
the map decomposition is not finished.

### Map presentation atomicity

Map data readiness and presentation ownership are separate states. Flight is an explicit F1-F4 presentation target rather than the absence of UI; F5-F8 services and F9-F12 navigation are peer direct selectors under one `GamePresentationCoordinator`. Physical F1-F12 presses are message-backed `Window` events rather than frame-polled presentation latches. A first F9-F12 request resolves the exact player-navigation target before `sceneTarget` is armed, and the committed outgoing presentation remains visible until the requested map scene and the persistent side-panel payload for the same request generation are both prepared. The map panel owns a WebView separate from the fullscreen service front/back pair, so service navigation cannot evict or re-prewarm it. The former `playerNavigationMapEntry*` state machine is removed; `SpaceState` is only the Navigation scene producer.

Framebuffer crossfades that remain inside the Navigation domain are renderer-owned transactions. The renderer that produced the outgoing map image captures that exact completed framebuffer before an internal map-to-map mode change; after the outgoing frame is swapped, the destination renders one full warmup frame beneath an opaque snapshot and only then begins the smootherstep blend. Cross-domain Flight/Service/Navigation ownership is not a renderer crossfade and belongs exclusively to `GamePresentationCoordinator`. Capturing an arbitrary back buffer at the beginning of a later frame is not a valid source-frame contract.

The System Map side panel remains a child-HWND GameWebView and therefore cannot be alpha-composited pixel-perfectly inside the OpenGL framebuffer. Its guarantee is **prepare while native-hidden -> show only complete state**, synchronized with the OpenGL transition. A future requirement for one compositor-level alpha blend across panel + map would require a native/OpenGL panel or WebView2 composition/offscreen path rather than timing hacks.

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

These contracts are no longer purely shadow infrastructure. **Runtime Stage 4A**
lets the stabilized `Active / Prewarm / Coarse` plan control NPC tactical AI,
internal service systems (reactor/thermal/cooling/life-support), and structural/
repair maintenance. **Stage 4B** additionally splits ship motion into expensive
control/rate evaluation and cheap kinematic propagation. `Active` remains on the
established 50 Hz full-control path; `Prewarm` refreshes motion control at roughly
25 Hz and `Coarse` at roughly 5 Hz, while orientation and HubTactical translation
continue fixed-step propagation using the last authoritative rates/engine
acceleration between control evaluations. Client prediction still uses the
unchanged full `SharedShipPhysics::integrate()` wrapper. GameSimulation still
produces a full authoritative source set, but Multiplayer Stage M7 now decimates
**per-session ship transport rows** independently: Controlled/Tactical stay on
normal publication cadence, Nearby/Coarse are less frequent, and omission means
retain. Signals/objects/hubs still use the previous publication cadence. True
`Scheduled` materialization/collapse remains future work; production ships/hubs/
modules have **not yet been migrated** wholesale to persistent runtime-policy records.

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

Functional migration is currently at **Migration Stage 3G complete for the four primary map views**:

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
- Stage 3E: System presentation is fully client-composed. Production infrastructure, hubs and ships come from retained ordinary `SimulationSnapshot` history, while static/celestial data comes from the endpoint-local StarAtlas/CelestialRuntime. F10 anchors all of those inputs to the metadata of the latest already accepted `SimulationSnapshot`; `SystemMapRequest/SystemMapResponse` and `GameServer::buildSystemMapSnapshot()` are removed, so opening the map performs no second server-time handshake. The Hub Motion Lab analytic probe is presentation diagnostics and is composed client-side as well.
- Stage 3F: Details presentation is fully client-composed from the semantic `DetailTarget`, endpoint-local StarAtlas/CelestialRuntime and retained ordinary replication at the latest accepted `SimulationSnapshot` epoch. `DetailMapRequest/DetailMapResponse`, deferred response waiting and response-epoch catch-up are removed; F11 no longer performs a map RPC. Static-object and hub world velocities remain first-class replication facts.
- Stage 3G: Hub Map presentation is fully client-composed from `(systemId, hubId)`, endpoint-local celestial state and retained ordinary hub/module/ship replication at the latest accepted `SimulationSnapshot` epoch. The client reconstructs the tactical `prograde/radial/normal` frame (including frame angular velocity) from that coherent sample. `HubMapRequest/HubMapResponse` and their response-wait path are removed; F12 no longer performs a map RPC.
- Stage 3H: a real standalone `EliteServer` executable now boots the same authoritative `ServerRuntime` without `GameClient`, loopback client ownership, GLFW/OpenGL/Freetype/WebView/UI or render sources. `ELITE_BUILD_CLIENT=OFF` is a supported configure path, and the ready harness builds `EliteServer` in that mode and runs a finite authoritative fixed-step self-test. The temporary `HeadlessServerTransport`/`HeadlessDebugChannel` remain process-local harness endpoints, not a socket transport; normal standalone mode has empty inbound queues, while self-test may inject protocol messages to prove server routing.
- Runtime build provenance is operationally single-path on Windows/MSYS2: graphical client is `build/EliteGame.exe`, dedicated server is `build/headless_server/EliteServer.exe`, and process/runtime acceptance consumes those exact binaries. Compile-only scratch builds are namespaced under `build/tests/`; alternate long-lived server/client build directories are forbidden and cleaned by the canonical build helper.


The clock/revision work that followed is infrastructure for this migration.
First-class entity system membership and the response-epoch rule are now the
main safety seams for continuing Stage 3 without mixing coordinate domains or
map epochs.

### Next functional migration stage

**Migration Stage 3 presentation ownership is now functionally complete for
Galaxy/System/Details/Hub.** The server publishes authoritative IDs, world/runtime
facts and response epochs; the client combines those facts with endpoint-local
catalog/celestial state to construct the presentation snapshots. System, Details
and Hub dynamic entities are sampled from retained ordinary replication at the
exact response server-time epoch, while deterministic celestial state is resolved
at the response universe-time epoch.

The map-presentation migration and standalone headless executable boundary are
complete at this stage. Runtime Stage 4A/4B now consumes the activation plan for
real materialized CPU work while preserving fixed-step kinematic propagation and
the established Active trajectory path. Sparse replication is intentionally
paused behind the multiplayer session/interest boundary: omission cadence is a
per-client decision, not a property of the simulated entity alone. Multiplayer
M1/M2 establish server-owned sessions and multi-transport fan-out, M3 separates
local from remote human identity on the client, M4 makes navigation fully
session-derived on the server, M5 runs two real `GameClient` state machines
against one authoritative runtime, M6 establishes per-session ship interest plus
explicit retain/update/remove semantics, and M7 consumes that interest as actual
sparse per-entity ship cadence with canonical full bootstrap/re-entry hydration.
Stage M8A defines the process-independent wire boundary: a versioned, network-byte-order byte-stream frame with bounded payloads plus explicit codecs for SessionWelcome, ClientMessage, MapRequest and time sync. Stage M8B completes the data plane: SimulationSnapshot and MapResponse are serialized by one canonical ordered schema, then handed to an opaque byte-to-byte compression seam. Stage M8C adds a real standalone-Asio TCP byte stream plus typed transport adapters and validates the complete protocol over localhost kernel sockets. Stage M8D connects those adapters to separate EliteServer/EliteGame processes with server-owned admission and remote-session lifecycle.
Field-level delta compression should be designed only after packet/reliability and
baseline semantics are explicit. Persistent-world work then returns to true
`Scheduled <-> Coarse <-> Prewarm <-> Active` materialization/collapse and
multi-system runtime.

## Authoritative world bootstrap

The initial dynamic world is data-driven through `initial_world_state.json`.
Player start, physical-system map facts and orbital hubs are validated before
scene construction. Production startup must fail loudly on invalid authored world
data; it must not fabricate a Sol/Earth promo fallback. Political/map labels are
server-owned world facts and must never be inferred on the client from numeric
physical-system IDs. Diagnostic scenarios may keep explicit Sol/Earth constants,
but those constants must stay inside diagnostic/promo code paths.

## Known migration blockers / debt

- The four primary map views no longer use a server-built production Hub/Detail
  presentation DTO. Galaxy still receives authoritative world overlays, and
  System Map may retain explicit diagnostic presentation probes, but ordinary
  ships/infrastructure/hubs are sourced from normal replication. System/Details/
  Hub use exact response-epoch joins, so map requests are not a second dynamic
  world-state channel.
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

Stage 3E is the first production execution gate, initially limited to NPC
tactical AI think cadence. Runtime Stage 4A extends the gate to internal service
and maintenance lanes. Runtime Stage 4B then separates control-force/rate
evaluation from kinematic propagation: `Active` evaluates motion control every
fixed tick, `Prewarm` roughly 25 Hz, `Coarse` roughly 5 Hz, while orientation and
HubTactical translation continue to propagate every authoritative fixed tick.
The authoritative simulation/source snapshot remains full-presence, while M7 now
decimates per-session **ship** publication at the transport boundary. Explicit
sparse-retention semantics are regression-locked, first/re-entry publication is
hydrated from canonical retained server state, and destruction/interest exit uses
explicit removal. Signals/objects/hubs remain on their previous cadence for now.

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

### Multiplayer Stage M1-M7 — session authority, client identity, navigation, two-client acceptance and sparse replication

- M1 added a platform-neutral `ServerSessionId` and `ServerSessionRegistry`; authoritative
  connection/session identity is no longer the same concept as a ship `EntityId`. The later
  persistent-identity migration now stores `PlayerId` in the session registry and resolves
  `session -> PlayerId -> ControlRegistry -> EntityId` before authoritative commands enter queues.
- `GameSimulation` tracks
  an explicit set of player-controlled ships, so NPC authority and activation pinning no
  longer depend on the legacy singleton `m_playerId`.
- M2 turns `ServerRunner` into a multi-endpoint fan-in/fan-out boundary. Multiple
  `(IServerTransport*, ServerSessionId)` bindings drain inbound traffic before one shared
  authoritative `GameServer::update()`, then receive session-owned outbound responses.
  A connection never receives a private simulation tick.
- `ServerRuntime` can admit/detach secondary player-session transports. Each admitted
  session gets its own `SessionWelcome` and bootstrap snapshot; map responses retain their
  destination session, while time-sync responses return directly through the originating
  endpoint.
- The authoritative source snapshot remains full-world/full-presence, while each connection
  now receives its own sparse ship view. The snapshot session view is composed per controlled
  entity: `snapshot.session.playerNavigation` is never copied from a primary player.
- `EliteServer --self-test` now drives two process-local transport endpoints in one runtime,
  assigns two different controlled ships, verifies independent control acknowledgements,
  map/time-sync routing, per-session navigation, and secondary disconnect. The harness is
  not the future network transport.
- Registry reconnect semantics reject stale authority when a replacement live session has
  already claimed the same persistent `PlayerId`; current production reconnect/resume token
  handoff is still future work.
- M3 removes the client-side identity conflation between `ShipRole::Player` and **my local
  ship**. `SessionWelcome.controlledEntityId` is copied into `ClientWorldState` and gates
  prediction, fractional local presentation, player-system lookup and local System/Details/Hub
  map markers. A different replicated `ShipRole::Player` is a remote human entity and follows
  ordinary snapshot interpolation; it cannot receive local prediction merely because its role
  is `Player`.
- M4 removes the server-side singleton `m_playerNavigation`. Shared publication state carries
  no player's navigation identity; `copySnapshotForSession()` resolves navigation from the
  destination session's server-owned `controlledEntityId`. `GameServer` therefore has no
  primary-player navigation view to leak into another connection.
- The remaining **single active celestial-system runtime** is a world-materialization limit,
  not a session-navigation source. Its temporary resolver considers all player-controlled
  entities in the materialized runtime and refuses to arbitrarily choose a primary player
  when controlled ships name different systems. True split-system play remains a later
  multi-system-runtime task.
- M5 adds a real two-client acceptance gate: two `LocalLoopbackTransport` endpoints and two `GameClient` instances share one `ServerRuntime`, reach `Ready` independently, receive different controlled entities, preserve opposite local/remote identity, derive navigation from their own entity, and maintain independent numbered input/acknowledgement streams without bypassing `ITransport`.
- M6 introduces a server-owned per-session ship replication-interest policy that is separate from
  simulation activation. `Controlled / Tactical / Nearby / Coarse / None` describe transport cost
  demand from the destination session's controlled entity; they are explicitly **not** sensor or
  gameplay-visibility authorization.
- `SimulationSnapshot` carries explicit entity-set semantics: `FullAuthoritativeSet` preserves legacy
  omission=remove, while `SparseRetainMissing` means omission=no update and requires explicit
  ship/object/hub removal rows. `ClientWorldState` honors that distinction and materializes canonical
  retained history samples so existing interpolation/map samplers do not consume sparse holes directly.
- M7 makes the policy operational. `ServerRunner` owns per-connection publication memory and sends
  Controlled/Tactical ship rows at normal snapshot cadence, Nearby/Coarse rows at their target interval,
  and `None`/destruction as explicit removal. The first packet after interest re-entry is marked for
  hydration instead of being treated as an ordinary sparse-field update.
- `GameServer` retains a canonical field-complete replication snapshot by merging nested graph flags
  independently from entity-presence semantics. Initial/late-join bootstrap and re-entry hydration use
  that canonical source, so a client cannot join between dirty graph publications and receive only half
  of an existing ship's runtime state. Objects/hubs remain full-cadence in M7, with explicit lifecycle
  removal because the shared envelope is sparse.
- Stage M8A adds `WireProtocol.h`: ABI-independent magic/version/kind/length/sequence framing, bounded stream decoding and explicit connection/control-plane codecs. Stage M8B adds `WireBinaryCodec.h` + canonical `WireDataSchema.h` + top-level `WireDataCodec.h`, so one logical snapshot/map response becomes one raw byte buffer before compression/framing. The first production remote transport is intentionally a reliable ordered TCP byte stream; socket APIs stay below `ITransport`/`IServerTransport`. Field-level
  delta compression is deferred until packet ordering/reliability and baseline/version semantics are
  explicit rather than assuming every prior sparse packet arrived.
- Session/authority/runner code contains no Win32/POSIX socket primitives. Platform-specific
  networking stays behind transport adapters so the same authoritative runtime can build on
  Windows and Linux.

### Multiplayer Stage M8A — portable versioned wire control plane

- `src/game/network/WireProtocol.h` is the first process boundary that is independent from compiler/STL object layout. It never raw-copies `std::string`, `std::vector`, `std::variant` or aggregate protocol structs. Scalars use explicit network byte order; float/double transport their bit patterns through fixed-width integers.
- Every frame carries `ELIT` magic, protocol version, message kind, bounded payload length and a per-direction sequence field. `WireFrameDecoder` accepts arbitrary TCP-style fragmentation/coalescing and rejects bad magic/version/oversize frames.
- M8A codecs cover `SessionWelcome`, both `ClientMessage` variants, the remaining Galaxy `MapRequest` and time-sync request/response. Variable-length strings and whole-frame payloads are bounded before allocation/copy.
- M8A reserved `SimulationSnapshot` and `MapResponse` message ids; M8B now round-trips both payloads, including sparse replication lifecycle, hydrated runtime graph state, per-session state and the existing client-composed map response contracts.
- Initial process networking uses reliable ordered TCP. Optional future datagram channels must introduce their own loss/reordering/sequence/ack/baseline semantics rather than weakening the authoritative protocol by assumption.
- No Win32/POSIX socket types belong in `GameServer`, `ServerRuntime`, `ServerRunner` or the wire codec. M8C implements standalone-Asio TCP underneath the existing `ITransport` / `IServerTransport` seam; Asio itself is hidden in `TcpWireStream.cpp` and does not leak through public protocol/gameplay headers.
- M8A also fixes project-local include spelling that was only accidentally valid on case-insensitive Windows filesystems (`EntityId.h` vs `EntityID.h`, `Log.h` vs `log.h`). `check_case_sensitive_project_includes.py` keeps this Linux portability contract from regressing.

### Multiplayer Stage M8B — canonical ordered data schema

- `WireDataSchema.h` is the single protocol-order registry for data-plane DTO fields. Encode and decode both traverse the same `std::tie(...)` field tuple, so there are not two manually maintained field-order lists to drift apart.
- `WireBinaryCodec.h` is generic machinery only: primitives, bounded strings/vectors, variants, GLM vectors/matrices, validated enums and registered schemas. It has no ship/map-specific field list.
- `WireDataCodec.h` serializes one complete logical `SimulationSnapshot` or `MapResponse` to one raw byte buffer with an explicit data-schema version. Adding normal replicated fields should not require changes to framing, TCP, ServerRunner or compression.
- Compression is a separate byte-to-byte seam (`IWireCompressor`). It never sees entity/module counts or DTO types. M8B ships a `NoWireCompression` passthrough plus compression envelope metadata so M8C can transport the exact same pipeline and a future LZ4/Zstd implementation can be swapped in below serialization.
- The data-plane contract test exercises a sparse snapshot with lifecycle removals, kinematics, reference frames, signal/radar/damage state, ship systems, module/repair/detached-fragment graphs, objects, hubs, session navigation and all four MapResponse variants. It also checks schema-version rejection, vector/enum bounds, compression opacity and fragmented frame reconstruction.

### Multiplayer Stage M8C — real Asio TCP transport boundary

- `TcpWireStream` is the schema-blind socket layer. It accepts `WireMessageKind + opaque payload`, applies the existing `WireFrame` header, validates strictly monotonic per-direction frame sequence, handles arbitrary TCP fragmentation/coalescing through `WireFrameDecoder`, and bounds pending writes so a stalled peer cannot grow memory without limit.
- Standalone Asio is included only in `TcpWireStream.cpp`; public TCP headers, `GameServer`, `ServerRuntime` and `ServerRunner` remain free of Asio/WinSock/POSIX socket types. Windows linking adds `ws2_32/mswsock` only to targets that compile the adapter.
- `WireMessageCodec.h` is the typed bridge between protocol objects and opaque stream payloads. Control-plane payloads remain direct; `SimulationSnapshot` and `MapResponse` go `canonical serializer -> schema-blind compressor envelope -> frame` and reverse. TCP never reads snapshot fields or entity/module counts.
- `TcpClientTransport` and `TcpServerTransport` implement the existing endpoint interfaces, so later process integration does not alter `GameClient`, `GameServer` or replication semantics. Direction-invalid frame kinds are treated as protocol violations.
- `TcpWireTransportContractTests` opens a real `127.0.0.1` listener on an ephemeral port and round-trips SessionWelcome, sparse/hydrated SimulationSnapshot lifecycle data, MapResponse, time sync, client control and map request through the OS TCP stack, then verifies peer disconnect observation. This is transport-boundary proof, not yet a separate-process game-session acceptance test.
- M8D owns initial process lifecycle/admission: standalone `EliteServer --listen HOST:PORT`, remote `EliteGame --connect HOST:PORT`, server-owned controlled-entity assignment, authoritative fixed-step bootstrap and a two-process localhost acceptance gate. Explicit reconnect/resume identity is deliberately left for the next lifecycle stage.

### Multiplayer Stage M8D — separate-process authoritative session

- `ServerRuntime` now has a dedicated-server construction path with zero initial gameplay transports. `ServerRunner` can advance the authoritative world before/after clients exist; embedded local play still creates its legacy primary session through the compatibility constructor.
- `NetworkServerHost` owns `TcpServerListener` + accepted `TcpServerTransport` lifetimes around one `ServerRuntime`. Network admission calls the server-owned selector without receiving an EntityId from the connection; disconnect reaps the transport and revokes that session authority.
- `RemoteGameSession` owns only `TcpClientTransport + GameClient`; it never creates `ServerRuntime`, `ServerWorker` or `GameServer`. Remote debug control is currently a safe no-op facade until debug authorization/protocol is designed separately.
- `SessionWelcome` now carries the authoritative fixed-step duration, and wire protocol version is bumped accordingly. Remote prediction therefore does not hard-code the server tick rate after bootstrap.
- Separate-process acceptance also fences endpoint-local static-definition bootstrap: `EliteGame` initializes object/assembly descriptor catalogs independently instead of inheriting process globals that happened to be initialized by an in-process `GameSimulation`. Initialization is one-time so local server/client coexistence cannot invalidate descriptor pointers by rebuilding the registries.
- `EliteServer --listen HOST:PORT` and `EliteGame --connect HOST:PORT` are production-facing process seams. `--self-test-one-client` / `--self-test-remote-client` are test-only modes used by the ready harness to launch both executables separately and prove bootstrap, `GameClient::Ready`, numbered authoritative input acknowledgement and peer disconnect through real localhost TCP.
- M8E.0 first removed arbitrary-NPC admission. The dedicated bootstrap still creates two server-owned player/ship identities separated by 50 m for graphical acceptance, but M8E.2 no longer assigns one merely because an unknown TCP credential arrived first: unknown `SignIn` is rejected and account binding is created only by explicit `Register`.
- Manual M8E graphical acceptance has proved two separate remote `EliteGame` processes against one dedicated server, distinct server-owned identities, shared-world visibility and clean reconnect control epochs. Durable account persistence across server restart remains open.

### Multiplayer Stage M8E.0 — multi-process client preflight and graphical baseline

- Every graphical client owns a process-local WebUI endpoint. `HtmlUiServer` listens on port `0`, reads back the OS-assigned local port and `Application` builds all WebView URLs from that exact port. WebView2 also uses a process-local user-data directory, so a second `EliteGame` does not share UI/browser runtime state with the first.
- A remote client may start before `EliteServer`. Initial TCP absence maps to `GameSessionState::WaitingForServer`; `RemoteGameSession` retries on a bounded cadence. Once TCP has connected, protocol/catalog/admission failures and later disconnects remain fatal until explicit reconnect/resume semantics are designed.
- Loading UI represents `WaitingForServer` through the normal localization key path. The status line is an animated terminal presentation: alphabetic/localized strings are typed with a block cursor, cleared and repeated; Simplified Chinese visually imitates pinyin IME composition (`zheng zai` -> `正在`, `deng dai` -> `等待`, `fu wu qi` -> `服务器`). This animation is presentation-only and must never leak locale-specific behavior into session/network code.
- Real two-client testing exposed a GLFW 3.4 Win32 process-safety defect in `_glfwPollEventsWin32`: `GetActiveWindow()` could return another process' GLFW HWND and `GetPropW(..., L"GLFW")` then yields a pointer meaningful only in that foreign process. The first client crashed when GLFW dereferenced it. Because an app-side pre-check has a TOCTOU race, Windows `Window::pollEvents()` now uses the native `PeekMessageW/TranslateMessage/DispatchMessageW` pump and avoids the unsafe GLFW post-poll path while still delivering messages to GLFW's WndProc.
- Manual graphical acceptance then proved two simultaneous remote clients, distinct server-owned identities and shared-world visibility at ~50 m. M8E.1 closed reconnect ownership semantics and M8E.2 added explicit sign-in/register plus typed admission rejection; the next identity gate is durable server-owned account/character storage across restart, not another client selector hack.

### Multiplayer Stage M8E.1 — reconnect control epoch recovery gate

- `ShipControlState::controlTick` is a **live-session input sequence**, not persistent ship state. A fresh `GameClient` starts its sequence again at `1`. The server must therefore never carry an old `FixedStepControlQueue` acknowledgement across a new `ServerSessionId` merely because the same persistent ship rematerializes as the same `EntityId`.
- Manual two-process testing on 2026-08-16 exposed the lifetime mismatch: reconnecting to a long-running server inherited the old EntityId-owned control stream; new inputs were rejected as stale, local prediction moved briefly, authoritative snapshots rolled it back, and control became normal only after the new counter overtook the old one.
- The recovery contract is: on successful session create and on final disconnect for that player, discard the old numbered-input queue, discard pending one-shot ship commands, and neutralize continuous `ShipControlState` while preserving authoritative kinematic state. A reconnect acceptance test must create a **new `GameClient`** for the same persistent player/ship/entity and prove that bootstrap acknowledgement returns to the fresh epoch.
- This is separate from the remaining Win32 UI-thread responsiveness issue: the native event pump is still required to avoid the GLFW 3.4 foreign-HWND crash, while `WM_NCLBUTTONDOWN/HTCAPTION` may independently hold `DispatchMessageW` inside the Windows move/size modal loop. Do not treat that UI stall as server/session cross-talk.
- The full MinGW ready suite and manual `pilot-a -> reconnect A -> pilot-b -> reconnect B` process acceptance are now green on the canonical runtime binaries. Reconnect control rollback is closed; only short loading-screen presentation stalls remain observed.

### Multiplayer Stage M8E.2 — explicit authentication and admission

- `loading.html` is transient presentation, never a terminal error state. A failed remote attach must return to the multiplayer authorization form with the preserved server/client reason; local failure returns to main menu. M8E.2f fixes the WebView2 navigation race discovered by manual acceptance; the presentation layer now strengthens this with navigation generations, so `main_menu.html`/other documents can apply pending native state only when `*_ready/*_prepared` belongs to the exact current navigation.
- `SessionHello` carries stable `AccountHandle` + opaque device bearer token + explicit `AuthenticationIntent::{SignIn, Register}`. The handle grammar is shared by UI/client/server (`3..24`, lowercase `a-z0-9_-`, alphanumeric first character). `SignIn` is resolution-only: an unknown handle receives `UnknownAccount`, a known handle with the wrong token receives `InvalidCredential`, and neither can fall through into account creation. `Register` is the only first-contact operation allowed to create an in-memory `AccountId -> PlayerId` binding; a handle already bound to another credential returns `AccountHandleTaken`.
- Rejection is a typed protocol message (`SessionReject`) carried by loopback and TCP. Duplicate live login returns `AlreadyActive`; registration capacity and bootstrap/session failures have distinct reasons. `NetworkServerHost` keeps rejected TCP alive for a bounded flush grace so the structured reason wins over a generic peer-close error.
- Unauthenticated network lifetime is bounded by a handshake deadline and maximum pending-auth connection count. Authentication failure therefore cannot accumulate unlimited idle connection objects.
- Multiplayer WebUI exposes endpoint + AccountHandle + localized input rules + explicit `SIGN IN` / `REGISTER` / `BACK`. The handle is a login identifier and local OS credential-slot key, never `PlayerId`/`EntityId`; localized player display names remain a separate future field. The form is height-responsive (`clamp()` sizing + compact media queries + scroll fallback) so short windows do not destroy vertical composition. Local -> Multiplayer enters this authorization form instead of reusing local gameplay identity as remote authority. Normal process startup no longer creates a `default` remote credential slot; without explicit `--profile` the handle field is intentionally empty. LocalGameSession uses its own private bootstrap identity and never consumes a remote credential.
- Raw bearer tokens stay client-side/at the admission boundary; `AccountRegistry` stores SHA-256 digest + server-owned `AccountId + PlayerId`. Gameplay authority remains `ServerSessionId -> PlayerId -> ShipInstanceId/ControlRegistry -> EntityId`.
- Runtime source no longer contains a developer-specific `D:/__elite/work` asset fallback; relative executable/build asset resolution is protected by an architecture guard. The obsolete pre-native-pump GLFW input guard was removed.
- Normal runtime logging is intentionally quiet. Detailed successful M8E startup/process/WebView/connect/auth/bootstrap/control tracing is available only with `ELITE_TRACE_RUNTIME=1`; actionable reject/timeout/crash and threshold-based slow-path diagnostics remain unconditional, and self-test output is unaffected.
- Dedicated server exposes development/test `--reset-auth-state`; it may clear account bindings only before gameplay sessions are admitted. Today the registry is RAM-only, so restart already resets it; M8E.3 preserves the CLI contract against the durable repository.
- M8E.2 intentionally does **not** claim durable persistence or Internet-grade transport security. Password hashing, recovery-secret storage, credential rotation and TLS belong to M8E.3/security work. The formal password/recovery contract lives in `src/game/identity/AUTHENTICATION_ARCHITECTURE.md`: password KDF behind `IPasswordHasher`, one-time high-entropy recovery secret, recovery-driven device-token rotation/session invalidation, optional external `IRecoveryChannel`, no security questions/admin-readable passwords.

### Multiplayer Stage M8E.3 — durable authoritative universe persistence

M8E.3 owns one persistence architecture for identity **and** mutable universe state. It must not create a short-lived account database that later has to be merged with a different world-save system. The planned shape is a `PersistenceCoordinator` over separate account/player/ship/universe repositories, with a common versioned checkpoint/recovery contract.

Durable keys are stable domain IDs (`AccountId`, `PlayerId`, `ShipInstanceId`, authored/static IDs and future stable dynamic-object IDs). Materialized `EntityId` is deliberately transient and is reconstructed when a durable record enters `Prewarm/Active`. This keeps Scheduled/Coarse/Prewarm/Active as lifecycle states of one persistent object rather than independent identities.

Static deterministic endpoint content is referenced by schema/content fingerprints, not copied into saves. A save image contains only mutable authoritative state plus universe metadata (`UniverseId`, save sequence, universe epoch). The authoritative worker captures an immutable image at a fixed-step boundary; persistence I/O occurs off the simulation thread. Atomic replace, last-known-good backup, explicit schema migration and fail-loud corruption handling are required contracts, not optional polish.

Implementation order: (a) schema/coordinator + test backend; (b) account handle/password/device/recovery records + player/owned-current ship records and restart-stable login; (c) durable dynamic ship/object/hub state; (d) append journal/checkpoint compaction plus Scheduled/Coarse/Prewarm/Active lifecycle facts; (e) clean restart, interrupted write, corruption recovery, password/recovery rotation and materialize/dematerialize acceptance. Local play reuses the same schema/backend interfaces with a separate save root.

### Persistent identity Phase 1 — player / ship / session / control separation

The server now treats four identifiers as different domains:

- `PlayerId` — persistent player/character identity;
- `ShipInstanceId` — stable identity of a concrete ship instance;
- `ServerSessionId` — transient connection identity;
- `EntityId` — current materialized simulation handle.

`PlayerRegistry` owns `PlayerId -> current ShipInstanceId`. `ShipInstanceRegistry` owns the stable ship record and current `ShipInstanceId <-> materialized EntityId` mapping; its explicit dematerialization seam is the bridge to future Scheduled/Coarse states. `ControlRegistry` is a separate authority axis (`Human / AI / Autopilot / None`), with human control currently wired as `PlayerId -> EntityId`. `ServerSessionRegistry` stores `session -> PlayerId`, never ship/entity identity.

`SessionWelcome` now carries `playerId`, `controlledShipInstanceId` and `controlledEntityId`; `ShipSnapshot.instanceId` lets every client identify the same concrete ship independently of its local presentation role. This fixed the old structural assumption that one `ShipRole::Player` was the only local player and the rest of the world was NPC presentation.

Current limitations are deliberate: `AccountId` + bearer-token digest validation + explicit registration/sign-in now exist, but `AccountRegistry` and mutable universe records are still in-memory/bootstrap-owned. M8E.3 therefore introduces one durable authoritative-universe persistence architecture: account/player/owned-current ship identity is the first slice, then mutable ship/object/hub state and lifecycle/journal facts use the same versioned checkpoint/recovery system. A client must never obtain authority by sending an arbitrary PlayerId/ShipInstanceId/EntityId, and transient `EntityId` must never become the durable save key.
