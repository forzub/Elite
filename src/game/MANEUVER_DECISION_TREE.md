# Maneuver decision tree — ownership above Navigation v2

**Status:** architecture contract / active Stage-12 integration
**Updated:** 2026-09-18 Europe/Kyiv
**Related:** NAVIGATION_WORLD_V2.md, NAVIGATION_PLANNING_ARCHITECTURE.md, EMERGENCY_CONTACT_SEVERITY_MODEL.md, PILOT_SKILL_MODEL.md

## Core invariant

~~~text
no collision-free proof != no navigation command
no global route != stop by default
~~~

A missing safe route is a decision state, not permission for Navigation to turn control off.

Navigation owns geometry, reachability and physically truthful maneuver products. A higher game/control layer owns the decision about which product to execute under mission, combat and damage context.

~~~text
world / vehicle / damage / threat truth
            |
            v
navigation candidate generation
            |
            +-- global corridor / portals
            +-- bounded line-of-sight bypass
            +-- precision passage
            +-- emergency contact candidates
            +-- stop / brake / reverse candidate when physically available
            |
            v
ManeuverDecisionController
            |
            v
selected maneuver directive / candidate
            |
            v
NavigationRuntimeControlBridge
    -> PilotSkillExecutor
    -> flight control
    -> authoritative physics
    -> collision / damage
~~~

Navigation must not decide that a radar is expendable, that keeping speed matters more than paint damage, or that presenting a smaller silhouette to a pursuer is worth a tighter passage. Those are game/tactical decisions.

## Decision hierarchy

### 1. Global route

Used when the problem contains topology: maze, asteroid ravine, station interior, wreck interior, region graph, portal sequence or system route.

It answers which sequence of regions/passages can eventually reach the goal. It does not continuously solve centimeter-level flight to the final point.

### 2. Local bounded visibility

Default ordinary movement.

~~~text
current A -> accepted target B
    direct hull-sized corridor inside physical horizon clear?
        yes -> continue toward B
        no  -> smallest safe bounded deflection
next update:
    test B directly first again
~~~

The horizon contains only physically relevant near future: latency travel + braking distance + turn-distance allowance + safety margin.

### 3. Precision passage

Used when the selected route requires a specific constrained opening: door, tunnel, hangar mouth, docking corridor, wreck breach, debris aperture or ravine throat.

The passage product may require entry position, entry normal/tangent, velocity-vector alignment, vehicle-forward alignment, roll, cross-track speed, transit speed and exit state.

A drone entering a wreck calmly is still a precision-passage problem even when there is no emergency and the desired speed is low.

### 4. Emergency / contact-expected passage

If collision-free passage cannot be achieved before the entry/contact event, control must remain alive.

Existing accepted components already provide lower-level physical pieces:

~~~text
EmergencyPassageMitigator
    reachable best-effort attitude
    braking opportunity
    passage-axis intent
    explicit contactExpected

EmergencyContactSeverityScorer
    glancing vs normal contact
    contact-point relative velocity including omega x r
    peak closing normal speed
    energy / momentum proxies
    incidence angle
    geometry deficit
    useful progress
~~~

These components do not know whether a radar, solar panel, armor plate, reactor, cockpit, engine or mission payload is strategically expendable. That semantic damage valuation belongs above Navigation.

### 5. ManeuverDecisionController

This is the owner of tactical choice. It consumes candidate annotations from several authorities:

~~~text
Navigation
    collision-free / contact-expected
    route progress
    minimum clearance
    time / exit state
    required attitude / control authority
    emergency contact severity

Damage / structural
    predicted contacted semantic component
    expected detach / breach / loss
    criticality
    mission-specific component value
    sacrificial / expendable value

Threat / combat
    threat direction
    line of fire / cover
    projected silhouette toward threat
    exposure duration
    expected threat intensity

Mission
    must reach / may abort
    deadline
    retrieval / docking / escape / attack
    payload constraints

