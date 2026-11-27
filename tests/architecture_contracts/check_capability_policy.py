"""Stabilization stage 9 barrier: the preview package's differences are ONE capability policy.

Only src/core/BuildFlavor.cpp may ask the flavour (isPreviewBuild); every other site asks
core::capabilityAllowed(core::Capability::X) - a named capability with its reason in BuildFlavor.h."""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
bad = []
for d, _, files in os.walk(os.path.join(ROOT, "src")):
    for f in files:
        if not f.endswith((".cpp", ".h")) or f in ("BuildFlavor.cpp", "BuildFlavor.h"):
            continue
        p = os.path.join(d, f)
        for n, line in enumerate(open(p, encoding="utf-8", errors="replace"), 1):
            if re.search(r"\bisPreviewBuild\s*\(", line.split("//")[0]):
                bad.append(f"{os.path.relpath(p, ROOT)}:{n}: {line.strip()[:110]}")
h = open(os.path.join(ROOT, "src", "core", "BuildFlavor.h"), encoding="utf-8").read()
caps = re.findall(r"^\s+(\w+),\s+//", h[h.index("enum class Capability"):h.index("};", h.index("enum class Capability"))], re.M)
if bad:
    print("[FAIL] the flavour asked outside the capability policy:")
    for b in bad:
        print("  " + b)
    sys.exit(1)
print(f"[PASS] capability policy: {len(caps)} named capabilities, the flavour asked only in BuildFlavor.cpp")
