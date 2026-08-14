# Regression gates

Run all blocks that are currently treated as stable from the repository root:

```bash
bash tests/run_all_mingw64.sh
```

The launcher deliberately runs every block even if an earlier block fails, so
one regression does not hide another.

## Ready blocks

### World runtime + global time contract

`tests/world_runtime/run_mingw64.sh`

Locks down:

- one client estimate of authoritative server time;
- bounded clock-rate correction under latency/jitter/drift;
- frame-rate-independent clock estimation;
- one server-time -> universe-time affine timeline per revision;
- monotonic server time even while gameplay is frozen for accelerated universe
  diagnostics;
- accelerated universe time remaining an exact affine function of server time;
- rotating Hub-frame coordinate/velocity invariants already covered by the
  world-runtime suite;
- accelerated trajectory diagnostics use a transactional alternate branch for
  every eligible real ship; the production branch is frozen and the alternate
  branch is discarded on exit;
- HubTactical diagnostic seeds are reconstructed from canonical ship-owned travel-frame
  state, including the rotating-frame `omega x r` term;
- universe-timeline revision changes are hard interpolation/cache fences.

### Cross-timeline + entity architecture contracts

`tests/architecture_contracts/run_mingw64.sh`

Locks down the seams between runtime time/revision ownership and map state,
including transactional accelerated diagnostics, Hub -> Detail reacquisition
after a revision fence, first-class star-system membership, explicit
single-active-system runtime context, inactive-system gameplay freezing,
spatial-vs-map parent separation, and system-isolated repair/signal/radar domains.

### Client presentation pipeline

`tests/presentation_pipeline/run_mingw64.sh`

Locks down the accepted server-to-render path: buffered presentation time, one
shared snapshot bracket/alpha, remote interpolation without newest-snapshot
hold, recovery after a large client/server timing discontinuity, and
fractional local-player presentation between fixed prediction ticks.

A live Hub Motion Lab CSV can additionally be checked with:

```bash
python tests/hub_motion_lab/verify_capture.py hub_motion_lab_presentation.csv
```

### System map behavior + architecture

`tests/system_map/run_mingw64.sh`

Locks down the existing map camera/navigation/picking/presentation contracts.
Wall-clock presentation time may not locally advance world snapshots, and production
cloud motion may not apply tooling-only debug wind multipliers.
Local map data is now resolved in `GameState::prepareFrame()` before input, and
must not be replaced again between map input and rendering.

When another subsystem is considered stable, add its runner here and to
`tests/run_all_mingw64.sh` instead of creating another top-level command.

### Client acceptance harness

`tests/client_acceptance/run_mingw64.sh`

Boots the real headless local client/server session and drives production
control/network paths. It locks down keyboard-to-control mapping, startup/idle
invariants, accelerated-time round trips, player orientation and manoeuvre
motion, remote NPC presentation, command acknowledgement, and the live
Galaxy/System/Details/Hub request pipeline.

The existing `system_map` block remains the owner of map camera, picking and
cubic-navigation interaction contracts; the acceptance harness verifies the
live authoritative data path feeding those views.

### Multiplayer client acceptance

`tests/multiplayer_client_acceptance/run_mingw64.sh`

Boots one production `ServerRuntime` with two independent loopback endpoints and
two real `GameClient` state machines. It locks down distinct server-assigned
controlled entities, opposite local/remote identity on each client, per-session
navigation, one shared authoritative world, and independent numbered input /
authoritative acknowledgement streams without bypassing `ITransport`.

### Replication interest / sparse-retention contract

`tests/architecture_contracts/ReplicationInterestContractTests.cpp` plus
`check_replication_interest_retention.py` lock down the Stage-M6/M7 seam between
server simulation and per-session network decimation. Interest is computed per
destination session without changing simulation activation. Production ship
publication now consumes that cadence: Controlled/Tactical rows publish at the
normal snapshot rate, Nearby/Coarse rows less often, and interest exit/destruction
uses explicit removals. Full bootstrap and first publication after re-entry are
hydrated from canonical retained server state so sparse nested graph fields cannot
leave a late client with a partial runtime baseline. Objects/hubs remain full-cadence
in M7 but use explicit lifecycle removal under the sparse envelope.

### Hub co-frame presentation

Hub-attached infrastructure carries its stable local hub binding in simulation
snapshots. When the player is rendered in the same hub frame, gameplay
presentation reconstructs the infrastructure from that exact frame sample rather
than blending an independently delayed world pose. This prevents mixed-epoch
micro-jitter near rotating orbital infrastructure while preserving the
authoritative world pose as a fallback outside that frame.

### Portable wire protocol contract (Stage M8A)

`WireProtocolContractTests.cpp` and `check_wire_protocol_boundary.py` lock down the first real process boundary before sockets are enabled. The test verifies deterministic network-byte-order framing, one-byte TCP-style fragmentation, coalesced frames, magic/version/oversize rejection, and round trips for `SessionWelcome`, both `ClientMessage` variants, every `MapRequest`, and time sync. The architecture guard forbids platform socket APIs and raw aggregate/ABI serialization in the portable codec. `SimulationSnapshot`/`MapResponse` remain intentionally unsupported until M8B.
`check_case_sensitive_project_includes.py` additionally scans quoted project-local includes using exact filesystem spelling, so Windows-only case mistakes are caught before the dedicated Linux build is introduced.
