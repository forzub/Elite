# Legacy Universe / Generation-Ship Import — 2026-09-17

**Status:** preserved source index / NOT YET CANONICALIZED  
**Canonical branch:** `main`  
**Purpose:** preserve the older worldbuilding corpus before normalization so no ship, colony, event, contradiction or discarded branch is lost.

## 1. Source snapshots preserved

Two user-provided archives were imported as raw historical source material:

```text
Universe.7z
SHA-256: 5e162a144c1cdfeab21cdadef73611eb2eec489241d9304e2b5654ce49b8633c
size: 32,773 bytes

generation ships(20260917-055332).zip
SHA-256: 9cb337c6ab0b73d8980a713ce166b6ef05d6afeefc690f63efdd314254fa48cf
size: 282,430 bytes
```

Persistent raw copies and extracted source dumps are stored in the user's Library under:

```text
/Elite Legacy Sources/2026-09-17/
```

Contents there:

```text
Universe.7z
generation ships(20260917-055332).zip
RAW_UNIVERSE_SOURCE_DUMP.txt
RAW_GENERATION_SHIPS_SOURCE_DUMP.md
manifest.json
```

The raw dumps preserve source-file boundaries. `manifest.json` contains per-file SHA-256 hashes and a machine-generated ship index.

**Do not edit these snapshots as canon.** Normalize into separate design/canon documents.

## 2. Source-family interpretation

The two archives clearly represent different iterations of the same worldbuilding work.

### A. `Universe.7z`

Contains six older world-level files:

```text
actors.txt
Earth@generation-des(physic).txt
Earth@generation-des.txt
Earth@generation.txt
nodes.txt
UN_tracker.txt
```

This branch contains:

- early ship/fate lists;
- physical ship concepts;
- later political nodes / civilizations;
- 3024-era populations and spheres of influence;
- alternate/older mission IDs and outcomes.

It is valuable especially for **world-state and civilization ideas**, but must not automatically override later detailed ship dossiers.

### B. `generation ships(20260917-055332).zip`

Contains one master file plus 32 detailed Markdown files representing 31 numbered entries, with a numbering collision at `13`:

```text
13. NEW-HELLAS-1.md
13. RESOURCER-1.md
```

The detailed dossiers are generally much richer than the older `Universe` generation lists and include:

- launch politics;
- sponsor motives;
- class composition;
- ship architecture;
- cryo / awake policy;
- multi-generation mentality change;
- colony social development;
- revised engineering discussions;
- mystery/horror branches;
- contradictory alternatives from iterative design discussions.

This archive should be treated as the primary **raw design-work corpus**, not yet as a cleaned canon.

## 3. Detailed ship index recovered

| # | ID | Start | Population / crew | Primary target / type |
|---:|---|---:|---:|---|
| 1 | MAYFLOWER-2 | 2088 | 25,000 | Tau Ceti e |
| 2 | SVAROG-1 | 2095 | 15,000 | Proxima Centauri b |
| 3 | TIANHE-1 | 2098 | 35,000 | Gliese 832 c |
| 4 | UBUNTU-1 | 2105 | 25,000 | Gliese 667 Cc |
| 5 | HERACLES-1 | 2112 | 12,000 | Lalande 21185 b |
| 6 | CELESTIAL-DESTINY-1 | 2125 | 25,000 | Epsilon Eridani b |
| 7 | GREEN-ARK-1 | 2135 | 12,000 | GJ 832 c |
| 8 | DHARMA-1 | 2140 | 30,000 | Delta Pavonis b |
| 9 | IRON-SHIELD-1 | 2150 | 12,000 | Wolf 359 asteroid belt |
| 10 | AL-QUDS-1 | 2160 | 18,000 | Lalande 21185 b |
| 11 | NEXT-1 | 2175 | 6,000 | Gliese 876 c |
| 12 | EUROPA-1 | 2250 | 12,000 | GJ 581 g |
| 13A | NEW-HELLAS-1 | 2260 | 15,000 | HD 20794 d |
| 13B | RESOURCER-1 | 2260 | 10,000 | 61 Cygni asteroid belt |
| 14 | NEW-JERUSALEM-1 | 2270 | 12,000 | GJ 667 Cc |
| 15 | UHURU-2 | 2280 | 30,000 | HD 40307 g |
| 16 | ODYSSEY-1 | 2290 | 15,000 | HIP 116454 b |
| 17 | NIRVANA-1 | 2300 | 3,500 | Kepler-62 f |
| 18 | DHARMA-2 | 2400 | 25,000 | HD 20794 d |
| 19 | AMAZONIA-1 | 2320 | 15,000 | HD 69830 d |
| 20 | FREE-WORLD-1 | 2450 | 12,000 | GJ 581 g |
| 21 | CELESTIAL-DESTINY-2 | 2480 | 25,000 | HD 40307 g |
| 22 | IRON-SHIELD-2 | 2355 | 3,000 | intra-system Wolf 359 asteroid expedition |
| 23 | GAIA-2 | 2500 | 12,000 | TRAPPIST-1 e |
| 24 | KEPLER-1 | 2550 | 10,000 | Kepler-442 b |
| 25 | NOMAD-1 | 2485 | 300 | endless / luxury cruise concept |
| 26 | FINAL-ARK-1 | 2500 | 20,000 | HD 40307 g |
| 27 | LEGACY-1 | 2520 | 0 nominal colonists | GJ 1214 b / archive mission |
| 28 | DEEP-SEEK-1 | 2690 | 14,000 | HD 40307 g |
| 29 | HOMEWARD-1 | 2555 | 2,000 | intra-system Epsilon Eridani expansion |
| 30 | FAREWELL-1 | 2599 | 500 | wandering artistic caravan / GJ 581 g reference |
| 31 | HYPERION-1 | 2750 | 200 crew | experimental hyperjump to Tau Ceti and return |

