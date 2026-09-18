# Elite — CURRENT STATE

**Updated:** 2026-09-18
**Canonical branch:** `main`
**Current public HEAD:** `29aa06c6d96f2a32a540f5623eb53edc433ef6ca`

## Stage 12 status

- 12A-1 — ACCEPTED
- 12A-2 — ACCEPTED
- 12A-3 — ACCEPTED
- 12A-4 exact static HitVolume OBB — ACCEPTED
- 12A-5 static/dynamic ownership cleanup — CANDIDATE

## Latest target-machine result

Run on `f8e7c3e1890a621d65d5596e0ad66fc8b759e66c`:

```text
architecture PASS
EliteGame PASS
EliteServer PASS

first_live_probe_blocked=0
first_live_agent_map=(1014.51,-1885.04,-5940.26)
expected_start=(975,-1300,-6200)
```

The previous stale-local-velocity fix changed the observed offset but did not
eliminate it.

## Root cause now identified

The remaining displacement magnitude is about 641 m.

At the server fixed step (~0.02 s), a reference frame moving at ~30 km/s changes
world position by roughly 600 m. The production update ordering matched this
exact scale:

```text
old order:
rebuild current HubNavigationFrame
    -> AI/navigation reads ship.worldPosition from previous frame epoch
    -> later refresh matched travel frame
    -> later updateLocalFrameMotion rematerializes worldPosition
```

Therefore navigation compared a previous-epoch ship pose against a current-epoch
hub origin/basis.

## Correction

Matched HubTactical ships are now synchronized immediately after the current
HubNavigationFrame rebuild and before AI/navigation.

The synchronization:
- refreshes the matched travel-frame epoch;
- rematerializes worldPosition from authoritative localPositionMeters;
- rematerializes worldVelocity from current localVelocityMps;
- does not integrate or change local flight state.

The later duplicate `updateShipReferenceFrames(dt)` call is removed so there
is one authoritative ordering point per fixed step.

Architecture contract now pins:

```text
rebuildHubNavigationFrames
    -> updateShipReferenceFrames
    -> AI/navigation
```

Next live gate must show the first agent map remaining at the configured start
instead of lagging one frame behind the hub.
