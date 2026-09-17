# Dead Reckoning: The Long Drift — generation-ship reference notes

**Reviewed:** 2026-09-17  
**Source:** uploaded Kingston Myles video `Слишком РЕАЛИСТИЧНЫЙ симулятор колонизации космоса.mp4`, official Steam/press-kit material, and Colonist Handbook.

## Provenance

This is a reference note, not a design-origin claim.

The core Elite ideas — generation ships as historical actors, route/time simulation, cryo/awake populations, generational change, arrival losses, colony founding and later settlement history — predate this review.

The value of `Dead Reckoning` is that it turns several similar ideas into explicit, legible mechanics.

It is not by the `Objects in Space` developers. `Objects in Space` was made by Flat Earth Games. `Dead Reckoning` is by Garan Lorn / Selenodrome and explicitly cites `Seedship` as an inspiration.

## Mechanics visible/useful in the reviewed material

### 1. One operational year has an explicit order

The reference simulation evaluates, in sequence:

- watch rotation;
- physical ship advance;
- ageing;
- probe returns;
- mortality;
- cryo-pod loss;
- births;
- resource decay;
- research completion;
- subsystem decay/cascades;
- overcrowding;
- drift/morale;
- failure/arrival checks;
- threshold crises;
- dispatches/events.

This is useful because state changes have deterministic causal order instead of all systems mutating independently.

### 2. Cryo and standing watch are mechanically different populations

Reference starting state is 100 awake / 900 cryo. Rotation doctrine can be long, medium or short.

Consequences:

- sleepers avoid ordinary awake life costs and ageing risk depending on implementation;
- repeated rotation wears cryo;
- too-long awake tours increase isolation strain;
- cryo failures wake unplanned population and therefore raise food/life-support demand;
- awake population determines work capability.

This strongly supports keeping Elite's existing awake/cryo split and multi-year duty rotations as first-class historical state.

### 3. Six coupled ship systems

Reference systems:

- reactor;
- engine;
- bridge/navigation;
- cryo;
- life support;
- hull.

They decay separately and failures cascade through dependencies. This is more useful than one global ship-health score.

### 4. Drift across generations

The reference game tracks five cumulative axes:

- genetic diversity loss;
- ideological drift;
- AI autonomy/dependency;
- technical regression;
- class stratification.

Elite should not copy the exact axes blindly, but the core lesson is strong: voyage duration changes the society that arrives, and the changes are persistent rather than reset between generations.

### 5. Knowledge can become a capability limit

In the reference design, research slows as knowledge is lost and eventually some new research may become impossible.

Useful implication for Elite prehistory: two colonies founded by nominally similar ships can diverge technologically because one voyage preserved expertise and another did not.

### 6. Destination knowledge is uncertain

Candidate targets carry:

- star/world type;
- estimated habitability;
- confidence/error;
- route type/hazard;
- survey quality.

Probes can reduce uncertainty but take years/decades to return and consume resources. Rejecting a target costs travel time.

This is directly useful for historical procedural settlement because the actual colony map can differ from the original launch plan for causal reasons rather than random relocation.

### 7. Landing is another failure point

Arrival does not guarantee a colony.

Landing losses depend on world type, ship condition, preparation, accumulated social state and descent policy. The resulting founding population may therefore differ substantially from the population that entered the target system.

### 8. Colony phase matters

The reference game now simulates colony population after landing, including births and separate death causes. It also models local buildings, maintenance, gene-pool support and the option to salvage the generation ship for material.

The particularly useful idea is that the ship's post-landing fate remains part of world history rather than vanishing at `colony_founded`.

### 9. Persistent history is an explicit development direction

Current `Dead Reckoning` updates mention persistent colonies, wrecks and archaeological encounters that can affect later expeditions. This aligns very closely with the Elite requirement that old generation-ship history remain physically discoverable in the later playable world.

### 10. Deterministic seeded runs

The reference game uses seeded deterministic simulation and later introduced named Chronicles to preserve the same stars/history context between voyages.

This is a strong fit for Elite world generation: one world seed should reproduce the same colonization history, including lost ships, colony lineages and population distribution.

## What is most useful specifically for Elite

Not the event prose or exact five drift meters.

The high-value mechanics are:

1. explicit yearly/event causal order;
2. awake/cryo population as a coupled systems variable;
3. subsystem dependency/cascade model;
4. irreversible generational state drift;
5. technical-literacy loss as a real capability limit;
6. uncertain target knowledge + delayed probes;
7. route/target revision during flight;
8. landing as a separate risk phase;
9. post-landing ship disposition;
10. deterministic historical persistence.

## Key scale difference

`Dead Reckoning` is designed around following one ship closely for a full run.

Elite needs to simulate many historical expeditions over a much larger world-history problem. Therefore the reference mechanics should be converted into cohort/event models, not copied at per-person detail for every background mission.

Use detailed named individuals only for historically important people/lineages or when a mission becomes a foreground story. The bulk settlement generator should operate on deterministic cohorts, thresholds and events.

## Procedural-settlement implication

The strongest derived idea is to generate the playable human geography recursively:

```text
Earth/Sol launch wave
 -> mission outcomes
 -> first-generation colonies
 -> colony growth / failure / regression
 -> daughter expedition capability
 -> second-wave launches
 -> further colonies
 -> later trade/navigation links
```

This naturally produces an irregular settlement tree instead of a uniform colonization radius.

The map then contains causes, not decoration:

- old core colonies;
- later frontier branches;
- dead ends;
- failed worlds;
- lost expeditions;
- derelict generation ships;
- culturally isolated populations;
- old navigation corridors that later become economic corridors.

That is the main mechanic worth carrying forward from this reference review.