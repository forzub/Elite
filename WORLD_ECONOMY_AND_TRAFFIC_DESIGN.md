# World Economy, Traffic and Civilized Navigation

**Status:** design direction / world-simulation contract draft  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`

## 1. Purpose

This document turns previously discussed world/economy/navigation ideas into a more explicit game-design direction.

Important provenance note: the core ideas below were already part of the Elite project direction before the `Objects in Space` reference was reviewed. That game is useful because several of the same ideas are presented there in a comparatively finished, readable form. Treat it as a reference that helps formalize the design, not as the origin of the design and not as a template to copy literally.

The major deliberate difference is **scale**. `Objects in Space` presents a comparatively compact-feeling universe. Elite should preserve much larger distances, stronger isolation between developed regions, and a sharper contrast between civilized infrastructure and genuinely open/poorly supported space.

## 2. Core world principle

Economy, traffic, navigation infrastructure and risk should form one causal system.

```text
production / consumption / demand
            |
            v
       trade flows
            |
            v
civilized routes / jump nodes / beacons
            |
            v
     actual NPC traffic
            |
            +--> services / fees / taxes
            +--> piracy / protection / insurance
            +--> contracts / shortages / opportunities
            +--> navigation density / route value
```

NPC ships should not exist merely to make the scene look busy. Most meaningful traffic should have a reason to exist: cargo, passenger flow, service task, military/security duty, maintenance, salvage, towing, emergency response, courier work or another world-state cause.

## 3. Civilization should be visible in navigation itself

Civilized space is not only a political label. It is physically expressed by navigation infrastructure.

Developed inter-system and intra-system flows may concentrate around:

- established trade corridors;
- jump nodes / transition nodes;
- navigation beacons;
- approach lanes;
- station traffic control regions;
- serviced transfer points;
- refuelling / repair / rescue coverage;
- mapped and maintained routes.

The player may use this infrastructure because it is safer, easier to navigate, better mapped and better supported — but not necessarily free.

Outside developed civilization, the contract changes:

```text
civilized space
    known infrastructure
    mapped routes
    beacons / traffic services
    commercial support
    rescue / towing / repair availability

frontier / undeveloped space
    incomplete or stale maps
    few or no beacons
    weak service coverage
    sparse traffic
    player-owned navigation problem
```

**Design rule:** beyond developed civilization, navigation increasingly becomes the player's responsibility. The game should not silently replace missing infrastructure with perfect omniscient route guidance.

This distinction should remain compatible with `NAVIGATION_WORLD_V2.md`: runtime pathfinding and collision avoidance are technical safety/navigation systems, while civilized corridors, beacons and jump infrastructure are world-level semantic inputs and constraints.

## 4. Trade flows create routes; routes are not arbitrary decoration

Trade lanes should emerge from economically meaningful endpoints.

Example:

```text
mine
  -> refinery
      -> component plant
          -> shipyard
              -> military / commercial consumers
```

The flow between those endpoints can create:

- repeated NPC freight traffic;
- economically important corridors;
- traffic-density differences;
- infrastructure investment;
- patrols and inspections;
- piracy opportunities;
- escort demand;
- insurance-price differences;
- service businesses along the route.

A route therefore has value because the world uses it, not because a designer painted a glowing line on the map.

## 5. NPC traffic must be causal

Accepted direction:

> NPC traffic should not be random decorative traffic.

A ship that appears in the local simulation should, where practical, be the materialized form of a higher-level world task or flow.

Possible causes:

```text
CargoFlow
PassengerFlow
CourierTask
RepairTask
TowTask
RescueTask
SecurityPatrol
InspectionTask
MilitaryLogistics
SalvageTask
ConstructionSupply
```

The distant world does not need to simulate every thruster burn for every ship. Long-range traffic may exist as aggregated logistics/world-state records and materialize into full NPC ships only when local simulation requires it.

Conceptual layering:

```text
WORLD / ECONOMY STATE
        |
        v
AGGREGATED FLOW / TASK
        |
        v
ROUTE / SCHEDULE / RISK
        |
        v
LOCAL MATERIALIZATION
        |
        v
NavigationWorld + flight/control
```

This supports a large universe without requiring every distant transport to run full navigation and physics continuously.

## 6. The economy should be expensive to participate in

The intended tone is not frictionless space commerce. Civilized infrastructure is useful because somebody built, maintains, regulates and monetizes it.

The player should regularly pay for access to civilization.

Potential money sinks / service charges include:

- landing / docking fees;
- port or berth fees;
- cargo handling;
- customs and duties;
- trade taxes;
- transaction / market fees;
- navigation infrastructure fees where appropriate;
- jump-node / gate usage where appropriate;
- refuelling;
- repair labour and parts;
- towing;
- rescue / recovery;
- storage;
- fines;
- permits / licenses;
- inspection-related costs;
- insurance premiums and deductibles.

The economic feeling should be closer to:

```text
gross revenue != profit
```

A nominally profitable route may become mediocre after fuel, maintenance, docking, taxes, insurance, time and risk are included.

This is intentional. The player should have reasons to care about efficient routing, ship choice, reputation, service access and risk.

## 7. Insurance is an explicit addition

Insurance should be a first-class economic system rather than an invisible respawn subsidy.

Possible insured subjects:

- hull;
- fitted modules;
- cargo;
- high-value mission cargo;
- towing / recovery coverage;
- liability or commercial operating coverage where useful to gameplay.

Possible pricing inputs:

```text
ship value
module value
cargo value
pilot / company history
route risk
system risk
piracy level
war / instability
claim history
coverage tier
insured deductible
```

The design goal is not paperwork for its own sake. Insurance should convert world risk into an economic choice.

Example:

```text
safe civilized corridor
    higher fees / taxes
    lower operational uncertainty
    lower insurance risk

