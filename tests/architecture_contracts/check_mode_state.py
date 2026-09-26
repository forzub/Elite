#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.is_file():
        raise AssertionError(f"missing {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def require(rel: str, *tokens: str) -> None:
    body = read(rel)
    for token in tokens:
        if token not in body:
            raise AssertionError(f"{rel}: missing mode-state token {token!r}")


try:
    require(
        "src/ui/platform/ClientModeState.h",
        "struct ClientModeState",
        'std::string uiLocale = "en"',
        "bool constellationsEnabled = false",
        "std::string skyCultureId",
        "std::uint64_t revision",
        "setUiLocale",
        "setConstellationsEnabled",
        "setSkyCultureId",
    )

    require(
        "src/core/Application.h",
        "ClientModeState m_clientModeState",
        "clientModeState() const",
        "setConstellationsEnabled",
        "setSkyCultureId",
    )
    require(
        "src/core/Application.cpp",
        "m_localization.nextLocale(m_clientModeState.uiLocale)",
        "setUiLanguage(next)",
        "m_clientModeState.setUiLocale(locale)",
        "m_clientModeState.setConstellationsEnabled(enabled)",
        "m_clientModeState.setSkyCultureId(cultureId)",
        "persistClientModeState()",
    )

    app = read("src/core/Application.cpp")
    if "m_localization.cycleLocale()" in app:
        raise AssertionError(
            "Application bypassed ClientModeState by mutating localization selection directly"
        )

    require(
        "src/game/localization/LocalizationService.h",
        "hasLocale",
        "nextLocale",
        "Projection setter",
    )

    require(
        "src/game/SpaceState.cpp",
        "context().app->clientModeState().constellationsEnabled",
        "context().app->setConstellationsEnabled(next)",
        "context().app->setSkyCultureId(next)",
        "applyClientModeState()",
        "m_sceneRenderer.setConstellationOverlayEnabled(",
        "m_sceneRenderer.setConstellationCultureId(",
        "m_sceneRenderer.nextConstellationCultureId(",
    )
    space_h = read("src/game/SpaceState.h")
    space_cpp = read("src/game/SpaceState.cpp")
    if "m_constellationOverlayEnabled" in space_h + space_cpp:
        raise AssertionError(
            "SpaceState reintroduced a constellation-enabled shadow state"
        )

    require(
        "src/render/starfield/GalaxyStarfieldRenderer.h",
        "setConstellationCultureId",
        "nextConstellationCultureId",
    )
    star_h = read("src/render/starfield/GalaxyStarfieldRenderer.h")
    star_cpp = read("src/render/starfield/GalaxyStarfieldRenderer.cpp")
    if "cycleConstellationCulture" in star_h + star_cpp:
        raise AssertionError(
            "renderer again owns sky-culture mode transitions"
        )

    require(
        "src/ui/platform/ClientPreferencesStore.h",
        "bool constellationsEnabled = false",
        "std::string skyCultureId",
    )
    require(
        "src/ui/platform/ClientPreferencesStore.cpp",
        '"constellations_enabled"',
        '"sky_culture_id"',
    )

    # Existing mode systems that are already state-owned must stay that way.
    require(
        "src/core/Application.h",
        "GamePresentationCoordinator m_gameUi",
    )
    require(
        "src/game/SpaceState.cpp",
        "m_navigationWorkspace.modules().setEnabled",
        "m_navigationWorkspace.modules().toggle",
    )

    print("[PASS] client mode-state ownership: locale / constellations / sky culture")
except AssertionError as exc:
    print(f"[FAIL] {exc}", file=sys.stderr)
    raise SystemExit(1)
