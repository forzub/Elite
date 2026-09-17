# Generation-Ship Social Adaptation and Colony Culture

**Status:** design direction / historical social-simulation contract draft  
**Updated:** 2026-09-17 Europe/Kyiv  
**Canonical branch:** `main`

## 1. Purpose

This document restores and formalizes the deeper social layer of the project's earlier generation-ship history model.

The core idea predates the `Dead Reckoning: The Long Drift` reference and is deliberately richer than a generic statement that "culture drifts over time".

The intended causal chain is:

```text
launch-population mentality
        +
ship doctrine / governance
        +
physical and social conditions of the voyage
        |
        v
multi-generation adaptation inside a finite artificial world
        |
        v
arrival mentality / institutions / habits
        +
planetary environment and landing outcome
        |
        v
colony architecture + political regime + social norms
        |
        v
rate and style of expansion beyond the initial sealed habitat
```

The colony is therefore not generated from a generic cultural template. It is the historical descendant of a specific population that spent generations adapting to a specific ship.

## 2. Important scientific constraint

Isolation and confinement are strong pressures, but they do **not** imply one inevitable political outcome.

Relevant isolated/confined-environment literature supports effects such as stress, sleep disruption, interpersonal tension, dependence on group cohesion, need for privacy, sensory monotony, adaptation to small-group norms, and the importance of mission context. It does not justify a deterministic rule such as:

```text
long confinement -> dictatorship
```

The simulation should therefore model pressures and adaptive responses, not hard-code political labels as direct environmental effects.

## 3. Launch-population mentality is a first-class initial condition

A mission does not depart with "generic humans".

The launch population has a social prior inherited from its source society and from the selection process used to choose colonists.

Suggested aggregate dimensions:

```text
social_trust
hierarchy_acceptance
individual_autonomy
collective_duty
rule_internalization
dissent_tolerance
privacy_expectation
family_collectivism
institutional_trust
technical_authority_trust
risk_tolerance
conflict_avoidance
resource_sharing_norm
status_rigidity
adaptability
mission_identity_strength
```

These are not moral scores. They describe how a population is likely to organize under pressure.

Selection also matters. A mission may intentionally recruit disciplined specialists, families, political loyalists, religious communities, volunteers, refugees, corporate employees, military-trained personnel, or a broad civilian cross-section. That changes the prior before the ship even launches.

## 4. The ship is not merely a vehicle; it becomes the entire experienced world

For Earth-born launch generations, the ship is initially understood as an enclosure inside a larger universe.

For people born and raised aboard it, the psychological relationship is different. The ship is the only directly experienced habitable world.

Critical environmental properties include:

```text
finite visible spatial envelope
no physically accessible external world
no spontaneous escape from the social group
survival dependence on shared machinery
hard resource accounting
high consequence of negligence
limited privacy
repeated routes / rooms / visual horizons
artificial day-night cycle
artificial climate
managed reproduction / population limits
continuous maintenance obligations
known physical boundaries to exploration
```

This distinction is central to the project.

A person who has never experienced an open horizon, uncontrolled weather, wilderness, or the possibility of simply walking away from the settlement does not merely have "lower morale". Their intuitive model of what a normal human environment is may be different.

## 5. Closed-world adaptation

The simulation should track adaptation to the finite artificial environment separately from generic stress.

Possible derived dimensions:

```text
enclosure_normalization
open_space_anxiety
external_environment_distrust
system_dependency_acceptance
maintenance_discipline
resource_accounting_internalization
privacy_need
crowding_tolerance
rule_visibility_acceptance
collective_risk_awareness
unsupervised_behavior_tolerance
novelty_seeking
exploration_drive
```

Important: these variables can move in different directions.

For example, long confinement may simultaneously increase:

- tolerance for physical enclosure;
- expectation that air/water/energy are collectively managed;
- sensitivity to others violating safety rules;

while also increasing:

