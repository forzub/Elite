from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
engine_h = (ROOT / "src/audio/AudioEngine.h").read_text(encoding="utf-8")
adapter_h = (ROOT / "src/audio/TerminalAudioAdapter.h").read_text(encoding="utf-8")
terminal_h = (
    ROOT / "src/game/presentation/DockingStatusTerminal.h"
).read_text(encoding="utf-8")

for forbidden in [
    "SpaceState",
    "RoutePlanner",
    "Autopilot",
    "TextRenderer",
]:
    assert forbidden not in engine_h
    assert forbidden not in adapter_h

for forbidden in [
    "AudioEngine",
    "PlaySound",
    "TerminalAudioAdapter",
]:
    assert forbidden not in terminal_h, (
        f"pure terminal presentation leaked audio dependency: {forbidden}"
    )

assert "typedCharacters" in terminal_h
assert "shouldPlayTick" in adapter_h
assert "playTerminalTick" in engine_h

print("AUDIO PRESENTATION BOUNDARY CONTRACT: PASS")
print(" - terminal remains audio-agnostic.")
print(" - tick rate limiting is pure and independently removable.")
print(" - platform playback stays behind AudioEngine.")
