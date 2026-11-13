# Cleanup candidates (to delete LATER, after the author's go)

Author 2026-11-11: "сделай просто отдельный файл где будешь фиксировать мусор который надо будет удалить. и потом
удалим. пока не удаляй." Nothing in this list has been deleted. Add new finds here instead of deleting them.

Status: `ready` = nothing reads it any more (verified how), `check` = still read somewhere / needs a decision.
Sizes are disk sizes on 2026-11-11.

| # | Path | Size | Tracked in git | Status | Why it is junk / what still needs it |
|---|---|---|---|---|---|
| 1 | `assets/generated/celestial/` (baked texture cache) | 1.7 GB | no | check | The map bakes every body from its surface field (memory step 3); `ELITE_MAP_GENERATED_ASSETS=0` draws Earth / Jupiter / Phobos the same. Still read only as optional fallbacks: Hub map preview / normal while a bake runs; flight albedo fallback `lod/global/albedo_2048.tga` for a Sol body without `albedo_source` (none today). Plan: play a while with `ELITE_MAP_GENERATED_ASSETS=0`, then delete. Tiles already deleted (author-authorized, 7.88 GB). |
| 2 | `build/assets/generated/` (runtime copy of #1) | 1.7 GB | no | check | Same as #1; goes with it. |
| 3 | `assets/generated/celestial/tau_ceti/` (part of #1) | ~0.8 GB before tiles | no | check | Tau Ceti bodies are baked from their descriptors now; this cache predates the generator. Goes with #1. |
| 4 | `src/assets/compiled/models/station_legacy_dump_v3/` | 48 MB | no | check | Old station dump (MODEL_ASSET_STORAGE.md). Confirm nothing loads it before deleting. |
| 5 | `src/assets/compiled/models/station/` | 64 MB | no | check | Unpacked copy next to the tracked `station.elmodel` (69 MB, in git). Confirm the game reads only the `.elmodel`. |
| 6 | ~~`assets/models/layout_sketches/`~~ | - | - | done | Moved to `D:/__elite/assets_source/models/layout_sketches` with the other raw sources (author 2026-11-13). |
| 7 | `build-relwithdebinfo/` | 8.0 GB | no | check | Second build tree (last used 2026-11-09 for RelWithDebInfo live runs). Rebuildable; delete if that build is not needed. |
| 8 | `assets/generated/**/lod/map`, `preview*.tga`, duplicate `albedo.tga` copies | inside #1 | no | check | Several uncompressed copies of the same albedo per body; go with #1. |
| 9 | `build/assets_models_before_move/` | ~880 MB | no | ready | The old runtime copy of `assets/models`, set aside 2026-11-13 when the raw sources moved to `D:/__elite/assets_source`. Every file in it is in `assets_source/models` (or in `assets` / `src/assets`); the game ran from a clean `build/assets/models` and all 49 mesh paths of the code matched it byte for byte. |

Already removed (author-authorized): root `headless_server/` (6.1 GB, stale build), stray `$' '` file, root and
`tools/navigation_runtime/` traces / logs, `third_party/asio-1-24-0.tar.gz`, all `lod/tiles` (7.88 GB).

Not junk (kept on purpose): `assets/source/celestial/` (curated maps, OBJ shapes - read by flight and map),
`src/assets/compiled/models/station.elmodel` (the game's station), `Notes/` and `tools/colonization_lab/` (lore and
world data sources), legacy state files `CURRENT_STATE.md` / `CURRENT_TASK.md` (history; may move to docs/history).
