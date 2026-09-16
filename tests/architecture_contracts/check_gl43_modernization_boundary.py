#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}

COMPATIBILITY_PATTERNS = {
    "glBegin": re.compile(r"\bglBegin\s*\("),
    "glEnd": re.compile(r"\bglEnd\s*\("),
    "glVertex-immediate": re.compile(r"\bglVertex(?:2|3|4)[a-zA-Z]*\s*\("),
    "glColor-immediate": re.compile(r"\bglColor(?:3|4)[a-zA-Z]*\s*\("),
    "glTexCoord-immediate": re.compile(r"\bglTexCoord[1-4][a-zA-Z]*\s*\("),
    "glNormal-immediate": re.compile(r"\bglNormal3[a-zA-Z]*\s*\("),
    "glMatrixMode": re.compile(r"\bglMatrixMode\s*\("),
    "glPushMatrix": re.compile(r"\bglPushMatrix\s*\("),
    "glPopMatrix": re.compile(r"\bglPopMatrix\s*\("),
    "glLoadIdentity": re.compile(r"\bglLoadIdentity\s*\("),
    "glLoadMatrix": re.compile(r"\bglLoadMatrix[a-zA-Z]*\s*\("),
    "glMultMatrix": re.compile(r"\bglMultMatrix[a-zA-Z]*\s*\("),
    "glOrtho": re.compile(r"\bglOrtho\s*\("),
    "glPushAttrib": re.compile(r"\bglPushAttrib\s*\("),
    "glPopAttrib": re.compile(r"\bglPopAttrib\s*\("),
    "glPushClientAttrib": re.compile(r"\bglPushClientAttrib\s*\("),
    "glPopClientAttrib": re.compile(r"\bglPopClientAttrib\s*\("),
    "GL_VIEWPORT_BIT": re.compile(r"\bGL_VIEWPORT_BIT\b"),
    "GL_TRANSFORM_BIT": re.compile(r"\bGL_TRANSFORM_BIT\b"),
    "GL_CURRENT_COLOR": re.compile(r"\bGL_CURRENT_COLOR\b"),
    "GL_MODELVIEW": re.compile(r"\bGL_MODELVIEW\b"),
    "GL_PROJECTION": re.compile(r"\bGL_PROJECTION\b"),
    "GL_MATRIX_MODE": re.compile(r"\bGL_MATRIX_MODE\b"),
    "glVertexPointer": re.compile(r"\bglVertexPointer\s*\("),
    "glColorPointer": re.compile(r"\bglColorPointer\s*\("),
    "glTexCoordPointer": re.compile(r"\bglTexCoordPointer\s*\("),
    "glEnableClientState": re.compile(r"\bglEnableClientState\s*\("),
    "glDisableClientState": re.compile(r"\bglDisableClientState\s*\("),
    "fixed-texture-enable": re.compile(
        r"\bgl(?:Enable|Disable|IsEnabled)\s*\(\s*GL_TEXTURE_2D\s*\)"
    ),
    "fixed-texture-env-call": re.compile(r"\bgl(?:Get)?TexEnv[a-zA-Z]*\s*\("),
    "fixed-alpha-test-call": re.compile(r"\bglAlphaFunc\s*\("),
    "GL_TEXTURE_ENV": re.compile(r"\bGL_TEXTURE_ENV(?:_MODE)?\b"),
    "GL_MODULATE": re.compile(r"\bGL_MODULATE\b"),
    "GL_ALPHA_TEST": re.compile(r"\bGL_ALPHA_TEST(?:_FUNC|_REF)?\b"),
}


def strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)

inventory = {}
for path in SRC.rglob("*"):
    if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
        continue
    relative = path.relative_to(ROOT).as_posix()
    source = strip_cpp_comments(path.read_text(encoding="utf-8"))
    hits = {}
    for name, pattern in COMPATIBILITY_PATTERNS.items():
        count = len(pattern.findall(source))
        if count:
            hits[name] = count
    if hits:
        inventory[relative] = hits

assert not inventory, "Compatibility-only OpenGL remains: " + repr(inventory)

window = (ROOT / "src/window/Window.cpp").read_text(encoding="utf-8")
assert "glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);" in window
assert "glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);" in window
assert "glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);" in window
assert "GLFW_OPENGL_COMPAT_PROFILE" not in window

glad_header = (ROOT / "glad/include/glad/gl.h").read_text(encoding="utf-8")
assert "gl:core=4.3" in glad_header
assert "gl:compatibility=4.3" not in glad_header

# Mechanical Core-header coverage: every raw OpenGL function/constant used by
# production sources must actually exist in the bundled Core 4.3 GLAD header.
# Local helpers that intentionally begin with `gl` are explicitly excluded.
LOCAL_GL_HELPERS = {"glString"}
missing_core_symbols = {}
for path in SRC.rglob("*"):
    if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
        continue
    relative = path.relative_to(ROOT).as_posix()
    source = strip_cpp_comments(path.read_text(encoding="utf-8"))
    functions = sorted(set(re.findall(r"\b(gl[A-Z][A-Za-z0-9_]*)\s*\(", source)))
    constants = sorted(set(re.findall(r"\b(GL_[A-Z0-9_]+)\b", source)))
    missing = [
        token for token in functions + constants
        if token not in glad_header and token not in LOCAL_GL_HELPERS
    ]
    if missing:
        missing_core_symbols[relative] = missing

assert not missing_core_symbols, (
    "Production code references symbols absent from bundled OpenGL 4.3 Core GLAD: "
    + repr(missing_core_symbols)
)

runtime_caps_h = (ROOT / "src/render/gpu/GlRuntimeCapabilities.h").read_text(encoding="utf-8")
runtime_caps_cpp = (ROOT / "src/render/gpu/GlRuntimeCapabilities.cpp").read_text(encoding="utf-8")
assert "bool coreProfile = false;" in runtime_caps_h
assert "compatibilityProfile" not in runtime_caps_h
assert "GL_CONTEXT_CORE_PROFILE_BIT" in runtime_caps_cpp
assert "GL_CONTEXT_COMPATIBILITY_PROFILE_BIT" not in runtime_caps_cpp
assert "requires OpenGL 4.3+ core profile" in runtime_caps_cpp

bridge = (ROOT / "src/render/legacy/CoreGlLegacyBridge.h").read_text(encoding="utf-8")
assert "#version 430 core" in bridge
assert "glDrawArrays" in bridge
assert "QuadsToken" in bridge

print("GL43 CORE MODERNIZATION BOUNDARY: PASS")
print(" - GLFW requests OpenGL 4.3 Core Profile")
print(" - bundled GLAD is generated for gl:core=4.3")
print(" - production src/ has zero forbidden compatibility-only API tokens")
print(" - legacy matrix/current-color presentation syntax is translated by the Core GLSL/VAO/VBO bridge")
print(" - compatibility attribute stacks are forbidden; viewport/matrix state must be restored explicitly")