- desire for privacy;
- desire for novelty;
- hostility toward intrusive authority if institutions are perceived as illegitimate.

There is no single `confinement_authoritarianism` meter.

## 6. Why institutions may become stricter aboard ship

A generation ship contains unusually strong structural incentives for procedural discipline:

```text
one person's fire can kill thousands
one contaminated loop can threaten everyone
one neglected maintenance task can become a mission failure
population cannot freely leave
critical resources are measurable and finite
```

This can make some rules socially legitimate even in populations with strong individual-autonomy values.

The relevant simulation question is therefore not simply "how authoritarian is the regime?" but:

```text
Which areas of life are treated as collective survival domains?
Which areas remain personal?
Who is trusted to make emergency decisions?
How reversible are emergency powers?
How much dissent is tolerated when safety is at stake?
How transparent are resource and risk decisions?
```

Two societies can enforce equally strict airlock and water rules while having radically different political systems.

## 7. Institutional-path dependence

Emergency structures can persist after the emergency that created them.

Examples:

```text
engineering council gains emergency authority
    -> performs well
    -> becomes permanent technocratic chamber

captain receives wartime powers
    -> succession rule hardens
    -> hereditary / oligarchic command culture appears

resource allocation becomes algorithmic
    -> high trust in system
    -> strong administrative/AI governance after arrival

crew assemblies resolve repeated crises
    -> deliberative governance becomes culturally sacred
```

The simulation should generate institutions from repeated successful/failed responses, not from one random ideology event.

## 8. Generational discontinuity

Each generation inherits both institutions and a different lived baseline.

```text
Generation 0
remembers open Earth / planets / large cities

Generation 1
heard direct stories from Earth-born parents

Generation 2+
knows open worlds mostly through recordings, education and simulation
```

This changes how the same rule is perceived.

For example, a mandatory air/resource quota may feel temporary and oppressive to the launch generation but simply normal to a generation that has never experienced unmetered access to air, land or water.

The social simulator should therefore distinguish:

```text
memory_of_open_world
cultural_memory_of_origin
personal_experience_of_origin
ship_native_fraction
```

## 9. Arrival is a psychological discontinuity, not only a logistics event

For ship-native colonists, a planetary environment can be profoundly alien even if biologically habitable.

Possible pressures after landing:

```text
open horizon / no visible enclosing wall
weather
uncontrolled sound and movement
large distances
unbounded walking range
irregular natural terrain
real sky / stars / clouds
unmetered-looking resources
new pathogens / ecology
loss of constant machine-mediated environmental control
```

Some colonists may experience liberation; others may experience fear, disorientation or distrust.

This should influence the speed with which the colony leaves sealed habitats.

## 10. Dome exit / environmental expansion model

The first settlement should not automatically expand outdoors as quickly as technology allows.

Expansion is constrained by both physical safety and cultural readiness.

Suggested factors:

```text
planetary_habitability
outside_environment_risk
life_support_reliability
population_open_space_anxiety
external_environment_distrust
exploration_drive
institutional_risk_tolerance
leadership_legitimacy
resource_pressure_inside_habitat
availability_of_remote_robots
recent_outdoor_accidents
```

Possible outcomes:

### Fast exterior expansion

A population with high adaptability, high exploration drive and trusted institutions may leave the initial habitat quickly once objective safety is demonstrated.

### Controlled staged expansion

A procedure-oriented society may expand through increasingly large controlled zones:

```text
ship / sealed landing habitat
 -> connected pressure domes
 -> fenced environmental zones
 -> supervised external work
 -> permanent open settlements
```

### Prolonged enclosure

A population strongly adapted to ship life may retain enclosed architecture long after the outside environment becomes objectively safe.

This can create cities whose form is culturally inherited from the generation ship rather than technically necessary.

## 11. Colony architecture is social evidence

Settlement geometry should emerge partly from mentality.

Examples:

