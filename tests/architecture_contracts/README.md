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
