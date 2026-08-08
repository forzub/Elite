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
- HubTactical diagnostic seeds are reconstructed from canonical hub-local
  state, including the rotating-frame `omega x r` term;
- universe-timeline revision changes are hard interpolation/cache fences.

### Cross-timeline + entity architecture contracts

`tests/architecture_contracts/run_mingw64.sh`

Locks down the seams between runtime time/revision ownership and map state,
including transactional accelerated diagnostics, Hub -> Detail reacquisition
after a revision fence, first-class star-system membership, explicit
single-active-system runtime context, inactive-system gameplay freezing,
spatial-vs-map parent separation, and system-isolated repair/signal/radar domains.

### System map behavior + architecture

`tests/system_map/run_mingw64.sh`

Locks down the existing map camera/navigation/picking/presentation contracts.
Wall-clock presentation time may not locally advance world snapshots, and production
cloud motion may not apply tooling-only debug wind multipliers.
Local map data is now resolved in `GameState::prepareFrame()` before input, and
must not be replaced again between map input and rendering.

When another subsystem is considered stable, add its runner here and to
`tests/run_all_mingw64.sh` instead of creating another top-level command.

### Hub co-frame presentation

Hub-attached infrastructure carries its stable local hub binding in simulation
snapshots. When the player is rendered in the same hub frame, gameplay
presentation reconstructs the infrastructure from that exact frame sample rather
than blending an independently delayed world pose. This prevents mixed-epoch
micro-jitter near rotating orbital infrastructure while preserving the
authoritative world pose as a fallback outside that frame.
