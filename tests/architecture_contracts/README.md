# Cross-timeline and diagnostic architecture contracts

Run from the repository root under MSYS2 MinGW64:

```bash
bash tests/architecture_contracts/run_mingw64.sh
```

This suite closes gaps that were not covered by the earlier clock and map
architecture checks. It locks down:

- normal -> accelerated -> normal `UniverseClock` rewind semantics;
- transactional multi-ship diagnostic state that is discarded rather than
  committed to production state;
- universe-timeline revision as an interpolation/transition fence;
- deterministic absolute-time orbital state after a rewind;
- observable cloud debug-speed behavior, including stop/x5 and km->m units;
- source-level ownership rules preventing debug-only trajectory state from
  leaking back into `DynamicMotionState`;
- pre-input synchronization before map-frame preparation;
- non-persistence of an active diagnostic session in Debug Control defaults.

The existing `world_runtime` and `system_map` suites remain authoritative for
their established contracts. This suite is intentionally cross-cutting: it
checks the seams between them, because those seams are where a monotonic server
clock and a rewinding universe timeline can otherwise produce mixed-branch
frames.

## Hub -> Detail across timeline revision fences

A revision change invalidates branch-local Detail/Hub snapshot bytes but keeps
the semantic `m_loadedDetailTarget`. Returning from Hub to Detail must therefore
reacquire that target on the active revision when the cached Detail snapshot was
invalidated. Navigation semantics may not depend on cache lifetime.

## Stage 10A: first-class system membership

Dynamic and static runtime objects now carry explicit star-system membership.
`WorldPosition` remains system-local, so numeric coordinates may never be used
to infer that two entities share a system. The Stage-10A contract checks that:

- ships, hub/reference frames, diagnostic trajectory states and snapshots carry
  `systemId` end to end;
- static-object spatial membership is separate from System-map visibility;
- player navigation derives its current system from the authoritative player
  ship rather than maintaining an unrelated mutable copy;
- System, Detail and Hub snapshot builders reject entities from other systems;
- System-map snapshots publish real ships without inventing orbit metadata;
- promo/debug helpers cannot mutate the frozen production branch while
  accelerated diagnostics are active.

This is a prerequisite for Migration Stage 3. It does not yet implement
inter-system transfer or per-system dynamic simulation contexts.

## Stage 10F: HubTactical client translation prediction

The predicted player must advance the same HubTactical local-motion equations
as the authoritative server. Reference-frame snapshots describe the rotating
frame geometry; predicted entity-local position/velocity live in
`ShipTransform.motion`. Rendering may smooth predicted local state, but it may
not fall back to snapshot-only local translation targets.

## Stage 10G: fixed-step prediction/reconciliation stream

`ShipControlState` contains axes and rates that are integrated over a fixed
simulation step. The server must therefore consume queued control samples in
sequence, at most one sample per authoritative fixed step. A snapshot may
acknowledge a client control tick only after that exact sample has been consumed
by an authoritative step.

This contract deliberately rejects `queue.back() + queue.clear()` coalescing.
That old path let one authoritative step acknowledge several client-predicted
steps. The client then removed all of them from replay, periodically moving its
prediction target back toward an older snapshot and producing a visible
forward/correction/forward saw in the player camera. During accelerated
universe diagnostics prediction is disabled, so queued production inputs are
explicitly discarded and acknowledged as a branch fence instead of leaking
through when normal gameplay resumes. Fixed-step input history may not be
silently truncated either; overload requires an explicit reconciliation-reset
contract rather than an acknowledgement jump.

## Runtime entity/motion/presentation policy contracts

`RuntimePolicyContractTests.cpp` locks the first layer of the server/client
runtime split before production entities are migrated onto it.

The contract deliberately separates:

- entity identity (`EntityType`);
- motion law (`MotionModel`);
- server simulation level (`SimulationMode`);
- authoritative timeline/ownership (`TimelineDomain`, `AuthorityPolicy`);
- per-observer client presentation (`PresentationPolicyResolver`).

`PresentationPolicy` is intentionally **not** a permanent property of an
entity. The same active player ship is locally predicted for its controlling
client and snapshot-interpolated for every remote observer.

The tests also lock the materialization fence for scheduled ships:
`Scheduled -> Prewarm -> Active`, with the motion law changing from an analytic
trajectory to dynamic physics only through a valid runtime transition. The
reverse collapse follows the same contract instead of silently snapping an
active ship back onto its old schedule.

