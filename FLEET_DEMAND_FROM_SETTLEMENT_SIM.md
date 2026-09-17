# Fleet Demand from Settlement and Economy Simulation

**Status:** design direction / world-generation contract draft  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`

## 1. Principle

Fleet composition must be an output of the generated inhabited world, not a manually assigned decoration layer.

The legacy fleet sketches are useful for defining **classes of ship that a functioning economy needs**, but old absolute counts such as `5000-10000 shuttles` or `10-20 bulk carriers` were estimates based on a manually defined world and therefore must not be treated as canonical.

The intended causal chain is:

```text
historical settlement simulation
        -> colonies / stations / mines / bases / enclaves
        -> population and industrial capability
        -> production / consumption / import / export demand
        -> trade and service flows
        -> route graph / frequency / risk / infrastructure
        -> required transport capacity
        -> fleet classes and fleet counts
        -> actual NPC traffic
```

This closes the loop between:

```text
GENERATION_SHIP_SETTLEMENT_SIM.md
GENERATION_SHIP_SOCIAL_ADAPTATION.md
WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md
```

and the runtime population of civilian/service ships.

## 2. Preserve the class taxonomy; regenerate the counts

The later legacy classification is a good starting taxonomy:

```text
PLAN-L      Planetary Shuttle
FRN-U       Frontier Utility
SYS-M       System Transport
MIN-TUG     Asteroid Tug
INT-C       Inter-system Merchant
INT-A       Armed Transport
FAST-X      Fast Courier
MEGA-C      Container Carrier
CORE-LINE   Orbital / hub freight infrastructure
SEC-ESC     Escort
```

These are **economic roles**, not mandatory exact hulls.

One role may have multiple manufacturers, factions, generations and size bands. One hull may also cover more than one adjacent role.

Existing mapping retained as provisional design intent:

```text
Cobra Mk I    -> lower FRN-U range
Cobra Mk III  -> upper FRN-U / light INT-A range
```

## 3. Colony-generated demand inputs

Each generated inhabited node should publish economic/logistical state such as:

```text
population
population growth / decline
settlement type
industrial capacity
agricultural capacity
resource extraction
energy production
shipyard capacity
local self-sufficiency
import demand by commodity
export supply by commodity
passenger demand
administrative / mail / data demand
military demand
scientific demand
medical demand
construction activity
hazard / conflict state
surface / orbital split
planet gravity / atmosphere constraints
```

Node types may include, but are not limited to:

```text
capital / dense urban world
agricultural colony
resource colony / asteroid settlement
industrial world
military base
scientific station
penal / labor settlement
religious or cultural enclave
trade hub
frontier colony
quarantine / anomaly zone
```

The generator should not infer fleet counts from population alone. A 200-million-person mining system can require more bulk transport than a one-billion-person highly self-sufficient world.

## 4. Route-generated transport demand

For every economically active origin/destination pair, derive a flow:

```text
CommodityFlow
{
    origin
    destination
    commodity
    tons_per_year
    value_per_ton
    time_sensitivity
    hazard_class
    security_requirement
    containerization
    surface_delivery_requirement
}
```

The route layer adds:

```text
cycle_time
jump / corridor topology
in-system travel time
loading / unloading delay
customs / port delay
refuel / service delay
route risk
convoy requirement
reliability requirement
```

For a flow `r` served by class `c`, a first-order ship-count estimate is:

```text
N_required ~=
    annual_flow_tons * round_trip_time_years
    -------------------------------------------------
    payload_tons * load_factor * operational_availability
```

Then apply reserve and service-frequency constraints:

```text
N_final = max(
    capacity_required,
    minimum_departure_frequency_required
) * reserve_factor
```

This means a thin but high-priority medical route can require several FAST-X / FRN-U ships despite low annual tonnage, while an ore corridor is dominated by payload capacity.

## 5. Last-mile demand is separate from inter-system demand

A system receiving 10,000 t/year through one inter-system freighter does not imply one ship can perform the whole logistics chain.

Keep distinct layers:

```text
inter-system trunk
    -> orbital hub / depot
    -> system distribution
    -> orbital-to-surface
    -> local planetary distribution
```

Therefore PLAN-L and local utility counts may be much larger than the number of inter-system freighters.

The old observation that shuttles are the most numerous class remains plausible, but their actual number must emerge from:

```text
surface population
number of separate settlements
orbital industry
planetary gravity / atmosphere
surface logistics alternatives
port throughput
```

## 6. Ship stock is historical, not instantaneous

Required transport capacity does not directly equal the number of ships physically present in the world.

The game-start fleet should be generated historically:

```text
fleet_stock(t+1) =
    fleet_stock(t)
    + new_builds
    + imports / transfers
    - combat_losses
    - accidents
    - retirements
    - scrapping
    - conversions to other roles
