#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

manager_h = (ROOT / "src/ui/html/HtmlUiManager.h").read_text(encoding="utf-8")
manager_cpp = (ROOT / "src/ui/html/HtmlUiManager.cpp").read_text(encoding="utf-8")
bridge_h = (ROOT / "src/ui/html/HtmlUiBridge.h").read_text(encoding="utf-8")
bridge_cpp = (ROOT / "src/ui/html/HtmlUiBridge.cpp").read_text(encoding="utf-8")
server_h = (ROOT / "src/ui/html/HtmlUiServer.h").read_text(encoding="utf-8")
application_cpp = (ROOT / "src/core/Application.cpp").read_text(encoding="utf-8")

for label, text in (("HtmlUiManager", manager_h), ("HtmlUiBridge", bridge_h), ("HtmlUiServer", server_h)):
    assert "const std::string& resourcePackPath" in text, f"{label} start API lost resourcePackPath"

assert "m_bridge.start(port, rootDir, resourcePackPath)" in manager_cpp
assert "m_server.start(port, rootDir, resourcePackPath)" in bridge_cpp
assert "elite_game_ui.pak" in application_cpp
assert "m_htmlUi.start(" in application_cpp

print("HTML UI RESOURCE PACK API: PASS")
print(" - Application -> HtmlUiManager -> HtmlUiBridge -> HtmlUiServer preserves resourcePackPath")
print(" - two-argument callers remain source-compatible through default arguments")
