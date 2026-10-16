from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[2]

strip_path = ROOT / "src/game/presentation/TemporaryShortcutStrip.h"
strip = strip_path.read_text(encoding="utf-8")
space = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
loc_path = ROOT / "src/assets/localization/ui/cockpit/flight.json"
loc = json.loads(loc_path.read_text(encoding="utf-8"))

assert "TemporaryShortcutStrip" in strip
assert "TemporaryShortcutStrip::render" in space

for forbidden in [
    "GLFW_KEY_",
    "PlayerInputMapper",
    "ShipControlState",
    "Autopilot",
    "PhysicalControlModel",
]:
    assert forbidden not in strip, f"temporary strip leaked control dependency: {forbidden}"

# User explicitly excluded function-key controls/switches from the strip.
for forbidden in ["F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"]:
    assert forbidden not in strip, f"function key leaked into temporary strip: {forbidden}"

for required in ["Esc", "Backspace", "Ctrl+R", "L-Ctrl+Q"]:
    assert required in strip, f"missing shortcut hint: {required}"

keys = [
    "cockpit.shortcut_strip.menu",
    "cockpit.shortcut_strip.detail",
    "cockpit.shortcut_strip.back",
    "cockpit.shortcut_strip.repair",
    "cockpit.shortcut_strip.eject",
]
locales = ["en", "ru", "zh-Hans", "es", "ja"]
for key in keys:
    assert key in loc["strings"], f"missing localization key: {key}"
    for locale in locales:
        value = loc["strings"][key].get(locale, "")
        assert value, f"missing {locale} translation for {key}"

print("TEMPORARY SHORTCUT STRIP: PASS")
print(" - one presentation-only removable component.")
print(" - no flight-control or F-key hints.")
print(" - all five UI locales present.")
