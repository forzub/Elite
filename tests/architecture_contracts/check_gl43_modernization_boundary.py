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
        r"\bgl(?:Enable|Disable)\s*\(\s*GL_TEXTURE_2D\s*\)"
    ),
}

IMMEDIATE_MODE_PATTERNS = {
    name: pattern
    for name, pattern in COMPATIBILITY_PATTERNS.items()
    if name in {
        "glBegin",
        "glEnd",
        "glVertex-immediate",
        "glColor-immediate",
        "glTexCoord-immediate",
        "glNormal-immediate",
    }
}

# This set grows monotonically as migration waves retire legacy submission.
# A listed file may still contain another explicitly documented compatibility
# dependency, but immediate-mode submission is forbidden from returning.
NO_IMMEDIATE_MODE_FILES = {
    "src/game/system_map/LocalMapPrimitiveRenderer.cpp",
}

# These files have crossed the stronger boundary: no compatibility-only API
# from the inventory above may return at all.
NO_COMPATIBILITY_FILES = {
    "src/game/system_map/DetailMapGeometryPass.cpp",
}


def strip_cpp_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


def production_sources():
    for path in SRC.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        relative = path.relative_to(ROOT).as_posix()
        yield relative, strip_cpp_comments(path.read_text(encoding="utf-8"))


inventory = {}

for relative, source in production_sources():
    hits = {}
    for name, pattern in COMPATIBILITY_PATTERNS.items():
        count = len(pattern.findall(source))
        if count:
            hits[name] = count
    if hits:
        inventory[relative] = hits

for relative in sorted(NO_IMMEDIATE_MODE_FILES):
    source = strip_cpp_comments(
        (ROOT / relative).read_text(encoding="utf-8")
    )
    for name, pattern in IMMEDIATE_MODE_PATTERNS.items():
        assert not pattern.search(source), (
            f"{relative} reintroduced retired immediate-mode API: {name}"
        )

for relative in sorted(NO_COMPATIBILITY_FILES):
    source = strip_cpp_comments(
        (ROOT / relative).read_text(encoding="utf-8")
    )
    for name, pattern in COMPATIBILITY_PATTERNS.items():
        assert not pattern.search(source), (
            f"{relative} reintroduced compatibility-only API: {name}"
        )

primitive_header = (
    ROOT / "src/game/system_map/LocalMapPrimitiveRenderer.h"
).read_text(encoding="utf-8")
for token in (
    "const glm::vec4& color",
    "drawLocalMapLine",
    "drawLocalMapCross",
    "drawLocalMapCircle",
):
    assert token in primitive_header, (
        f"LocalMapPrimitiveRenderer lost explicit-color API token: {token}"
    )

window = (ROOT / "src/window/Window.cpp").read_text(encoding="utf-8")
assert "glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);" in window
assert "glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);" in window
assert "glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);" in window

print("GL43 MODERNIZATION BOUNDARY: PASS")
print(" - OpenGL 4.3 Compatibility remains the temporary migration scaffold")
print(" - migrated files cannot reintroduce retired compatibility APIs")
print(" - LocalMapPrimitiveRenderer exposes explicit-color submission")
print(" - current compatibility debt inventory:")

if not inventory:
    print("   <clean>")
else:
    for relative in sorted(inventory):
        summary = ", ".join(
            f"{name}={count}"
            for name, count in sorted(inventory[relative].items())
        )
        print(f"   {relative}: {summary}")
