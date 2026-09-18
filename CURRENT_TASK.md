# Elite — CURRENT TASK

**Updated:** 2026-09-18  
**Canonical branch:** `main`  
**Track:** Navigation v2 live integration  
**Stage:** 11B-2 — replicated guidance/debug truth gate

## Progress

```text
[█████████████████████░░] 10 / 12 major stages closed

1–10 ACCEPTED
11A  ACCEPTED
11B-1 ACCEPTED
11B-2 ACTIVE
12    PENDING
```

## Newly accepted — 11B-1

Target-machine gate on:

```text
fb83b8d80f29c6c5e4e12b8a2fca731ffea7b8e8
```

passed:
- live runtime architecture;
- live NPC ownership architecture;
- runtime-control 1/1;
- trajectory/pilot 11/11;
- EliteGame;
- EliteServer.

## Active 11B-2 candidate

Production:

```text
src/game/simulation/NavigationExecutionSnapshot.h
src/game/simulation/ShipSnapshot.h
src/game/simulation/GameSimulation.cpp
src/game/network/WireDataSchema.h
src/game/network/WireDataCodec.h
src/game/client/ClientWorldState.h/.cpp
src/game/navigation/ReplicatedNavigationExecutionState.h
src/game/navigation/ClientNavigationWorkspace.h
src/game/presentation/GuidanceHudPresentation.h
src/game/SpaceState.cpp
```

Contracts/tests:

```text
tests/architecture_contracts/check_navigation_live_replication_guidance.py
tests/architecture_contracts/check_wire_data_schema.py
tests/architecture_contracts/WireDataPlaneContractTests.cpp
tests/navigation_runtime/NavigationReplicationTruthTests.cpp
```

### Required replication chain

```text
same server ExecutionSnapshot used for control
    -> ShipSnapshot.navigationExecution
    -> ordered binary wire schema v8
    -> ClientShipState.navigationExecution
    -> read-only ClientNavigationWorkspace mirror
    -> GuidanceHudPresentation diagnostics
```

### Required ownership separation

Client planners must not consume replicated execution truth:

```text
LocalGuidancePlanner
DockingPathPlanner
```

remain independent manual/advisory planning components.

The client may display the server-executed NPC command, but may not reinterpret it as a locally accepted maneuver.

## RUN NOW

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_live_runtime_control.py
python tests/architecture_contracts/check_navigation_live_npc_ownership.py
python tests/architecture_contracts/check_navigation_live_replication_guidance.py
python tests/architecture_contracts/check_wire_data_schema.py

bash tests/navigation_runtime/run_mingw64.sh
bash tests/navigation_trajectory/run_mingw64.sh

cmake -S tests/architecture_contracts \
      -B build/tests/architecture_contracts \
      -G Ninja
cmake --build build/tests/architecture_contracts \
      --target wire_data_plane_contract_tests
ctest --test-dir build/tests/architecture_contracts \
      -R '^wire_data_plane_contracts$' \
      --output-on-failure

bash build_mingw64.sh
```

Expected:

```text
live runtime architecture PASS
live NPC ownership architecture PASS
live replication/guidance architecture PASS
wire schema architecture PASS

navigation_runtime:
    2/2 PASS
      navigation_runtime_control
      navigation_replication_truth

navigation_trajectory:
    11/11 PASS

wire_data_plane_contracts:
    1/1 PASS

EliteGame build PASS
EliteServer build PASS
```

## Next after green

Close stage 11 completely, progress -> 11/12, then immediately start stage 12 end-to-end runtime scenarios and stress/debug/legacy retirement.
