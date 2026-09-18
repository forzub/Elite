# Elite — CURRENT TASK

**Updated:** 2026-09-18
**Stage:** 12A-5 — verify authoritative reference-frame placement reset

## Candidate HEAD

```text
e83a0a2bd5d54efb7737607c40ff789cafee5cf5
```

## Previous failure

The first live planner probe started at:

```text
(1014.47,-1767.96,-5679.82)
```

instead of configured:

```text
(975,-1300,-6200)
```

Root cause: `placeShipInReferenceFrame()` retained stale
`motion.localVelocityMps` and propulsion state.

## Correction

Reference-frame placement now clears all authoritative local motion/control
residue before the ship enters the new frame.

## RUN

Only the affected architecture/build/live gate is needed:

```bash
cd /d/__elite/work

git fetch origin
git switch main
git merge --ff-only origin/main

git rev-parse HEAD

python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

bash build_mingw64.sh

./build/headless_server/EliteServer.exe --self-test-navigation
```

## Expected interpretation

If the placement bug is closed, the first live probe should begin near:

```text
first_live_agent_map ~= (975,-1300,-6200)
```

and then:

```text
first_live_probe_blocked=1
first_live_blocker_entity=<CUBE 08 entity>
exact_obstacle_block=1
exact_static_block=1
adjusted=1
```

If physical execution later violates exact geometry, the self-test will stop
immediately and print the exact violation entity and swept motion witness.
