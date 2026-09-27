# Systemic simulation reference code

Research-only source snapshots relevant to Elite's simple causal simulation.

## Star Ruler 2

Upstream: BlindMindStudios/StarRuler2-Source
Pinned commit: beec9bff697ffbebafaeb66d0cba1856a02cb6db

Star Ruler 2 source code is MIT licensed. The upstream COPYING file is mirrored in this directory.

Included source areas:

- ship_generation/random_designs.as — function-driven procedural ship layout;
- modular_damage/blueprint.cpp/.h — directional penetration, local module HP, subsystem degradation;
- economy/system_pathing.as — Dijkstra and trade-route constraints;
- economy/Civilian.as — physical civilian freighter behavior;
- economy/Resources.as — resource destinations/pressure/state;
- economy/RegionObjects.as — trade demand, freighter allocation, traffic counters and trade-station scaling.

These files are references. Elite production code should use its own types and architecture.

## Pioneer

Pioneer remains a GPL-3.0 research reference and is fetched separately rather than mirrored into this MIT snapshot.
Relevant paths include src/galaxy/StarSystemGenerator.cpp, src/galaxy/Economy.cpp and src/terrain/.

Blockchain systems are intentionally outside this research scope.