`check_runtime_policy_boundaries.py` prevents authoritative server/simulation
code from depending on client presentation policy and prevents the new low-level
entity/motion declarations from acquiring client/server dependencies.


## Hub Motion Lab

`HubMotionLabContractTests.cpp` installs a deliberately small motion laboratory
around the Earth orbital hub before production presentation policies are
migrated. It contains three authoritative server-side NPC probes and one
client-side analytic reference cube:

- `LAB NPC SLOW` follows the ordinary remote-snapshot presentation path at
  45 m/s tangential speed;
- `LAB NPC FAST` uses the same path at 180 m/s;
- `LAB NPC MATCH` stays close to the player with only 1 m/s relative motion,
  making reference-frame/epoch errors obvious near the camera;
- `LAB ANALYTIC CUBE` is evaluated directly from synchronized presentation
  server time and never waits for a ship snapshot.

The NPC probes are real `ShipRole::NPC` entities and therefore pass through the
normal `GameSimulation -> ShipSnapshot -> ClientWorldState` path. Their
controlled lab driver is excluded from normal NPC AI/physics so the test input
is deterministic. The cube is presentation-only and must never acquire server
physics authority.

The client writes `hub_motion_lab_presentation.csv` while the lab is enabled.
Each row captures requested versus clamped render time, interpolation bracket
and alpha, snapshot-starvation clamps, and presentation error for the slow,
fast and near-co-moving probes. This file is diagnostic instrumentation, not a
production network contract.

`check_hub_motion_lab.py` also locks the map and architecture seams so the lab
ships remain ordinary map-visible entities while the analytic cube stays a
diagnostic probe.

## Presentation pipeline lock

The accepted Hub Motion Lab behavior is protected by both static and compiled
contracts. `check_presentation_pipeline.py` prevents duplicate render-time
window selection from reappearing in object branches, while
`LocalPredictedPresentationContractTests.cpp` verifies that fractional local
presentation advances only a copy of fixed prediction and never integrates more
than one fixed step.

The cadence/interpolation behavior itself lives in
`tests/presentation_pipeline` and is also part of `tests/run_all_mingw64.sh`.


## Replication snapshot publication boundary

`GameSimulation::update()` advances authoritative simulation state only. It must
not materialize `SimulationSnapshot` DTOs on every 50 Hz fixed step. Snapshot
construction is explicitly requested by `GameServer` only when a snapshot is
actually being published.

This contract is not merely a CPU optimization. Structural dirty flags are
consumed by replication serialization, so consuming them on a non-published
simulation tick can silently drop a structural update. Ship graph resend
redundancy is therefore counted in published snapshots rather than simulation
frames.

`check_replication_snapshot_boundary.py` rejects the old continuously rebuilt
snapshot cache, DTO construction inside `GameSimulation::update()`, and direct
server use of the removed `GameSimulation::snapshot()/setTick()` lifecycle.

### Headless server assembly boundary

`check_headless_server_boundary.py` and
`HeadlessServerBoundaryCompileTests.cpp` lock the CPU/GPU assembly split:
server/simulation headers and the shared assembly library must compile without
glad/OpenGL, while GPU uploads live only in `render::geometry::AssemblyGpuLibrary`.

### Server runtime / transport ownership boundary

`check_server_transport_boundary.py` and `check_server_runtime_ownership.py`
lock the local-session ownership seam: `LocalLoopbackTransport` carries messages
and copied snapshots only, `ServerRuntime` is the sole production owner of
`GameServer`, and the client learns its controlled entity from server-assigned
session metadata rather than choosing an `EntityId` in command packets.

### Asynchronous debug/control boundary

Debug HTML pages are retained, but they are not allowed to bypass server
ownership. `LocalDebugSessionControl` exposes separate tool-side
`IDebugSessionControl` and server-side `IServerDebugChannel` endpoints. Commands
are queued requests; structural snapshots and universe-time diagnostic state
cross back as copied value state with monotonically increasing local revisions.

`check_debug_session_boundary.py` rejects direct `ServerRuntime`/`GameServer`
access and reference-returning debug snapshots. `DebugSessionBoundaryContractTests.cpp`
verifies the value-copy and command-queue semantics at runtime.

### Server worker thread boundary

