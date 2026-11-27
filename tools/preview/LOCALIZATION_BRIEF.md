# Localization pass brief (audit #7 D7, HantedPreview) - shared by the parallel workers

Goal: no Russian (or any one-language) text built in C++ reaches the player. English must be complete; Russian must
read exactly as today. Repo: D:/__elite/work (C++/OpenGL game "Hanted", MinGW). Communicate results in English.

## The API (already committed, a40cbda59)
- `src/game/localization/LocalizedMessage.h`: `game::localization::Message("key").arg("name", value)...` -> a
  language-neutral string (\x1F key \x1E name=value ...). `.arg(name, double, decimals)` formats numbers. A Message
  can be a parameter of another Message (nested reason etc).
- `src/game/localization/LocalizationService.h`:
  - `loc.text(key, englishFallback)`, `loc.format(key, {{"name", value}, ...}, englishTemplate)` with `{name}`
    placeholders, `loc.render(str)` renders a Message (plain strings pass through unchanged).
  - CLIENT-ONLY free helpers when no service reference is at hand: `game::localization::tr(key, params,
    englishTemplate)`, `tr(key, englishText)`, `renderMessage(str)`.
- Translation files: JSON under `src/assets/localization/` (discovered recursively; any new file is loaded):
  ```json
  { "schema_version": 1, "kind": "ui_strings", "strings": {
      "pad.dispatch.cleared": { "en": "Pad {pad}: cleared to land, place {place}", "ru": "Площадка {pad}: посадка разрешена, место {place}" }
  } }
  ```
  Supply "en" and "ru" for every key (other languages zh-Hans/es/ja: leave out - not our job now; do NOT add languages).

## Rules
1. Server / simulation code (src/game/server, src/game/simulation, src/game/economy, anything the dedicated server
   links) must NOT produce finished text: build `Message(...)` strings instead (keys + params). Never call tr() /
   activeService() there.
2. Client code that shows a producer's string (a notice, a service result, a contract text, a dispatcher answer...)
   must pass it through `render()` / `renderMessage()` before showing it.
3. Client-built text: `tr()` / `loc.format()` with named parameters. NEVER glue fragments of one language
   ("Площадка " + x + " занята"): one template per sentence with `{x}`.
4. Keep "ru" = the current Russian text exactly (same words, punctuation, key hints); "en" = natural, concise
   English (cockpit-terminal style where it is a terminal line; keep key names like F8, Ctrl+U, G as they are).
5. Do not change behaviour, layout, positions, colours, font sizes, widths or which text is shown when - text content
   only. The cockpit UI is frozen by the author (no redesign, no moving instruments).
6. Logs (std::cerr / std::cout / printf diagnostics, "[Tag] ..." lines) and DEV-only verbs (src/game/dev,
   ELITE_DEV_* paths, the dev panel) are NOT translated - leave them.
7. Comments are not translated.
8. Do not touch files outside your group except: adding your own new JSON file(s) under src/assets/localization/
   (name them after your group, e.g. `ui/cockpit/flight_messages.json`). If a needed change lies in another group's
   file, report it instead of editing.
9. Key naming: dotted, lowercase, prefixed by area (`cockpit.pad.*`, `map.info.*`, `economy.*`, `service.*`...).
   Check that a key does not already exist (grep the localization folder) before adding it.
10. Verify each changed .cpp with `python tools/preview/syntax_check.py <file.cpp> </dev/null` (parallel-safe,
    -fsyntax-only). For a changed header, check one .cpp that includes it. NEVER run ninja / cmake / the game, never
    git commit / checkout / stash, never delete files. NEVER run `python -` (stdin): write scripts to files.
11. Validate every JSON you write (python json.load in a script file).
12. When done, report: files changed, keys added (count), any literals you deliberately left (with reason), any
    cross-group change needed, and anything that may change behaviour.

Measure: `python tools/preview/find_cyrillic_literals.py <dir>` lists the Cyrillic literals left (logs marked LOG).
