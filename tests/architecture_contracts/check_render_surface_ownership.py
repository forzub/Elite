#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

def fail(message):
    print(f"[FAIL] render-surface ownership: {message}", file=sys.stderr)
    raise SystemExit(1)

renderer = read("src/render/Renderer.cpp")
app = read("src/core/Application.cpp")
space = read("src/game/SpaceState.cpp")

begin_start = renderer.find("bool Renderer::beginPostProcess(")
begin_end = renderer.find("\nvoid Renderer::endPostProcess", begin_start)
if begin_start < 0 or begin_end < 0:
    fail("Renderer::beginPostProcess body not found")
begin = renderer[begin_start:begin_end]

bind = begin.find("glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer)")
clear = begin.find("glClear(", bind)
active = begin.find("m_postProcessActive = true", bind)
if bind < 0 or clear < 0 or active < 0 or not (bind < clear < active):
    fail("persistent scene FBO is not cleared before becoming active")

for token in (
    "glDisable(GL_SCISSOR_TEST)",
    "glViewport(0, 0, framebufferWidth, framebufferHeight)",
    "GL_COLOR_BUFFER_BIT",
    "GL_DEPTH_BUFFER_BIT",
    "GL_STENCIL_BUFFER_BIT",
    "gameViewportWidth",
    "glScissor(",
    "gameViewportHeight",
):
    if token not in begin:
        fail(f"full-FBO clear/viewport restore contract missing: {token}")

# The active-session surface is now single-owner OpenGL. Only true 3D scene
# domains (Flight and Navigation) may enter the cinematic off-screen chain.
# Native F5-F8 services render directly into the freshly-cleared default
# framebuffer, so an old Flight/Map texture cannot become their backing.
scene = app.find("const bool scene3dActive =")
gate = app.find("const bool postProcessActive =", scene)
if scene < 0 or gate < 0:
    fail("single-surface scene/post-process activation seam not found")
gate_body = app[scene:gate + 420]
for token in (
    "flightMode || systemMapMode",
    "scene3dActive &&",
    "m_renderer.beginPostProcess(",
):
    if token not in gate_body:
        fail(f"single-surface post-process ownership contract missing: {token}")
if "serviceMode" in gate_body.split("const bool scene3dActive =", 1)[1].split("const bool postProcessActive =", 1)[0]:
    fail("native F5-F8 service presentation incorrectly participates in the persistent post-process FBO")

# F1-F4 are a domain exit from Navigation. A half-finished internal map
# transition must be cancelled even when the requested flight layout equals the
# already selected layout.
flight_start = space.find("void SpaceState::setFlightScreenLayout")
flight_end = space.find("\n}", flight_start)
flight = space[flight_start:flight_end + 2]
cancel = flight.find("m_systemMapRenderer.cancelMapTransition()")
early_return = flight.find("if (m_layout == layout)")
if cancel < 0 or early_return < 0 or cancel > early_return:
    fail("Flight selector can leave an old map transition alive behind the next frame")

print("[PASS] render surface is frame-owned: no stale post-process/map backing can leak between targets")
