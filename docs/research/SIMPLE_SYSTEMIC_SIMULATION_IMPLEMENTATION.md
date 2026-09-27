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


---

## 14. Procedural ship generation: function first

Best open implementation found:

~~~text
BlindMindStudios/StarRuler2-Source
scripts/shared/util/random_designs.as
~~~

Pinned source commit:

~~~text
beec9bff697ffbebafaeb66d0cba1856a02cb6db
~~~

The important architecture is not random visual assembly. It is:

~~~text
ShipRole
 -> functional composition recipe
 -> connected structural footprint
 -> functional subsystem placement
 -> remaining-space fill
 -> armor
 -> validation
 -> compatible visual hull
~~~

This is the correct direction for Elite.

## 15. Star Ruler 2 shape algorithm

The original uses a hex grid.

Simplified:

~~~text
start at center

while target size not reached:
    choose an occupied cell
        weighted by its free adjacent cells

    choose a free adjacent direction
        using directional weights

    occupy that cell

    often occupy the mirrored counterpart
~~~

This produces a connected, varied, controllably symmetric footprint cheaply.

Elite should transfer the principle, not necessarily the hex grid.

## 16. Elite topology-first generator

Recommended chain:

~~~text
ROLE
 -> SIZE
 -> FUNCTION BUDGET
 -> STRUCTURAL SPINE
 -> EXTERNAL MODULES
 -> INTERNAL MODULES
 -> FILLER / CARGO
 -> HULL / ARMOR
 -> PHYSICAL VALIDATION
 -> VISUAL ASSEMBLY
~~~

Suggested structural representation:

~~~cpp
struct StructuralNode {
    NodeId id;
    Transform transform;
    std::vector<Socket> sockets;
};

struct Socket {
    SocketType type;
    SizeClass size;
    Transform transform;
    TagSet allowedTags;
};
~~~

Example topology:

~~~text
          bridge
            |
nose -- core -- cargo -- cargo -- engines
            |
          reactor
~~~

The visual meshes are selected after functional topology is valid.

## 17. Role recipes

Freighter example:

~~~cpp
ShipRecipe Freighter {
    core      = 0.05;
    reactor   = 0.08;
    engines   = 0.22;
    cargo     = 0.38;
    radiator  = 0.08;
    fuel      = 0.10;
    docking   = 0.03;
    sensors   = 0.02;
    armor     = remainder;
};
~~~

Fighter:

~~~text
high engine budget
high weapon budget
medium fuel
medium armor
tiny cargo
~~~

Mining vessel:

~~~text
high cargo
high power
high mining equipment
high radiator capacity
medium engine
~~~

These are internal role weights, not player-facing tuning sliders.

## 18. Place constrained external systems first

Star Ruler 2 explicitly distinguishes placement strategies such as Weapon, Exhaust, Internal, Filler and ArmorLayer.

Elite should use the same concept.

Suggested order:

~~~text
1 engines
2 docking / landing hardware
3 weapons
4 radiators
5 sensors
6 external bridge if required
7 reactor / core / tanks
8 cargo / crew / filler
9 armor / hull fairing
~~~

Reasons:

~~~text
engine needs exhaust clearance
weapon needs firing arc
dock needs approach clearance
radiator needs exposed surface
cargo can occupy remaining internal space
~~~

## 19. Module placement rules

~~~cpp
struct ModulePlacementRule {
    bool requiresExterior;
    bool requiresRear;
    bool requiresForwardArc;
    bool requiresClearance;

    double symmetryPreference;
    double centerPreference;

    TagSet adjacencyPreferred;
    TagSet adjacencyForbidden;
};
~~~

Candidate score:

~~~cpp
double placementScore(
    const ModuleDef& m,
    const CandidateSocket& s)
{
    double score = 1.0;

    score *= exteriorScore(m, s);
    score *= directionScore(m, s);
    score *= symmetryScore(m, s);
    score *= adjacencyScore(m, s);
    score *= centerOfMassScore(m, s);

    return score;
}
~~~

Choose one valid candidate by weighted random.

This allows manufacturer/style DNA to modify preferences without changing functional constraints.

