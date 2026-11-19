# Hub construction: who is being built, expansion plans, halts

Author's requests (2026-11-19): «пересмотреть все хабы - какие из них полноценно функционируют, а какие только
строятся ... добавить в модули хаба строящиеся модули на разном этапе строительства, пересобрать строящиеся хабы ...
центральные части уже готовы, периферия только начинает собираться, ... транспорты и челноки, временные платформы,
временное жилье для рабочих барачного вида»; «добавить строящиеся элементы в существующие готовые хабы ... план
расширения ... постепенно во времени ... ситуации, в которых строительство прекращается или замораживается ... и
орбитальных хабов, и поселений, и тех, что болтаются в пространстве»; «краны - только на планетах и там, где есть
гравитация ... на орбитальном и пространственном хабе - компактные тягачи, ассистед корабль с парой больших клешней».

## The chain

| step | file | what |
|---|---|---|
| classification + plans | `tools/colonization_lab/hub_construction.py` | `construction` (the hub itself is being built) and `expansion` (projects of an operating hub) on every site row and relay (beacon) of `sites_3126.json`. Called by `world_sites.py runtime_export()` (regeneration keeps it); `python hub_construction.py` re-annotates the runtime file alone. |
| city plans | `tools/models/hub_expansion.py` | for every hub with a frozen city layout: `src/assets/data/navigation/hub_expansion/<hub>.json` - which layout nodes are built with which construction kit and readiness window, the expansions' new decks / buildings / props, temporary decks and build-front points. |
| models | `tools/models/units/hub_construction.py` | `hubc_*` stage kits, props, construction craft (see `docs/HUB_CONSTRUCTION_MODELS.md`). |
| runtime: city | `src/model_asset/HubExpansion.h/.cpp`, `HubCityAssembly.cpp` | the plan applied to the layout at the current game day: stage modules swapped in, expansion nodes appended after the layout's nodes (the server's per-node damage indices stay). |
| runtime: re-assembly | `src/render/geometry/ModelAssetGpuLibrary.cpp` | when the day's stage key changes: the city re-assembled off-thread, swapped in (one city per 50 ms), the old package freed 10 s later. |
| runtime: server | `GameSceneSetup.cpp refreshHubCityConstructionCollision`, `GameServer.cpp` | the server sets the construction clock from its UniverseClock every step and re-assembles the hit volumes of a city whose stages changed (one city per scan). |
| runtime: traffic | `src/scene/HubConstructionTraffic.h/.cpp` (+ a small block in `SceneRenderer.cpp`) | haulers, claw ships with elements, tugs, worker pods, crane jibs (gravity only). |
| runtime: surface | `SurfaceSettlementLayout.h` (`appendSurfaceConstruction`), `SettlementMesh.h`, `WorldSitesCatalog.h` (`WorldSite::projects`) | surface settlements: building frames by stage, cranes (gravity), barracks, temporary decks. |
| clock | `src/world/time/ConstructionClock.h` | the universe time the stages follow (server step / client frame; the real clock + 1100 y before the first set). |

## Time

Days count from 3126-01-01 (universe second 853005600; `UniverseClock`: 1 game day = 1 real day, so the game runs
in late 3126 / 3127 now). A project: `start_day`, `work_days`, `halts: [[from, to | null, cause]]`.
`progress(day) = (day - start_day - halted days) / work_days`, clamped to 0..1; status planned / building / halted /
abandoned (a halt with `to = null` that has begun) / done. The same function in Python (`progress_at`) and C++
(`projectProgress`, `siteProjectProgress`). The city at a date is a pure function of the plan and the day, evaluated
once per day (`hubExpansionStageKey` is cached per hub per day).

Stage of a node: u = (progress - a) / (b - a); u < 0.25 -> s1 (foundation / anchor pad), < 0.5 -> s2 (bare frame),
< 0.75 -> s3 (partly clad), < 1 -> s4 (nearly done, scaffold), else the finished module (3-stage kits: thirds). The
hub's own build: a = 0.62 r^1.3 (r = the node's normalised distance from the city centre, docks earlier),
b = a + 0.22..0.32 - the centre is finished first, the periphery only starts. The city's infrastructure (girder
lattice, streets, tubes, monorail, canopy) is the frame of the city and is kept as is.