Pilot / doctrine
    risk preference
    comfort preference
    skill execution profile
~~~

## Candidate data, not one magic score

Do not collapse everything into a single weighted scalar before hard constraints are handled.

Each candidate should retain at least:

~~~text
candidate id / maneuver family
collisionFree
contactExpected
progressesObjective
canStop / canReverse
timeToObjective / timeToCover
entrySpeed / exitSpeed
minimumClearance
escapeReserve / returnability

peakClosingNormalSpeed
normalImpactEnergyProxy
incidenceAngle

criticalDamageRisk
missionDamageCost
expendableDamageCost

projectedThreatExposure
projectedAreaTowardThreat integral

fuel / delta-v where relevant
~~~

The damage layer may annotate a predicted contact with component semantics. Outer armor/fairing may be cheap; radar/antenna often expendable; cargo or mission sensors context-dependent; maneuver thrusters expensive; main drive, reactor/fuel tank and cockpit/crew volume critical.

## Projected silhouette under fire

For combat escape, orientation has tactical value beyond passage fit.

~~~text
exposure ~= integral(
    projectedAreaTowardThreat(t)
    * threatIntensity(t)
    * lineOfFireVisibility(t)
    dt
)
~~~

A smaller silhouette can justify a different roll/yaw attitude even when both trajectories are geometrically safe. Passage fit and threat silhouette remain separate facts; the decision owner trades them according to doctrine.

## Movement doctrines

### Rational

Purpose: finish the task while preserving ship condition and future options.

~~~text
1. avoid catastrophic loss
2. collision-free candidate if physically available
3. preserve mission-critical systems
4. preserve escape reserve / returnability
5. reduce threat exposure
6. reduce speed when doing so creates a materially safer solution
7. minimize expendable damage
8. time / fuel / comfort
~~~

Braking is a tool, not a failure. If stopping safely is better than unavoidable damage and the mission permits stopping, a stop/brake candidate may win.

### Precision / retrieval

Purpose: enter a required constrained place deliberately. Examples include a drone entering a broken ship through a breach, a repair craft entering debris, or a ship threading a maintenance corridor.

Typical behavior: slow down -> acquire entry frame -> align pose/velocity -> transit with large control margin. If the objective requires entry and no collision-free pose exists, escalate to the emergency/contact-expected branch rather than silently abandoning control.

### Extreme

Purpose: survive / escape / intercept when time dominates comfort and moderate damage.

~~~text
1. avoid catastrophic destruction / crew loss
2. make required progress / reach cover
3. minimize time exposed
4. preserve or increase useful speed
5. minimize silhouette to active threat
6. prefer glancing contact
7. sacrifice expendable structure before critical systems
8. preserve future control authority when possible
~~~

An extreme maneuver may deliberately accept scraped armor, torn fairing, lost radar/antenna, detached non-critical appendage or a survivable minor breach if the alternative is materially worse.

Faster is better in Extreme means useful progress/escape speed, not blindly max throttle into a normal impact.

### Combat escape

A specialization of Extreme where threat exposure is explicit. The decision can prefer a narrow silhouette to a pursuer, rapid cover acquisition, high exit speed and short line-of-fire time even when that costs clearance or expendable structure.

## When the ship does not fit and there is no time

~~~text
collision-free fit/reorientation still reachable?
    yes -> use it

can braking create time for a collision-free fit?
    yes and doctrine permits slowing -> brake / reorient

another free local passage exists inside bounded search?
    yes -> use it

global topology provides another route in available time?
    yes -> use it

contact now unavoidable?
    -> DO NOT HOLD
    -> generate bounded emergency entry/contact candidates
    -> favor reachable attitudes that reduce geometry deficit
    -> rank physical contact severity
    -> annotate structural/component loss
    -> annotate threat exposure and time
    -> ManeuverDecisionController selects according to doctrine
    -> execute selected contact-expected intent
