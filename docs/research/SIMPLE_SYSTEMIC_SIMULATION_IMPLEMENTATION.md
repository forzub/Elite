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
