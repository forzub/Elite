# Elite — Space Games 2026 Research

Date: 2026-09-27
Source video: Лучшие космические игры 2026 (по-другому) — SeraX Космоигры
https://www.youtube.com/watch?v=KnAODgPTgXE

## Scope

The video is a broad survey of space games relevant in 2026, not a list of titles all released in 2026. For Elite research the most useful projects are Starship Simulator, Stars Reach, SpaceCraft, Starminer, Stellar Trader, In The Black, Star Wrath, Space Engineers 2, X4 and EVE Frontier.

## Main conclusion

Compared with the 2025 survey, the strongest change is the appearance of integrated systemic layers rather than isolated features:

- automated interplanetary logistics;
- modular ship and station construction;
- procedural worlds with erosion, biomes and ecology;
- physically motivated star-system formation;
- persistent infrastructure and production networks;
- real transport routes and cargo traffic.

The opportunity for Elite is no longer merely procedural planets or modular ships. The stronger target is causal integration:

Galaxy and body generation -> economy -> transport demand -> real ships -> traffic infrastructure -> Planner/Follower -> docking and queues -> visible living system.

## Starship Simulator / Genesis

Starship Simulator is the single most relevant 2026 reference for our StellarSystemGenerator research.

Public 2026 developer notes describe a unified C++ Genesis system combining galaxy and system generation. It includes more than 21 million catalogued real stars plus procedural stellar populations.

The public formation chain now includes:

- galactic metallicity regions;
- circumstellar disk formation and composition;
- disk evaporation;
- planetesimal formation;
- pebble accretion;
- orbit clearing;
- planetary collisions;
- moon formation;
- gas accretion;
- planetary migration;
- Kuiper belts;
- multi-star systems.

This independently validates the causal architecture we designed:

GalacticContext -> star -> disk -> formation history -> final planetary architecture -> CelestialBodyDescriptor.

Important lesson: do not generate a final planet list directly. Generate latent formation state and derive final bodies from it. Runtime can still use calibrated approximations rather than multi-Gyr numerical simulation.

Reference:
https://store.steampowered.com/app/1332100/Starship_Simulator/

## Stars Reach

Stars Reach is the strongest new surface-generation reference in the 2026 list.

Its 2026 world-generation material explicitly discusses topography, very large mountain ranges, erosion, biome distribution, wildlife habitats, vegetation placement and multiple terrain topology configurations.

Useful Elite chain:

macro topology -> erosion/drainage character -> biome field -> habitat suitability -> vegetation/wildlife -> settlement suitability.

It is also useful visually: Stars Reach proves that a large systemic procedural world can use a stylized, softer art direction instead of high-frequency photorealistic PBR.

Reference:
https://starsreach.com/world-gen-system-behind-the-devs-with-sean-grady/

## Space Engineers 2

Space Engineers 2 reinforces the hybrid procedural approach:

base planetary terrain + biome masks + procedural vegetation + voxel formations + authored cave archetypes + procedural distribution.

Their caves are built as a small family of good Houdini shapes, voxelized, then distributed/subtracted from planetary terrain.

Elite lesson: a procedural generator does not need to invent every geological feature mathematically. A small library of good caves, canyons, lava tubes, crater families and rock formations can be transformed and distributed procedurally.

Reference:
https://2.spaceengineersgame.com/space-engineers-2-vs2-planets-survival-foundations-live-now/

## SpaceCraft

SpaceCraft is important because transport infrastructure is now explicit.

It includes ship construction from parts, planetary bases, automated production, drones, cargo ships, delivery lines and interplanetary logistics.

Elite lesson: civilian traffic should be caused by the economy, not spawned as decoration.

ProductionDemand -> ShipmentOrder -> TransportAssignment -> Route -> RealShipMovement -> StationArrival -> Dock -> Unload.

Reference:
https://store.steampowered.com/app/3276050/SpaceCraft/

## Stellar Trader

Stellar Trader connects planetary economies, shortages, strategic logistics, station/colony development and production lines.

Useful Elite model:

EconomicNode -> inventory -> production recipe -> demand -> shipment need -> assigned transport capacity -> route -> station traffic.

Reference:
https://store.steampowered.com/app/3867570/

## Starminer

Starminer combines modular stations and vessels with mining networks, production automation, trade routes, docked transport ships and physically simulated large vessels.

This is important because logistics and physical spacecraft live in the same simulation layer.

A freighter should not be a token moving between inventory nodes. It can be a real entity with mass, cargo, propulsion, route, arrival/departure, docking, damage, fuel and operating state.

References:
https://starminer.net/
https://store.steampowered.com/app/1116050/Starminer/

## EVE Frontier

The useful idea is programmable infrastructure through Smart Assemblies. The blockchain implementation itself is not important for Elite.

Station infrastructure could expose configurable behavior for docking permissions, traffic priority, cargo routing, defense, queue handling, maintenance and production.

Reference:
https://nova.evefrontier.com/en/faq

## Combat: In The Black

In The Black is the strongest 2026 combat reference in the list.

It explicitly focuses on science-grounded combat with Newtonian movement, plausible propulsion, nuclear-powered spacecraft, kinetic/thermal/radiation damage concepts, component damage and real Solar System battlespaces.

