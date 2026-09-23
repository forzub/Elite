# CONTINUE PROMPT — replace Elite physical maneuver authoring

Continue in repository `forzub/Elite`, branch `main`.

The normative architecture is
`src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`. M1 is
accepted. The M2 scenario-I/O boundary is in place. Do not spend the next
iteration preserving the obsolete translation-first maneuver author: direct
replacement is authorized.

## Read before editing

1. `AGENTS.md`;
2. `CURRENT_STATE.md`;
3. `CURRENT_TASK.md`;
4. `PROJECT_STATE.md`;
5. `src/game/navigation/NAVIGATION_LAYER_IMPLEMENTATION_BLUEPRINT.md`;
6. final dated sections of `src/game/navigation/STAGE12_END_TO_END.md`;
7. `src/game/navigation/NAVIGATION_API_CONTRACT.md`.

For the active slice inspect
`OrdinaryPhysicalManeuverCompiler.{h,cpp}`, its focused tests and only the files
directly referenced by the replacement seam.

## Non-negotiable model

A ship trajectory is not translational P/V/A with attitude fitted afterward.
It is one coupled rigid-body/control-law maneuver whose translation, rotation,
installed actuator allocation, reserves, resources and corridor occupancy are
simultaneously feasible.

The persistent chain is:

```text
NavigationIntent + TerminalContract
 -> ranked corridor/terminal alternatives
 -> physical maneuver solve
 -> candidate or typed InfeasibilityWitness
 -> bounded coordinator mutation and retry
 -> exact capability/resource + continuous swept-hull proof
 -> AcceptedManeuverProgram
 -> literal actuator execution + bounded feedback reserve
```

Failed solve does not disable navigation. The objective and planner state stay
active; the coordinator changes legal corridor, terminal, speed and time
parameters. A last still-proved short program may continue. If collision-free
motion is exhausted, use a separate physically executable and explicitly
contact-predicted `UnavoidableContactMitigation` solve.

## Immediate work

Complete the typed physical-solve contract. Success returns bounded physical
candidates. Failure returns a quantitative witness distinguishing invalid
input/body frame, unsupported law, missing translation or attitude authority,
unmodeled initial angular state, insufficient horizon and numerical failure.
Tests must prove rotate-before-burn and actionable short-horizon/no-angular-
authority rejection.

Then implement the coordinator as a deterministic bounded search over explicit
alternatives. Do not convert the witness into a terminal navigation failure.

## Forbidden shortcuts

- do not increase tracking-loss timeout;
- do not weaken the high-speed assertion;
- do not accept any interval with infeasible actuator allocation;
- do not label an unreachable mathematical reference a trajectory;
- do not use sphere/AABB broadphase as exact free-space truth;
- do not let STANDARD/EXTREME change ship physics;
- do not let pilot skill repair or redefine nominal physical feasibility;
- do not invent reverse-main authority in ASSISTED mode.

## Iteration protocol

After each state-affecting event update the blueprint, current state/task,
project state, Stage-12 journal and this prompt. Run all available gates, inspect
the entire diff, commit and push one coherent iteration to `main`. Never claim a
target result that was not run, and preserve the fact that the earlier supplied
pipeline output did not include its checkout hash.
