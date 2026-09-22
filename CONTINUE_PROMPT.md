# CONTINUE PROMPT — execute the Elite navigation-layer blueprint

Continue in GitHub repository `forzub/Elite`, branch `main`.

Your task is to implement the navigation layer described by the normative
architecture document. Do not infer the target architecture from the current
`tools/navigation_runtime` implementation: that path is explicitly
transitional.

## Mandatory reading order

Read these files completely before editing code:

1. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`
2. `CURRENT_STATE.md`
3. `CURRENT_TASK.md`
4. `PROJECT_STATE.md`
5. `src/game/navigation/STAGE12_END_TO_END.md`
6. `src/game/navigation/NAVIGATION_API_CONTRACT.md`

Then inspect only the implementation and tests referenced by the active
migration stage. Follow external references from that code when required; do
not perform an unrelated repository-wide redesign.

## Governing invariant

```text
The program accepted for execution is the exact physical maneuver that was
capability-checked and continuously collision-proved.
```

The required high-level chain is:

```text
navigation world
 -> hierarchical global corridor
 -> local physical maneuver candidates
 -> hull/actuator/resource feasibility
 -> continuous swept-hull proof
 -> time parameterization
 -> immutable accepted state + actuator program
 -> literal execution plus bounded reserved correction
```

Ruckig is an inner timing solver after maneuver-family and physical feasibility
selection. It is not the owner of spacecraft geometry.

## Execute only the active migration stage

The active stage is **M1 — repair coordinate and API boundaries**. Use the exact
requirements and acceptance gate in `CURRENT_TASK.md` and the M1 section of the
blueprint.

Immediate work:

1. trace every semantic-frame conversion in the active runtime;
2. remove the tool-local `toSystemIntent()` vector copy;
3. use `NavigationFrameBoundary` for control intent and initial rigid-body state;
4. make required frame/time snapshots explicit API inputs;
5. add a real non-identity translated/rotated/moving-frame E2E;
6. repair purity/architecture checks so they detect semantic bypasses without
   depending on exact line formatting;
7. run the applicable architecture, build and runtime gates;
8. record exact evidence and remaining failures without weakening contracts.

Do not skip to a later migration stage. If M1 exposes another defect, either fix
it inside M1 scope or document it as a blocker for the next iteration.

## Prohibited shortcuts

- no follower-gain or timeout tuning to hide an invalid plan;
- no acceptance of `propulsionFeasible == false`;
- no direct NavLocal/System/World vector copies;
- no hidden world/config/clock/registry lookups in calculation code;
- no new all-pairs or per-actor-per-frame global search;
- no claim that dense sampling is continuous collision proof;
- no independent interpolation of inconsistent P/V/A states;
- no second actuator allocator downstream of an accepted actuator program;
- no test-authored intermediate product in a product-chain E2E.

## API rule

```text
orchestration resolves explicit immutable inputs
    -> narrow calculation request
    -> pure value result
```

A deterministic stateful executor is allowed to own explicitly passed state.
It may not fetch missing facts through `GameSimulation`, descriptors, globals,
filesystem, environment, ambient time or another layer's internals.

## Verification and documentation protocol

Run all checks relevant to the changed stage. On the target MinGW64 machine,
include:

```bash
bash tests/navigation_runtime/run_stage1_mingw64.sh
ctest --test-dir build/tools/navigation_runtime \
      -R "^navigation_runtime_pipeline$" \
      --output-on-failure
```

Never report a check as passed if it could not run. Preserve failing logs and
separate architecture/build failures from known physics failures.

Every state-affecting iteration MUST:

1. update `CURRENT_STATE.md`;
2. update `CURRENT_TASK.md` to the next exact gate;
3. update `PROJECT_STATE.md`;
4. update `src/game/navigation/STAGE12_END_TO_END.md`;
5. synchronize `NAVIGATION_PIPELINE_AUDIT.md`,
   `CONTROL_LAW_MANEUVER_MODEL.md` and `NAVIGATION_API_CONTRACT.md` when their
   contracts change;
6. recreate this `CONTINUE_PROMPT.md` from scratch;
7. commit and push the coherent iteration to `main` only after reviewing the
   diff and recording the evidence.

When M1 is fully green, advance `CURRENT_TASK.md` to M2 from the blueprint. The
final goal is not a prettier diagnostic stand; it is a scalable, truthful 3D
navigation layer for many heterogeneous autonomous objects.
