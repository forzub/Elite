#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
result_header = ROOT / "src/world/types/SignalReceptionResult.h"
receiver_cpp = ROOT / "src/game/equipment/signalNode/processing/SignalReceiver.cpp"
presentation_cpp = ROOT / "src/game/ship/sensors/ShipSignalPresentation.cpp"
awareness_h = ROOT / "src/game/ship/sensors/NpcSignalAwareness.h"

errors = []

result_text = result_header.read_text(encoding="utf-8")
receiver_text = receiver_cpp.read_text(encoding="utf-8")
presentation_text = presentation_cpp.read_text(encoding="utf-8")
awareness_text = awareness_h.read_text(encoding="utf-8")

if re.search(r"WorldSignal\s*\*|WorldSignal\s*&", result_text):
    errors.append("SignalReceptionResult must not contain WorldSignal pointers/references")

if "sourceDisplayClass" not in result_text or "sourceLabel" not in result_text:
    errors.append("SignalReceptionResult must carry snapshot-safe presentation metadata by value")

if "result.source =" in receiver_text:
    errors.append("SignalReceiver must not export a pointer to a WorldSignal")

if "result.source->" in presentation_text:
    errors.append("ShipSignalPresentation must not dereference a server-side WorldSignal pointer")

if "const void* source" in awareness_text or "r.source" in awareness_text:
    errors.append("NpcSignalAwareness must use a stable source identifier, not a pointer")

if '#include "world/types/SignalSemanticState.h"' not in awareness_text:
    errors.append("NpcSignalAwareness must include SignalSemanticState directly instead of relying on transitive includes")

if errors:
    print("Signal reception snapshot contract FAILED:")
    for error in errors:
        print(f"  - {error}")
    sys.exit(1)

print("Signal reception snapshot contract: PASS")
