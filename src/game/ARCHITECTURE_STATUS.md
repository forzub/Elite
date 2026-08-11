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

Functional migration is currently at **Migration Stage 2 complete**:

- Stage 0: client-facing state stopped depending directly on `GameServer`;
  `IGameSession`/`ITransport` and the local host own the server boundary.
- Stage 1: the client owns `StarAtlasDatabase` and
  `CelestialRuntimeRegistry` and can resolve deterministic celestial state from
  synchronized universe time.
- Stage 2: predictable celestial presentation fields (time/orientation) for
  Detail/Hub are reconstructed client-side.

The clock/revision work that followed is infrastructure for this migration; it
did **not** migrate dynamic map geometry. First-class entity system membership is
now in place as a prerequisite for Stage 3, but server map DTO construction still
owns the dynamic composition.

### Next functional migration stage

The authority seams required before Stage 3 are now explicit: timeline branches,
entity membership, single-active-system runtime context and sensor domains no
longer rely on coordinate coincidence or map metadata. The next architecture
change should therefore be the presentation migration itself rather than another
server-side map feature.

**Migration Stage 3 is still pending:** move dynamic spatial map composition to
the client.

The server should eventually publish compact authoritative dynamic entity state
(ships, hubs/infrastructure identities and gameplay-owned transforms/bindings),
while the client combines that stream with its local catalog/celestial runtime
to construct Galaxy/System/Detail/Hub presentation snapshots.

Until that stage is complete, `GameServer` still builds the full map DTOs and
`ClientCelestialMapBridge` must update only predictable fields; it must not mix
new client-side celestial translations with old server dynamic geometry.

## Authoritative world bootstrap

The initial dynamic world is data-driven through `initial_world_state.json`.
Player start, physical-system map facts and orbital hubs are validated before
scene construction. Production startup must fail loudly on invalid authored world
data; it must not fabricate a Sol/Earth promo fallback. Political/map labels are
server-owned world facts and must never be inferred on the client from numeric
physical-system IDs. Diagnostic scenarios may keep explicit Sol/Earth constants,
but those constants must stay inside diagnostic/promo code paths.

## Known migration blockers / debt

- `GameServer` still builds Galaxy/System/Detail/Hub snapshots, including much
  deterministic catalog/celestial data already available on the client.
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
- System-map snapshots now publish player/NPC ships that belong to the requested
  system. The current System-map interaction path still has explicit picking and
  selection only for hubs/bodies, so ship selection/Details navigation remains an
  unfinished functional contract.
- `IDebugSessionControl::snapshot()` still exposes a complete
  `SimulationSnapshot` to local tooling. Production client code does not depend
  on `GameServer`, but this debug facade should eventually return a narrower
  debug DTO.
- Procedural cloud morphology is presentation-only wall-time work. Its timing is
  intentionally separate from universe time, but texture generation still runs
  synchronously on the render thread and remains a performance/LOD concern.

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
