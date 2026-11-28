# Hub navigation — conceptual audit (2026-11-28, revision 2)

Requested by the author before any further work on the voxel planner or a "hub envelope". Facts are from the source at
76ee41b09 (file:line), the live logs and controlled experiments; proposals are marked as such. The hypothesis under
test: the failures come from how navigation TASKS are posed (destination, arrival conditions, their owners), not from
the search.

**Verdict: confirmed.** The B2-04 failure is a mis-posed task: the request demanded a mandatory terminal straight of
8820 m on an 8374 m route (section 11). With that one value corrected the ORIGINAL continuous planner solves it at the
first attempt. The revision-1 proposal (a hub envelope with lanes mandatory inside it) is withdrawn as the primary
remedy (section 8).

## 1. Current architecture (what each part does)

| part | decides | feasibility it checks |
|---|---|---|
| Continuous planner (`ContinuousAssistedPathOptimizer`, via `RoutePlanner::planContinuousAssisted`) | one leg's shape: hard start line along `start.tangent` -> cubic -> optional cruise straight -> cubic -> hard goal line along `goal.tangent`; a speed profile per candidate (slows for curvature, braking / acceleration passes) | HARD (violation): collision (>= 1.0), curvature above 1/Rcomfort, a start speed it cannot shed, angular acceleration; exact by construction: both positions, both tangents, both protected straights |
| `HubLaneNetwork::chooseRoute` | the topology (lanes, connectors, free join / exit / direct legs), costed in seconds | a free leg = a straight clear by hull + 10 m and a turn-room heuristic |
| `SegmentedRouteComposer` | the chain's legs, each solved by the continuous planner, recomputing their own leads / terminal straights | per leg |
| Hex dock (server, `DockServiceTraffic`, `DockTrafficController`) | admission, the pad, the capture box, the corridor, docking, the exit along the axis | its own geometry (`HexDockGeometry`) |
| Pad service (server, `GameSimulationPadLanding`) | the pad approach: climb to the transit height, across, down; hangar door / bay steps | its own route |
| Ring sync (client) | the sync gate on the axis, the hold, the co-rotating leg | client thresholds |
| LiftClear / PreplanningSafetyCourse / voxel fallback (client) | out of contact / onto a safe course / a polyline when everything failed | rays, contact normals, voxels |

## 2. Confirmed errors in how tasks are posed

1. **The terminal straight is computed at the ship's maximum speed (500 m/s) BEFORE the hub limit (150 m/s) is applied,
   and the direct (non-segmented) path never recomputes it** (`SpaceStateDocking.cpp:3470` computes it; the lanes run
   `HubLaneRouting.cpp:80-94` caps the speed; only obstacles, speeds and straights are copied back at `:3628-3631`).
   B2-04: 1.25 x 500^2 / (2 x 17.65) = 8820 m of mandatory straight on an 8374 m chord. The segmented path recomputes its
   legs (`SegmentedRouteComposer.cpp:743-751, 973-1022`) - which is why the lanes-only retry worked.
