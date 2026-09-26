#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"Client-mode-state architecture check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


state = read("src/ui/platform/ClientModeState.h")
app = read("src/core/Application.cpp")
app_h = read("src/core/Application.h")
space = read("src/game/SpaceState.cpp")
renderer = read("src/game/system_map/SystemMapRenderer.cpp")
coordinate_h = read("src/game/navigation/CoordinateDisplayService.h")
coordinate_cpp = read("src/game/navigation/CoordinateDisplayService.cpp")
prefs_h = read("src/ui/platform/ClientPreferencesStore.h")
prefs_cpp = read("src/ui/platform/ClientPreferencesStore.cpp")
localization_h = read("src/game/localization/LocalizationService.h")

for token in (
    'std::string uiLocale = "en"',
    "bool constellationsEnabled = false",
    "std::string skyCultureId",
    'std::string coordinateDisplayFormatId = "hierarchical"',
    "std::uint64_t revision",
    "setUiLocale",
    "setConstellationsEnabled",
    "setSkyCultureId",
    "setCoordinateDisplayFormatId",
):
    if token not in state:
        fail(f"ClientModeState lost authoritative mode field/transition: {token}")

for field in (
    "uiLocale",
    "constellationsEnabled",
    "skyCultureId",
    "coordinateDisplayFormatId",
):
    if re.search(rf"m_clientModeState\.{field}\s*=", app):
        fail(f"Application writes ClientModeState.{field} directly")

for token in (
    "cycleCoordinateDisplayFormat",
    "setCoordinateDisplayFormatId",
    "CoordinateDisplayService::instance().setFormat",
    "persistClientModeState",
):
    if token not in app + app_h:
        fail(f"coordinate display transition lost state-owned path: {token}")

if "CoordinateDisplayService::instance().cycle()" in app:
    fail("Application bypassed ClientModeState through coordinate singleton cycle")

if "void cycle() noexcept" in coordinate_h or "CoordinateDisplayService::cycle" in coordinate_cpp:
    fail("CoordinateDisplayService still owns a hidden mutable mode transition")

if "CoordinateDisplayService::instance().setFormat" in renderer:
    fail("SystemMapRenderer still overwrites global coordinate display mode")

for token in (
    "preferredLocale",
    "constellationsEnabled",
    "skyCultureId",
    "coordinateDisplayFormatId",
):
    if token not in prefs_h:
        fail(f"client preference projection missing field: {token}")

for token in (
    '"preferred_locale"',
    '"constellations_enabled"',
    '"sky_culture_id"',
    '"coordinate_display_format_id"',
):
    if token not in prefs_cpp:
        fail(f"client preference serialization missing field: {token}")

for token in (
    "clientModeState().constellationsEnabled",
    "setConstellationsEnabled(next)",
    "clientModeState().skyCultureId",
    "setSkyCultureId(next)",
    "m_sceneRenderer.setConstellationOverlayEnabled",
    "m_sceneRenderer.setConstellationCultureId",
):
    if token not in space:
        fail(f"constellation mode lost state -> projection path: {token}")

if "Projection setter. Application/ClientModeState owns the selected mode." not in localization_h:
    fail("LocalizationService no longer declares itself a projection")

print("[PASS] global client modes have one authoritative state owner")
