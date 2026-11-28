# Hub navigation — conceptual audit (2026-11-28)

Requested by the author before any further work on the voxel planner. Facts are from the source at 8f12bdd92 and the
live grid logs (D:/tmp/grid_full2, D:/tmp/grid_lanes); proposals are marked as such.

## 1. What each part solves today

| part | what it decides | how it checks feasibility |
|---|---|---|
| Continuous planner (`ContinuousAssistedPathOptimizer`, via `RoutePlanner`) | the SHAPE of one leg: two Béziers + a cruise straight between a start state (position, velocity, heading) and a goal state (position, arrival direction, a mandatory terminal straight), time-optimal | lateral / angular limits, turn radius at speed, clearance against every obstacle - hard constraints |
| `HubLaneNetwork::chooseRoute` | the TOPOLOGY: a Dijkstra over lane stations / connectors plus free join / exit / direct legs | a free leg = a straight segment clear by hull + 10 m, plus a turn-room heuristic (`MinFreeLegMeters + turnRadius x turn`) |
| `SegmentedRouteComposer` | turns the chosen chain into legs, each solved by the continuous planner | per leg (the optimizer) |
| Pad service (server, `GameSimulationPadLanding`) | the final approach: from its takeover point climb to the transit height (525 m over the deck), across (round tall buildings), down onto the pad / into a hangar | its own geometry (`hubPadRoute`, door / bay points) |
| LiftClear / PreplanningSafetyCourse (client) | getting out of contact / onto a safe course before a route exists | rays / contact normals |
| Voxel fallback (2026-11-28, new) | a polyline when everything above failed | voxel search + exact segment checks |

## 2. Why the lanes became a "fallback"

The cost unit changed from weighted metres to SECONDS (70b993f76, 2026-11-24, the author's «все выбираем по времени»).
In the metres mode a free metre inside a hub cost `FreeInHubFactor` = 2.5 and a lane metre `LaneCostFactor` = 0.35 - the
lanes' priority (the author 2026-11-22: «на территории хаба зелёная зона приоритетнее пустого пространства из соображений
безопасности»). In the time mode both factors are gone (`freeLeg` returns `freeTime(length + turn penalty)` at the free
cruise; `HubLaneNetwork.cpp:302-307`): a free metre in the hub costs the same as anywhere. A 6 km direct leg at 150 m/s
(64 s) beats the lane chain (86 s) every time. **The safety priority was a weight in the old cost and silently vanished
with the new unit - a regression of an agreed rule, not a planner limit.**

## 3. Why it planned a free trajectory where a network exists

Two levels judge feasibility differently. The chain search accepts a free leg when its STRAIGHT segment is clear and the
turn-room heuristic passes; the optimizer must then fly it as a curve that starts at the ship's heading and arrives along
the goal's direction after a 764 m mandatory straight with a 1275 m turn radius. In the failed starts the straight was
clear (the leg ran above the city, to the approach point 525 m over the deck) but the optimizer's best shape violated its
limits (violation 0.106, 5300 evaluations, 0 valid) - three times, deterministically. The search chose a leg it cannot
know to be flyable; the lanes-only retry (2026-11-28) works because the lane chain's legs are short and well posed.

## 4. Why not first climb out into free space

No such strategy exists on the client. "Over the structures" exists only in the SERVER's pad service (the transit height)
and, for contact, in LiftClear (along the contact normal, ~60 m). The planner never composes "climb / move outward to
open space, then plan" - so a start low among buildings or under the transit layer has no simple way out other than the
optimizer or (now) the voxel search.

## 5. Where a real voxel search is needed

Not demonstrated by any live scenario. The only live use was a DEV start inside the hub's solid core box - an invalid
physical state (audit #12 rightly calls it so). Hangars: the pad service flies the door / bay points itself; docks: their
corridors. The candidate cases are local and rare: a ship wedged under an overhang or in a gap after manual flight /
a collision, where the straight climb is blocked. **A 3D maze search over the whole city is not needed when the problem
is solved by leaving to open space.**

## 6. Duplications

- The segment "over the city to the pad" has two owners: the client planner (its goal = the pad's approach point, or a
  lane exit) and the server pad service (climb / across / down from its takeover).
- "Out of contact" has three: LiftClear, the PreplanningSafetyCourse escape, the voxel escape.
- The voxel fallback brought a SECOND route executor (`m_dockFallback`, stop-turn-go) next to the route autopilot - against
  B3 (one executed route).

## 7. Proposed model (keeps every agreed rule)

1. **The hub envelope**: the city's built volume + margin, from the hub plan (blocks, heights, the ring's deck); the lane
   tubes are inside it.
2. **Rules first, time second (lexicographic, not weights)**:
   - R1 the hard rules as today (no cut through a lane tube, joins from open sky, clearances, the ring's sync gate on the
     axis, Newtonian ships to the outskirts docks, the dispatcher);
   - R2 inside the envelope only: lanes, their join / exit legs (bounded length), and the pad service's transit; a free
     leg through the envelope's interior is not an option;
   - R3 outside the envelope: free flight at cruise;
   - R4 among the options R1-R3 allow: minimum time (the author's 2026-11-24 rule, now a tie-breaker inside safety).
   (The author's 2026-11-22 «баланс» rule for the pad service's straight way stays R2's bounded exception.)
3. **Strategy ladder** (each finite, told):
   1. the plan under R1-R4;
   2. no reachable lane entry / no feasible leg: a deterministic CLIMB-OUT leg (the hub's outward normal / "up" over the
      deck, the ring's axis side) to outside the envelope, then 1. again;
   3. the climb is blocked (confined: an overhang, a gap): a LOCAL voxel escape bounded to that space (not the city);
   4. refusal only for a proven enclosure (the author's exception).
4. **Owners**: the pad service owns the final approach from the lane exit (the client's goal for a pad = the lane exit,
   `endAtLaneExit`, as designed 2026-11-22); one route executor (the autopilot) flies every leg - the climb-out and a
   local escape polyline as ordinary route gates; `m_dockFallback` goes.
5. **Feasibility in one place**: the chain search's free-leg test uses the same limits the optimizer will apply (the
   terminal straight, the turn radius at the leg's speed), so a chosen leg is a flyable leg.

## 8. What this means for the current code (proposal; nothing changed yet)

- Restore the lanes' priority as constraint R2 in `HubLaneNetwork` (the envelope test on free legs), time as R4.
- Replace strategy 3 of 2026-11-28 (the city-wide voxel route) by the climb-out leg; keep the voxel code only as the
  local confined escape (strategy 3 of the ladder), its extension stopped until a scenario proves the need.
- Fold the fallback executor into the route autopilot (B3).
- Then re-run the B2-04 grid: success must come from strategy 1 (lanes), not from retries.
