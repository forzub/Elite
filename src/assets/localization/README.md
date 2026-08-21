# Localization assets

All player-facing translated text lives under this tree. Runtime code discovers `*.json` recursively; filenames are for humans, stable IDs inside files are authoritative. Malformed files are skipped at runtime and rejected by validation tests. English is the mandatory fallback locale.

- `ui/` — interface, maps and cockpit text. Native OpenGL map/route renderers
  receive resolved text profiles from `LocalizationService`; per-language string
  branches in renderer C++ are forbidden.
- `sky/` — localized sky-culture and constellation names only; topology remains in `data/galaxy/sky_cultures`.
- `world/star_systems/` — one localization file per star system, including that system's celestial bodies and named hubs.
- `world/interstellar/` — named objects outside star systems.
- `world/navigation_regions/` — faction/language navigation-region names.
- `game/` — type/manufacturer/equipment display catalogs.

Release packaging may compile this tree into a single binary localization package; source JSON is the development representation.
