# CURRENT TASK

**Updated:** 2026-09-20 Europe/Kyiv

## Accepted baseline

```
3fe9b54eda0135b0cdebb7dc835d8a4b17580808
```

## Latest tested composite checkout

```
18f93e15f3baa5218d459b289ae89beb170f1c54
```

Architecture PASS, runtime 18/19.

Production planner now correctly returns:
- nominal dynamic conflict = 1;
- `AdjustedClear`;
- 20 bounded probes;
- no static block.

The only failure is the test-side physical replacement execution.

## Diagnosis

The old replacement curve:
- duration 6 s;
- exit speed 8 m/s;
- target = production adjusted target.

From the exact live P/V shown by the test, that quintic requires about 5 m/s2 transverse acceleration.

That is not compatible with the 2 m/s2 Cobra manoeuvre authority plus B10 feedback reserve.

So the test was asking physics to execute a route target with an invalid time parameterization.

## Current candidate

```
7444c5930586300d6cac48bd4b2fa63b27e96bd6
```

The test-side replacement authoring now fits duration/exit speed to physical authority.

Fit gate:
- peak transverse FF <=1.35 m/s2;
- minimum planned speed >=0.50 m/s;
- minimum planned dynamic clearance >=1.50 m;
- shortest valid candidate from duration 8..32 s and exit speed 4/2 m/s.

Execution gate:
- zero tracking-envelope violations;
- actual dynamic clearance >0.5 m.

## Target commands

```bash
cd /d/__elite/work
git pull --ff-only
git rev-parse HEAD

OUT="navigation_test_$(date +%Y%m%d-%H%M%S).txt"

{
    echo "===== TESTED HEAD ====="
    git rev-parse HEAD

    echo
    echo "===== ARCHITECTURE CONTRACT ====="
    python tests/architecture_contracts/check_navigation_stage12_runtime_planner.py

    echo
    echo "===== NAVIGATION RUNTIME ====="
    bash tests/navigation_runtime/run_mingw64.sh
} 2>&1 | tee "$OUT"

echo
echo "===== FINAL COMPOSITE SUMMARY ====="
grep -E '\[COMPOSITE-PLAN\]|\[COMPOSITE-REPLACEMENT\]|\[COMPOSITE-REPLACEMENT-ACTUAL\]|\[COMPOSITE\]|NAVIGATION COMPOSITE PROVING GROUND|tests passed|tests failed|TESTED HEAD' "$OUT" || true

echo "$PWD/$OUT"
```

Upload the complete log.

## Exit

If 19/19:
- record exact target checkout + composite metrics;
- close synthetic behavior lab;
- next task = real NAV STRESS/game corridor + physical trajectory visualization.

If red:
- compare planned and actual replacement evidence;
- fix the exact physical/reference seam;
- do not loosen authority, clearance or terminal requirements.

## Iteration rule

After every state/evidence change, synchronize all MDs and recreate `CONTINUE_PROMPT.md` from scratch.
