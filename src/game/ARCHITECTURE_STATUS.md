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
  eligible real ship (player and NPC). Entry is all-or-nothing: a partial branch
  is rejected rather than mixing accelerated and frozen production ships.
  Leaving the mode discards that session; it never commits diagnostic
  coordinates into production state.
- Celestial bodies, orbital hubs and orbital static infrastructure are derived
  from absolute universe time. They do not require a checkpoint: after a
  timeline rewind they are recomputed from the new canonical time. Their
  position and velocity caches must be derived from the same epoch.
- Client synchronization that can switch timeline revision is consumed before
  map preparation and input. A branch change must never land between map
  picking and rendering in the same application frame.
- Active diagnostic mode is runtime state and must never be persisted as a
  startup Debug Control default.

## Map subsystem decomposition

Detail/Hub have completed the Stage-6D ownership split: their backends and
shared celestial render services no longer depend on privileged facade access.
Galaxy/System still use the shared facade/low-level `.inl` backend pipeline, so
the map decomposition is not finished.

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

## Known migration blockers / debt

- `GameServer` still builds Galaxy/System/Detail/Hub snapshots, including much
  deterministic catalog/celestial data already available on the client.
- Runtime system membership is now first-class for ships, hub reference frames,
  static objects and sensor-space sources. The current dynamic simulation remains
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