short dangerous route
    lower infrastructure cost
    higher loss probability
    higher premium / deductible
```

That creates meaningful route choice beyond shortest distance.

## 8. Services are part of the economy, not UI conveniences

Landing, repair, towing and rescue should not be free magic buttons.

If a player makes a bad decision, civilization may save the ship — for a price.

Examples:

### Repair

Cost may depend on:

- part availability;
- local labour rate;
- urgency;
- damage severity;
- station capability;
- faction / company relationship.

### Towing

Towing can depend on:

- distance;
- ship mass;
- hazard level;
- jurisdiction;
- response time;
- availability of suitable service craft.

### Rescue

Emergency rescue may be expensive, delayed or unavailable outside covered regions.

The player should feel the difference between being stranded near a major trade hub and being stranded beyond developed civilization.

## 9. Taxes and fees help define political/economic geography

Taxes should not be globally uniform.

Different jurisdictions may have different combinations of:

- customs rate;
- docking rate;
- market fee;
- corporate tax / tariff;
- prohibited or restricted goods;
- inspection probability;
- insurance environment;
- service quality;
- corruption / unofficial payment pressure if later supported by world fiction.

This can make two physically similar systems economically very different.

A longer route through a low-tax, safe jurisdiction may beat a short high-tax route. A nominally lucrative market may be unattractive after fees and inspection risk.

## 10. Map gameplay should expose causes, not omniscient answers

The galaxy/system map can become an analysis instrument rather than only a destination selector.

Potential overlays / layers:

- trade-flow intensity;
- known demand / shortages;
- known supply surplus;
- traffic density;
- route risk;
- piracy reports;
- insurance risk;
- jurisdiction / tax regime;
- navigation infrastructure coverage;
- beacon / jump-node coverage;
- rescue / towing / repair coverage;
- information freshness.

A key principle is that the map should not necessarily know everything perfectly. Information itself can have age, source and reliability.

Example:

```text
MARKET REPORT
source: Zenith exchange relay
age: 37 min
reliability: high

TRAFFIC / RISK REPORT
source: commercial beacon net
age: 11 min
coverage: partial
```

Outside developed civilization, data becomes sparse, stale or absent.

## 11. Scale is the intentional divergence

The reference game feels compact. Elite should not.

The intended contrast is stronger:

### Developed core / mature trade regions

- recognizable traffic flows;
- established routes;
- beacons / nodes / services;
- frequent commercial interaction;
- taxes and fees everywhere;
- meaningful regulation and rescue coverage.

### Peripheral / frontier regions

- long distances;
- sparse traffic;
- weak or absent infrastructure;
- incomplete navigation support;
- low service availability;
- stronger consequences for fuel, damage and planning errors;
- greater importance of the player's own navigation and preparation.

### Deep / undeveloped space

- no assumption that a convenient trade lane exists;
- no assumption that a beacon exists;
- no assumption that rescue arrives;
- no assumption that the map contains a current safe route;
- distance itself becomes gameplay.

This scale difference should remain visible in travel time, information quality, support availability and economic risk, not only in the number printed for kilometres.

## 12. What is being adopted from the reference

Not copied assets, setting or exact mechanics. The useful part is **formalization** of ideas already compatible with this project:

- commerce has friction and overhead;
- civilized services cost money;
- taxes matter;
- repair and rescue are economic events;
- traffic has causes;
- trade flows imply routes;
- route infrastructure is part of civilization;
- the world can create opportunities from its own state rather than spawning arbitrary quests;
- economics and navigation can feed each other.

The reference helps demonstrate that these ideas can be presented clearly to a player without reducing the game to a spreadsheet.

## 13. Things not to inherit blindly

Do **not** inherit the compact-world feeling if it undermines the intended scale.

Do not make every region equally connected, equally mapped or equally serviceable.

Do not turn trade lanes into mandatory invisible rails. They are infrastructure and economic attractors, not a replacement for physical navigation.

Do not make NPC traffic decorative again after building an economic model capable of generating it causally.

Do not make taxes/fees meaningless token deductions. They should be large enough to affect route profitability and ship-operation decisions.

Do not use insurance as a consequence-free reset button. Premium, deductible, exclusions and claim consequences should preserve risk.

## 14. High-level implementation implication

This document is not yet a code contract, but the likely architecture should separate at least:

```text
EconomyState
    production / consumption / stock / price / jurisdiction

WorldFlowState
    cargo / passenger / service / security flows

InfrastructureState
    corridors / jump nodes / beacons / service coverage

RiskState
    piracy / conflict / hazard / route loss history

CommercialState
    taxes / fees / insurance / contracts / reputation

Materialization
    aggregated flow -> local NPC actor/task

NavigationWorld
    actual local physical navigation / collision / control
```

Economy should not directly own local flight control. Navigation should not fabricate trade demand. The world-state layer creates causes; local runtime systems execute them.

## 15. Design summary

The intended world is not "a large map with shops".

It is a large physical space where civilization leaves a visible economic and navigational footprint.

```text
civilization
    builds infrastructure
    creates trade
    taxes trade
    services ships
    insures risk
    concentrates traffic

traffic
    creates routes
    creates opportunities
    creates targets for crime
    creates need for security

frontier
    removes those guarantees
    returns navigation and survival responsibility to the player
```

The economic system therefore does more than set commodity prices. It helps explain why ships are present, why routes matter, why some regions are safe, why some regions are expensive, why infrastructure exists, and why leaving civilization changes the game.