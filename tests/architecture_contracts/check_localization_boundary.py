#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LOC = ROOT / "src/assets/localization"


def fail(message: str) -> None:
    print(f"[FAIL] localization contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def text(rel: str) -> str:
    p = ROOT / rel
    require(p.exists(), f"missing required file: {rel}")
    return p.read_text(encoding="utf-8", errors="replace")


def read_json(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        fail(f"invalid JSON {path.relative_to(ROOT)}: {exc}")
    require(isinstance(value, dict), f"JSON root must be object: {path.relative_to(ROOT)}")
    return value


def safe_id_part(value: str) -> str:
    return "".join("_" if c in " ()/\\" else c for c in value)


def expected_catalog_ids() -> tuple[set[str], set[str]]:
    systems: set[str] = set()
    bodies: set[str] = set()
    for directory in (
        ROOT / "src/assets/data/galaxy_details/systems_details",
        ROOT / "src/assets/data/galaxy_details/distant_systems_details",
    ):
        for p in sorted(directory.glob("*.json")):
            root = read_json(p)
            sid = int(root["id"])
            systems.add(str(sid))
            details = root.get("details", {})

            def add_planets(parent: str, planets: list[dict]) -> None:
                for planet in planets or []:
                    pid = parent + "." + safe_id_part(planet.get("name", "Planet"))
                    bodies.add(f"{sid}:{pid}")
                    for moon in planet.get("moons", []) or []:
                        mid = pid + "." + safe_id_part(moon.get("name", "Moon"))
                        bodies.add(f"{sid}:{mid}")

            for star in details.get("stars", []) or []:
                star_id = f"system_{sid}." + safe_id_part(star.get("name", "Star"))
                bodies.add(f"{sid}:{star_id}")
                add_planets(star_id, star.get("planets", []) or [])
                for index, _belt in enumerate(star.get("asteroid_belts", []) or []):
                    bodies.add(f"{sid}:{star_id}.belt_{index}")

            bary = f"system_{sid}.barycenter"
            add_planets(bary, details.get("system_planets", []) or [])
            for index, _belt in enumerate(details.get("asteroid_belts", []) or []):
                bodies.add(f"{sid}:{bary}.belt_{index}")

    return systems, bodies


def require_english_table(entries: dict, label: str) -> None:
    for stable_id, translations in entries.items():
        require(isinstance(translations, dict), f"{label}/{stable_id} must be translation object")
        require(isinstance(translations.get("en"), str) and translations["en"],
                f"{label}/{stable_id} has no English fallback")


def main() -> int:
    require(LOC.is_dir(), "single assets/localization root is missing")
    for legacy in (
        ROOT / "src/assets/webui/localization",
        ROOT / "src/assets/data/localization/catalog_names.json",
        ROOT / "src/assets/data/navigation/region_names.json",
    ):
        require(not legacy.exists(), f"legacy localization source still exists: {legacy.relative_to(ROOT)}")

    json_files = sorted(LOC.rglob("*.json"))
    require(json_files, "localization tree contains no JSON files")
    kinds: dict[str, list[Path]] = {}
    for path in json_files:
        root = read_json(path)
        require(root.get("schema_version") == 1, f"schema_version != 1: {path.relative_to(ROOT)}")
        kind = root.get("kind")
        require(isinstance(kind, str) and kind, f"missing kind: {path.relative_to(ROOT)}")
        kinds.setdefault(kind, []).append(path)

    # Language registry is the only locale enable/order authority.
    languages_files = kinds.get("languages", [])
    require(len(languages_files) == 1, "exactly one languages registry is required")
    languages = read_json(languages_files[0])
    require(languages.get("default_locale") == "en", "English must remain mandatory fallback")
    order = languages.get("locale_order", [])
    require(order and order[0] == "en", "locale cycle must start with English")
    require(len(order) == len(set(order)), "locale cycle contains duplicates")
    require({"en", "ru", "zh-Hans", "es", "ja"}.issubset(order), "approved locale set changed")
    require_english_table(languages.get("languages", {}), "languages")

    # UI is split by category, but keys remain globally unique.
    ui: dict[str, dict] = {}
    for path in kinds.get("ui_strings", []):
        root = read_json(path)
        for key, value in root.get("strings", {}).items():
            require(key not in ui, f"duplicate UI key {key!r} in {path.relative_to(ROOT)}")
            ui[key] = value
    require_english_table(ui, "ui")
    for required in ("map.title", "overlay.player", "cockpit.mode.newtonian", "main.new_local_game", "main.multiplayer"):
        require(required in ui, f"required UI key disappeared: {required}")
    require((LOC / "ui/maps").is_dir() and (LOC / "ui/cockpit").is_dir(),
            "maps/cockpit localization categories are not separated")

    # One independently recoverable file per gameplay star system.
    system_files = sorted((LOC / "world/star_systems").glob("*.json"))
    expected_systems, expected_bodies = expected_catalog_ids()
    require(len(system_files) == len(expected_systems),
            f"expected {len(expected_systems)} per-system files, got {len(system_files)}")
    systems: set[str] = set()
    bodies: set[str] = set()
    hubs: set[str] = set()
    for path in system_files:
        root = read_json(path)
        require(root.get("kind") == "star_system", f"wrong kind in {path.name}")
        sid = str(root.get("system_id", ""))
        require(sid and sid not in systems, f"duplicate/empty system_id {sid!r}")
        require(not re.match(r"^\d{3}_", path.name), f"legacy numeric filename prefix retained: {path.name}")
        systems.add(sid)
        require_english_table({sid: root.get("names", {})}, "systems")
        for body_id, translations in root.get("bodies", {}).items():
            require(body_id.startswith(sid + ":"),
                    f"{path.name} contains body from another system: {body_id}")
            require(body_id not in bodies, f"duplicate body ID {body_id}")
            bodies.add(body_id)
            require_english_table({body_id: translations}, "bodies")
        for hub_id, translations in root.get("hubs", {}).items():
            require(hub_id not in hubs, f"duplicate hub ID {hub_id}")
            hubs.add(hub_id)
            require_english_table({hub_id: translations}, "hubs")
    require(expected_systems == systems, f"per-system localization IDs drifted: missing={sorted(expected_systems-systems)}")
    require(expected_bodies.issubset(bodies),
            f"per-system localization missing bodies: {sorted(expected_bodies-bodies)[:20]}")

    # Future-facing localization domains exist without forcing a manifest edit.
    require((LOC / "world/interstellar").is_dir(), "interstellar localization category missing")
    for folder in ("manufacturers", "ship_types", "station_types", "beacon_types"):
        require((LOC / "game" / folder).is_dir(), f"game localization category missing: {folder}")

    service_h = text("src/game/localization/LocalizationService.h")
    service_cpp = text("src/game/localization/LocalizationService.cpp")
    app = text("src/core/Application.cpp")
    app_h = text("src/core/Application.h")
    web_i18n = text("src/assets/webui/game_i18n.js")
    nav_cpp = text("src/game/navigation/NavigationRegionCatalog.cpp")
    map_cpp = text("src/game/system_map/SystemMapRenderer.cpp")

    require("loadDirectory(" in service_h and "recursive_directory_iterator" in service_cpp,
            "LocalizationService is not recursive/data-driven")
    require("std::sort(files.begin(), files.end()" in service_cpp,
            "recursive localization load order is not deterministic")
    for token in ("skippedFileCount", "duplicate UI key", "duplicate catalog object",
                  "contains a body stable ID belonging to another system"):
        require(token in service_h + service_cpp, f"localization validation lost: {token}")
    require("webUiBundleJson()" in service_h and "runtime_ui.json" in app and "runtime_ui.json" in web_i18n,
            "WebUI does not consume generated bundle from the native localization service")
    require("setVirtualFile" in app, "WebUI localization bundle is not served from memory")
    require("LocalizationService m_localization" in app_h, "Application lost global localization owner")
    require("assets/localization/world/navigation_regions" in map_cpp and "loadFromDirectory" in nav_cpp,
            "navigation-region translations are not in the unified localization tree")

    # Active player-facing WebUI pages stay on the shared runtime bundle.
    for rel in ("main_menu.html", "loading.html", "system_map_panel.html", "confirm_exit.html"):
        html = text(f"src/assets/webui/{rel}")
        require('/game_i18n.js' in html, f"{rel} bypasses global localization")
        for key in re.findall(r'data-i18n=["\']([^"\']+)["\']', html):
            require(key in ui and ui[key].get("en"), f"{rel} references missing key {key!r}")

    # Server/simulation own stable IDs only; localization remains client presentation data.
    for code_root in (ROOT / "src/game/server", ROOT / "src/game/simulation"):
        for path in code_root.rglob("*"):
            if path.is_file() and path.suffix in {".h", ".hpp", ".cpp", ".inl"}:
                contents = path.read_text(encoding="utf-8", errors="replace")
                require("LocalizationService" not in contents and "assets/localization" not in contents,
                        f"localization leaked into authoritative code: {path.relative_to(ROOT)}")

    print("[PASS] recursive categorized localization + per-system isolation + resilient shared WebUI source")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