```

New-build capacity depends on generated colony history:

```text
shipyard_capacity
industrial technology
materials availability
capital / state budget
war mobilization
trade profitability
political priorities
```

Thus a poor frontier may need 40 utility ships but own only 17, creating expensive freight rates and gameplay opportunities.

A rich old core may possess excess obsolete tonnage and export used ships to the frontier.

## 7. Fleet age and technological layers

The world should contain multiple generations of ships.

For each faction / region:

```text
current production hulls
recent mainstream hulls
old but maintained hulls
converted military/civilian hulls
obsolete frontier hand-me-downs
locally improvised hulls
prestige / experimental designs
```

This is important for visual and economic variety and prevents every NPC ship from being a current model.

## 8. Faction-specific solutions

Economic role is universal; implementation is cultural and industrial.

The same `FRN-U` role may be solved differently by:

```text
high-tech core faction
resource-poor frontier colony
religious enclave
pirate polity
corporate state
old isolationist colony
```

Differences may include:

```text
reactor architecture
thermal margin
automation level
crew requirement
armament
repairability
standardization
container interfaces
safety factor
maintenance burden
fuel choice
```

Legacy humorous reactor concepts are retained only as faction-flavor/reference until physical feasibility is reconciled with the current energy/thermal model.

## 9. Military and escort demand is generated from civilian flow

SEC-ESC count should not be independently hand-authored.

Escort demand derives from:

```text
cargo value
route risk
piracy rate
war state
insurance requirements
state doctrine
convoy size
available armed transports
```

A route becoming economically important should naturally increase:

```text
freighter traffic
pirate opportunity
insurance cost
escort demand
military presence
```

This connects the fleet generator to the same causal economy model as civilian traffic.

## 10. Asteroid and industrial fleets

MIN-TUG and SYS-M demand should come from physical industry rather than generic population.

Inputs include:

```text
number / mass of active extraction sites
ore moved per year
processing location
average asteroid transfer distance
industrial construction rate
orbital fuel production
habitat construction
station maintenance
```

A small asteroid civilization can therefore own an enormous industrial fleet relative to its population.

## 11. Passenger and service traffic

Not all traffic is cargo.

Generate demand for:

```text
commuter / worker rotation
scientific crews
military rotation
medical evacuation
migration
religious / cultural travel
administrative travel
repair / rescue / towing
inspection / customs
couriers / secure data
```

Some of these create dedicated hull classes later; initially they may be variants of PLAN-L, FRN-U, FAST-X or larger transports.

## 12. Runtime materialization

The world simulator may track most fleet activity statistically when far from the player.

Recommended hierarchy:

```text
background economy
    -> route flow / fleet stock / schedule
    -> abstract voyage state
    -> system-entry event
    -> materialized NPC ship near relevant play area
```

A materialized civilian ship should therefore correspond to an actual scheduled or economically motivated movement whenever practical.

This preserves the existing rule:

> NPC ships should not be random decorative traffic.

## 13. Legacy numbers are calibration targets only

Old sketch estimates included approximately:

```text
5,000-10,000 light shuttles
500-1,000 regional freighters
50-100 mainline haulers
10-20 very large bulk carriers
30-50 armored transports
```

These are useful only as an order-of-magnitude sanity check for one old version of the world.

They must be recalculated after colony generation stabilizes.

If the generated economy implies radically different counts, investigate the causes rather than forcing the generator to reproduce the old table.

## 14. Required output of the future fleet generator

At game-start world generation, produce at least:

```text
fleet stock by faction
fleet stock by class / hull family
fleet age distribution
shipyard production capacity
retirement / replacement rate
route assignments
idle / reserve stock
convoy schedules
service / rescue coverage
local system-work fleets
known shortages of transport capacity
```

This data then drives:

```text
NPC spawning/materialization
market freight prices
ship availability and used-ship market
route congestion
piracy opportunity
insurance
maintenance demand
shipyard economy
```

## 15. Dependency order

Do not finalize absolute fleet counts before these are stable enough:

```text
1. historical settlement tree
2. colony population / development states
3. production / consumption model
4. route and infrastructure graph
5. commodity/passenger flows
6. fleet class capacities and cycle times
7. shipyard / fleet-stock history
8. game-start fleet counts
```

The fleet catalogue can be developed earlier; the fleet **quantity model cannot be authoritative earlier**.