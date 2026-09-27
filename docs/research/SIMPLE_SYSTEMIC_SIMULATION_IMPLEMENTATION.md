# Elite — Simple Systemic Simulation Implementation

Date: 2026-09-27

Goal: simple causal imitation of realism. Simulate only variables that change a decision, action, visible result, or system capability.

Related research: PROCEDURAL_WORLD_AND_ASSET_RESEARCH.md, STELLAR_SYSTEM_AND_CELESTIAL_BODY_GENERATOR.md, AUTONOMOUS_NAVIGATION_AND_DOCKING_RESEARCH.md, SPACE_GAMES_2026_RESEARCH.md.


---

## 1. Core rule: simple rules, real consequences

Bad direction:

~~~text
reactor -> electrical bus -> every cable -> coolant pipe -> dozens of parameters
~~~

Desired direction:

~~~text
reactor damaged
 -> available power -35%
 -> engine / weapon / repair output falls
 -> ShipCapability changes
 -> Planner checks whether the current program is still feasible
~~~

If a variable does not change a decision, action, visible result or system capability, it probably does not need runtime simulation.

## 2. Runtime simulation LOD

Not every remote object needs full physics.

~~~text
ACTIVE
  rigid body
  Planner/Follower
  collision
  module damage
  docking

REGIONAL
  discrete route progress
  ETA
  aggregated damage and events

STATISTICAL
  inventory flow
  production and consumption
  scheduled shipment
  no physical ship object
~~~

An AbstractShipment may materialize as a physical ship when it enters an active region and collapse back to an abstract record when it leaves.

Persistent shipment state:

~~~text
cargo
origin
destination
departure time
arrival time
ship class
ship design seed
damage summary
route state
~~~

This is the main performance trick for a large living economy.

## 3. Fast procedural stellar-system generation

Useful open reference:

pioneerspacesim/pioneer
src/galaxy/StarSystemGenerator.cpp

Pioneer uses a fast game-scale model:

~~~text
stellar hierarchy
 -> barycentres
 -> disk radial interval and density
 -> orbital slices
 -> body mass from disk area/density
 -> body type
 -> recursive moons
 -> Hill-radius limits
~~~

Recommended Elite runtime data:

~~~cpp
struct DiskDescriptor {
    double innerAu;
    double outerAu;
    double solidMassEarth;
    double densityExponent;
    double metallicity;
    double snowLineAu;
};
~~~

The disk can use a simple radial surface-density law:

~~~text
Sigma(r) = Sigma0 * r^(-p)
~~~

Planet placement can be sequential and correlated rather than independent random draws:

~~~cpp
double r = disk.innerAu * randomRange(1.0, 1.4);

while (r < disk.outerAu && bodies.size() < maxBodies) {
    OrbitSlice slice = sampleOrbitSlice(seed, r, disk.outerAu);

    double mass =
        integrateDiskMass(disk, slice.inner, slice.outer)
        * logNormal(seed, 0.7, 1.3);

    Body body = makeBody(mass, slice, context);
    bodies.push_back(body);

    r = body.apoapsis * randomRange(1.25, 1.9);
}
~~~

This produces correlated systems without a multi-billion-year formation simulation.

For moons, use a gameplay-friendly fraction of Hill radius as the outer stable region. Pioneer explicitly tightens the physical bound partly to avoid creating useless distant moons; that is exactly the intended Elite philosophy.

After generation, apply cheap stability gates. Full N-body integration remains an offline validation tool only.

## 4. Procedural planetary surface

The detailed terrain algorithms are already documented in PROCEDURAL_WORLD_AND_ASSET_RESEARCH.md.

The simple runtime chain is:

~~~text
CelestialBodyDescriptor
 -> MacroShape
 -> TerrainRegions
 -> GeologicalStamps
 -> BiomeField
 -> MaterialField
 -> LocalDetail
~~~

Macro terrain needs only a few coherent fields:

~~~text
continent / basin field
+ ridged mountain field
+ domain warp
+ regional modifiers
~~~

Do not force noise to invent every geological feature. Use deterministic authored feature families:

~~~text
craters
canyons
mesas
lava tubes
caves
cliffs
ice rifts
~~~

Placement can be deterministic per macro cell:

~~~cpp
for (MacroCell cell : cells) {
    uint64_t s = hash(planetSeed, cell.id, FeatureLayer);

    if (!chance(s, featureDensity))
        continue;

    FeatureStamp f =
        chooseCompatibleFeature(body.geology, s);

    placeFeature(f, transformFromSeed(s));
}
~~~

Use a small shared material library such as rock, basalt, soil, sand, ice, snow, regolith and metal-rich rock. Blend by biome, slope, altitude, moisture, temperature and geology. No planet-sized unique texture set is required.


---

## 5. Economy: resource flow, not a market simulator

Each economically meaningful place is a node.

~~~cpp
struct EconomicNode {
    NodeId id;

    Inventory inventory;
    ProductionProfile production;
    ConsumptionProfile consumption;

    Inventory targetStock;
    Inventory reserveStock;
    Capacity storageCapacity;
};
~~~

Possible nodes include mine, refinery, factory, farm, colony, station, shipyard and military base.

