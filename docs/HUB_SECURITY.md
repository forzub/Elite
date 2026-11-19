# Hub security, traffic control, collision avoidance

Author's requests (2026-11-19):
«оборонительные установки - военный охранный периметр + уничтожение непрошенных астероидов в мирное время ... как-то
все увязать все компоненты хаба в работающую реагирующую на все систему. если мы летим по красному корридору - влоб
против движения кораблей - корабли должны уходить от столкновения. ну и за красные корридоры включатся какие-то
санкции, в основном - денежные»; «любой мирный корабль при попытке столкнуться с ним должен выполнять маневр уклонения
в меру своих возможностей»; «любой корабль попавший в близкую зону видимости игрока становится настоящим».

Principles: the server judges (fines, locks, promotion, evasion, intercepts); the clients present. Plausible physics:
every reaction has a cause and a limit (a ship cannot sidestep instantly; a laser needs dwell time; a slug needs flight
time). Peacetime never kills: the escalation stops at a fire-control lock.

## 1. One component per hub

`GameSimulation::updateHubSecurity` (`src/game/simulation/GameSimulationHubSecurity.cpp`), pure rules in
`src/game/traffic/HubSecurity.{h,cpp}`. Per hub city (a `HubBeltCity` "<hub>.city" object with a lane network /
plan) a state `HubSecurityState`: **calm / alert / violation**, violators (ship -> level), counters (fines, strays
destroyed, evasions, promotions). Everything reports into it: the lanes (wrong way), the zone fines, the promoted air
traffic, the dock traffic's evasions (the dispatcher's ships), the defence ring. Logs `[HubSecurity]`:
state changes (`calm -> alert (1 stray inbound, 0 violator(s))`), radar tracks, kills, locks, a summary each minute
with activity, `perf: x ms/tick`.

Cost: the cities are cached (rebuilt when the static object count / active system changes); the lanes / promotion /
strays run at 4 Hz and only for cities with a player within perimeter + 25 km; the promoted ships and the evasion layer
run per tick (a handful of ships).

## 2. Flying against the traffic (the "red corridor")

A lane (`assets/data/navigation/hub_lanes/<hub>.json`) is one-way; the HUD draws it red when it runs against the
player's travel. The server (`probeLane` + `stepWrongWay`):

| rule | value |
|---|---|
| inside a lane | within its radius + 10 m of the centre line |
| against it | speed vs the city >= 15 m/s and cos(velocity, lane) < -0.5 (120..180 deg) |
| confirmed | 1 s continuously (a crossing is not an incident) |
| closes | 2 s clear: out of the lane, or cos > -0.2 (hysteresis), or out of the hub zone |
| warning | at confirmation: the controller's call (`motion.securityEvent` 1) |
| first fine | 5 s after the warning: **1 500 Cr** |
| then | +750 Cr every 10 s, +300 Cr per km flown against the lane |
| repeat | x1.5 per earlier incident in the last 30 min (max x4) |
| cap | 15 000 Cr per incident |
| lock | 30 s against the traffic, or the 3rd incident in 30 min: **fire-control lock** (level 3): the turrets track the ship, a patrol is "called" (notice); no fire in peacetime |
| level decay | 60 s after the incident closes -> level 0 |

Example: 30 s at 150 m/s against a lane = 1 500 + 2 x 750 + 4 x 300 = **4 200 Cr** (unit-checked).

Replicated (`DynamicMotionState`, wire schema 49): `securityLevel`, `securitySerial`, `securityEvent` (1 warning,
2 fine, 3 lock, 4 closed), `securityEventFineCr`, `securityEventDebtCr`, `securityHubId`, `securityLaneId`,
`securityFinesTotalCr`. The client (`SpaceState::updateZoneViolations`) shows the banner, keys
`cockpit.security.warning / fine / debt / lock / clear` (flight.json, 5 languages).

Fines are banked: `game::economy::service::chargeFine` -> the hub polity's national bank; an unpayable fine becomes a
0 % fine debt, never refused, never a seizure (docs/ECONOMY_BANKING.md §9a). The navigation-zone fines
(`GameSimulationZones.cpp`, red zones / skipped gates) now go the same way.

Logs: `[Corridor] ship N WRONG WAY in lane L of H: v m/s, cos c ... -> controller warning`,
`[Corridor] ship N fined X cr (banked: paid P, debt D) - lane L of H, s / m against the traffic, incident, total`,
`[Corridor] ship N clear of lane L ...`, `[Corridor] client notice: ...`.

## 3. Near the player everything is real (promotion of the air traffic)

The hub's air traffic is a pure function of the universe time (`src/game/navigation/traffic/HubAirTraffic.{h,cpp}`,
shared by the client presentation `src/scene/HubCityTraffic` and the server): slot = one ship of one lane, 70 % of the
lane's speed, one per 3.5 km of lane, the hull a weighted pick of the hub bloc's fleet.