The useful lesson is not simply to make physics strict. Physics must create tactical choices.

Reference:
https://intheblack.gg/

## Combat: Star Wrath

Star Wrath is also relevant because ship mass, engine type and modular configuration affect inertia and maneuverability, while damage can remove individual parts.

Useful integration target:

combat geometry + sensors + propulsion + module topology + damage state.

Reference:
https://starwrath.com/en/

## Navigation and docking

The 2026 list does not replace the stronger navigation references already researched separately.

The most useful architecture remains:

- Pioneer for Newtonian FlyTo, FlyAround and docking control;
- X4 for station-owned semantic docking geometry;
- Naev for infrastructure graph routing;
- NASA/ESA RPOD for phase-based docking and Hold/Retreat/Escape semantics.

The new significance is that SpaceCraft, Starminer and Stellar Trader make our navigation layer a world-simulation primitive. If cargo ships physically serve production networks, Planner/Follower, DockingPortGuidance and station traffic control are shared infrastructure, not merely player autopilot.

## Modular ships and assets

SpaceCraft and Starminer reinforce function-driven modular construction.

Elite procedural asset generation should work as:

role -> functional requirements -> compatible modules -> physical validation -> visual variants.

A module should carry visual, collision, mass, center-of-mass, power, thermal, thruster, hardpoint, cargo, service, socket and LOD/material information.

## Graphics

The broad 2026 market still uses a great deal of expensive realistic PBR rendering, but Stars Reach and other stylized titles show that procedural/systemic scale does not require photorealistic microdetail.

Elite should continue toward animation-oriented NPR:

- large readable masses;
- softened geometry;
- controlled material response;
- limited microtexture;
- restrained contrast;
- strong silhouettes;
- atmospheric depth.

This is cheaper and more distinctive than competing on asset count.

## Hardware observations

Current official minimum/recommended examples:

Starminer: GTX 1050 / RX 560 minimum, 8 GB RAM.
SpaceCraft: GTX 1060 minimum; RTX 3060 recommended.
Stars Reach: GTX 1070 minimum; RTX 2060-class recommended; 16/32 GB RAM.
Starship Simulator: GTX 1070 minimum; RTX 3080 Ti recommended; DX12 feature level 12_1 / SM 6.6.
Space Engineers 2: GTX 1660 Super minimum; RTX 2080 Ti / 3080-class recommended.
Jump Space: GTX 1070-class minimum but explicitly 8 GB VRAM; RTX 3060 Ti-class recommended.

The broad lesson is unchanged: sophisticated procedural generation is not necessarily expensive. Photorealistic rendering, high asset density and VRAM pressure dominate the upper hardware requirement.

## Priority references for Elite

Tier A:
- Starship Simulator / Genesis: physical galaxy and stellar-system formation.
- Stars Reach: procedural topology, erosion, biomes and stylized rendering.
- SpaceCraft: automated production and interplanetary logistics.
- Starminer: modular physical fleets plus logistics and trade routes.
- In The Black: realistic physics used to produce tactical combat.

Tier B:
- Space Engineers 2: voxel/procedural hybrid terrain.
- Stellar Trader: economy/logistics and station development.
- EVE Frontier: programmable infrastructure concepts.
- X4: economy, traffic and docking semantics.
- Falling Frontier: strategic logistics/fleet movement.

## What changed from the 2025 conclusions

2025: transport infrastructure looked largely absent.
2026: SpaceCraft, Starminer and Stellar Trader explicitly model production/logistics/cargo transport.

Our opportunity therefore changes from inventing logistics to integrating logistics with real autonomous ships, docking queues and traffic networks.

2025: procedural generation existed but was fragmented.
2026: Starship Simulator Genesis is approaching an end-to-end causal system generator.

2025: realistic interesting combat remained weak.
2026: In The Black and Star Wrath are now serious references worth studying.

## Strongest remaining opportunity

The distinctive Elite chain is:

physical/procedural world -> economic causality -> transport demand -> real ships -> traffic infrastructure -> Planner/Follower -> docking/queues/port operations -> visible living system.

A refinery needs ore. A shipment is created. A real freighter receives the job. It obtains a route, flies through system infrastructure, joins a station arrival route, waits if needed, receives a docking corridor, docks and unloads.

That complete causal chain remains a strong target.

## Next research actions

1. Deep-dive Starship Simulator Genesis and compare it directly with STELLAR_SYSTEM_AND_CELESTIAL_BODY_GENERATOR.md.
2. Deep-dive Stars Reach world-generation material for concrete terrain/topology/erosion parameters.
3. Inspect SpaceCraft production and delivery-line representation.
4. Inspect Starminer trade routes and docked-hangarship behavior.
5. Inspect In The Black flight/damage model for mechanics where realistic physics improves gameplay.
6. Feed results into existing Elite research docs.

## Bottom line

Elite should not compete on asset count or raw photorealism. It should compete on causal integration.

Starship Simulator validates astrophysical generation.
Stars Reach validates systemic procedural terrain and stylized presentation.
SpaceCraft validates automated interplanetary logistics.
Starminer validates physical modular fleets connected to logistics.
In The Black validates physics as a source of tactical gameplay.

The remaining opportunity is to connect those layers into one coherent simulated world.