```text
high privacy need
    -> smaller residential units / acoustic isolation

high collective identity
    -> large shared commons / communal dining

high external distrust
    -> compact sealed structures / controlled gateways

high exploration drive
    -> distributed outposts / frontier settlements

strong hierarchy
    -> spatially explicit administrative core / controlled access

strong local autonomy
    -> modular semi-independent districts

ship-normalized population
    -> corridors, decks, compartmentalized zoning reproduced on land
```

Thus architecture can tell the player something about settlement history without a lore dump.

## 12. Political regime must be emergent and multidimensional

Do not store one scalar `authoritarianism` and derive everything from it.

At minimum distinguish:

```text
decision_centralization
legal_formalism
emergency_power_scope
leadership_turnover
information_transparency
private_life_autonomy
resource_control
mobility_control
dissent_tolerance
institutional_legitimacy
coercion_level
social_welfare_obligation
```

A regime may be highly centralized and rule-bound yet maintain strong welfare guarantees and substantial private-life autonomy. Another may be formally decentralized while being coercive through clans or corporations.

This is important for avoiding cartoon political cultures.

## 13. Source-society inheritance versus ship adaptation

Arrival culture can be thought of as a transformation, not replacement:

```text
arrival_culture = F(
    source_culture,
    colonist_selection,
    mission_doctrine,
    ship_layout,
    voyage_duration,
    awake/cryo policy,
    scarcity history,
    disaster history,
    leadership history,
    generational turnover,
    institutional success/failure,
    technical regression
)
```

The same voyage environment can therefore produce different outcomes from different initial populations.

Likewise, similar source populations can diverge sharply because their ships experience different crises and leadership histories.

## 14. Colony feedback after landing

The post-landing culture continues to evolve from the inherited ship culture.

Important feedback loops:

```text
fear of exterior
 -> delayed expansion
 -> crowding inside habitat
 -> stronger regulation
 -> exterior feels even more exceptional

successful exploration
 -> higher environmental confidence
 -> more autonomous outposts
 -> weaker central physical control

repeated external disasters
 -> return to sealed architecture
 -> stronger safety bureaucracy

resource abundance
 -> weakens ship-era scarcity norms

resource scarcity
 -> reinforces rationing / centralized allocation traditions
```

The first 50–100 years of the colony may therefore matter as much as the voyage itself.

## 15. Daughter expeditions inherit a transformed culture

A daughter ship launched from a mature colony should inherit the colony's evolved social state, not the original Earth template.

Therefore procedural settlement creates cultural phylogeny:

```text
Earth source culture
        |
        v
Generation Ship A adaptation
        |
        v
Colony A culture
        |
        +--> Daughter A1 -> Colony A1 culture
        |
        `--> Daughter A2 -> Colony A2 culture
```

Cultural regions in the game world can therefore be genealogical as well as geographical.

## 16. Simulation implementation guidance

The background world generator should not simulate individual psychology for every colonist.

Use cohort/population state plus institutional state:

```text
PopulationMentality
InstitutionalState
EnvironmentalAdaptation
CulturalMemory
PopulationCohorts
```

Update them through event-driven pressures and response rules.

Named historical figures may temporarily modify institutional choices, but no single leader should directly overwrite population mentality without mechanisms and time.

## 17. Relationship to Dead Reckoning reference

`Dead Reckoning` is useful for:

- explicit generations;
- persistent social/technical drift;
- coupled ship systems;
- long-term consequences.

Elite's intended social simulation goes further in a different direction:

- source-population mentality matters before launch;
- the absence of an accessible external world is itself a formative environment;
- ship-born generations normalize finite artificial space;
- arrival at an open planet produces a second adaptation shock;
- settlement architecture and political institutions derive from that history;
- willingness to leave the dome / sealed city is socially simulated, not only technologically gated;
- daughter colonies inherit transformed cultural state.

This social layer should be treated as a first-class input to `GENERATION_SHIP_SETTLEMENT_SIM.md`, not as flavor text added after population and engineering simulation.