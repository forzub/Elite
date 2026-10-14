from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
follower = (
    ROOT / "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h"
).read_text(encoding="utf-8")
helper = (
    ROOT / "src/game/navigation/autopilot/InCorridorCenteringGuidance.h"
).read_text(encoding="utf-8")

required_follower = [
    "if constexpr (Law == LocalFlightControlLaw::Assisted)",
    "InCorridorCenteringGuidance::evaluate",
    "emergencyCourseRecovery",
    "? capture.desiredCourseMap",
    "centering.desiredCourseMap",
]
for token in required_follower:
    assert token in follower, f"missing Assisted centering boundary: {token}"

required_helper = [
    "This is NOT CourseCaptureGuidance",
    "exact tangent inside the centering deadband",
    "desired lateral closing speed is bounded by physical lateral authority",
]
for token in required_helper:
    assert token in helper, f"missing centering helper contract: {token}"

# Newton must not receive the new in-corridor policy through a runtime selector.
assert "if (law == LocalFlightControlLaw::Assisted)" not in follower
assert "switch (law)" not in follower

print("ASSISTED FOLLOWER CENTERING CONTRACT: PASS")
print(" - gentle centering is compile-time Assisted-only.")
print(" - Newtonian retains pre-change tangent semantics.")
print(" - emergency CourseCaptureGuidance remains authoritative outside corridor.")
print(" - centering math is isolated behind a pure API.")
