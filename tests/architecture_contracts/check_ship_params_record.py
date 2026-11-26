"""Every ShipParams field is in ShipParamsRecord.h (persisted cases are written by field NAME, never raw bytes)."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
src = (ROOT / "src/game/ship/core/ShipParams.h").read_text(encoding="utf-8")
body = re.search(r"^struct ShipParams\s*\{(.*?)^\};", src, re.S | re.M).group(1)
fields = set(re.findall(r"^\s+[A-Za-z:<>_0-9]+ +([A-Za-z_0-9]+)(?: *=|;| *\{)", body, re.M))
record = (ROOT / "src/game/ship/core/ShipParamsRecord.h").read_text(encoding="utf-8")
listed = set(re.findall(r'f\("([A-Za-z_0-9]+)", p\.\1\)', record))
missing = sorted(fields - listed)
if missing:
    print("[FAIL] ShipParams fields missing from ShipParamsRecord.h (persisted cases would drop them): " + ", ".join(missing))
    sys.exit(1)
case = (ROOT / "src/game/navigation/diagnostics/AutopilotCaseFile.h").read_text(encoding="utf-8")
if "reinterpret_cast<const unsigned char*>(&c.physics)" in case:
    print("[FAIL] AutopilotCaseFile writes ShipParams as raw bytes again")
    sys.exit(1)
print(f"[OK] ShipParamsRecord lists all {len(fields)} ShipParams fields; cases are written by name")
