"""Stabilization stage 2 barrier: the client's render functions draw - they do not drive state.

Every `void SpaceState::render*(...)` in src/game/SpaceState*.cpp is scanned for writes to SpaceState members
(m_x = ..., m_x.push_back / erase / insert / clear / emplace, ++m_x / m_x++) and for known state drivers (setJumpState,
CockpitRadio update / takeNotice, sending to the server). State derived from the world is computed in the update step
(SpaceState::update -> update*State), so hiding the HUD, a hidden widget, a minimized window, the camera, F-keys or the
Preview flavor cannot change what the game does (the Preview build skipped renderDockingWidget and lost the console line;
2026-11-27).

ALLOW: render-only caches (layout / GPU buffers / draw scratch) - each named with its reason. A write outside the list fails.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
GAME = os.path.join(ROOT, "src", "game")
WRITE = re.compile(r"(?<![\w.>])(m_\w+)(?:\s*(?:\[[^\]]*\])?\s*(?:\.\w+)*\s*=(?!=)|\s*\.(?:push_back|erase|insert|clear|emplace|emplace_back)\s*\(|\s*\+\+|\s*--)|(?:\+\+|--)(m_\w+)")
DRIVERS = re.compile(r"setJumpState\s*\(|radio\.update\s*\(|takeNotice\s*\(|m_client->send|sendCommand\s*\(|submitIntent\s*\(")
ALLOW = {
    "m_activeMainCamera": "the camera the frame is drawn with (render scratch; picking reads the last drawn camera)",
    "m_preparedScene": "the frame's prepared scene (render scratch)",
    "m_mapPrewarmAtMs": "map GPU prewarm scheduling (render-only)",
    "m_jShakePulse": "the impact camera-shake pulse (a visual effect of the drawn frame only)",
    "m_jImpactSerialSeen": "the impact serial the camera shake consumed (visual effect only)",
    "m_cockpitUiLaidOut": "the cockpit UI layout cache (re-laid out on a viewport change)",
    "m_lastHudViewport": "layout cache",
    "m_perfHudMs": "frame timing diagnostics",
    "m_perfRenderMs": "frame timing diagnostics",
}

def functions(text):
    for m in re.finditer(r"^void SpaceState::(render\w*)\([^)]*\)[^\n{]*\n\{", text, re.M):
        start = m.end()
        depth, i = 1, start
        while depth and i < len(text):
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
            i += 1
        yield m.group(1), text.count("\n", 0, m.start()) + 1, text[start:i]

problems = []
count = [0]
for f in sorted(os.listdir(GAME)):
    if not (f.startswith("SpaceState") and f.endswith(".cpp")):
        continue
    text = open(os.path.join(GAME, f), encoding="utf-8", errors="replace").read()
    for name, line0, body in functions(text):
        count[0] += 1
        for n, line in enumerate(body.splitlines(), 1):
            code = line.split("//")[0]
            for w in WRITE.finditer(code):
                member = w.group(1) or w.group(2)
                if member not in ALLOW and not member.startswith("m_perf"):
                    problems.append(f"{f}:{line0 + n}: {name} writes {member}: {line.strip()[:100]}")
            if DRIVERS.search(code):
                problems.append(f"{f}:{line0 + n}: {name} drives state: {line.strip()[:100]}")
if problems:
    print("[FAIL] render functions change state (stabilization stage 2):")
    for p in problems:
        print("  " + p)
    sys.exit(1)
print(f"[PASS] render purity: {count[0]} SpaceState::render* functions write no member (beyond render scratch) and drive nothing")