## 20. Generate, validate, retry

One of the best patterns in Star Ruler 2 is deliberately simple:

~~~text
generate candidate
 -> validate
 -> reject if bad
 -> retry
~~~

Its random designer can retry many times instead of making every placement rule perfect.

Elite:

~~~cpp
for (int attempt = 0; attempt < 32; ++attempt) {
    ShipDesign d =
        buildCandidate(seed, attempt);

    ValidationResult v =
        validate(d);

    if (v.ok())
        return d;
}

return fallbackDesign(role, size);
~~~

When attempt number is part of deterministic seed derivation, the result remains reproducible.

## 21. Ship validation contract

Minimum checks:

~~~text
mandatory systems exist
all critical modules connected
no illegal module overlap
mass is valid
center of mass is acceptable
main thrust meets role requirement
mandatory power demand is covered
radiator capacity is acceptable
dock / landing clearance is free
weapon arcs are useful
control mode is feasible
~~~

The generator should reject physically nonsensical combinations rather than invent compensating magic.

## 22. Visual variation comes after validity

Cheap uniqueness:

~~~text
same functional topology
+ mesh-family variants
+ proportions
+ manufacturer DNA
+ palette
+ decals
+ wear
+ antennas
+ optional cosmetic shell pieces
~~~

Do not create a new functional topology merely to get another paint scheme or silhouette.

## 23. Economy can determine procedural ship shape

A shipment already knows:

~~~text
cargo amount
route distance
route danger
desired delivery time
~~~

This can define a FreighterSpec:

~~~cpp
FreighterSpec makeFreighterSpec(
    double cargoMass,
    double routeDistance,
    RouteClass route)
{
    FreighterSpec s;

    s.cargoVolume =
        cargoMass / averageCargoDensity;

    s.targetMass =
        cargoMass / targetPayloadFraction;

    s.requiredAcceleration =
        roleAcceleration(route);

    s.requiredMainThrust =
        s.targetMass * s.requiredAcceleration;

    s.fuelMass =
        estimateFuel(
            s.targetMass,
            routeDistance);

    s.power =
        estimatePower(s);

    return s;
}
~~~

Then the procedural ship generator builds the vessel from that specification.

Result: different jobs naturally create different ships.


---

## 24. Modular damage: spatial modules, not a single HP bar

Best open reference found:

~~~text
Star Ruler 2
source/game/obj/blueprint.cpp
source/game/obj/blueprint.h
~~~

The original implementation uses a spatial hex grid.

Incoming hit:

~~~text
impact direction
 -> locate entry cell
 -> walk through cells
 -> resistance consumes penetration
 -> local module takes damage
 -> residual energy continues
 -> subsystem efficiency falls
 -> vital module may disable subsystem
~~~

This is exactly the right causal depth for Elite.

## 25. Elite damage geometry can be simpler

We already have ship modules and collision volumes.

Use:

~~~text
module OBB/AABB
+ module BVH / spatial index
+ projectile ray or swept segment
~~~

~~~cpp
auto hits =
    ship.moduleBVH.raycastAll(impactRay);

sortByEntryDistance(hits);
~~~

Then pass residual energy through the ordered intersections.

## 26. Simple penetration model

~~~cpp
void applyPenetratingHit(
    Ship& ship,
    const Ray& ray,
    double energy)
{
    auto hits =
        ship.moduleBVH.raycastAll(ray);

    sortByDistance(hits);

    for (const ModuleHit& hit : hits) {
        if (energy <= 0.0)
            break;

        ModuleState& m =
            ship.modules[hit.moduleId];

        double armorCost =
            m.armorResistance
            * hit.pathLengthInsideArmor;

        double absorbed =
            std::min(energy, armorCost);

        energy -= absorbed;

        if (energy <= 0.0)
            break;

        double damage =
            std::min(
                m.hp,
                energy * m.damagePerEnergy);

        m.hp -= damage;

        energy -=
            damage * m.energyPerHp;

        updateModuleState(ship, m);
    }
}
~~~

This can produce a readable physical result:

~~~text
armor partially absorbs hit
 -> projectile enters cargo bay
 -> residual energy continues
 -> reactor is damaged
 -> reactor output falls
