"""Physical (StarAtlas) and political (GalaxyDatabase) star-system ids are
different namespaces; they must never be joined by a plain int.

Audit 2026-10-29: political ids coincide with physical ids only for Sol (0)
and GJ 876 (13); e.g. political 1 = tau Ceti = physical 8. The political
GalaxyDatabase is not loaded by the game. This check keeps it that way until
an explicit cross-catalog mapping exists:
- no game / world code outside src/galaxy loads or queries GalaxyDatabase;
- world-state system ids are validated against the StarAtlas on the server.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
violations = []
for path in list((ROOT / "src").rglob("*.cpp")) + list((ROOT / "src").rglob("*.h")):
    rel = path.relative_to(ROOT).as_posix()
    if rel.startswith("src/galaxy/"):
        continue
    text = path.read_text(encoding="utf-8", errors="replace")
    for pattern in (r"GalaxyDatabase\s*[&*]?\s*\w+\s*[;({=]", r"\.loadFromDirectory\s*\(", r"galaxy/GalaxyDatabase\.h"):
        for m in re.finditer(pattern, text):
            line = text.count("\n", 0, m.start()) + 1
            violations.append(f"{rel}:{line}: {m.group(0).strip()}")

server = (ROOT / "src/game/server/GameServer.cpp").read_text(encoding="utf-8")
if "system_states references a physical system absent from StarAtlas" not in server:
    violations.append("GameServer.cpp: world-state system ids are not validated against StarAtlas")

if violations:
    print("[FAIL] physical / political system id namespaces:")
    for v in violations:
        print("  " + v)
    sys.exit(1)
print("[PASS] physical StarAtlas ids and political GalaxyDatabase ids are never joined implicitly")
