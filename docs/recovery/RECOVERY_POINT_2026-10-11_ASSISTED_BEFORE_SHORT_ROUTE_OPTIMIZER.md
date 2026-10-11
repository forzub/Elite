# RECOVERY POINT — Assisted optimizer before short-route rewrite

Date: 2026-10-11

This recovery point freezes the last state before the route-length / travel-time
rewrite of the Continuous Assisted optimizer.

## Immutable recovery commit

```
9412d7c35cf7469422e9c239afcc4890f3ad68a2
```

Commit message:

```
test: prove stationary safety uses hull-forward launch ray
```

## Recovery branch

```
restore/assisted-before-short-route-optimizer-2026-10-11
```

This branch was created directly from the commit above before modifying the
optimizer.

## Restore commands

Inspect without moving the working branch:

```bash
git fetch origin
git switch restore/assisted-before-short-route-optimizer-2026-10-11
```

Reset the experimental branch back to this exact point if the optimizer rewrite
must be abandoned:

```bash
git switch feature/assisted-continuous-optimizer-lab-2026-10-11
git reset --hard 9412d7c35cf7469422e9c239afcc4890f3ad68a2
```

Do not delete the recovery branch until the short-route optimizer has passed the
navigation runtime suite and the live BLUE proving run.
