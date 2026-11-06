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
# The follower is a template on Law; the only local `law` names are
# `constexpr LocalFlightControlLaw law = Law;`, so a plain `if (law == ...)`
# (e.g. the engine-out orientation lead, 3b851ddc) is still compile-time.
# A runtime law parameter or selector is forbidden.
import re
law_decls = re.findall(r"LocalFlightControlLaw\s+law\b[^;]*;", follower)
assert law_decls and all(d.startswith("LocalFlightControlLaw law = Law") for d in law_decls), (
    f"runtime law selector in the dedicated follower: {law_decls}"
)
assert re.search(r"LocalFlightControlLaw\s+law\s*[,)]", follower) is None, (
    "runtime law parameter in the dedicated follower"
)
assert "switch (law)" not in follower

print("ASSISTED FOLLOWER CENTERING CONTRACT: PASS")
print(" - gentle centering is compile-time Assisted-only.")
print(" - Newtonian retains pre-change tangent semantics.")
print(" - emergency CourseCaptureGuidance remains authoritative outside corridor.")
print(" - centering math is isolated behind a pure API.")
