# Generation-Ship Population Selection and Social Composition

**Status:** design direction / historical social-generation contract draft  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`

## 1. Principle

A generation ship does not launch with a statistically random sample of its source country or civilization.

Its population is produced by the institution that pays for, owns, regulates and politically legitimizes the mission.

Therefore the initial social structure should be generated from:

```text
source society
+ sponsor / owner
+ financing model
+ legal regime
+ labor model
+ recruitment channel
+ prestige / ideological goals
+ demographic pressure
+ mission risk
= launch population composition
```

This starting composition is a first-class input to `GENERATION_SHIP_SOCIAL_ADAPTATION.md`.

## 2. Social class is structural, not cosmetic

Track population by functional/social cohorts rather than one generic colonist count.

Possible cohort dimensions:

```text
political / administrative elite
capital owners / patricians
scientific / academic elite
engineering specialists
contract workers
military / security
service workers
religious / ideological cadre
selected families
volunteers
refugees / disaster-zone recruits
debtors / convicts / coerced labor
informal / undocumented passengers
```

The exact categories vary by mission. They should not be forced into one universal caste table.

## 3. Historical Elite examples already established

### TIANHE-1 — Chinese state project

Launch population: 35,000.

```text
8,000  party / administrative / academic families
19,000 contract specialists
8,000  "volunteers" from overpopulated provinces
```

The mission begins with a large organized administrative cadre, a numerically dominant technical/contract workforce, and a politically weaker mass cohort.

### HERACLES-1 — Mediterranean consortium

Launch population: 12,000.

```text
3,000  patrician / investor class
6,500  contract workers
2,500  convicts, debtors and irregular migrants from the Maghreb
```

The vessel therefore begins with an explicit class hierarchy and a large subordinated labor population.

### GREEN-ARK-1 — ecological movement / foundations

Launch population: 12,000.

```text
2,000  activist-scientists
7,000  engineers, bionics specialists and agronomists
3,000  movement followers
```

The dominant numerical cohort is technical rather than wealthy or administrative, with a strong ideological mission identity.

### FINAL-ARK-1 — late Earth / UN emergency project

Launch population: 20,000.

```text
5,000  patrician / privileged cohort
10,000 contract workers
5,000  volunteers / recruits from disaster zones
```

This is a deliberately unstable late-civilization mixture: privilege, indispensable labor and people with little remaining terrestrial alternative.

### NOMAD-1 — private billionaire consortium

Approximate population: 300.

The population is not a colony sample at all; it is a luxury micro-society dominated by ultra-wealthy owners/patricians plus servants and support staff. The mission's social pathology must therefore emerge from extreme class imbalance and leisure rather than mass labor or demographic pressure.

### UBUNTU-1 — class/ethnic-political fault line

Launch population: 25,000.

Existing history includes a strong conflict between African contract workers and European elites, eventually contributing to an uprising with about 1,500 deaths.

This is an example where launch composition directly creates a later historical fault line.

## 4. Country is not itself a personality score

Do not encode:

```text
country X -> authoritarian
country Y -> individualist
```

Instead encode institutions and selection mechanisms.

A state-led mission, corporate mission, religious mission, refugee ark and billionaire yacht built in the same country can launch radically different populations.

Country / civilization matters through:

```text
available institutions
class structure
labor law
family structure
state capacity
prestige incentives
migration pressure
political legitimacy
technical education base
religious / ideological landscape
```

The actual ship population is then filtered by the sponsor's selection policy.

## 5. Dominant class matters in several different ways

Do not equate `largest cohort` with `ruling cohort`.

Track at least:

```text
numerical_dominance
institutional_control
technical_dependency
security_control
resource_ownership
reproductive_share
cultural_prestige
```

A small engineering cohort may become politically powerful because nobody else can maintain the reactor.

A wealthy patrician cohort may own the mission legally but become socially irrelevant after several generations if capital titles no longer correspond to useful functions.

A large labor cohort may be numerically dominant but politically weak at launch and later overturn the structure.

## 6. Selection influences later demography

Initial cohorts should affect:

```text
marriage / family network
fertility policy
age structure
sex ratio
genetic diversity
education inheritance
occupation inheritance
status mobility
conflict probability
leadership recruitment
```

Therefore a mission's class composition should not disappear after the first simulation tick.

## 7. Interaction with confinement

The correct causal model is:

```text
launch social composition
        +
source mentality
        +
ship governance
        +
closed-world adaptation
        +
crises / successful institutions
        ->
arrival social order
```

The ship environment transforms the starting society; it does not erase the starting society.

This is why two technically identical ships launched by different sponsors can found very different colonies.

## 8. Procedural generation use

For background world generation:

1. choose sponsor / mission origin from historical timeline;
2. derive financing and political model;
3. generate cohort proportions within plausible bounds;
4. derive initial mentality / legitimacy relationships;
5. simulate voyage adaptation and cohort reproduction;
6. pass resulting structure into colony founding;
7. allow later daughter expeditions to select from the already transformed colony population.

This produces cultural and class phylogeny together with geographic settlement phylogeny.