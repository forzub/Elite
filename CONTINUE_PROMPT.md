# Elite Navigation v2 — continue prompt

Use this file as the complete handoff prompt when continuing the work in a new chat. **Replace this entire file on every state-affecting iteration; do not append history here.** Detailed history stays in `CURRENT_STATE.md`, `CURRENT_TASK.md`, `PROJECT_STATE.md`, and `src/game/navigation/STAGE12_END_TO_END.md`.

## Repository / environment

- GitHub: `forzub/Elite`, branch `main`.
- Local target machine: Windows 10, MSYS2 MinGW64, g++ 15.2.0, CMake + Ninja.
- Local checkout: `D:/__elite/work`.
- Work directly in the repository, then give exact local verification commands.
- After every state-affecting event, synchronize the four state MD files above and fully rewrite this file.

## Navigation v2 contract already fixed

Automatic navigation is:

```text
PLAN -> prove short segment -> ACCEPT -> EXECUTE + MONITOR -> REPLAN only on invalidation
```

Do not reintroduce frame-by-frame planning. Navigation authority must never silently stop. Manual and automatic behavior remain separate; manual mode ultimately needs a visible guidance tunnel, while automatic mode executes typed control intent.

Control regimes remain distinct:
- Assisted / airplane-like.
- Newtonian.

Coordinate rule:
- NavigationMap / NavigationSpace / planner / follower are NavLocal-only.
- System/world values cross only through `NavigationFrameBoundary`.
- Do not pass mutable owner objects across navigation boundaries.

Legacy route systems remain hard-off:
- old client docking route pipeline;
- old repair-drone route pipeline (fail-closed until ported);
- dormant LocalGuidance/Ruckig must not become runtime authority.

## Last verified architecture/build state

Target-machine checkout `46f6a37da6775a1d044391f773476df1bb07bc6a` successfully reached the freshly built live Stage-12 self-test. Earlier in this chain:
- foundation architecture gate passed;
- Stage-12 runtime-planner gate passed;
- navigation runtime tests passed;
- canonical client/server build passed.

Dynamic obstacle correction already implemented:
- NavigationMap swept sphere is broadphase only when exact geometry is available.
- Dynamic candidates can carry value-owned exact HitVolume-derived OBBs.
- Translation-only dynamic objects receive exact OBB narrow-phase in `LocalHorizonPlanner`.
- Rotating/accelerating dynamic objects conservatively fall back to sphere until continuous swept-OBB ownership exists.
- The live moving-gap fixture now has a real 140 m aperture offset from the direct route; it no longer depends on a false enclosing-sphere collision.
- Source-precision-aware tolerances are pinned for StaticObject float angular/linear velocity storage.

## Latest target-machine evidence

The corrected moving aperture now produces a real visibility bypass and the self-test gets through the same-tick replication proof. The current failure is:

```text
[FAIL] visibility-bypass replication succeeded but the ordered live flight did not complete moving-pair bypass, direct recovery and exact-static tunnel passage inside the 120 s bound

moving_gap_passed=0
slit_portal=0
slit_entry_capture=0
slit_entry_crossed_aligned=0
passed_obstacle_plane=0
exact_static_violation=0
simulated_s=120
slit_entry_cross_track_m=148.3
```

Interpretation:
- visibility bypass itself is now real and replicated;
- exact-static safety remains intact;
- the actor fails to finish the moving-pair bypass / cross its plane / recover to the direct nominal route;
- the slit/tunnel phase is not reached in this run, so do not debug tunnel capture yet;
- this is no longer a stale gate and no longer the old false-sphere deadlock.

## Current task

Trace the accepted adjusted segment **after visibility bypass becomes active**.

Inspect and instrument as needed:
1. selected adjusted target and its relation to the moving-pair plane;
2. accepted segment revision, target, validity/expiry, completion status;
3. every replan reason after bypass;
4. tracking-error and exact-static monitor invalidations;
5. follower desired velocity / PilotSkill executed demand;
6. actual ship progress along the route and lateral displacement;
7. the exact condition that should make the planner return from adjusted visibility steering to the direct nominal target.

The main hypothesis to test is that receding-horizon adjusted targets or segment expiry/completion are preventing forward progress after a valid bypass. Do not assume that hypothesis is correct; trace the live ownership chain.

Do **not**:
- weaken the ordered self-test just to pass;
- restore sphere collision authority;
- re-enable any legacy planner;
- treat `slit_entry_cross_track_m=148.3` as a tunnel failure when `slit_portal=0`;
- claim acceptance until the freshly built self-test passes.

## State bookkeeping rule

After the next code/test iteration:
- append detailed evidence/root cause to the four historical state MD files;
- fully replace this file with the new concise handoff state;
- distinguish tested checkout from docs-only HEAD;
- keep the last actually accepted target-machine baseline separate from unverified candidates.

## Recommended next target-machine command after the next candidate

```bash
git pull --ff-only
git rev-parse HEAD

python tests/architecture_contracts/check_navigation_foundation_lock.py &&
python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py &&
bash tests/navigation_runtime/run_mingw64.sh &&
bash build_mingw64.sh &&
./build/headless_server/EliteServer.exe --self-test-navigation
```

If NavigationMap/local-avoidance code changes again, also run their isolated MinGW test scripts before the runtime suite.
