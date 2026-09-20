# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest final-composite evidence

Tested:
```
8011cc3ed19fc027fba256ee4aecca7c93a4ce0f
```

The new diagnostics changed the interpretation of the remaining failure.

At the problematic replans:
- transverse continuity is valid;
- same-branch safe candidate count is zero.

Therefore branch switching is not gratuitous ping-pong at that state. It is a necessary topology/local-free-space change.

## New ownership contract

Local avoidance may identify a safe opposite branch, but a forced branch switch is not itself a physically executable maneuver.

Production now exposes:
```
avoidanceBranchSwitchRequired
```

Higher maneuver ownership must respond with recovery/brake before accepting the new branch if the current motion cannot transition continuously.

This preserves the architecture split:
- B4/local geometry identifies safe target/branch state;
- B5/B7/higher maneuver layer chooses the physical transition;
- B8-B10 execute the accepted physical program;
- physics remains authoritative.

## Composite implementation

The final lab now exercises:
```
accepted branch
 -> no same-branch safe continuation
 -> branch-switch escalation
 -> physical brake/recovery
 -> old branch commitment retired
 -> world re-published
 -> replan from actual stopped state
 -> new branch accepted only after fresh proof
```

Recovery is bounded by real manoeuvre authority and both static/dynamic clearance.

## Focused cross-ring fixture

The regression blocker was moved onto the actual first-ring preferred ray.

This should finally distinguish:
- same branch available at larger angle -> preserve branch;
- same branch unavailable anywhere -> signal recovery before branch switch.

## Longer-term B4/B5 meaning

The route-aligned corridor architecture should eventually carry this structurally:
- branch identity;
- branch exhaustion;
- required branch transition;
- physical recovery/transition candidate.

The current signal is an explicit transitional contract, not hidden planner memory.

## Exit criterion

Final composite green -> synthetic maneuver behavior lab closes.

Then:
- actual NAV STRESS/game;
- accepted corridor/tunnel visualization;
- accepted physical trajectory visualization;
- live NPC/autopilot behavior evaluation.

## State protocol

After every state-affecting event, synchronize all project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