~~~

Contact-expected is never mislabeled safe.

## No-route fallback tree

~~~text
GLOBAL ROUTE FOUND?
    yes
      -> follow next corridor / portal
    no
      -> is bounded local physical visibility progress possible?
           yes -> continue locally and request global replan
           no
             -> can reverse/backtrack to known viable state?
                  yes -> candidate
                  no
                    -> can stop safely and does doctrine allow it?
                         yes -> candidate
                         no / must progress
                           -> precision/emergency candidate generation
                           -> least-bad physically reachable contact path
~~~

NoRoute is not an executable maneuver. It is a request for the decision layer to choose among fallback candidates.

A planner may output no collision-free route proven. It may not translate that fact directly into turn control off.

## Escape reserve / viability

Before committing into a constrained region, candidate metadata should say whether at least one continuation exists after the maneuver: forward continuation, alternate portal, reverse/backtrack, safe stop, or accepted emergency continuation.

Rational doctrine normally rejects a candidate that consumes all escape reserve. Extreme doctrine may deliberately consume it when the mission/threat context justifies the commitment.

This distinguishes we were trapped by changing circumstances from the autopilot casually flew into a dead end and then gave up.

## Cinematic navigation principle

Cinematic behavior must emerge from truthful decisions, not from random stunt selection.

Good outcomes are naturally spectacular when the system combines real inertia, limited thrust, late hazards, narrow geometry, orientation-dependent fit, moving obstacles, threat exposure, semantic damage, detachable structure and pilot skill/latency.

Examples that should emerge without scripted cheating:

~~~text
roll to fit a breach while still sliding sideways
scrape an expendable antenna instead of striking the cockpit
flip-and-burn at the last useful moment
dive behind wreckage with minimum silhouette to a pursuer
thread moving debris at high speed because slowing increases combat exposure
crawl a repair drone through the same debris at 0.5 m/s when there is no urgency
~~~

The same world geometry can therefore produce radically different maneuvers under Rational, Precision and Extreme doctrine.

## Implementation order

1. keep bounded-visibility local movement as default ordinary transit;
2. finish oriented tunnel/portal capture;
3. expose maneuver candidate products instead of auto-translating failure to hold;
4. add ManeuverDecisionController above Navigation;
5. connect accepted EmergencyPassageMitigator and EmergencyContactSeverityScorer as contact-expected candidate sources;
6. add structural semantic-loss annotations;
7. add threat projected-area/exposure annotations;
8. add escape-reserve / returnability annotation;
9. run live fixtures for Rational, Precision/Retrieval, Extreme and CombatEscape;
10. derive manual HUD guidance from the same selected maneuver product.

## Acceptance invariants

~~~text
no safe route != no command
no global route != automatic hold
contact-expected != safe
navigation never assigns semantic component value
damage system never invents route geometry
threat assessor never mutates physics
decision controller never invents vehicle authority
pilot skill changes execution quality, not geometry truth
physics remains final contact/damage authority
~~~


## Control-law-specific recovery

Detailed authority: `src/game/navigation/CONTROL_LAW_MANEUVER_MODEL.md`.

The maneuver selector now filters candidates against the active
`LocalFlightControlLaw` before doctrine ranking:

```text
Any
AssistedOnly
NewtonianOnly
```

Examples:
- `NewtonianFlipAndBurn` cannot be selected in Assisted;
- wider visibility recovery/backtrack may be valid for either law;
- a control-law-specific generator is responsible for producing only physically
  meaningful candidates.

The ordinary visibility fan ending at 75 degrees is not a terminal condition.
LocalAvoidance now publishes `ordinarySearchExhausted`; runtime composition
publishes `ordinaryVisibilitySearchExhausted`. That signal means:
"ordinary progress-preserving local steering is exhausted; request recovery
candidate generation."

It does not mean "disable control" and does not mean "the vehicle cannot turn
farther."
