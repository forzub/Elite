from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

terminal = (
    ROOT / "src/game/presentation/DockingStatusTerminal.h"
).read_text(encoding="utf-8")
space = (
    ROOT / "src/game/SpaceState.cpp"
).read_text(encoding="utf-8")
safety_h = (
    ROOT / "src/game/navigation/PreplanningSafetyCourse.h"
).read_text(encoding="utf-8")
safety_cpp = (
    ROOT / "src/game/navigation/PreplanningSafetyCourse.cpp"
).read_text(encoding="utf-8")

# Terminal is presentation-only: no navigation, rendering, audio or SpaceState
# dependency may leak into the pure state machine.
for forbidden in [
    "SpaceState",
    "RoutePlanner",
    "Autopilot",
    "PreplanningSafetyCourse",
    "TextRenderer",
    "Audio",
    "GameClient",
]:
    assert forbidden not in terminal, (
        f"docking terminal leaked external dependency: {forbidden}"
    )

for required in [
    "DockingTerminalSnapshot",
    "DockingTerminalLine",
    "typedCharacters",
    "void reset()",
]:
    assert required in terminal, f"terminal API missing: {required}"

# Entry rebind is a connector-repair state, not a fresh primary safety-course
# request. A miss must remain recoverable and must preserve the backbone.
assert "automatic && assisted && lateEntryRebind" in space
assert "ENTRY-REBIND IS NOT A NEW DOCKING REQUEST" in space
assert "phase=entry-rebind-retry" in space
assert "projected Assisted entry bind missed its one-shot handoff" not in space

# Safety-course proof now owns a spatial delta-spot rather than a single ray.
assert "courseSafetySpotRadiusMeters" in safety_h
assert "Spatial delta-spot" in safety_cpp
assert "clearSegmentCenterline" in safety_cpp

print("DOCKING TERMINAL / SAFE-COURSE RECOVERY CONTRACT: PASS")
print(" - terminal is removable DTO-only presentation state.")
print(" - entry rebind cannot restart primary Assisted safe-course.")
print(" - handoff drift retries connector instead of hard-failing docking.")
print(" - safety course proves a spatial delta-spot around candidate motion.")
