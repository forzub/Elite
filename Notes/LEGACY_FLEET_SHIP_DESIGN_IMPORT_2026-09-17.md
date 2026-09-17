# Legacy Fleet / Ship Design Import — 2026-09-17

**Status:** source index / preservation note  
**Canonical branch:** `main`

## Source preservation

Original DOCX files were preserved in ChatGPT Library under:

```text
/Elite Legacy Sources/2026-09-17/fleet-sketches/
```

Files:

```text
грузовики.docx
Полная_классификация_флота_3026.docx
фракции идиоты и их реакторы.docx
Cobra Mk3 геометрия.docx
Ship_Cobra MK1_Configuration.docx
Ship_Energy_Thermal_Model_Specification.docx
```

This note does not overwrite the source material. It classifies what each sketch is useful for and what must remain provisional.

---

## 1. `грузовики.docx`

### Useful content

Early world-logistics analysis based on a manually specified 14-faction map.

It identifies:

- capitals;
- agricultural colonies;
- resource colonies;
- military bases;
- scientific facilities;
- penal facilities;
- enclaves;
- quarantine/anomaly zones;
- hubs;
- long routes;
- conflict routes;
- last-mile orbital/surface logistics.

It also proposes five early freight categories:

```text
light shuttle          5-15 t
regional freighter    50-100 t
mainline hauler       200-500 t
bulk carrier          1000+ t
armored transport      50-150 t
```

Legacy order-of-magnitude counts:

```text
light shuttles          5,000-10,000
regional freighters       500-1,000
mainline haulers            50-100
bulk carriers                10-20
armored transports           30-50
```

Legacy estimated simultaneous carrying capacity: roughly 150,000 t.
Legacy annual turnover estimate: roughly 1.5-7.5 million t/year under the sketch's assumed trip frequency.

### Status

**REFERENCE, NOT CANONICAL COUNTS.**

The logic identifying different logistics layers remains useful. Absolute fleet numbers must now come from `FLEET_DEMAND_FROM_SETTLEMENT_SIM.md` after colony/economy generation.

### Known stale assumptions

The sketch was written against an older manually populated world and mixes route distance / range assumptions that predate the cleaner later jump/logistics model.

It also references a much larger Cobra geometry (`55 x 70 x 15 m`) than later dedicated Cobra documents. Do not use that dimensional statement as current geometry.

---

## 2. `Полная_классификация_флота_3026.docx`

### Useful content

This is the cleaner later fleet-role taxonomy and should be retained as the current starting point for civilian/logistics classification:

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

Important design assumptions recorded there:

```text
hyperjump practically instantaneous
logistical cost dominated by in-system movement to/from jump infrastructure
cruise approximately 0.03c-0.1c in the sketch
maneuverability constrained by acceleration / g limits
```

Cobra role mapping in that document:

```text
Cobra Mk I   -> FRN-U lower range
Cobra Mk III -> FRN-U / INT-A upper/light range
```

### Status

**TAXONOMY CANDIDATE / PROVISIONAL PHYSICAL NUMBERS.**

The role structure is worth keeping. Payload, speed, armament and exact size bands remain subject to current gameplay, physics and generated-economy validation.

---

## 3. `фракции идиоты и их реакторы.docx`

### Useful content

Faction-flavor concepts:

```text
Warpstanis
    reactor assembled from components of several civilizations
    unstable efficiency / one-shot overdrive idea

Restauros
    copied centuries-old plans without understanding them
    absurdly narrow thermal operating point

Novo-Fivaida
    deliberately unstable reactor as a religious trial

Selta-Verde
    living biological reactor whose performance depends on biological state
```

### Status

**FLAVOR / BLACK-COMEDY REFERENCE.**

Do not treat the literal reactor behavior as current physics. Preserve the cultural design principle:

> factions may solve the same engineering need in technically eccentric ways that reveal their history, ideology and industrial competence.

Any implementation must later pass the current energy/thermal and world-technology contracts.

---

## 4. `Cobra Mk3 геометрия.docx`

### Useful content

One detailed Cobra Mk III engineering sketch.

Recorded configuration includes approximately:

```text
21.5 m length
18.0 m width
7.2 m height
920 m3 internal volume
470 t full mass
100 t cargo
250 m3 cargo volume
4.5 GW nominal / 6.0 GW peak reactor
```

It also contains detailed power modes, propulsion, radiator and weapon assumptions.

### Status

**LEGACY TECHNICAL VARIANT.**

Useful as an engineering exploration, not current source of truth for the actual model asset or gameplay hull until reconciled against current Cobra geometry, asset editor data, physics and energy contracts.

---

## 5. `Ship_Cobra MK1_Configuration.docx`

### Useful content

Detailed Cobra Mk I engineering exploration:

- internal-volume allocation;
- mass breakdown;
- reactor and radiator model;
- combat power budget;
- weapon thermal logic;
- plasma-shield concept;
- propulsion modes;
- landing limitations.

### Internal conflict to preserve, not silently fix

The document itself contains more than one geometry/version.

Early block:

```text
15.1 m length
24.8 m width
5.5 m height
```

Later `final configuration` block:

```text
16.5 m length
14.0 m width
5.8 m height
```

Both use roughly 500 m3 internal volume and approximately 182 t full mass in that sketch.

### Status

**MULTIPLE LEGACY VARIANTS / REQUIRES RECONCILIATION.**

Do not choose one geometry automatically. Actual current mesh/asset data should win when this is revisited.

---

## 6. `Ship_Energy_Thermal_Model_Specification.docx`

### Useful content

This is conceptually different from the other files: it is a deterministic input/output specification rather than one hull sketch.

Inputs include:

```text
L / W / H
radiative area fraction
radiator panel area
thermal capacity
maximum temperature
emissivity
reactor nominal / peak electrical power
heat-generation coefficient
pump power
maximum heat-transfer capacity
```

Core equations include:

```text
ellipsoid surface approximation
effective radiative area
radiator panel count
reactor heat generation
Stefan-Boltzmann radiation
heat-transfer bottleneck
steady-state temperature
optional thermal dT/dt
```

Outputs include:

```text
surface / radiator area
panel count
power consumption by mode
power reserve
heat input / radiation / transfer
steady-state temperature
temperature margin
```

### Status

**HIGH-VALUE TECHNICAL REFERENCE / REQUIRES PHYSICS REVIEW.**

The architecture is useful: ship generation should derive thermal viability from deterministic inputs rather than inventing radiator size independently.

The exact formulas/coefficients must be reconciled with the current ship physics implementation before becoming a runtime contract.

---

## 7. Current normalization decision

Keep three layers separate:

### A. World-generated fleet demand

Canonical design direction:

```text
FLEET_DEMAND_FROM_SETTLEMENT_SIM.md
```

This determines why ships exist and how many are required.

### B. Fleet role taxonomy

Current best source seed:

```text
Полная_классификация_флота_3026.docx
```

The role names may evolve, but absolute counts do not belong here.

### C. Hull engineering

Cobra and thermal documents are engineering references. They must be reconciled against current meshes, asset metadata and current physics before any numbers are promoted to canon.

## 8. Key dependency

Future ship creation should run approximately:

```text
generated colony / economy demand
        -> required economic role
        -> faction / manufacturer / era
        -> hull family
        -> payload / endurance / protection requirements
        -> geometry / mass budget
        -> reactor / propulsion budget
        -> thermal viability
        -> maintenance / cost / production feasibility
        -> actual manufactured stock over history
```

This is preferable to designing a fleet catalogue first and then inventing traffic to justify it.