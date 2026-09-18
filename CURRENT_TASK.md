# Elite — CURRENT TASK

**Updated:** 2026-09-18 Europe/Kyiv  
**Stage:** 12A-6b — live moving-gap / moving-passage composition  
**Last target-machine verified baseline:** `a0efa9190180043b05da3103a5d466d256744935`

## Accepted prerequisites

12A-5 and 12A-6a are closed.

Live runtime already proves:

```text
stationary infrastructure
    -> NavigationSpace exact HitVolume OBB authority

time-varying infrastructure
    -> NavigationMap compact dynamic candidate
    -> linear + angular motion publication
```

12A-6a target-machine evidence:

```text
rotating_actor_seen=1
rotating_actor_omega_verified=1
rotating_actor_omega_error=0

obstacle_candidate=0
exact_obstacle_block=1
adjusted=1
exact_static_violation=0
replication_error_mps2=0
canonical_replication_error_mps2=0
```

## Current implementation target

Compose the already accepted moving-geometry precision chain into the real Stage-12 runtime path:

```text
bounded NavigationMap dynamic candidates
    -> selected obstacle pair / moving gap
    -> MovingGapPredictor
    -> MovingPassageTrajectoryEvaluator
    -> accepted bounded maneuver result
    -> NavigationRuntimePlanner intent
    -> mapIntentToWorld(...)
    -> NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> authoritative physics
    -> same-tick replication truth
```

This is integration work, not a new global planner.

## Required ownership rules

Keep the already accepted separation:

```text
stationary HitVolume geometry
    -> NavigationSpace exact static layer

moving / rotating infrastructure
    -> NavigationMap dynamic publication
    -> bounded moving-gap / moving-passage precision only when relevant
```

Forbidden:
- global all-pairs moving-gap scans;
- a second client/presentation planner;
- treating conservative spheres as exact static collision truth;
- direct navigation writes to authoritative position/velocity;
- weakening fail-closed behavior merely to make the fixture pass;
- reintroducing stationary infrastructure into NavigationMap dynamic ownership.

## First live 12A-6b gate

Use a deterministic real scene fixture in which time-varying geometry materially changes passage feasibility.

The gate must prove, from one bounded runtime snapshot:
- the relevant moving/rotating actors are selected from NavigationMap;
- their P/V/A/angular-velocity state reaches `MovingGapPredictor`;
- a moving-gap prediction is actually consumed by `MovingPassageTrajectoryEvaluator`;
- the moving passage result changes or constrains the authoritative maneuver when required;
- the result still crosses the existing map->world control seam;
- exact-static safety remains collision-free;
- sparse/canonical execution replication remains exact at the same authoritative tick.

Do not declare 12A-6b accepted from DTO plumbing alone. 12A-6a already proved the DTO.

## Baseline verification commands

Until the 12A-6b-specific contract/test is added, keep the accepted Stage-12 regression set green:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash tests/navigation_map/run_mingw64.sh

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

When 12A-6b introduces a new deterministic gate, add its exact command and acceptance evidence here immediately.

## Documentation invariant

After every state-affecting result or scope change, update together:
- `CURRENT_STATE.md`;
- `CURRENT_TASK.md`;
- `PROJECT_STATE.md`;
- `src/game/navigation/STAGE12_END_TO_END.md`.

Use a **verified baseline hash**, not "current HEAD", because a documentation commit necessarily changes HEAD.