The economy should update at low frequency, for example once every 10–60 game seconds.

~~~cpp
void updateNode(EconomicNode& n, double dt) {
    for (const Recipe& r : n.production.recipes) {
        double batches =
            maxExecutableBatches(n.inventory, r, dt);

        consumeInputs(n.inventory, r, batches);
        addOutputs(n.inventory, r, batches);
    }

    applyConsumption(
        n.inventory,
        n.consumption,
        dt);
}
~~~

## 6. Shortage and surplus

A full agent market is unnecessary.

~~~text
shortage = max(0, targetStock - inventory)
surplus  = max(0, inventory - reserveStock)
~~~

~~~cpp
double shortageScore(
    const EconomicNode& node,
    CommodityId c)
{
    double target = node.targetStock[c];

    if (target <= 0.0)
        return 0.0;

    return clamp(
        (target - node.inventory[c]) / target,
        0.0,
        1.0);
}
~~~

This alone creates useful transport demand.

## 7. Price is a projection of node state

The player does not need a simulated exchange order book.

~~~text
price =
    basePrice
  * shortageModifier
  * localCostModifier
  * riskModifier
~~~

Simple version:

~~~cpp
double localPrice(
    double base,
    double stock,
    double target)
{
    if (target <= 0.0)
        return base;

    double shortage =
        clamp((target - stock) / target, -1.0, 1.0);

    return base * (1.0 + 0.75 * shortage);
}
~~~

The UI can expose only useful conclusions such as cheap, normal, expensive and shortage.

## 8. Shipment generation

A shortage searches for compatible surplus.

~~~cpp
struct ShipmentRequest {
    NodeId source;
    NodeId destination;

    CommodityId commodity;
    double amount;

    double priority;
};
~~~

Process:

~~~text
destination shortage
 -> find surplus sources
 -> calculate strategic route cost
 -> choose acceptable source
 -> create ShipmentRequest
~~~

Route cost may combine:

~~~text
travel time
+ fuel / delta-v
+ danger
+ congestion
+ political or legal penalty
~~~

## 9. Strategic route graph

Useful open references:

Star Ruler 2:
scripts/shared/system_pathing.as

Naev:
dat/ai/core/misc/lanes.lua

Star Ruler 2 uses Dijkstra and extends it with trade permissions and special gate links.

The global route must not know detailed collision geometry.

~~~text
StrategicRoutePlanner:
    systems
    gates
    infrastructure lanes
    access rules
    risk

Local Planner:
    physical route / maneuver corridor
~~~

Skeleton:

~~~cpp
distance[start] = 0;
open.push(start, 0);

while (!open.empty()) {
    Node u = open.popLowestCost();

    for (Edge e : graph.edges(u)) {
        if (!tradeAllowed(e))
            continue;

        double next =
            distance[u] + edgeCost(e);

        if (next < distance[e.to]) {
            distance[e.to] = next;
            previous[e.to] = u;
            open.push(e.to, next);
        }
    }
}
~~~

## 10. Physical freighters generated by economic demand

Star Ruler 2 contains a particularly useful complete chain in:

~~~text
Resources.as
RegionObjects.as
Civilian.as
system_pathing.as
~~~

The code does approximately:

~~~text
resource has a destination
 -> civilian trade demand/timer
 -> create or reuse a real freighter
 -> move to origin
 -> pick up cargo
 -> TradePath selects next systems
 -> optionally use trade station / intermediate
 -> reach destination
 -> fire delivery event
~~~

Elite version:

~~~text
ShipmentRequest
 -> FleetDispatcher
 -> choose or generate freighter
 -> StrategicRoute
 -> Planner/Follower
 -> StationTrafficController
 -> DockingPortGuidance
 -> unload
 -> inventory update
~~~

## 11. Remote shipment representation

~~~cpp
struct AbstractShipment {
    CommodityId commodity;
    double amount;

    NodeId origin;
    NodeId destination;

    double departureTime;
    double arrivalTime;

    ShipTemplateId shipClass;
    uint64_t shipDesignSeed;

    DamageSummary damage;
};
~~~

Far from active gameplay there is no rigid body, collision or Planner.

Near active gameplay:

~~~text
AbstractShipment
 -> materialize physical Ship
 -> restore design, cargo and damage
 -> place consistently with ETA/progress
 -> activate Planner/Follower
~~~

The reverse conversion occurs on leaving the active region.

## 12. Simple production chain example

~~~text
Mine:
    Ore +4/min

Refinery:
    Ore -3/min
    Metal +2/min

Factory:
    Metal -1/min
    Components +0.7/min

Shipyard:
    Metal -2/min
    Components -1/min
    ShipProgress +X

Colony:
    Food -1/min
    ConsumerGoods -0.3/min
~~~

This is already enough to generate visible economic behavior.

## 13. Destroyed transport has an economic consequence

~~~text
freighter destroyed
 -> cargo not delivered
 -> destination stock stays low
 -> shortage rises
 -> local price rises
 -> production may slow
 -> replacement ShipmentRequest appears
~~~

This makes piracy, escorting, blockades and infrastructure damage meaningful without a complicated macroeconomic model.