~~~

No finite-element structural simulation is required.

## 27. Module efficiency

Star Ruler 2 scales subsystem effectiveness by the fraction of surviving cells.

Elite can use module health directly:

~~~cpp
double moduleEfficiency(const ModuleState& m)
{
    double h =
        clamp(m.hp / m.maxHp, 0.0, 1.0);

    if (h <= m.failureThreshold)
        return 0.0;

    return smoothstep(
        m.failureThreshold,
        1.0,
        h);
}
~~~

Apply it to real capabilities:

~~~text
engine thrust *= efficiency
reactor power *= efficiency
sensor range *= efficiency
weapon reload/power *= efficiency
radiator capacity *= efficiency
repair capability *= efficiency
~~~

## 28. Gameplay-relevant module failures

We do not need to simulate every cable or pipe.

Useful consequences:

~~~text
main engine destroyed
 -> main thrust lost

front / reverse engine damaged
 -> braking authority reduced

RCS cluster destroyed
 -> lateral and angular authority reduced

reactor damaged
 -> global power budget reduced

sensor damaged
 -> detection range / precision reduced

cargo module breached
 -> cargo loss

docking mechanism damaged
 -> some docking ports become unusable
~~~

The ship remains a coherent physical object but its behavior changes.

## 29. Damage must update navigation capability

Each ship should own a capability revision.

~~~cpp
uint64_t capabilityRevision;
~~~

When a damage threshold changes a capability:

~~~cpp
void onCapabilityModuleChanged(Ship& ship)
{
    ++ship.capabilityRevision;

    ShipCapability cap =
        rebuildCapability(ship);

    navigation.notifyCapabilityChanged(
        ship.id,
        cap,
        ship.capabilityRevision);
}
~~~

Navigation then does:

~~~text
ShipCapability changed
 -> validate remaining AcceptedManeuverProgram

still feasible
 -> continue

not feasible
 -> SafetySupervisor chooses recovery
 -> Planner replans
~~~

A random hit must not simply turn autopilot off.

## 30. Visual damage can remain cheap

~~~text
module-health threshold
 -> decal
 -> sparks
 -> smoke / fire
 -> emissive off
 -> animation stop
 -> optional mesh-group hide or detach
~~~

Large detachable modules can be added later. They are not required for the first damage model.

## 31. Repair

Simple automatic priority:

~~~text
Priority 1:
control
reactor
propulsion

Priority 2:
sensors
docking
weapons

Priority 3:
cargo
armor
cosmetic modules
~~~

~~~cpp
repairBudgetPerSecond
 -> distribute by priority
 -> restore module hp
~~~

No per-bolt repair interface is required.

---

## 32. Economy creates station traffic

Each active ShipmentRequest creates a real traffic intent:

~~~text
Freighter
 -> StrategicRoute
 -> local Planner
 -> station ArrivalGate
 -> TrafficController
 -> Hold / queue
 -> assigned Dock
 -> DockingPortGuidance
 -> Follower
 -> unload
~~~

This means civilian traffic is never decorative. It exists because a resource needs to move.

## 33. Simple emergent station infrastructure

Star Ruler 2 contains another useful cheap trick.

Its region code counts trade events. The number of civilian trade stations is then increased or decreased from the amount of real trade traffic.

Conceptually:

~~~text
trade activity rises
 -> tradeCounter rises
 -> desired station count rises
 -> station infrastructure appears

trade activity falls
 -> excessive station infrastructure disappears
~~~

We should not necessarily spawn/delete major player-facing stations this way, but the principle is useful for secondary infrastructure:

~~~text
traffic volume
 -> demand for docks
 -> demand for warehouses
 -> demand for refuelling
 -> demand for tug/repair capacity
 -> local infrastructure growth
~~~

A simple moving average of traffic can drive this.

~~~cpp
infrastructurePressure =
    lerp(
        infrastructurePressure,
        observedTrafficPerHour,
        smoothing);
~~~

Then thresholds can create or upgrade service modules.

## 34. Player-facing economy should expose consequences

Useful messages:

~~~text
Hub fuel stock is low.