## Who is being built (classification rules, `is_new`)

1. **A colony founded in the last years** (canon `world.json` colonies: HD 114783 «Hellas-3» 3122, 83 Leonis «Zion»
   3126): every site of the colony is under construction; the capital starts at the founding, the others 0.2-2 years
   later; cities take 1.5x the type's build time.
2. **A recorded founding year** within the type's build time before the reference date (REF 3127.0).
3. **Growth-driven new hubs**: the share of a type that is new at any time = steady state (build time / service
   life) x (1 + 25 x the polity's growth per year): mines 12 % (a mine lives ~30 y and takes 4-7 y to build),
   industrial 5 %, science 6 %, trade 3 %, beacons 6 % (35 % in the newest frontier systems 24 / 37 / 38 / 41 / 45),
   outposts at least 8 %. Excluded: the Sol hubs bound to authored runtime hubs, cities and ports with > 30 000 people
   or a `city0x` / `port0x` id (their populations say they are old), graveyards, abandoned hubs, pirate enclaves,
   hidden and illegal sites. The progress at REF is uniform over the build.

Build durations (years; real anchors: a mine 3-8 y from the decision, a 300 m tower 4-6 y, a container terminal
3-5 y, a new town 10-20 y; space x1.5): mine 4-7, industrial 5-9, orbital / moon hub 7-12, trade 5-9, science 3-5,
military 4-8, quarantine 2-4, belt 4-8, free port 3-6, refuel point 2-4, surface port 8-15, beacon 1-2, outposts 1-3.

## Expansion of operating hubs (`expansion_plan`)

A Poisson stream of projects over 3100-3150 per hub: rate = 0.02 + 2.5 x growth + 0.01 x (income index - 2.5) per
year, x1.4 for a level-A hub, x(1 + 0.25 log10(population / 2000)), x1.3 when the docks are > 55 % utilised, x0.4
for outposts and beacons. Kinds by the hub's family (residential: tower / slab / hab block; industrial: hall / yard /
dock; mining: new shaft / hall / yard; port: pier / yard / hall; small: hab / mast) with durations 1-6 y and a
budget (Mcr) per kind. In a city a new project gets a deck plate (`hubb_parking`) on the city's rim joined by girder
spokes, the building on it in stages (finished: the city's own most common module of that kit), a temporary deck,
a jig, a stack of materials and barracks for the big ones; a finished expansion stays (the city grows).

## Halts and freezes (`halt_hazards`, `HALT`)

Per working year, by cause (anchor: about one in three large projects is stopped at least once - Flyvbjerg's
megaproject samples; stalled projects ~10 % by value in India's CMIE census):

| cause | hazard / working year | duration | P(abandoned for good) |
|---|---|---|---|
| funding (bank / treasury freeze) | 0.015 + 0.03 x (3 - income index) / 3 (+0.02 when the economy shrinks) | 6-36 months | 25 % |
| supply (steel and building materials) | 0.005 + 0.04 x metals import share + 0.04 x construction import share | 2-9 months | 5 % |
| blockade / piracy on the routes | 0.06 x (system piracy index - 0.3) | 12-48 months | 15 % |
| accident on site | 0.012 | 1-6 months | 5 % |
| political (regime change / court) | by regime: clan contracts 0.05, litigating city-states 0.06, exile republic 0.04, ... else 0.004 | 12-36 months | 30 % |

A halted site looks abandoned: no traffic (a tug parked on each temporary deck), work lights off on the surface,
cranes left with the hook up. An abandoned one keeps its skeleton for good (the readiness stays where it stopped).

## Space vs gravity

Orbital and free-space cities (`gravity: false`, every city layout now): no cranes. Temporary truss decks for the
cargo haulers, truss jigs / strongbacks, claw ships carrying beams and panels from the decks to the build front on an
arc, slow final approach, hold, release, back off; two tugs escort each claw ship and push the element's ends while
it is held; tethered worker pods drift at the front. Surface settlements on bodies of >= 600 km radius (g >= ~0.3
m/s2): tower cranes beside every frame; smaller bodies: no cranes.

## Counts (REF 3127.0)

expansion status at REF: {'done': 779, 'planned': 789, 'building': 114, 'halted': 10, 'abandoned': 32}
active kinds: {'hab': 24, 'hall': 20, 'yard': 11, 'dock': 15, 'tower': 20, 'slab': 21, 'mast': 5, 'minehead': 8}
hubs with active/halted expansion: 105
halt causes over all projects: {'funding': 88, 'political (abandoned)': 17, 'supply': 63, 'accident': 60, 'political': 33, 'funding (abandoned)': 40, 'blockade': 45, 'blockade (abandoned)': 2, 'supply (abandoned)': 2, 'accident (abandoned)': 2}
hubs with plans: 519

Hubs under construction (32 sites + 16 beacons):

| site | type | placement | status | stage | progress | started | eta | halts | evidence |
|---|---|---|---|---|---|---|---|---|---|
| `plant.031.kern.deep_dock` | mining_hub | belt | building | foundation | 0.005 | 3126.97 | 3133.67 | supply 3132.31-3132.55 | новая шахта: доля новых для типа 16% (рост полиса 1.2 %/г) |
| `plant.000.sol_reactor.canaveral` | industrial_hub | surface | building | cladding | 0.646 | 3123.59 | 3128.86 | — | новый хаб: доля новых для типа 5% (рост полиса -0.0 %/г) |
| `site.031.cyg61_kern.mining_hub02` | mining_hub | orbit | building | frame | 0.356 | 3125.45 | 3130.0 | supply 3129.1-3129.3 | новая шахта: доля новых для типа 16% (рост полиса 1.2 %/г) |
| `site.022.delta_pav_freekarma.quarantine01` (город) | quarantine_hub | orbit | building | cladding | 0.536 | 3125.52 | 3128.28 | — | новый хаб: доля новых для типа 5% (рост полиса 0.6 %/г) |
| `site.022.delta_pav_karma.mining_hub02` | mining_hub | orbit | building | cladding | 0.505 | 3124.83 | 3129.12 | — | новая шахта: доля новых для типа 15% (рост полиса 1.1 %/г) |
| `site.009.eps_eri_harmony.mining_hub01` | mining_hub | belt | building | cladding | 0.49 | 3121.88 | 3130.17 | blockade 3124.05-3126.12 | новая шахта: доля новых для типа 12% (рост полиса 0.1 %/г) |
| `site.009.eps_eri_strife.industrial_hub01` (город) | industrial_hub | orbit | building | fitting | 0.901 | 3119.6 | 3127.82 | — | новый хаб: доля новых для типа 6% (рост полиса 0.8 %/г) |
| `site.010.gj581_freeclans.mining_hub03` | mining_hub | belt | building | fitting | 0.922 | 3122.94 | 3127.34 | — | новая шахта: доля новых для типа 16% (рост полиса 1.5 %/г) |
| `site.037.hd114783_hellas3.city01` | moon_hub | surface | building | frame | 0.282 | 3122.0 | 3140.62 | accident 3124.58-3125.0, funding 3135.98-3136.62, political 3138.08-3139.37 | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.city02` | moon_hub | surface | building | frame | 0.18 | 3123.8 | 3142.66 | political 3136.27-3137.34 | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.city03` | moon_hub | surface | abandoned | foundation | 0.13 | 3123.52 | — | funding 3124.09-3124.71, political 3126.22-навсегда | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.trade_hub01` (город) | trade_hub | orbit | building | frame | 0.287 | 3122.59 | 3133.73 | political 3123.47-3125.69, funding 3129.25-3130.54 | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.mining_hub01` | mining_hub | belt | building | cladding | 0.582 | 3123.76 | 3129.33 | — | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.industrial_hub01` (город) | industrial_hub | orbit | halted | cladding | 0.65 | 3122.44 | 3131.43 | political 3126.39-3129.31 | колония основана в 3122: город и хабы ещё строятся |
| `site.037.hd114783_hellas3.science_hub01` (город) | science_hub | orbit | building | frame | 0.399 | 3123.18 | 3129.03 | political 3124.09-3126.57 | колония основана в 3122: город и хабы ещё строятся |
| `site.014.hd20794_hellas2.trade_hub01` (город) | trade_hub | orbit | building | fitting | 0.926 | 3120.46 | 3127.52 | — | новый хаб: доля новых для типа 3% (рост полиса 0.2 %/г) |
| `site.014.hd20794_hellas2.industrial_hub04` (город) | industrial_hub | orbit | building | cladding | 0.593 | 3123.71 | 3132.24 | blockade 3128.98-3131.97 | новый хаб: доля новых для типа 5% (рост полиса 0.2 %/г) |
| `site.014.hd20794_newindia.mining_hub02` | mining_hub | orbit | building | frame | 0.428 | 3124.15 | 3130.8 | — | новая шахта: доля новых для типа 15% (рост полиса 0.9 %/г) |
| `site.015.hd40307_maland.science_hub01` (город) | science_hub | orbit | building | frame | 0.291 | 3125.87 | 3129.76 | — | новый хаб: доля новых для типа 8% (рост полиса 1.0 %/г) |
| `site.004.lalande_mecca.mining_hub06` | mining_hub | orbit | building | cladding | 0.645 | 3124.09 | 3128.44 | supply 3126.16-3126.45 | новая шахта: доля новых для типа 14% (рост полиса 0.7 %/г) |
| `site.045.leo83_zion.city01` | planetary_surface_port | surface | building | foundation | 0.065 | 3126.0 | 3141.32 | — | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.city02` | planetary_surface_port | surface | building | foundation | 0.001 | 3126.99 | — | funding 3132.78-навсегда | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.city03` | moon_hub | surface | building | foundation | 0.036 | 3126.45 | 3141.81 | — | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.trade_hub01` (город) | trade_hub | orbit | planned | survey | 0.0 | 3127.33 | 3135.4 | — | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.mining_hub01` | mining_hub | belt | planned | survey | 0.0 | 3127.5 | 3132.17 | — | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.industrial_hub01` (город) | industrial_hub | orbit | building | foundation | 0.019 | 3126.85 | 3134.75 | — | колония основана в 3126: город и хабы ещё строятся |
| `site.045.leo83_zion.science_hub01` (город) | science_hub | orbit | planned | survey | 0.0 | 3127.83 | — | funding 3130.48-навсегда | колония основана в 3126: город и хабы ещё строятся |
| `site.060.proxima_outpost.industrial_hub01` (город) | industrial_hub | orbit | building | frame | 0.175 | 3125.46 | 3136.61 | funding 3129.61-3131.95 | новый хаб: доля новых для типа 6% (рост полиса 0.5 %/г) |
| `site.008.tau_ceti_cognium.industrial_hub05` (город) | industrial_hub | orbit | halted | foundation | 0.134 | 3125.73 | 3136.53 | blockade 3126.61-3130.05, funding 3135.08-3135.92 | новый хаб: доля новых для типа 5% (рост полиса 0.1 %/г) |
| `site.003.wolf359_citadel.mining_hub04` | mining_hub | orbit | building | foundation | 0.125 | 3126.46 | 3130.79 | — | новая шахта: доля новых для типа 15% (рост полиса 0.9 %/г) |
| `site.037.hd114783_hellas3.free_port01` | free_port | belt | building | fitting | 0.85 | 3123.54 | 3127.61 | — | колония основана в 3122: город и хабы ещё строятся |
| `lab.041.crossing` | science_hub | orbit | building | foundation | 0.138 | 3126.67 | 3129.08 | — | новый аванпост: доля новых для типа 8% (рост полиса 0.4 %/г) |
| `relay.sol.mars.2` | beacon | orbit | building | cladding | 0.481 | 3126.16 | 3127.91 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.4 %/г) |
| `relay.sol.belt.3` | beacon | heliocentric | building | frame | 0.283 | 3126.6 | 3128.02 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.4 %/г) |
| `relay.sol.mercury.1` | beacon | orbit | building | cladding | 0.597 | 3126.15 | 3127.58 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.4 %/г) |
| `relay.sol.jupiter.1` | beacon | orbit | building | frame | 0.315 | 3126.5 | 3128.09 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.4 %/г) |
| `BCN-024-BR` | beacon | lagrange_point | building | cladding | 0.54 | 3126.22 | 3127.67 | — | новый маяк / ретранслятор: доля новых для типа 35% (рост полиса 1.2 %/г) |
| `BCN-031-BR` | beacon | lagrange_point | building | cladding | 0.501 | 3126.47 | 3127.53 | — | новый маяк / ретранслятор: доля новых для типа 8% (рост полиса 1.2 %/г) |
| `BCN-041-BR` | beacon | lagrange_point | building | foundation | 0.003 | 3126.99 | 3128.7 | — | новый маяк / ретранслятор: доля новых для типа 35% (рост полиса 0.1 %/г) |
| `BCN-031-R-cyg61_kern-000a` | beacon | lagrange_point | building | frame | 0.325 | 3126.61 | 3127.8 | — | новый маяк / ретранслятор: доля новых для типа 8% (рост полиса 1.2 %/г) |
| `BCN-009-R-eps_eri_strife-003a` | beacon | lagrange_point | building | fitting | 0.895 | 3125.55 | 3127.17 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.8 %/г) |
| `BCN-008-R-tau_ceti_cognium-000a` | beacon | lagrange_point | building | frame | 0.432 | 3126.54 | 3127.61 | — | новый маяк / ретранслятор: доля новых для типа 6% (рост полиса 0.1 %/г) |
| `BCN-008-R-tau_ceti_cognium-009b` | beacon | lagrange_point | building | cladding | 0.57 | 3126.28 | 3127.54 | — | новый маяк / ретранслятор: доля новых для типа 6% (рост полиса 0.1 %/г) |
| `BCN-009-M-eps_eri_strife-08` | beacon | orbit | building | cladding | 0.705 | 3125.78 | 3127.51 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.8 %/г) |
| `BCN-009-M-eps_eri_strife-11` | beacon | orbit | building | frame | 0.21 | 3126.74 | 3127.97 | — | новый маяк / ретранслятор: доля новых для типа 7% (рост полиса 0.8 %/г) |
| `BCN-014-M-hd20794_hellas2-01` | beacon | orbit | building | fitting | 0.823 | 3126.05 | 3127.2 | — | новый маяк / ретранслятор: доля новых для типа 6% (рост полиса 0.2 %/г) |
| `relay.ism.000_012.1` | beacon | deep_space | building | cladding | 0.457 | 3126.16 | 3127.99 | — | новый маяк / ретранслятор: доля новых для типа 6% (рост полиса -0.0 %/г) |
| `relay.ism.037_045.1` | beacon | deep_space | halted | cladding | 0.499 | 3125.94 | 3128.15 | supply 3126.88-3127.21 | новый маяк / ретранслятор: доля новых для типа 8% (рост полиса 1.2 %/г) |

## Not done yet

- Free-space sites without a city layout (beacons, outposts, plain command stations, mines): the classification and
  plans are in the data; their models (mast / minehead stage kits exist) are not placed in the game yet.
- Surface settlement construction is a static mesh built when the body's settlements are built (the stage of that
  day); a stage change shows on the next rebuild of the body's settlements; no surface traffic.
- The economy chain (world_sites.py -> economy_live.py -> fuel_stations.py) was not regenerated: no site ids or hubs
  were added (only fields), so its outputs stay valid.