- **Promotion**: within **5 km** of any player the server spawns the slot's catalog ship (its passport from the plan's
  `air_fleet`) as a kinematic traffic shell exactly at the slot's pose and velocity, and lists the slot in the city
  object's `promotedAirSlots` (replicated) - the clients stop drawing the presentation copy. Why 5 km: a 50 m hull is
  ~0.6 deg there (~10 px at 1080p: plainly a ship), and a head-on closing speed of ~255 m/s (105 m/s lane traffic +
  150 m/s hub limit) leaves ~20 s to the CPA - what a 56 m freighter's RCS needs to clear its own beam (2 x 28 m + 20 m
  + 40 m margin).
- It flies the rest of its lane (`along0 + v t` = the presentation's own pose, unit-checked to 0.00 m) plus its
  evasion layer; past the lane's end it flies straight on into open sky.
- **Hand-back** (despawn, the presentation resumes): far from every player (> **6.5 km**, hysteresis) *and* back on its
  track (evasion settled) - the presentation continues at exactly the same pose; or at its lane's end with nobody
  within 3 km (deviated ships only hand back there).
- Cap **16** promoted per hub. Hulls without a catalog passport (the city shuttle `hubt_shuttle`) stay presentation.
- Logs `[TrafficPromote] <hub> slot S (hull, passport) -> real ship N at lane L +m, d m from a player`,
  `... handed back to the presentation: <why>, nearest player d m`.

## 4. Collision avoidance (any peaceful ship, within its own limits)

`planEvasion` / `stepEvasionOffset` (pure), applied by `GameSimulation::applyTrafficEvasion` to every scripted
(kinematic) peaceful ship: the promoted air traffic and the dock service traffic (`GameSimulationDockTraffic.cpp`;
not on / near the pads - the dispatcher's area). Threats: the real free-flying ships of the active system (players,
AI ships under physics), refreshed once per tick.

- CPA from relative position / velocity: `t = -r.v / |v|^2`, `d = |r + v t|`; a threat = `0 < t < 30 s` and
  `d < own half length + 20 m + 40 m margin`, closing faster than 15 m/s (slower is the dispatcher's business).
- Direction: across the relative velocity, away from where the threat will be at the CPA; a dead head-on (< 2 m)
  goes to **starboard** (COLREGS rule 14 - a threat that follows the rule goes the other way).
- Reach by the CPA: RCS at once `0.5 a_lat t^2`, or the main drive after turning to the burn direction
  `0.5 a_main (t - angle/turn rate)^2` (RCS meanwhile) - the better one is used.
- Capabilities by hull length L (estimates from a 30 m hull, RCS force ~ area, mass ~ volume): lateral
  `1.2 x 30/L` m/s2 (0.25..2.5), main `12 x sqrt(30/L)` m/s2 (3..15), turn `0.35 x 30/L` rad/s (0.04..0.6). A 56 m
  freighter: 0.64 / 8.8 m/s2, 11 deg/s. (Shuttle orbiter RCS ~0.14 m/s2 sideways, Dragon ~0.15: the game's ships are
  exaggerated ~x5, as their drives are.)
- The burn lasts while the predicted miss is short; with no threat for 2 s the ship returns to its track, critically
  damped (omega 0.2 /s) within its lateral acceleration.
- Honest outcome: when even the best burn leaves the hulls touching, the log says so (`CANNOT CLEAR`) and the ship
  still burns - a head-on from 600 m at 255 m/s closing cannot be cleared by a 56 m freighter (unit check: miss 2.6 m).
- Logs `[Evade] ship N: threat M cpa t=.. s d=.. m (needs ..) -> burn a m/s2 RCS|main drive dir (x,y,z), predicted
  miss .. m[ - CANNOT CLEAR]`, `[Evade] ship N: threat M passed, back on its track`.

Newtonian players: nothing in this layer helps a Newtonian *player*; it is the traffic that gives way. A ship under
physics AI (NpcAi) is not yet steered by this layer (see §7).

## 5. The defence ring

Layout (`buildDefenceLayout`, deterministic from the hub plan + lanes; client and server compute the same):

| | |
|---|---|
| city radius | half the plan's bounds diagonal (earth_orbital_hub: 5.0 km) |
| perimeter (security zone) | the lanes' speed-limit zone, else 1.4 x city radius; >= city + 1.5 km (earth_orbital_hub: 9.6 km) |
| engagement radius | max(city + 0.8 km, 0.75 x perimeter) (earth_orbital_hub: 7.2 km) |
| rim turrets | the outer corner of every other rim block (military: every rim block) on its deck: PD lasers, a coilgun every 3rd |
| missile cells | military hubs: beside every coilgun (closed in peacetime) |
| radar | a 60 m mast on the services (police) block nearest the centre (S-band array, 10 rpm) |
| pickets | 8 (military 12) sentry platforms at 0.92 x perimeter, alternately 350 m over / under the plane, a PD turret each |
| military hub | the hub id says so (mil / fort / naval / garrison / bastion / citadel) or >= 4 services blocks and >= 1/4 of blocks |

earth_orbital_hub: 9 lasers, 4 coilguns, 8 pickets, 1 radar.

Stray bodies (`strayBody`, deterministic per hub + index): one per **60 s** (belt / ring hubs: 35 s), appearing at the
radar horizon (perimeter + 1.5 km), 0.3..4 m (power law), drifting 30..80 m/s; 65 % inbound (aimed into the city's
disc), the rest pass by (tracked only). An inbound body is engaged when it crosses the engagement radius by the nearest
able site: < 1.5 m -> PD laser (or picket), dwell **1.2 s + 0.8 s per metre** (a ~2 MW beam spalling a rock); bigger
-> coilgun, slug at **3 km/s** (flight time = distance / 3000 + 0.3 s). Unit-checked: 56 of 100 inbound, every one
killed inside the engagement radius, before the city.

Server: `[HubSecurity] <hub>: radar: stray #i, s m, v m/s, INBOUND|passing`, `stray #i (s m) destroyed by PD laser #k
at d m from the centre, g m from the gun`; the hub goes `calm -> alert` while an inbound body is tracked.

Client (`src/scene/HubDefencePresentation.{h,cpp}`, drawn by SceneRenderer with the city's traffic): bases + heads,
the heads slew (40 deg/s azimuth, 25 deg/s elevation) to their engagement (from 8 s before the shot), else to a ship
under fire-control lock within 8 km (replicated `securityLevel` 3), else an idle scan; the laser beam (drawn as its
glow, >= ~2 px wide) and the hot spot, the coilgun slug and muzzle flash, the kill flash and 8 fragments flying apart
(3 s). Rocks are drawn at least ~2 px (the eye follows the tracked body). DEV A/B: `ELITE_DEV_HUB_DEFENCE=0`;
`ELITE_DEV_TRAFFIC_LOG=1` logs `[HubDefence] sites, instances (mesh draws), bodies, shots, locked`.

Models: `tools/models/units/hub_defence.py` (`hubd_pdl_base/head`, `hubd_coil_base/head`, `hubd_vls`,
`hubd_radar_mast/dish`, `hubd_picket`, `hubd_rock`, `hubd_fx_beam`, `hubd_fx_flash`), built by
`build_equipment_models.py --only hubd_`, packaged by the mesh editor (shell + build) into
`assets/compiled/hub_modules/hubd_*`. Head pivots over the deck: laser 6.5 m, coilgun 5.0 m, picket 8.0 m, radar 61 m;
weapons along the head's +Z.

## 6. Checks

`EliteHubSecurityCheck` (tests/hub_security/HubSecurityCheck.cpp, run from the repo root): lane probe; no incident
with the traffic / crossing / 10 m/s drift; confirmation 1 s, first fine 5 s after the warning, 30 s = 4 200 Cr;
closing; hysteresis; repeat x1.5^3; the 15 000 Cr cap; fine paid from the account, into the overdraft, booked as a
0 % debt when unpayable, fuel still payable, a year of daily steps; CPA formulas; a head-on from 5 km cleared (miss
88 m) and back on track; from 600 m cannot clear (honest); starboard rule; evade away from an offset threat; promoted
track = presentation pose; defence layout counts; stray determinism and kills before the city.

## 7. Not done / next

- **Live, 2026-11-19 (Debug)**: the promotion works (16 slots of earth_orbital_hub promoted near the player, handed
  back at their lanes' ends). The wrong-way judge and the evasion did NOT trigger live: a free-flying player measured
  against the city object inside `updateHubSecurity` (stage 3 of the tick) is ~0.6-0.7 km off the point it was put at
  (DEV `ELITE_DEV_LANE_START`), so it was never inside a lane, and the same offset shows in the CPA of a head-on lane
  ship (d ~600-700 m). A one-step velocity projection (`m_securityStepDt`) did not remove it and is left at 0. Next:
  find the frame / epoch of a free-flying ship vs the city static object at that stage (DEV logs:
  `ELITE_DEV_SECURITY_LOG=1` -> `[Corridor] DEV ... city (x,y,z) v (...)`, `[Evade] DEV ... cpa`).
- Frame-cost A/B of the defence presentation (`ELITE_DEV_HUB_DEFENCE=0`) not measured yet; the defence log showed
  ~28 instances / 8-10 mesh draws near the rim.

- Ships under physics AI (NpcAi, materialized scheduled visits) are threats but do not yet evade (their navigation
  goals would need an avoidance intent); the scheduled visits (berth / pad traffic) are not promoted.
- The "patrol" of the lock is a notice only - no patrol craft is spawned yet; no war state (missile cells idle).
- The promoted ship keeps its nominal attitude while it sidesteps (the RCS model needs no turn; with the main drive the
  hull would visibly turn - not drawn).
- Stray bodies are not physical bodies (no collision with ships): a deterministic schedule shared by server and
  clients.