Refinery is waiting for ore.

Six ships are waiting for docking clearance.

Freighter 24 lost its main engine.

Shipyard production stopped: metal shortage.

Trade route became dangerous.
~~~

The player does not need to see every internal counter.

---

## 35. Minimal vertical slice

Start with one system:

~~~text
3 EconomicNodes
4 commodities
2–3 production recipes
1 station
4 docks
3 freighters

procedural freighter generator

damage modules:
    engine
    reactor
    cargo
    sensor or weapon

Planner/Follower integration
~~~

Test scenario:

~~~text
Mine produces Ore
 -> Refinery shortage
 -> ShipmentRequest
 -> freighter assigned
 -> route
 -> station queue
 -> docking
 -> delivery
 -> Metal production

pirate attacks freighter
 -> main engine damaged
 -> capability revision
 -> current maneuver no longer feasible
 -> replan
 -> delivery delayed
 -> shortage persists
 -> new economic consequences
~~~

If this looks alive, the architecture works.

## 36. Suggested code ownership

~~~text
src/world/economy/
    CommodityDef
    Inventory
    ProductionRecipe
    EconomicNode
    EconomyTick
    ShipmentRequest
    ShipmentDispatcher
    AbstractShipment

src/world/traffic/
    StrategicRoutePlanner
    TrafficController
    ArrivalQueue

src/ships/generation/
    ShipRoleRecipe
    ShipStructuralGraph
    ModuleCatalog
    ModulePlacementRule
    ProceduralShipGenerator
    ShipDesignValidator

src/ships/damage/
    ShipModuleState
    ShipModuleBVH
    DamageEvent
    PenetrationResolver
    CapabilityRebuilder

src/ships/
    ShipCapability
    ShipCapabilityRevision
~~~

## 37. Acceptance tests

Ship generation batch:

~~~text
generate 10,000 ships

100% mandatory systems present
0 illegal overlaps
critical modules connected
power budget valid
role thrust valid
center of mass valid
same seed -> same design checksum
~~~

Damage tests:

~~~text
front shot -> armor -> cargo
rear shot -> main engine
side shot -> reactor
grazing shot
overpenetration
multiple modules inline
~~~

Verify hit order, residual energy, module efficiency and capability revision.

Economy test:

~~~text
run 30 simulated days headless

inventories bounded
shortage creates shipments
surplus source selected
cargo conserved
destroyed shipment creates deficit
replacement shipment generated
~~~

Simulation-LOD test:

~~~text
abstract shipment final result
approximately equals
materialized physical shipment final result
~~~

with explicit tolerances.

## 38. Mirrored open-source code

Star Ruler 2 source code is MIT licensed.

Pinned source:

~~~text
BlindMindStudios/StarRuler2-Source
beec9bff697ffbebafaeb66d0cba1856a02cb6db
~~~

Mirrored under:

~~~text
third_party/systemic_reference/star_ruler_2/

COPYING

ship_generation/
    random_designs.as

modular_damage/
    blueprint.cpp
    blueprint.h

economy/
    system_pathing.as
    Civilian.as
    Resources.as
    RegionObjects.as
~~~

These are research references. Production Elite code should use our own types and architecture.

Pioneer remains a GPL-3.0 research reference:

~~~text
pioneerspacesim/pioneer
c62b938356e37c35d67406b32ebb69d57cf4eeb9

useful:
src/galaxy/StarSystemGenerator.cpp
src/galaxy/Economy.cpp
src/terrain/
~~~

Blockchain is intentionally excluded.

## 39. Final implementation rule

Elite should simulate:

~~~text
CAUSE
 -> VISIBLE CONSEQUENCE
 -> NEW GAMEPLAY STATE
~~~

Examples:

~~~text
reactor hit
 -> less power
 -> worse thrust / weapons

resource shortage
 -> shipment
 -> visible freighter traffic

freighter destroyed
 -> cargo loss
 -> local shortage

engine destroyed
 -> ShipCapability changes
 -> Planner adapts

different transport job
 -> different procedural ship modules/proportions
~~~

This is enough to create the impression of a coherent realistic world without turning the game into a professional simulator.
