# Generation-Ship Settlement Simulation

**Status:** design direction / historical-world generation draft  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`

## 1. Purpose

This document formalizes the historical simulation needed to generate the human settlement pattern of the playable region from generation-ship missions.

The project already used generation ships as historical actors with real launch dates, routes, speeds, populations, cryo/awake splits and voyage events. Example prior mission data included MAYFLOWER-2: Sol departure 2088, Tau Ceti arrival 2233, route about 11.91 ly, travel about 143.83 years, 25,000 initial colonists and 18,500 at arrival, with roughly 2,000 awake at a time and multi-year duty rotations.

`Dead Reckoning: The Long Drift` is useful here as a reference because it turns many similar ideas into explicit simulation mechanics. It is not the origin of this project direction.

## 2. Core principle

The historical settlement map should be an output of simulated missions, not a hand-painted distribution.

```text
launch waves
    -> generation-ship missions
    -> route / failure / diversion history
    -> arrival or loss
    -> colony founding state
    -> colony survival and growth
    -> daughter expeditions
    -> later settlement tree
    -> final historical population distribution
```

The same simulation should also leave negative-space history:

- ships still in flight at the game epoch;
- ships lost in transit;
- derelicts and wrecks;
- abandoned or failed colonies;
- colonies that regressed technologically;
- splinter societies and changed cultures;
- old beacons, probes and navigation infrastructure;
- routes that were once important but later died.

## 3. Mission state

A generation ship should have at least:

```text
identity
launch_year
source_system
planned_target
current_target
route_segments
propulsion / cruise capability
fuel / delta-v margin where applicable
hull / subsystem condition
population_total
population_awake
population_cryo
age/cohort structure
genetic diversity
technical knowledge
mission doctrine / governance profile
resources / food / life-support margin
research / repair capability
historical event stream
```

The canonical state should be derived from events where practical. Route distance and arrival time are derived from endpoints, segments and actual speed; they should not be manually duplicated as unrelated authoritative fields.

## 4. Voyage simulation

A useful yearly/event-driven order is:

```text
1. watch / awake-crew rotation
2. physical ship advance along current route segment
3. ageing / cohort transitions
4. pending probe or survey returns
5. mortality and accident losses
6. cryo failures / forced wakeups
7. births where current awake policy allows them
8. resource and life-support consumption
9. maintenance / research completion
10. subsystem decay and cascade failures
11. overcrowding / unexpected awake-load effects
12. social / cultural / technical drift
13. terminal-state checks
14. crises / threshold events
15. policy or mission decisions
```

For large historical batches the implementation does not need one expensive full tick per simulated individual per year. Stable periods may be advanced analytically or in multi-year chunks; exact yearly/event resolution is only needed near thresholds, incidents, route changes and arrivals.

## 5. Cryo and standing watch

Awake population must be a first-class variable, not a cosmetic statistic.

It affects:

- food consumption;
- life-support load;
- work/repair capacity;
- reproduction;
- social continuity;
- technical skill retention;
- cryo wear from repeated cycling;
- exposure to conflict, disease and accidents.

The project already considered 16–20-year rotations for some missions. Different missions may intentionally use different rotation doctrines. A cryo failure can wake people early and create a real cascade: higher awake population -> higher food/life-support load -> less maintenance margin -> faster subsystem deterioration.

## 6. Coupled ship systems

Generation ships should not be represented by one generic `condition` number.

At minimum distinguish coupled systems such as:

```text
reactor / power
propulsion
navigation / sensors / control
cryo
life support / food / water
hull / pressure structure
```

Failures propagate through an explicit dependency graph. This is valuable because two ships with the same nominal hull percentage may arrive with radically different populations and capabilities depending on which subsystem degraded.

## 7. Human drift

The exact `Dead Reckoning` five-meter scheme should not be copied literally, but the project should preserve long-term irreversible state changes.

Existing Elite historical heuristics included discipline, control acceptance, enclosure tolerance, identity rigidity, adaptability and conflict risk. These can coexist with more physical measures such as:

```text
genetic diversity
technical literacy
institutional continuity
social stratification
mission-purpose retention
AI/automation dependency
```

Important rule: the voyage should change the society that arrives. A colony founded after 140 years should not be culturally identical to the launch population merely because the ship survived.

## 8. Knowledge and technical regression

Technical capability is historical state.

Knowledge loss may:

- lengthen repairs;
- prevent advanced maintenance;
- reduce sensor interpretation quality;
- block some research branches;
- make later colonies less capable of recreating launch-era technology;
- change which daughter expeditions can be built.

This creates a natural explanation for uneven technological development between nearby colonies.

## 9. Navigation, probes and target revision

A mission may launch with a planned target, but the historical simulator should allow target revision.

Useful event types:

```text
probe_launch
probe_return
system_survey
target_rejected
course_change
off_course
navigation_failure
new_target_selected
arrival_system
```

Target acceptance should depend on incomplete knowledge. Candidate worlds have estimated habitability plus confidence/error. Better sensors and probes reduce uncertainty; probes may take years or decades to return. Continuing to another target costs time and increases accumulated ship risk.

This integrates directly with the earlier route/event work: a route is time-dependent segments created by launch and course-change events, not a single immutable line.

## 10. Colony founding

Arrival is not equivalent to successful settlement.

Founding state should derive from:

```text
arriving population
age / cohort distribution
genetic diversity
surviving technical knowledge
ship subsystem state
remaining stores
planet habitability and hazards
landing / descent losses
available heavy equipment
policy on ship salvage vs preservation
```

The ship itself becomes part of history after landing:

- dismantled for material;
- retained in orbit;
- used as infrastructure;
- abandoned as a wreck;
- left as a memorial/beacon;
- lost during descent.

These outcomes should remain visible to later gameplay where relevant.

## 11. Colony growth and daughter missions

A founded colony evolves independently. Population change should separate births and losses by cause rather than using one opaque growth rate.

Possible drivers:

```text
births
age deaths
illness
accidents
environmental mortality
hereditary disease / limited gene pool
food / housing / medical capacity
local carrying capacity
technology level
```

Once a colony exceeds a development threshold it may become a source of later expeditions. This creates a branching settlement tree:

```text
Sol
 |- Tau Ceti colony
 |   |- nearby daughter colony
 |   `- failed expedition
 |- Epsilon Eridani colony
 |   `- frontier branch
 `- lost generation ship
```

