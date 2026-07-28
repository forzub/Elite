# Distant star systems

Every `.json` document in this directory is loaded as a complete star system, but distant systems are not included in the ordinary local Galaxy Atlas or the decorative starfield.

They remain available through `StarAtlasDatabase::distantSystems()`, `findSystemSummary()` and `findSystem()`, so quests, generation-ship routes and other long-range navigation systems can target them.

Distant-system IDs use the reserved range beginning at `100000`. `Kepler-62` uses stable ID `100062`.

Required metadata:

```json
"catalog_scope": "distant",
"atlas_visible": false,
"route_target": true
```

Changing `atlas_visible` does not automatically add a distant system to the current map renderer. It records catalog intent for a later dedicated long-range atlas layer.