2. **A visual parameter is the planner's only hard minimum radius**: `minimumComfortRadiusMeters =
   requiredGuidanceTunnelRadiusMeters(gateSpacing 500 m) = 955 m`, whatever the speed (`RoutePlanner.cpp:488-500`,
   `OrdinaryRoutePolicy.h:209-234`). A U-turn from rest costs a ~1.9 km loop because the HUD's frames are 500 m apart.
   (The 1275 m in the lane log is v^2/a_lat at 150 m/s for the lane MODEL only; the optimizer never sees it.)
3. **An open pad is posed as a docking port** (`SpaceStateDocking.cpp:878`, `HubSemanticAnchorKind::DockingPort`): a
   mandatory HORIZONTAL arrival tangent "from the pad toward the ship" (`:868-873`, `terminalOutward = forward` `:3340`),
   the docking terminal policy (`requiredDockingAxisMinimumMeters` >= 700 m) and the route-up reference. None of it is a
   pad's requirement: the pad service flies the climb / across / down itself.
4. **The start must leave along the nose** (`start.tangent` = the hull nose at rest; the shape family has no turn in
   place): a ship at rest pointing away pays a loop bounded by error 2's 955 m floor.
5. **Handovers assumed, not confirmed**: `LandOnPad` -> `releaseClientAutopilot` + `requests.clear()` + `m_padAnchor = {}`
   at once (`SpaceStateDocking.cpp:1630-1640, 3652-3666, 4855-4865`); the server may refuse (`GameSimulationPadLanding.cpp
   168-243, 297-382`) and nothing retries. Also the station's early release (`SpaceState.cpp:1136-1147`) and
   `RouteService` (`SpaceStateRouteRun.cpp:177-185`).
6. **The dock goal is hand-copied**: the client plans to the JSON anchor `dock_gate_front` (`hub_semantic_anchors.json
   6-37`) + `max(300, 10L + 4 clearance)` (`:3324-3335`), not to `HexDockGeometry`'s axis join / the server's capture box
   (z -1000..-150, radial < 60 m, < 25 m/s, `GameSimulationDockTraffic.cpp:864-878`); a hull longer than ~80 m gets a goal
   outside the capture box (arithmetic). After the client route ends and before the capture nobody flies the ship
   (`SpaceStateDocking.cpp:4966-4976`).
7. **Dispatcher data unused**: pilot ships always go `directEntry` (no hold, `GameSimulationDockTraffic.cpp:605-611`) and
   are admitted with a hard-coded Cobrett envelope and Medium class (`:601-603, 778-779`) - the real hull is not checked.
8. **More than one control owner**: one shared flag `m_clientAutopilotControlActive` cleared by several paths without
   checking the owner; capsule rescue outside `arbitrateFlightTask`; several pilots may submit in one frame
   (`SpaceStateFormation.cpp:153-219`, `SpaceState.cpp:3345-3352`).
9. **The cost unit change lost the lanes' preference** (70b993f76: `FreeInHubFactor` 2.5 / `LaneCostFactor` 0.35 exist
   only in the metres mode) - an agreed rule (the author 2026-11-22) dropped silently.

## 3. Constraints per destination type (current -> proposed)

| destination | constraint | owner today | proposed owner / value |
|---|---|---|---|
| hex dock | entry corridor 100 m, slot 160x60, axis, spin 2 deg/s, speeds 12 / 8 m/s, pad fit | `HexDockGeometry` (server) | same; the client goal = the dock's own capture box (from the dock, not a JSON copy) |
| hex dock | terminal straight >= 700 m | `OrdinaryRoutePolicy` | the dock's corridor length + the braking room at the corridor's entry speed (12 m/s) |
| hex dock | waiting place | dispatcher (holds exist, pilots bypass them) | dispatcher (author's intent: the planner brings the ship to the ASSIGNED place) |
| open pad | horizontal arrival tangent, 700 m straight, docking up | `SpaceStateDocking` synthesis | **none**: arrive in the service's takeover AREA (around the hover point), any direction, speed <= the service's takeover speed |
| open pad | climb / across / down, the place | pad service (server) | same; the takeover CONFIRMED (the snapshot's `padService` phase) before the route is dropped |
| hangar | door, bay, steps, speeds | pad service | same (not inherited by open pads) |
| ring | sync gate on the axis, hold, roll sync | client | a gate task (arrive on the axis, match the spin); the co-rotating leg is a separate task in the city frame - the gate's axis does not pass to it |
| body surface | site, descent, ground | Landing / Body autopilot | unchanged (no docking axes; confirmed by `freeFlightSurfaceState`) |
| arbitrary point | near < 1500 m / < 20 m/s etc. | `RouteLegPilot` | the initiator's stated conditions only (pass / stop / area / speed / optional direction) |
| pursuit / formation | relative offset, matched velocity | `updateFormationTask` (client, DEV) | the initiator's relative state; then the escort service |

Physical limits are never a destination's: braking room, turn radius at a speed, angular acceleration come from the ship
(`RouteRequestCapability`) and are the planner's to satisfy, on curves too (section 5).

## 4. Owners of truth

| owner | creates | receives | decides | must not decide | hands over when |
|---|---|---|---|---|---|
| infrastructure config (dock geometry, hub plan, lanes, pads) | corridors, capture boxes, transit heights, lane tubes | - | - | the route | - |
| dispatcher (dock / ring / pad) | admission, place, queue | request + hull | who, where, when | the path | snapshot phase (confirmed) |
| task initiator (UI, script, capsule) | the destination + arrival conditions | - | what is wanted | how to fly | - |
| planner | the route (geometry + speed profile) | task + ship capability + world | the path | arrival conditions not in the task | - |
| route executor (autopilot) | control | the route | how to follow | the route | the service's CONFIRMATION |
| pad / dock / escort service | its own control | the ship at its takeover | its final manoeuvre | - | its own completion |
| server physics | the authoritative state | control | everything physical | - | - |
| HUD / maps | presentation | the executed route | nothing | anything | - |

## 5. Proposed task contract (on the existing `RoutePlanRequest`; no parallel structure)

- Spatial: goal point OR goal area (radius / box), frame, the frame's epoch, tolerance.
- Kinematic: arrival speed or range; `terminalOutward` OPTIONAL (`std::optional` / a `hasTerminalDirection` flag - today
  it has a default value and is always a hard tangent); hull attitude optional; relative velocity for moving targets.
- Infrastructure: mandatory corridors (exists: `mandatoryTangentStraights` - from the infrastructure only), speed limits by
  region, forbidden regions, the assigned place.
- Completion: reach point / area / state, or "until the service confirms the takeover".
- Absent = unconstrained. Derived values (braking room, leads) are computed by the PLANNER from the final capability and
  speed limits, never stored stale in the request.

## 6. Keep

The continuous optimizer and its speed profiler; the route-draft net; the lane network and the composer; the hex dock and
its dispatcher; the pad service (incl. hangars); the ring sync; the body / landing autopilots; the progress watchdogs;
the lanes-only recovery (it is a correct alternative topology).

## 7. Duplications

- "Over the city to the pad": client planner + pad service -> the pad service owns it from its takeover area.
- "Out of contact": LiftClear + safe course + voxel escape -> one escape (LiftClear), the voxel only for a proven
  confined case.
- A second route executor (`m_dockFallback`) -> removed (B3).
- The dock goal re-derived from a JSON copy -> taken from the dock's geometry.

## 8. Artificial constraints to remove (and the revision-1 envelope)

- The stale 500 m/s terminal straight (error 1) - a bug fix.
- The 955 m radius floor from the HUD gate spacing (error 2) - the physical limit is the speed profile's; the frames
  adapt their spacing to the curvature (the author's readability rule kept: the tunnel stays continuous).
- The pad's horizontal arrival tangent and the docking terminal policy for pads (error 3).
- The nose-tangent start at rest (error 4) - a turn-in-place start segment for a ship at rest (Assisted has the authority;
  the planner then starts along the needed course).
- The revision-1 "hub envelope with lanes mandatory inside" is NOT needed for B2-04 (its failure was error 1) and would
  forbid the reasonable case "already over the city in safe space". Instead the lanes' PREFERENCE returns as a policy
  term in the time cost (an agreed rule), with forbidden regions as hard constraints - order: rules -> feasibility ->
  safety policy -> time.

## 9. Planner vs task formation

| change | where |
|---|---|
| terminal straight from the final speed limits | task formation (the request's derived values recomputed after the lanes' caps, or computed in the planner) |
| optional arrival direction / arrival area | contract + task formation (pads, points) + the optimizer accepting a free goal tangent |
| radius floor from physics, frames adaptive | planner (drop the comfort floor as a hard constraint) + HUD frame placement |
| turn in place at rest | planner (a start segment) |
| confirmed handovers | executor / task lifecycle |
| dock goal from the dock geometry, real hull to the dispatcher | task formation + server |
| one control owner | client control arbitration |

## 10. Transition plan (each step: build, tests, the B2-04 grid, live landings, commit)

1. Fix error 1 (recompute the terminal straight / leads after the speed caps; the planner derives them). Expected:
   the grid succeeds on the FIRST attempt.
2. Pads as their own destination kind: an arrival area, no tangent, no docking terminal policy; the takeover confirmed
   by the snapshot before the route is dropped.
3. The radius floor: physics in the optimizer, adaptive frames on the HUD (measure the routes' length / time before and
   after; frames still continuous).
4. Turn-in-place start at rest.
5. Confirmed handovers for the station release and `RouteService`; one control owner.
6. Dock goal from `HexDockGeometry`; the real hull to the dispatcher.
7. Retire the voxel fallback from the normal ladder (keep the code for a proven confined case, its own executor removed).
8. The lanes' preference as a policy term (after 1-3 show where it still matters).

## 11. B2-04 — the actual request and the experiments

Failed request (live dump, `[RouteRequestDump]`, start point city (5588, 2810, 0)):

| parameter | value | source / owner | justified? |
|---|---|---|---|
| destination type | pad B2-04, synthesized DockingPort | `SpaceStateDocking.cpp:878` | no (error 3) |
| goal point | hover point 525 m over the deck (`laneCeiling + 40`), + 80 m along `forward` | `:859-862, 3324-3335` | the hover point yes; the +80 m and the docking ref no |
| arrival direction | (0.21, 0, -0.98) horizontal, pad -> ship | `:868-873`, `terminalOutward` | no |
| start speed / tangent | 0 / the hull nose (0.98, -0.22, 0) | `:3321-3323` | the nose tangent at rest: no (error 4) |
| arrival speed | 0 | the pad port | the service takes over moving ships too - to check |
| max / geometry speed | 150 / 150 (hub cap 500 -> 150) | `HubLaneRouting.cpp:80-94` | yes (the hub limit) |
| braking / lateral | 17.65 / 17.65 m/s^2 (the Assisted third of 0.9 x installed) | `RouteRequestCapability.h:66-88` | yes (physics + reserve) |
| protected start straight | 67 m | `requiredAssistedStartLeadMeters` at rest | ok |
| protected goal straight | **8820 m = 1.25 x 500^2 / (2 x 17.65)** | `:3470`, stale (error 1) | **no** |
| minimum radius | 955 m = 500 m gates / (pi/6) | `RoutePlanner.cpp:488-500` | no - visual (error 2) |
| 1275 m | v^2 / a_lat at 150 m/s, the lane model only | `HubLaneRouting.cpp:38` | not used by the optimizer |
| 764 m | the lane model's join approach | `freeLegModelFor` | not used by the optimizer |
| handover | 250 m from the hover point -> LandOnPad, the route dropped at once | `:4849-4865` | unconfirmed (error 5) |

Controlled experiments (`ELITE_DEV_NAV_EXP`, one constraint removed each, the first attempt's outcome):

| experiment | outcome | evaluations | route time |
|---|---|---|---|
| baseline | **infeasible** | 5433 | - |
| termcap (the terminal straight from the design speed: 797 m) | **valid, the net's fast path** | 244 | **67 s** |
| nofloor (no 955 m radius floor) | valid, fast path | 222 | 146 s |
| freegoal (no mandatory arrival tangent) | infeasible | 5411 | - |
| nostart (start straight 30 m) | infeasible | 5235 | - |
| all four | valid, fast path | 266 | 65 s |

The original continuous planner solves the task once the task is posed right; the search algorithm needs no change for
this case. The radius floor alone also unblocks it, at twice the route time (a loop).

## 12. Acceptance criteria

- The B2-04 grid (10 starts): every start LANDED on the FIRST plan (no replan) after step 1; route times recorded.
- No arrival constraint in a pad request beyond the service's takeover area (a source check + the request dump).
- No route dropped before the service's takeover is replicated (a source barrier + a live refusal case: the server
  refuses, the client keeps / re-plans and tells).
- One control writer per frame (a DEV counter of submitters per frame = 1).
- HUD / map route = the executed route (B3 barrier), frames continuous with adaptive spacing.
- Docks: a long-hull request gets a goal inside the capture box; the dispatcher sees the real hull.

## 13. Space-access zones — inventory against the access-regulation principle (2026-11-28)

The principle (the author): one dynamic system of space-access regulation; geometry and rights are separate; colour is
DERIVED per ship (green = preferred organized traffic, blue = open to THIS ship by a permit, red = forbidden to THIS ship,
none = free space); the server's service owns rights; planner / autopilot / HUD / maps only read. Red is a rule, not a wall.

| mechanism | geometry / frame | rights | colour | server enforcement | planner use |
|---|---|---|---|---|---|
| hub lanes + red boxes (`HubLaneNetwork`, `hub_lanes/*.json`) | lane tubes, connectors, OBB red boxes; city module frame (rotating on rings) | none (static `maxShipLength` only) | global (`kind=="blue"` only recolours the map) | wrong-way only (players); red boxes, speed limit, size NOT enforced | red boxes = obstacles; lanes = time-cost preference (lost, section 2) |
| navigation volumes + traffic graph (`NavigationVolume`, `TrafficRouteGraph`) | volumes in a module frame | none | policy KeepOutside / KeepInside / PreferInside, global | none | KeepInside validated (`cylinder_b.blue_lane`, hard-coded) |
| system route zones (`route_zones.json`, `RouteZones.h`) | spheres / capsules anchored to bodies / hubs / L-points (moving) | none | fixed field | 4 Hz judge, fines (players only) | system planner: red = avoid, blue gate = via point (official) |
| hub security (`HubSecurity`, `GameSimulationHubSecurity`) | lane tubes, defence perimeter | per-ship STATUS (warned / fined / locked), replicated | - | yes (players only) | - |
| pads / berths (`HubPadZone*`, `occupiedBerths`) | box over each pad, city frame | per-ship reservation (pad service), replicated | **derived per viewer** (HubCorridorHudRenderer:338-356) | capture does NOT check the berth right | - |
| hex-dock dispatcher (`DockTrafficController`) | none for its resources (`outer_lane`, `chamber_lane`); the corridor = hard-coded thresholds | **per-ship clearance, revision, holder** - server-owned, replicated, the same for NPCs and players | HUD terminal | admission / queue; an uncleared ship in the corridor is not judged | not used (timing only) |
| J keep-out (`JDrive`) | 20 km objects / 5 km ships | - | - | refusal / drop (all ships) | arrival aim point |
| interstellar lane records | jump lanes | per-ship knowledge | - | - | jump route |

Matches the principle: the dock dispatcher (server-owned per-ship rights, one mechanism for NPCs and players) and the berth
colour derived per viewer. Does not: everything else is a global colour without an owner / permit / validity; three
private geometry types; enforcement for players only; red boxes / speed limits / uncleared corridor entry not judged.

Proposed base (one contract, no parallel system): geometry = `NavigationVolumeDefinition` (+ converters from lane tubes,
red boxes, route-zone primitives, pad boxes, an AUTHORED dock corridor); rights = the `DockTrafficController` pattern
generalised into a server zone-access service (per ship per zone: open / permit / forbidden, permit id, validity,
revision; dock clearances and berth reservations become its grants), replicated per ship like
`ShipDockDispatcherSnapshot`; colour = one shared function `f(policy, access)` for planner, HUD and map; the planner maps
forbidden-for-me -> obstacle, blue-for-me -> KeepInside stage, green -> PreferInside cost; one judge for all ships,
violations separate from collisions. To be implemented with stage 6 of the universal-navigation programme.

## 14. Moving-target navigation (rendezvous / intercept) — baseline (2026-11-28, source audit)

The universal planner cannot pose or fly an intercept today. What exists chases a moving ship by per-tick velocity laws:
- `RouteLegPilot` ship leg (RouteLegPilot.cpp:144-180): the target's CURRENT position + velocity, no lead; arrival
  < 1500 m / < 20 m/s; ends in Stabilization (a hub / own orbit), no handover to formation.
- Formation (SpaceStateFormation.cpp:65-151, FormationGuidance.h): Wide -> Behind -> Close -> Hold, slot offsets,
  re-solved every frame; position from the leader's interpolated render pose, velocity from its raw transform (an epoch
  mix - to check).
- NPC slot tracking (`TrackFormationSlot`): `v = v_target + clamp(k dp)` toward a KNOWN script point.
- NPC combat / the DEV rammer (GameSimulationCombat.cpp:1056-1212): server-truth finite-difference tracks; it overwrites
  orientation and velocity directly - bypasses the ship plant and both control laws.
- Missiles: proportional-navigation-like.
- Evasion: constant-velocity threat prediction, 12 s horizon (avoidance only).

Gaps for a planner-based intercept: no time-parameterized goal or goal velocity in `RoutePlanRequest` / the optimizer
(boundary state = position, tangent, speed; no clock); obstacles static; no ship-fixed reference frame / carry-back (the
co-rotating map exists for ring pads only); the docking entry takes semantic anchors / hub frames only; no intercept-point
solver; no rendezvous -> hold handover; no target-manoeuvre replan trigger; NPC pursuit outside the plant.

The precedent to reuse: the ring pad's co-rotating map (CityCoRotationMap.h, SpaceStateDocking.cpp:749-790) - a route
planned against a target frozen at t0, the ship's state carried into that frame every tick. A translating target's frame
is Galilean (pointToMap = p - (p_T0 + v_T (t - t0)), vectorToMap = v - v_T): "stop at the slot in the target frame" =
"match its velocity at the slot". `KinematicFrame` already carries velocity / acceleration. Legitimate target data: the
replicated transform (+ velocity), radar `RadarScanReport` (relative position / velocity + uncertainty + latency, no
acceleration), never the target's commands.

Readiness (baseline, before stage 9): task posing 2/10, target prediction 3/10 (constant velocity exists in the evasion
code only), route building 1/10, execution 4/10 (the formation law reaches and holds), robustness to target manoeuvres
2/10, handover after closing 2/10. Harness for stage 9: `spawnCombatTarget` + `ELITE_DEV_RAM_STRAIGHT` (a constant-
velocity mover) or a lane traffic ship; baselines `leg:ship:any` and `join:<id>:column:0`; the headless
`NavigationScenarioRunner` for the planner in the frozen target frame.

## 15. Stage 1 (universal navigation programme) — derived constraints from the FINAL request (2026-11-28)

Cause (confirmed, section 11): the start lead and the terminal approach were derived once, from the ship's maximum speed,
before the hub lanes capped the speeds; the direct path kept the stale values. Owner now: one helper,
`deriveSpeedConstraints` in `updateDockingStabilizing` (SpaceStateDocking.cpp), called when the request is formed, when
the tunnel cadence is set, and again after the hub lanes' caps - every speed-derived constraint from the final speeds; the
terminal approach from the speed that may ARRIVE there (the geometry speed / the zone's limit), not the route maximum.
Log `[DockDerived]` (formed 8819.84 m -> after the caps 764.787 m at the B2-04 side point). The segmented path already
recomputed its legs (unchanged). Not a universal constant: the length follows the request's speeds, braking, hull.

Grid (10 starts, B2-04, `D:/tmp/grid_stage1/summary.txt`): **8 of the 9 valid starts LANDED on the FIRST plan (0
replans)**, 92-101 s landings; the DEV start inside the solid core (0,6000,0) refused finitely (an invalid state). Point
(-8000,500,-8000) recorded HANG: the plan was valid at the first attempt and flown at 43 m/s when the CLIENT main thread
stalled 606 s in one state update (`phase=state-update duration_ms=606515 ... foreground_pid=0` - no foreground window,
likely the desktop locked at night). Its re-run: LANDED on the first plan (`D:/tmp/grid_p10`). The stall is an open
intermittent defect (E2 / D2: the client-driven clock under a lost foreground), not a navigation one.
Before the fix: 6 / 10 landed, 3 of them only after replans; 3 refused.

## 16. Stages 2 / 8 (open pad as its own task), 4 (mandatory straights), 7 (real hull) — 2026-11-28

- Contract (`RoutePlanRequest::terminalDirectionFree`): an absent arrival direction is NO constraint. With it the
  optimizer's goal tangent is the start -> goal course (the shape's own, not a task constraint) and the lane model gets no
  goal axis. Docks / gates / corridors keep their axis (false by default).
- The open pad task (SpaceStateDocking): the goal = the pad service's takeover point (the hover point), no +80 m
  docking reference offset, no arrival direction, no terminal straight (the profile brakes on the way, curves too). The
  ring's first leg to its sync gate keeps the gate's axis (infrastructure). Grid (9 valid starts): 9 / 9 LANDED, 8 on the
  first plan; the start 6 km under the city (0,-6000,0) took 2 replans - its inbound lane-join leg had a 700 m derived
  straight (next item). Ring A1-03 landing PASS.
- Mandatory straights (stage 4): a lane join's straight is the lane's own join straight (a mandatory stage of the lane
  geometry); the inbound leg's derived terminal straight `max(docking-axis 700 m, 1.25 x braking)` is gone (the composer
  and the lane model alike) - the speed profile brakes to the lane's hand-off speed. The goal's own approach (a dock with an
  axis) is a separate field (`goalApproachMeters`), unchanged. Regression: below.
- Real hull to the dispatcher (stage 7): `pilotDockingEnvelope` - the ship's logical dimensions and current mass (the
  Cobrett traffic class only when the descriptor has none) in both pilot request paths; the dispatcher's physical fit now
  checks the real hull. Live: plane_demo dock - accepted, entry cleared, station handoff, DOCKED. The size CLASS stays the
  policy value Medium: there is no hull-size class rule in the data - defining one is the author's decision.
- Regression after the lane-join change (`D:/tmp/grid_stage4`): **9 / 9 LANDED, all on the FIRST plan (0 replans)** -
  the start under the city included; ring A1-03 PASS; plane_demo dock DOCKED. Python contracts 189 pass / the same 17
  baseline reds.

## 17. Stages 12, 11, 10 — 2026-11-28

- Stage 12 (449f9a35d): the voxel fallback is out of the normal ladder (plan -> lanes-only recovery -> told refusal); the
  fallback-course executor removed (one route executor, B3); a ship still in contact after three lifts is refused (no
  route through a solid). VoxelRoutePlanner + its unit test kept for a proven confined case.
- Stage 11 (frames / radius): the optimizer's minimum radius is a geometric non-degeneracy floor (two hull lengths, >= 20
  m) - not the HUD frames' spacing (955 m); the frames adapt (one per 30 deg of turn, the requested spacing on straights,
  never closer than a hull length). Grid 9 / 9 on the first plan; route times equal or shorter (point 2: 55.3 -> 46.0 s,
  point 9: 78.1 -> 69.3 s, the rest within 1 %); dock DOCKED.
- Ring sync recovery: at the ring the follower's traffic limiter (axial dock traffic crossing the gate) plus the evasion
  displaced the ship 27-36 m off the axis at the gate route's end - the sync could never start (the progress watchdog
  refused after 90 s, correctly). Now: at the route's end, at rest, off the axis for 8 s -> the gate leg is re-planned from
  where the ship is (<= 3 times). Live: ring landing PASS twice, one gate re-plan each. Residual: the sync gate sits on the
  axial dock's traffic axis - a space-access question for stage 6 (a gate reservation).
- Stage 10 (one control owner): ONE control gate `submitShipControl(writer, control)` for all 10 client write sites (no
  direct `submitInput` left); two different writers in one frame are counted and logged (`[ControlOwner] conflict`); an
  explicit release (releaseClientAutopilot) is a handover, not a conflict. The capsule rescue joins the flight-task
  arbitration with the emergency rule (while it owns the control, an automatic task is ended). Live: ring x2, three grid
  starts, the dock - 0 conflicts (the one seen at a refusal frame was the release handover, now classified). Barrier
  `check_control_owner.py`; five contracts updated to the gate token (the same guarantee: the autopilot input path).
