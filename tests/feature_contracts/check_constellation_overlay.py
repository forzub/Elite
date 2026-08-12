#!/usr/bin/env python3
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] constellation contract: {message}", file=sys.stderr)
    raise SystemExit(1)


def require(condition: bool, message: str) -> None:
    if not condition:
        fail(message)


def read(rel: str) -> str:
    path = ROOT / rel
    if not path.exists():
        fail(f"missing required file: {rel}")
    return path.read_text(encoding="utf-8", errors="replace")


def extract_function(text: str, signature: str) -> str:
    start = text.find(signature)
    if start < 0:
        fail(f"missing function: {signature}")
    brace = text.find("{", start)
    if brace < 0:
        fail(f"function has no body: {signature}")
    depth = 0
    for i in range(brace, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[start:i + 1]
    fail(f"unterminated function: {signature}")
    return ""


def main() -> int:
    star_h = read("src/render/starfield/GalaxyStarfieldRenderer.h")
    star_cpp = read("src/render/starfield/GalaxyStarfieldRenderer.cpp")
    overlay_h = read("src/render/starfield/ConstellationOverlayRenderer.h")
    overlay_cpp = read("src/render/starfield/ConstellationOverlayRenderer.cpp")
    scene_h = read("src/scene/SceneRenderer.h")
    scene_cpp = read("src/scene/SceneRenderer.cpp")

    for token in (
        "SkyCultureCatalog",
        "cycleConstellationCulture()",
        "constellationStarIdentifier()",
        "hipparcosCatalogId",
        "brightStarCatalogId",
    ):
        require(token in star_h, f"generic sky-culture starfield contract lost: {token}")

    for token in (
        "sky_cultures/manifest.json",
        "constellation_support_stars.json",
        "m_constellationOverlayRenderer.setCulture(*culture)",
        "m_constellationStarReferences",
    ):
        require(token in star_cpp, f"runtime sky-culture path lost: {token}")
    require("m_realStars.push_back(supportStar" not in star_cpp,
            "topology-only support points leaked into visible star sprites")

    for token in (
        "using ConstellationDefinition = SkyCultureCatalog::Constellation",
        "setCulture(const SkyCultureCatalog::Culture& culture)",
        "labelAnchors()",
        "starIdentifier() const",
    ):
        require(token in overlay_h, f"generic overlay contract lost: {token}")
    require("culture.constellations" in overlay_cpp and "culture.starIdentifier" in overlay_cpp,
            "overlay no longer rebuilds directly from selected culture")

    require("void setUiLocale(const std::string& locale)" in scene_h,
            "constellation labels lost global UI-locale input")
    require("m_uiLocale" in scene_h and "m_constellationLabelFont" in scene_h,
            "constellation label presentation state disappeared")

    label_fn = extract_function(scene_cpp, "void SceneRenderer::renderConstellationLabels")
    for token in (
        "constellationOverlayEnabled()",
        "getConstellationLabelAnchors()",
        "glm::mat4(glm::mat3(view))",
        "m_uiLocale",
        "measureText(label)",
        "0.28f",
    ):
        require(token in label_fn, f"screen-facing constellation-label contract lost: {token}")

    hover_fn = extract_function(scene_cpp, "void SceneRenderer::renderConstellationHoverOverlay")
    for token in (
        "constellationStarIdentifier()",
        "catalogId(identifier)",
        "displayName(",
        "m_uiLocale",
    ):
        require(token in hover_fn, f"culture-aware hover contract lost: {token}")

    font_fn = extract_function(scene_cpp, "void SceneRenderer::ensureConstellationLabelFont")
    require("NotoSansCJK" in font_fn and "msyh.ttc" in font_fn,
            "CJK-capable constellation-label font fallback disappeared")
    require("Roboto-Light.ttf" in font_fn,
            "Latin/Cyrillic constellation-label fallback disappeared")

    print("[PASS] switchable sky-culture topology + screen-facing global-language labels")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
