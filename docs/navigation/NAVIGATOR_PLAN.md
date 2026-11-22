# Navigator: planner + autopilot (author's task, 2026-11-22)

One navigator for every route kind: from where the ship is to a target with end conditions on demand (velocity
vector, speed, hull attitude). Same controls as the pilot. Plausible, light on the CPU, not hard physics.

## Hard rules (the author)
- Never refuse over trivia. "Assisted can't fly backwards" -> turn it round. Refuse only a real impossibility, with
  the reason.
- Smooth flight: no oscillation about the route direction, no speed sawtooth.
- The autopilot knows its speed, when to brake, and whether the ship in its current state can do the manoeuvre.
- In a hub the green lanes come before open space - with common sense: use them when the way crosses / runs near
  them, never a detour down to a far lane (a free metre costs `FreeInHubFactor` = 2.5 lane metres).
- Test in the hubs: many obstacles, moving ones. The radar sees everything (traffic, stray bodies, debris; nearest
  first when capped) and feeds the autopilot.
- Desyncs: one epoch per tick (server) and per frame (client) - see "Invariants".

## Layers
1. **Goal**: target kind (dock / pad / ship / orbit / settlement / free point), its frame, end conditions.
   Ship: stop in a safe zone (behind it first), close in, take the slot; the leader sets the formation (wedge,
   echelon, column, diamond, line) and the slot (`NavigationArrivalMode`, `NpcNavigationGoal::TrackFormationSlot`).
2. **Strategic**: inter-system jump -> in-system jump / J flight (global velocities, gravity, anomalies) -> orbit and
   hub velocity match -> hub lanes -> atmosphere / planet landing / city.
3. **Tactical**: the local planner (continuous Assisted optimizer + hub lanes + pad service).
4. **Executor**: one tracking law; a smooth speed profile (sample ripple shaved, 8 m/s2 comfort limits, a lagging
   craft keeps its profile); braking from the live capability.
5. **Reflex**: sudden-obstacle evasion from radar contacts, then back onto the route.
6. **Failures**: a capability monitor -> replan with the reduced envelope; emergency auto-control engages by itself
   when the autopilot flies or the pilot lost control (by hand: a warning only).
7. **Neural net**: not now. Candidate later - a warm-start seed for the optimizer trained on logged successful plans,
   kept only if the scenario suite shows it faster and no worse.

## Order
1. Executor smoothness / braking / turn-instead-of-refuse + the scenario suite with metrics  <- in progress
2. One goal spec (dock / pad / ship + formation) robust in the hubs
3. Evasion from radar contacts
4. J in-system + orbit + hub velocity match
5. Atmosphere / planet landing / city
6. Jumps
7. Failures

## Invariants (always on in the suite)
- `ELITE_DEV_EPOCH_CHECK=1` (server): a player's place in its city frame changes by its relative velocity x dt.
  Near the city 0.004 m; a stale epoch shows hundreds of metres.
- `ELITE_DEV_RENDER_EPOCH_CHECK=1` (client): the city in the player's render frame does not move (1e-4 m).

## Scenario suite
`scratchpad/navsuite.sh <tag>` (offscreen, watchdog): pad from outside, pad series across the hub, tower-top pad,
hub dock. Metrics (`navmetrics.py`): plans, max planning ms, replans, failures, collisions, cross-track mean / max,
speed error rms, target-speed reversals on straights, epoch residuals.