**Ordering is not chronological in the raw archive.** Keep IDs and dates separate from display order until cleanup.

## 4. Important recovered story: Citadel / IRON-SHIELD-2

This is the previously remembered asteroid-belt civilization that lost about 15% of its population.

### Pre-event state

`IRON-SHIELD-1` founded the Citadel civilization in the Wolf 359 asteroid belt. The later detailed branch describes Citadel as harsh and security-oriented, but **not yet fully isolationist/paranoid**.

Approximate population by 2355:

```text
~20,000
```

### IRON-SHIELD-2 expedition — 2355

Citadel launched an intra-system expansion mission to a neighboring asteroid:

```text
colonists: 3,000
travel: ~3–4 weeks
population share: 15%
```

Composition in the detailed source:

```text
800  patricians / command and clan leadership
1,800 contract workers / military engineers / miners / cybernetics specialists
400  convicts / service labor
```

The mission reached asteroid #2 and began reconnaissance.

Last distorted transmission in the raw dossier:

```text
“…не астероид… структура… она нас зовет… мы идем… да пребудет с нами…”
```

Communication ended.

A Citadel search mission arrived roughly a month later and found:

```text
no ship
no landing traces
no debris
no 3,000 colonists
```

### Historical consequence

The raw dossier explicitly makes this disappearance a **causal turning point**, not just a spooky anecdote:

```text
15% of population disappears without a visible enemy
        -> collective trauma
        -> expansion stops
        -> censorship / secrecy hardens
        -> reconnaissance + defense spending explodes
        -> outsiders become possible suspects
        -> paranoia becomes state ideology
        -> Citadel becomes fortress civilization
```

This is one of the strongest pieces of the old setting and should be preserved unless deliberately rewritten.

### Canon status

```text
EVENT CORE: STRONGLY PRESERVE
EXACT EXPLANATION: UNRESOLVED
ALIEN / ANCIENT / FALSE-FLAG THEORIES: NOT CANON YET
```

The disappearance should remain mysterious during normalization. The old file contains several mutually exclusive theories; none should be promoted automatically.

## 5. Other high-value mystery / lost-ship material recovered

The corpus contains several already-developed mystery branches that should be indexed before rewriting:

### UBUNTU-1

Existing detailed branch includes severe class/political conflict and a later lost-ship / unknown-signal storyline. Preserve separately from any simplified `Universe` fate text.

### GREEN-ARK-1

Older Universe branch contains an intact empty ship at an apparently successful ecological target and an anomalously developing biosphere. This is an older branch, not automatically current canon, but it is a valuable mystery seed.

### NIRVANA-1

Older branch describes a navigation failure and intermittent beacon detections from impossible/different directions, with competing mundane and mystical explanations.

### AMAZONIA-1 / isolated colony branch

Older Universe material includes a colony that loses contact, believes itself to be the last human society and evolves for centuries in isolation. Preserve as a social-world seed even if reassigned to a different ship/system.

### FREE-WORLD / FREE-VENTURE branch

Older and newer source families use different IDs and versions for libertarian/free-market experiments. Do not merge automatically.

## 6. NOMAD-1 — preserved concept status

The detailed raw file confirms the core direction:

```text
private billionaire consortium
~300 people
luxury interstellar cruise
psychological problem > engineering problem
realistic likely outcome = return / revolt / cult rather than literal eternity
```