`check_server_worker_thread.py` locks the execution seam: `LocalGameHost` owns
only a `ServerWorker`, `ServerRuntime` is constructed/advanced/destroyed inside
that worker, and both local gameplay/debug bridges protect shared state with
mutexes. `ThreadBoundaryChannelContractTests.cpp` exercises the message/debug
queues concurrently. The worker now uses a bounded single-flight asynchronous
pipeline: the newly submitted authoritative batch may overlap client/render work,
but the next submission waits for the previous batch so backlog/latency cannot
grow without bound and fixed-step inputs are never silently dropped.

### Client-owned System-map celestial layer

Stage 3 begins with deterministic System-map bodies. The server response keeps
the authoritative map epoch but leaves `SystemMapSnapshot::bodies` empty.
`ClientMapService` resolves the requested
system through its local `StarAtlasDatabase`/`CelestialRuntimeRegistry` at that
**same epoch** and rebuilds bodies, orbits and ring presentation before exposing
the snapshot to `SpaceState`.

`check_client_system_map_celestial.py` prevents deterministic body composition
from returning to `GameServer::buildSystemMapSnapshot` and locks the local catalog
dependency/epoch join. `SystemMapCelestialMigrationContractTests.cpp` verifies
that dynamic objects survive the join untouched, parent-relative orbit centers
are rebuilt from the same celestial sample, authored fallback positions survive,
and cross-system/cross-epoch composition is rejected.

### Client-owned System-map real-ship layer

Stage 3B removes ordinary player/NPC ships from `GameServer::buildSystemMapSnapshot`.
Those transforms already arrive through normal `SimulationSnapshot` replication,
so `ClientMapService` samples retained authoritative history at the exact
`SystemMapResponse::metadata.serverTimeSeconds` and composes the map ship layer
locally. A response that falls between replication publications waits until a
newer snapshot forms an interpolation bracket; stale responses are rejected
instead of being silently clamped to another epoch. Cross-system coordinates
are never interpolated.

`check_client_system_map_ships.py` prevents the duplicate server ship loop from
returning and locks the exact-epoch join. `SystemMapShipSamplingContractTests.cpp`
checks temporal interpolation, future/stale response handling and the system-domain
fence across an inter-system transfer.

### Client-owned System-map infrastructure/hub layer

Stage 3E removes production static infrastructure and orbital hubs from
`GameServer::buildSystemMapSnapshot`. Composite hubs now have ordinary
`OrbitalHubSnapshot` replication state; static object snapshots carry instance
identity/navigation facts alongside their authoritative transform.
`ClientMapService` samples both at the exact System-map response server epoch and
converts them to `SystemMapObject` presentation locally. Static system name and
galactic placement are also restored from the endpoint-local StarAtlas. Only
explicit analytic diagnostic probes may remain as server-built System-map rows.

`check_client_system_map_infrastructure.py` prevents the server infrastructure
loops from returning. `SystemMapInfrastructureSamplingContractTests.cpp` locks
exact-epoch interpolation, system-domain filtering, hub-owned orbit metadata and
client-side station/hub composition.

### Client-owned Details and Hub local-map composition

Stage 3F removes the complete server-built Details DTO; Stage 3G does the same
for Hub Map. Both views use the response only as an authoritative identity/epoch
acknowledgement and join it with retained ordinary `SimulationSnapshot` history.
The client resolves deterministic parent/body state from its endpoint-local
celestial runtime at the response universe epoch. Hub Map additionally carries
ordinary hub-frame angular velocity and prime-module identity through
`OrbitalHubSnapshot`, preserves authoritative HubTactical local ship state, and
reconstructs attached modules from stable `HubAttachmentSnapshot` offsets in one
common hub frame.

`check_client_detail_map_migration.py` / `check_client_hub_map_migration.py`
prevent server presentation builders from returning. The runtime sampling
contract tests lock future/stale response handling, local HubTactical state and
rotating-frame facts.


## Galaxy-map catalog ownership

`check_client_galaxy_map_catalog.py` and
`GalaxyMapCatalogMigrationContractTests.cpp` lock the Stage 3C seam:
static Galaxy systems/objects come from the client's local `StarAtlasDatabase`,
while the server response carries only authoritative world-state overlays
(currently jurisdiction) plus the authoritative universe epoch/date. Galaxy-map
requests must not become a second transport for catalog names, types, positions,
descriptions, or tags.
