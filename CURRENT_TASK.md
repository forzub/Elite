# Elite — CURRENT TASK

**Updated:** 2026-09-18
**Stage:** 12A-5 — verify current-epoch reference-frame sync

## Candidate HEAD

```text
29aa06c6d96f2a32a540f5623eb53edc433ef6ca
```

## What changed

The last ~641 m first-plan offset matched one fixed step of orbital/world frame
motion.

Reference-frame synchronization now occurs before AI/navigation:

```text
HubNavigationFrame current epoch
    -> refresh matched travel frame
    -> rematerialize ship world pose from localPositionMeters
    -> navigation planner
```

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

## Expected picture

First navigation state should now stay on the authored start:

```text
placement_map ~= (975,-1300,-6200)
first_live_agent_map ~= (975,-1300,-6200)
```

Then CUBE 08 should be detected:

```text
first_live_probe_blocked=1
exact_obstacle_block=1
exact_static_block=1
adjusted=1
```

If execution then intersects any exact HitVolume, self-test already fails fast
with the exact entity and swept-segment witness.
