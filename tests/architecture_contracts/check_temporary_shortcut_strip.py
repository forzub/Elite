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

# Modified F-key chords are the primary content of the strip.
for required in [
    "Ctrl+F9",
    "Ctrl+F10",
    "Ctrl+F11",
    "Ctrl+F12",
    "Alt+F12",
    "Ctrl+Alt+F12",
]:
    assert required in strip, f"missing modified function-key hint: {required}"

# No Shift+Fxx command exists in the current runtime contract.
assert "Shift+F" not in strip

for required in ["Esc", "Backspace", "Ctrl+R", "L-Ctrl+Q"]:
    assert required in strip, f"missing shortcut hint: {required}"

keys = [
    "cockpit.shortcut_strip.layout",
    "cockpit.shortcut_strip.flight_law",
    "cockpit.shortcut_strip.coordinates",
    "cockpit.shortcut_strip.constellations",
    "cockpit.shortcut_strip.sky_culture",
    "cockpit.shortcut_strip.language",
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
print(" - modified F-key chords are explicit; no Shift+Fxx command exists.")
print(" - all five UI locales present.")