The raw dossier explicitly contains:

```text
0–2 y   novelty / ego phase
2–20 y  meaning drift, boredom, escalating entertainment, conflicts
20–80 y structural social fractures
```

It also says engineers would design a **guaranteed return window** and treat human psychology as the dominant risk.

Current recollection from the user, to preserve but mark provisional until an older exact source is found:

```text
~5 years: secret automatic return timer installed by mechanics/engineers
~10 years: estimated horizon for the billionaires to become psychologically intolerable / destabilized by boredom
```

Do not silently convert these recollected numbers into hard canon yet.

## 7. Major source conflicts already detected

The older `Earth@generation-des(physic).txt` and newer detailed dossiers disagree strongly on populations and sometimes dates.

Examples:

```text
AL-QUDS-1              6,500  -> 18,000
AMAZONIA-1             4,500  -> 15,000
CELESTIAL-DESTINY-1    8,000  -> 25,000
CELESTIAL-DESTINY-2    2345 / 5,000 -> 2480 / 25,000
DEEP-SEEK-1            2540 / 1,500 -> 2690 / 14,000
DHARMA-1               7,000  -> 30,000
DHARMA-2               4,000  -> 25,000
EUROPA-1               5,000  -> 12,000
FAREWELL-1             100    -> 500
FINAL-ARK-1            2,000  -> 20,000
GAIA-2                  2370 / 2,000 -> 2500 / 12,000
GREEN-ARK-1            6,000  -> 12,000
HOMEWARD-1             800    -> 2,000
HYPERION-1             2595   -> 2750
IRON-SHIELD-1          4,000  -> 12,000
IRON-SHIELD-2          1,500  -> 3,000
NEW-JERUSALEM-1        4,000  -> 12,000
NEXT-1                  2,500 -> 6,000
NIRVANA-1               2,000 -> 3,500
ODYSSEY-1               3,000 -> 15,000
RESOURCER-1             3,500 -> 10,000
```

Therefore **no automatic field-level merge is allowed**.

## 8. IDs present only in older Universe branch

These do not have matching detailed dossiers in the supplied generation-ship archive:

```text
ETERNITY-1
FREE-VENTURE-1
GENESIS-1
GENESIS-2
LAST-LIGHT-1
STAR-SEED-1
UHURU-1
VOID-WALKER-1
```

They may represent:

- retired concepts;
- renamed missions;
- predecessor versions;
- world-state branches worth salvaging.

Keep all until explicitly classified.

## 9. IDs present only in the detailed generation-ship branch

```text
FREE-WORLD-1
HERACLES-1
KEPLER-1
MAYFLOWER-2
NEW-HELLAS-1
SVAROG-1
TIANHE-1
UBUNTU-1
UHURU-2
```

These are likely newer developments or replacements, but that is not yet a formal precedence rule.

## 10. Existing world-level material in Universe archive

`nodes.txt` and `actors.txt` contain later-era political/civilizational structures, including examples such as:

```text
Cognitum / technocratic sphere
Xin He / Chinese-derived civilization
Ujamaa-Jahi / Pan-African sphere
Skjoldburg / Citadel-derived fortress state
Agora-Prim / trade-information power
Prospectorate Kern / resource corporatocracy
Ilm-al-Nujum / science-faith synthesis
Dharma-Mandala / caste-derived civilization
```

These are useful outputs of the colonization history, but many numbers and names were generated during an older design phase. Preserve as **candidate descendants**, not final canon.

## 11. Normalization rule going forward

Do not attempt to clean everything in one pass.

For each ship/civilization create a future canonical dossier with four explicit sections:

```text
CANON
PROVISIONAL
ALTERNATE / OLD BRANCH
OPEN QUESTIONS
```

Normalization order should be:

1. identity / sponsor / launch year;
2. launch population and class composition;
3. ship engineering / speed / route;
4. awake/cryo doctrine;
5. voyage social evolution;
6. arrival / disappearance / failure;
7. colony formation and early regime;
8. daughter expeditions;
9. game-era descendants;
10. surviving physical artifacts and mysteries.

If two sources disagree, record both variants first. Do not choose silently.

## 12. Relationship to current design documents

The preserved legacy material feeds these newer contracts:

```text
GENERATION_SHIP_POPULATION_SELECTION.md
GENERATION_SHIP_SOCIAL_ADAPTATION.md
GENERATION_SHIP_SETTLEMENT_SIM.md
WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md
Notes/GENERATION_SHIP_MYSTERY_HORROR_REFERENCE.md
```

Those documents define *how to model the world*. This import document defines *what old material exists and must not be lost while that modeling framework is applied*.
