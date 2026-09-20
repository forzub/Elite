# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Evidence boundary

Last exact accepted target-machine baseline:

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

Latest actually tested checkout:

```
2ad1178bc5c778636748557ceb6c9a5b757c9a53
```

The latter passed architecture but never reached runtime behavior because of a
test-diagnostic compile error.

Current hard-replacement baseline before mandatory state-doc synchronization:

```
f8cd91d01006d9cba8327ae51efb6705e6df83e0
```

It is **UNVERIFIED** on the user's MinGW64 target machine.

## Canonical B0-B14 status

Strong / accepted:
- B0 Navigation World Snapshot;
- B7 Maneuver Decision mechanics and law/doctrine selection;
- B8 Maneuver Acceptance / program product;
- B9 Program Sampler;
- B10 bounded tracking;
- B12 PilotSkill;
- B13 propulsion / authoritative physics;
- B14 isolated scheduler mechanics.

Strong mechanics but production migration still incomplete:
- B5 physical maneuver compiler;
- B6 continuous maneuver proof;
- ordinary-live B7-B10 migration.

Open/transitional:
- B1 shared influence builder;
- B2 unified NavigationObjective;
- B3 coarse vehicle-aware topology feasibility;
- B11 explicit safety monitor / bounded reflex.

## B4 architecture — current production direction

B4 is no longer the angular ray-fan experiment.

Current owner:
`LocalHorizonPlanner + LocalAvoidancePlanner`.

Current ordinary unexpected-obstacle representation:
- nominal trajectory tangent defines the local forward axis;
- a stable normal plane defines lateral/vertical displacement;
- predicted moving occupancy is projected into that plane;
- candidate offsets are expressed in meters;
- exact static geometry filters candidate segments;
- a time-coupled dynamic check filters moving collisions;
- successful result publishes a temporary bypass target and an explicit merge
  target on the original trajectory.

A future longitudinal multi-slab route-aligned corridor may extend this same
B4 owner for complex known geometry. It must not be introduced as a parallel
fallback planner.

## Removed legacy mechanism

The following is historical only and must not be restored:
- angular deflection rings;
- azimuth fan search;
- branch continuity hints;
- same-branch ranking;
- branch-switch-required API;
- ordinary branch-switch Brake recovery.

Older Stage-12 entries remain only as chronological failure evidence and are
explicitly superseded by the current canon notice.

## Architecture enforcement

The Stage-12 architecture checker now:
- requires projected visible-horizon fields/implementation/tests;
- requires the live metric-offset integration;
- rejects old fan/branch identifiers if reintroduced.

This turns the user's recurring migration failure mode into a contract:
**the old mechanism cannot silently remain authoritative beside the new one.**

## Current milestone

Obtain the first target-machine compile + behavioral evidence for the hard B4
replacement.

A green target run is required before:
- calling the replacement accepted;
- changing the accepted baseline;
- closing the synthetic local-avoidance migration;
- proceeding to game visual evaluation on the strength of this replacement.

## State protocol

After every state-affecting event:
- synchronize `CURRENT_STATE.md`;
- synchronize `CURRENT_TASK.md`;
- synchronize `PROJECT_STATE.md`;
- synchronize active `src/game/navigation/STAGE12_END_TO_END.md`;
- recreate `CONTINUE_PROMPT.md` from scratch.
