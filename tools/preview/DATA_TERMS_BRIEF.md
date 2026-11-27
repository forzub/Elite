# Data terms translation brief (HantedPreview / audit #9 D7)

The game data (src/assets/data/**.json) carries Russian names and descriptions (polities, goods, ships, sellers,
faults, hub modules, sites...). The client now translates them by their SOURCE TEXT through "data_terms" tables
(LocalizationService::dataText; render() applies it to plain strings and message parameters). Your job: translate one
list of Russian source strings into English and write one table.

Input: a JSON array of Russian source strings, given in your task (D:/tmp/data_terms/<group>.json, or a slice of it).
Output: D:/__elite/work/src/assets/localization/data/<output name>.json, exactly:
```json
{ "schema_version": 1, "kind": "data_terms", "terms": {
    "<the Russian source string, byte-for-byte>": { "en": "<English>", "ru": "<the same Russian string>" }
} }
```

Rules
1. The key is the source string EXACTLY as given (copy it; do not trim, re-quote or normalize). "ru" = the key.
2. English: natural, concise, consistent game English. Keep numbers, units, ids, Latin parts, punctuation and any
   placeholders ({x}, %s, %d) exactly. Russian «» quotes -> "" (straight double quotes, escaped in JSON).
3. Proper names: use the established English form when one exists (Солнечная система -> Sol System, Луна -> Moon,
   Марс -> Mars, Земля -> Earth); otherwise transliterate readable (Высокий -> Vysokiy, Кольцо -> Ring is a translation
   of a meaningful name - translate names that are plain words: «Пояс» -> "Belt", «Фальк» -> "Falk").
   Company / shipyard names: translate the descriptive words, transliterate the proper part
   (Лунно-Марсианские верфи -> Luna-Mars Shipyards; Верфи Гармонии -> Harmony Shipyards; Арсенал «Бастион» ->
   Arsenal "Bastion").
4. A string that is already bilingual ("English / Russian" or "Russian (English)") -> keep only the English part.
5. Translate every string of your list; if one is pure data junk (an id, a path), still map it to itself.
6. Write the file with a Python SCRIPT FILE (never `python -` / stdin), json.dump(..., ensure_ascii=False, indent=1);
   then load it back to validate, and check every input string is a key. Report: count, anything doubtful.
7. Touch no other file. No builds, no git.
Work in chunks if the list is long (write the table incrementally - e.g. a dict saved after each chunk).
