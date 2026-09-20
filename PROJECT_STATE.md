# PROJECT STATE

**Project:** Elite Navigation v2
**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
a0f0991791665e30059be15efc47dedcdfafe090
```

## Current chained/limit evidence

Target checkout `350f7d593e22b8b89cb3ae4dbfbfbb7fbb53ea03`:
- architecture PASS;
- build PASS;
- 17/18 runtime tests;
- Newtonian chain healthy;
- Assisted phase 3 exposed an attitude-reference defect.

Assisted phase-3 evidence:
- only 1.58 deg slip at entry;
- grows to 25.79 deg and stays there;
- forward reference error reaches 25.96 deg;
- 171 tracking-envelope exceeded ticks.

So this is not a seam transient.

## Reference-frame defect

The chained fixture used a fixed world-up basis reconstruction with a seed switch near vertical forward.

That representation is discontinuous in roll even for a smooth velocity tangent.

Because B10 owns full-axis attitude tracking, the representation discontinuity is physically significant.

Fix candidate:
```
b8eb4641b013692c773d087d6cad96756c672b3c
```

The moving aligned frame is now parallel-transported/minimal-twist.

General design conclusion:
- ordinary path following should not invent roll from global up at singular headings;
- roll should remain continuous unless maneuver semantics explicitly command roll;
- exact final top/up orientation belongs to an explicit terminal-attitude requirement.

## Active roadmap

1. re-run chained/limit matrix with transported frame;
2. if green, accept chained + negative/limit block;
3. build final composite end-to-end proving ground;
4. then move primary quality evaluation into the game.

## State protocol

After every state-affecting event, synchronize project MDs and recreate `CONTINUE_PROMPT.md` from scratch.
