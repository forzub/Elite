#!/usr/bin/env python3
"""The UI component pass (author's brief 2026-11-24) - two invariants:

1. Concrete UI widgets do not read platform input. GLFW cursor / mouse-button reads are allowed only at the listed
   input boundary (and, until they move to the component model, the listed legacy renderers).
2. A component's render geometry and hit geometry come from ONE resolved layout: the magnitola has no hand-made hit
   boxes, its drawing file neither emits actions nor reads input.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SCANNED = ["src/render", "src/ui", "src/game/system_map"]
INPUT_TOKENS = re.compile(r"\bglfwGetCursorPos\b|\bglfwGetMouseButton\b")
ALLOWED = {
    # THE boundary of the UI pointer
    "src/ui/input/GlfwPointerSource.cpp",
    # legacy renderers (class C/D of the UI inventory) - to move behind the router
    "src/game/system_map/SystemMapRendererInput.cpp",
    "src/game/system_map/HubMapBackend.cpp",
    "src/render/SceneRenderer.cpp",
}

failures = []
for d in SCANNED:
    for path in (ROOT / d).rglob("*"):
        if path.suffix not in (".cpp", ".h", ".inl", ".hpp"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        text = path.read_text(encoding="utf-8", errors="replace")
        if INPUT_TOKENS.search(text) and rel not in ALLOWED:
            failures.append(f"{rel}: reads GLFW pointer input directly (route it through ui::UIInputRouter)")

radio = (ROOT / "src/render/cockpit/CockpitRadioWidget.h").read_text(encoding="utf-8")
radio_cpp = (ROOT / "src/render/cockpit/CockpitRadioWidget.cpp").read_text(encoding="utf-8")
radio_draw = (ROOT / "src/render/cockpit/CockpitRadioWidgetDraw.cpp").read_text(encoding="utf-8")
if re.search(r"struct\s+Box\b", radio + radio_cpp):
    failures.append("CockpitRadioWidget: a hand-made hit Box is back (one resolved rect per child)")
if "class CockpitRadioWidget : public UIComponent" not in radio:
    failures.append("CockpitRadioWidget is not a UIComponent")
for token in ("onAction(", "emit(", "glfw", "CockpitRadio::instance", "route("):
    if token in radio_draw:
        failures.append(f"CockpitRadioWidgetDraw.cpp: '{token}' in the drawing (render is presentation only)")
for token in ("CockpitRadio::instance", "glfw"):
    if token in radio_cpp:
        failures.append(f"CockpitRadioWidget.cpp: '{token}' (the widget is not the radio; no platform input)")

if failures:
    for f in failures:
        print("FAIL", f)
    sys.exit(1)
print("UI INPUT BOUNDARY: PASS")
