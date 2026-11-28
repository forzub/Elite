"""Navigation stage 10 barrier: ONE control gate on the client.

Every write of the player's ship control goes through SpaceState::submitShipControl(writer, control), which records the
writer of each frame and reports two different writers in one frame ([ControlOwner] conflict). A direct
m_client->submitInput( anywhere else in the game state code bypasses the gate. The capsule rescue takes part in the
flight-task arbitration (the emergency rule in arbitrateFlightTask)."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "src", "game")
problems = []
for f in sorted(os.listdir(GAME)):
    if not f.endswith(".cpp"):
        continue
    text = open(os.path.join(GAME, f), encoding="utf-8", errors="replace").read()
    gate = text.find("void SpaceState::submitShipControl(")
    for m in re.finditer(r"m_client->submitInput\(", text):
        if gate >= 0 and gate < m.start() < text.find("\n}\n", gate):
            continue   # the gate itself
        line = text.count("\n", 0, m.start()) + 1
        problems.append(f"{f}:{line}: a control write outside submitShipControl")
formation = open(os.path.join(GAME, "SpaceStateFormation.cpp"), encoding="utf-8").read()
if "m_capsuleRescue.ownsControl" not in formation:
    problems.append("SpaceStateFormation.cpp: the capsule rescue is not in the flight-task arbitration")
if problems:
    print("[FAIL] control owner (navigation stage 10):")
    for p in problems:
        print("  " + p)
    sys.exit(1)
print("[PASS] control owner: one control gate (submitShipControl), the capsule in the arbitration")