The final map should therefore be clustered and path-dependent, not a uniform sphere around Sol.

## 12. Procedural settlement generation

Recommended deterministic pipeline:

```text
seed
 -> generate/lock stellar and planetary catalogue
 -> generate Earth/Sol launch waves
 -> assign ships, populations and doctrines
 -> simulate each mission historically
 -> found colonies where missions succeed
 -> advance colonies until expansion threshold
 -> launch daughter missions according to local technology/economy/politics
 -> repeat until game-start year
 -> materialize resulting population, culture, infrastructure and historical artifacts
```

Each mission/colony uses deterministic sub-seeds. Re-running the same world seed must reproduce the same history unless an explicit authored override is present.

Target-selection score should combine, not replace with one random roll:

```text
habitability estimate
survey confidence
travel time
ship remaining lifetime / hull margin
remaining supplies
population condition
mission risk tolerance
known neighboring settlements
strategic/resource value
reachable alternatives
```

## 13. Important emergent results

This model can procedurally explain:

- why two close stars have radically different populations;
- why one region is densely colonized and another nearby region is empty;
- why some colonies are technologically backward;
- why specific old trade/navigation corridors later become important;
- where derelict generation ships and abandoned settlements exist;
- where genetically narrow populations or distinctive cultures arose;
- why some systems are old cores and others recent frontier settlements;
- why later economic and political geography has the shape it does.

## 14. Event model

The historical simulator needs one canonical event pipeline. Suggested event vocabulary includes:

```text
launch
watch_rotation
birth_wave
generation_shift
cryo_failure
subsystem_failure
repair
research_complete
mutiny / reform / governance_change
probe_launch
probe_return
course_change
off_course
target_rejected
arrival_system
landing
landing_casualties
colony_site_selected
colony_founded
ship_salvaged
ship_abandoned
colony_crisis
colony_failed
colony_growth_threshold
daughter_mission_launch
```

Mission passport, event history and computed state remain separate concepts. Derived generation shifts and population summaries should not become competing manual sources of truth.

## 15. Scale difference versus Dead Reckoning

The reference game follows one ship and intentionally compresses decisions into a playable generational narrative.

Elite needs a historical world generator capable of simulating many missions and colonies without turning each one into a player-facing campaign. Therefore:

- use aggregate cohorts and event-driven transitions for background history;
- retain named individuals only when historically important or when a local detailed simulation requires them;
- allow large time jumps during stable periods;
- preserve exact route/event outcomes and colony lineage;
- materialize detailed actors only when they matter to current gameplay.

The goal is not to reproduce `Dead Reckoning`; it is to use similarly explicit mechanics to make the already-planned generation-ship prehistory produce the playable world's population map.