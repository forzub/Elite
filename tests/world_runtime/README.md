# Shared world-runtime regression gate

Run from the repository root in MSYS2 MinGW64:

```bash
bash tests/world_runtime/run_mingw64.sh
```

The suite locks the first client-world migration contract:

- the server supplies an absolute universe-time anchor and time scale;
- the client advances presentation time locally between server snapshots;
- planetary state is reconstructed from the shared star atlas and the shared
  `CelestialSystemRuntime` implementation;
- `CelestialRuntimeRegistry` evaluates only systems that are requested;
- the old periodic `CelestialSnapshotRequest` network stream may not return.

This gate deliberately does not initialize GLFW or OpenGL. Render-coordinate
contracts are added in the following migration stages when map frame builders
start consuming the client-owned reconstructed world.

## Stage 2 coordinate/render-time contract

The suite also locks two runtime regressions that were visible in the maps:

- celestial and cloud presentation must consume the client-reconstructed
  universe time directly; render resources may not own a second wall-clock
  timeline or correct it independently;
- HubTactical coordinates are a rotating reference frame. Velocity transforms
  include the `omega x r` term, and an idle ship in HubTactical may not be
  accelerated toward the parent planet by absolute gravity. Free gravitational
  flight belongs to PassiveTrajectory.

`ClientCelestialMapBridge` is intentionally a migration boundary: it currently
replaces only predictable celestial time/orientation fields in Detail and Hub
presentation snapshots. Dynamic positions remain at the map-snapshot epoch
until the next client-world migration stage, preventing mixed-epoch geometry.

## Stage 3: clock synchronization and render timeline

`clock_sync_tests` is dependency-free and can be run independently with:

```bash
bash tests/world_runtime/run_clock_sync.sh
```

It emulates an 80 ppm client clock drift, 50 ms one-way base latency, jitter,
latency spikes, 20 ms authoritative server ticks and variable render frame
rates. Four synchronization strategies are compared on the same deterministic
network trace:

- latest midpoint offset;
- EMA offset;
- bounded phase correction without rate learning;
- the production robust affine PLL (`ClientServerClock`).

The production strategy is selected for smoothness rather than the smallest
instantaneous phase error. The deterministic reference scenario measured:

- latest midpoint: 22.133 ms RMS, 1543.485% maximum rate spike;
- EMA offset: 7.808 ms RMS, 132.208% maximum rate spike;
- bounded phase: 39.602 ms RMS, 0.258% maximum rate error;
- robust affine PLL: 31.606 ms RMS, 0.124% maximum rate error.

The regression gate therefore requires the production estimator to remain
within 45 ms RMS / 60 ms maximum phase error while keeping rate excursions
below 0.40%, and to outperform the bounded-phase baseline on both phase RMS and
maximum rate error in the fixed deterministic scenario.

Universe time is no longer frame-integrated on the client. A versioned
`ClientUniverseTimeline` maps the synchronized server simulation timeline to
universe time. `ClientServerClock` estimates authoritative server "now", while
`ClientPresentationClock` owns the single delayed render playhead consumed by
map/world presentation. The playhead normally follows the synchronized clock
but is constrained by actual snapshot history, so a long frame or fixed-step
debt discard cannot leave rendering permanently clamped to the newest
snapshot. `ClientWorldState` may not own another render clock.

## Stage 4: monotonic server timeline

The authoritative server timeline is now a separate clock from gameplay time.
During accelerated-universe diagnostics gameplay may be frozen, but server time
must continue advancing by the fixed authoritative server step. Universe time
then remains an affine mapping of server time even at high scales such as
`500x`.

The clock tests cover the production `ServerTimelineClock`, accelerated
`UniverseClock` mapping and client `ClientUniverseTimeline` agreement. The
architecture gate rejects a return to a raw `m_serverTime += gameplayDt`
accumulator, which was able to freeze server time while universe time kept
moving.


## Transactional universe trajectory diagnostics

Accelerated-universe diagnostics are not a production `MotionMode` transition.
`UniverseDiagnosticTrajectorySession` owns an alternate trajectory for every
eligible real ship. Production transforms, motion state, controls, sensors and
gameplay jobs stay frozen while the diagnostic branch is active, and exiting the
mode discards the branch rather than committing future coordinates.

For a `HubTactical` ship the diagnostic seed is reconstructed directly through
`HubNavigationFrame::localToWorldPosition()` and `localToWorldVelocity()`,
preserving the rotating-frame `omega x r` term. Diagnostic entry is
all-or-nothing: if any eligible ship cannot be seeded coherently, the branch is
rejected and the server returns to the normal timeline through the revisioned
debug-session API.
