# World save contract (stabilization stage 8)

## What a world's persisted state is

| part | owner | schema | content |
|---|---|---|---|
| `economy` | `EconomyServer` | `economy_state/5` | ledgers (cash, loans, standing, licences), markets (stock / price), contract board and contracts, cargo, port calls, SOS, employment, bots, construction offers / projects, mines, taxes |
| `ships` | `GameServer` | `runtime_ships/2` | the runtime (bought / hired / restored) ships: passport, product overrides, ownership, registration, name, system / hub / local position, who flies it; hires |
| accounts | `ServerRuntime` | its own file (`saveAccounts`) | player identities / credentials - NOT world state (a server's, kept apart) |

Not persisted, by design:
- universe time: the universe clock is the real calendar (`UniverseClock::reset` = real seconds since the epoch), so
  orbits and economic days continue from the wall clock;
- the simulation's transient state (flight in progress, the pad service, the dock dispatcher queues, NPC traffic,
  city damage) - a loaded world starts its ships where they were saved, at rest;
- DEV offsets (`econ:days`, the DEV economy tick).

## How it is written

`WorldSaveStore` (`src/game/economy/WorldSaveStore.h`):

1. The economy's flush (each server tick, at most once per 2 s; urgent on money moves) serializes the economy's state
   and, in the SAME tick, asks its companion provider for the other owners' parts (the ships).
2. The background writer writes every part of generation N as `<base>.g<N>.<part>.json`, each atomically (temp file,
   flush to disk, replace).
3. THE COMMIT: `<base>.manifest.json` is replaced atomically. It names generation N's parts (file, bytes, FNV-1a 64)
   and generation N-1's as the fallback.
4. Generation N-2's files are removed.

A crash at any moment leaves a complete generation in force: before step 3 the previous one, after it the new one.
The economy and the ships can no longer be torn apart (before: two files written at different moments).

A failed generation (a locked file, a full disk) is kept and retried every 2 s until written or superseded.

## How it is read

`WorldSaveStore::load(base)`:
- the manifest's generation, every part verified (size + hash); a damaged part -> the previous generation, all its parts;
- no usable generation -> nothing is loaded AND saving is disabled (the files are never written over);
- no manifest -> the legacy layout (`<base>`, `<base>.ships.json`), read once; the next save writes generation 1.

## Shutdown

`GameServer::~GameServer` writes the ships part and flushes the generation while every owner is alive, then detaches the
provider (its last parts stay with the economy, written again if the economy is dirty later).

## Where

Today the save is enabled by `ELITE_ECONOMY_STATE_FILE=<base>` (developer build). A default per-user location and a
"Continue" entry in the main menu are an AUTHOR decision (the product flow), not part of this contract.

## Evidence

- `EliteWorldSaveCheck` (tests/world_save): commit, one generation together, N-2 removed, a damaged newest falls back to
  the previous generation with both parts, parts without the manifest commit are not in force, no usable generation ->
  found and nothing loaded, the legacy layout read.
- Live (D:/tmp/save1): run 1 -> generation 1 committed; exit; run 2 -> "state restored ... generation 1 (day 10204)",
  continued, generation 2 committed with generation 1 kept.
