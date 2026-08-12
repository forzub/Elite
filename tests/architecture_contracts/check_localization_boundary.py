#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


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
            root = json.loads(p.read_text(encoding="utf-8"))
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


def main() -> int:
    ui = json.loads(text("src/assets/webui/localization/ui_strings.json"))
    catalog = json.loads(text("src/assets/data/localization/catalog_names.json"))

    require(ui.get("default_locale") == "en", "English must remain the mandatory fallback locale")
    order = ui.get("locale_order", [])
    require(order and order[0] == "en", "locale cycle must start with English")
    require(len(order) == len(set(order)), "locale cycle contains duplicates")
    require({"en", "ru", "zh-Hans", "es", "ja"}.issubset(order),
            "approved initial locale set changed")

    for key, translations in ui.get("strings", {}).items():
        require(isinstance(translations, dict) and isinstance(translations.get("en"), str) and translations["en"],
                f"UI key {key!r} has no English fallback")
    for locale in order:
        language = ui.get("languages", {}).get(locale, {})
        require(isinstance(language.get("en"), str) and language["en"],
                f"language {locale!r} has no English display name")

    domains = catalog.get("domains", {})
    systems = domains.get("systems", {})
    bodies = domains.get("bodies", {})
    expected_systems, expected_bodies = expected_catalog_ids()
    require(expected_systems.issubset(systems.keys()),
            f"catalog-name table missing systems: {sorted(expected_systems - systems.keys())}")
    require(expected_bodies.issubset(bodies.keys()),
            f"catalog-name table missing bodies: {sorted(expected_bodies - bodies.keys())[:20]}")
    for domain_name, entries in (("systems", systems), ("bodies", bodies),
                                 ("hubs", domains.get("hubs", {})),
                                 ("galaxy_objects", domains.get("galaxy_objects", {}))):
        for stable_id, translations in entries.items():
            require(isinstance(translations.get("en"), str) and translations["en"],
                    f"{domain_name}/{stable_id} has no English fallback")

    service_h = text("src/game/localization/LocalizationService.h")
    service_cpp = text("src/game/localization/LocalizationService.cpp")
    app = text("src/core/Application.cpp")
    app_h = text("src/core/Application.h")
    hotkey = text("src/game/ui/GameUiHotkeyPolicy.h")
    game_state = text("src/core/GameState.h")
    space = text("src/game/SpaceState.cpp")
    scene = text("src/scene/SceneRenderer.cpp")
    scene_h = text("src/scene/SceneRenderer.h")
    galaxy_renderer = text("src/game/system_map/GalaxyMapRenderer.cpp")
    font_cpp = text("src/render/Font.cpp")
    text_renderer_h = text("src/render/HUD/TextRenderer.h")
    flight_indicator = text("src/render/cockpit/FlightVectorIndicatorRenderer.cpp")
    web_i18n = text("src/assets/webui/game_i18n.js")

    for token in ("cycleLocale()", "catalogName(", "languageIndicator()"):
        require(token in service_h, f"LocalizationService lost API {token}")
    require('find("en")' in service_cpp or 'find("en")' in service_cpp.replace(' ', ''),
            "C++ localization lost English fallback")
    require("LocalizationService m_localization" in app_h,
            "Application no longer owns the global client UI locale")
    require("onUiLanguageChanged()" in game_state,
            "game states can no longer react to a global language change")

    for token in ("CycleUiLanguage", "CycleSkyCulture", "ToggleConstellations", "NavigateLocal"):
        require(token in hotkey, f"F12 hotkey policy lost {token}")
    require("resolveF12HotkeyAction" in app and "cycleUiLanguage();" in app,
            "Application no longer routes Ctrl+Alt+F12 through the tested policy")
    require("cycleSkyCulture();" in app and "toggleConstellationOverlay();" in app,
            "Alt/Ctrl F12 presentation chords disappeared")

    require("applyClientCatalogLocalization()" in space and 'catalogName(\n            "systems"' in space,
            "client map/catalog display names are no longer localized by stable IDs")
    require('catalogName(\n                "bodies"' in space,
            "celestial body names are no longer localized by stable IDs")
    require('"hubs"' in space and "setNavigationOverlayTextProfile" in space,
            "native map/hub localization no longer reaches the renderer")
    require("setGameSystemDisplayNames" in space and "setGameSystemDisplayNames" in scene_h,
            "in-flight game-system star labels no longer receive localized catalog names")
    require("setUiLocale(context().app->localization().locale())" in space,
            "global locale no longer reaches the scene renderer")
    require("displayName(\n                m_uiLocale" in scene or "displayName(m_uiLocale)" in scene,
            "constellation labels no longer use the global locale")
    require("m_uiLocale" in scene_h,
            "scene renderer lost its client UI-locale presentation state")
    require('"PLAYER"' not in galaxy_renderer,
            "Galaxy player marker still bypasses localized map-overlay text")
    require("m_fallbackFaces" in font_cpp and "msyh.ttc" in font_cpp and "YuGothR.ttc" in font_cpp,
            "native FreeType renderer lost CJK glyph fallback")

    active_webui = ("main_menu.html", "loading.html", "system_map_panel.html", "confirm_exit.html")
    for rel in active_webui:
        html = text(f"src/assets/webui/{rel}")
        require('/game_i18n.js' in html, f"player-facing WebUI page {rel} bypasses global localization")
        # Every static data-i18n key on an active player-facing page must exist
        # in the shared table and carry an English fallback. Dynamic text uses
        # LocalizationService/GameI18n.t with an explicit English fallback.
        import re
        for key in re.findall(r'data-i18n=["\']([^"\']+)["\']', html):
            translations = ui.get("strings", {}).get(key)
            require(isinstance(translations, dict) and translations.get("en"),
                    f"{rel} references missing/non-English localization key {key!r}")
    require("data-i18n" in text("src/assets/webui/main_menu.html"), "main menu lost localization keys")
    panel = text("src/assets/webui/system_map_panel.html")
    require('data-i18n="map.detail"' in panel and 'data-i18n="map.hub"' in panel,
            "System Map action buttons bypass global localization")
    require("const LANG" not in panel and "const I18N" not in panel,
            "System Map restored a private language table")
    require("gameUiLanguageIndicator" in web_i18n and "UI:" in web_i18n,
            "WebUI bottom-right language indicator disappeared")
    require("renderUiLanguageIndicator" in space,
            "native gameplay bottom-right language indicator disappeared")
    require("constellationCultureDisplayName" in space and "m_constellationOverlayEnabled" in space,
            "native language indicator no longer reports the active sky culture when constellations are visible")
    require("solidRectPx" in text_renderer_h and "solidRectPx(" in flight_indicator and "solidRectPx(" in space,
            "cockpit mode/language readability plates disappeared")
    require('"map.interstellar"' in space and '"hud.time_simulation_mode"' in space,
            "native player-facing navigation/time labels bypass global localization")
    for key in ("overlay.player", "overlay.format", "cockpit.front", "cockpit.cell",
                "cockpit.vrel", "cockpit.mode.newtonian"):
        require(f'"{key}"' in space,
                f"native player-facing key {key!r} is not routed through LocalizationService")
    require("map[locale]" in web_i18n and "map.en" in web_i18n,
            "WebUI lost exact/base/English fallback")

    # Manufacturer/cockpit language is a deliberately separate presentation domain.
    for root in (ROOT / "src/render/cockpit", ROOT / "src/game/ship/cockpit"):
        for p in root.rglob("*"):
            if p.is_file() and p.suffix in {".h", ".hpp", ".cpp", ".inl"}:
                require("LocalizationService" not in p.read_text(encoding="utf-8", errors="replace"),
                        f"global UI localization leaked into manufacturer cockpit layer: {p.relative_to(ROOT)}")

    # Server/simulation deal in IDs and authority, never locale tables or translated display strings.
    for root in (ROOT / "src/game/server", ROOT / "src/game/simulation"):
        for p in root.rglob("*"):
            if p.is_file() and p.suffix in {".h", ".hpp", ".cpp", ".inl"}:
                contents = p.read_text(encoding="utf-8", errors="replace")
                require("LocalizationService" not in contents and "catalog_names.json" not in contents and "ui_strings.json" not in contents,
                        f"client localization leaked into authoritative code: {p.relative_to(ROOT)}")

    print("[PASS] client-only global UI localization + stable-ID catalog names + English fallback")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
