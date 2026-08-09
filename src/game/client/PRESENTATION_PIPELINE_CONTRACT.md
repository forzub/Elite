# Client presentation pipeline contract

This document freezes the motion-presentation behavior accepted after the Hub
Motion Lab regression investigation. Changes to the client/server runtime must
preserve these invariants unless this contract and its tests are deliberately
updated together.

## Single render epoch

`ServerTimelineClock` is authoritative. `ClientServerClock` estimates current
server time. `ClientPresentationClock` derives one delayed render playhead from
that estimate plus received authoritative snapshot history.

Every snapshot-interpolated object and every interpolated reference frame in one
render frame uses the same `SnapshotPresentationWindow` and therefore the same
`renderTimeSeconds` and interpolation alpha. Object-specific rendering must not
recreate its own clamp or snapshot-pair selection logic.

The presentation playhead must remain behind received snapshot history. A large
client/server estimator discontinuity performs one buffered recovery rather than
degenerating into newest-snapshot hold. Under the accepted 50 Hz server / 16.67
Hz snapshot / ~80 Hz render laboratory cadence, mature history must retain an
interpolation bracket without starvation.

## Remote dynamic entities

Remote players, active NPCs and other server-dynamic entities are authoritative
on the server. The client renders them from adjacent authoritative snapshots on
the shared delayed presentation timeline.

Remote presentation may interpolate values but must not mutate authoritative
client snapshot state. Interpolation endpoints from different star-system or
timeline-revision domains must never be blended.

## Local controlled player

The local player's gameplay prediction and reconciliation remain fixed-step.
Presentation is allowed to sample a copy of the latest fixed predicted state at
the remaining client accumulator fraction.

The fractional sample:

- may never mutate fixed prediction/reconciliation history;
- may advance by at most one fixed step;
- must use the same deterministic attitude and HubTactical motion equations as
  fixed prediction/server simulation;
- is presentation-only and must never become authoritative gameplay state.

This removes the 50 Hz render-target staircase while preserving deterministic
prediction and server reconciliation.

## Analytic deterministic entities

Celestial bodies, orbital hubs and deterministic kinematic mechanisms are
rendered by evaluating their deterministic motion law at the appropriate shared
presentation epoch. Gameplay collision/authority remains server-side.

An analytic presentation object must not consume a separately accumulated local
render clock when a synchronized presentation epoch is available.

## Co-frame rule

The local player camera and hub-attached presentation must use compatible
reference-frame samples. Predicted local position may be combined with the
render-time hub/reference frame, but the renderer must not mix arbitrary newest
server frames with delayed presentation frames.

## Snapshot DTO safety

Server-to-client snapshots contain values and stable identifiers only. They must
not contain raw pointers or references to temporary server objects. Snapshot
transport must remain compatible with future encode/decode serialization rather
than depending on same-process object lifetime.

## Regression gates

Automated protection consists of:

- `tests/presentation_pipeline`: temporal/bracket/interpolation regression tests;
- `tests/world_runtime/ClockSyncTests.cpp`: server-clock and buffered render-clock
  behavior, including large estimator discontinuity recovery;
- `tests/architecture_contracts/LocalPredictedPresentationContractTests.cpp`:
  production fractional prediction sampling and fixed-state immutability;
- `tests/architecture_contracts/check_presentation_pipeline.py`: static ownership
  and single-window guard;
- existing Hub Motion Lab, prediction/reconciliation, reference-frame and
  snapshot-safety guards.

The optional Hub Motion Lab CSV verifier can be used after changes to networking,
clocks, prediction, reference frames or render cadence for an end-to-end runtime
acceptance capture.
