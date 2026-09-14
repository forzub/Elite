#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
BRIDGE_INCLUDE = '#include "src/render/legacy/CoreGlLegacyBridge.h"\n'
BRIDGE_PATH = "src/render/legacy/CoreGlLegacyBridge.h"
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp"}

CALL_REPLACEMENTS = {
    "glBegin": "elite::render::core_legacy::begin",
    "glEnd": "elite::render::core_legacy::end",
    "glColor3f": "elite::render::core_legacy::color3f",
    "glColor4f": "elite::render::core_legacy::color4f",
    "glLoadIdentity": "elite::render::core_legacy::loadIdentity",
    "glLoadMatrixf": "elite::render::core_legacy::loadMatrixf",
    "glMatrixMode": "elite::render::core_legacy::matrixMode",
    "glOrtho": "elite::render::core_legacy::ortho",
    "glPopMatrix": "elite::render::core_legacy::popMatrix",
    "glPushMatrix": "elite::render::core_legacy::pushMatrix",
    "glTexCoord2d": "elite::render::core_legacy::texCoord2d",
    "glTexCoord2f": "elite::render::core_legacy::texCoord2f",
    "glVertex2d": "elite::render::core_legacy::vertex2d",
    "glVertex2f": "elite::render::core_legacy::vertex2f",
    "glVertex3f": "elite::render::core_legacy::vertex3f",
}

CONSTANT_REPLACEMENTS = {
    "GL_MODELVIEW": "elite::render::core_legacy::ModelViewToken",
    "GL_PROJECTION": "elite::render::core_legacy::ProjectionToken",
    "GL_MATRIX_MODE": "elite::render::core_legacy::MatrixModeToken",
    "GL_CURRENT_COLOR": "elite::render::core_legacy::CurrentColorToken",
    "GL_QUADS": "elite::render::core_legacy::QuadsToken",
}


def insert_bridge_include(text: str) -> str:
    if BRIDGE_INCLUDE.strip() in text:
        return text
    return BRIDGE_INCLUDE + text


def migrate_source(path: Path) -> bool:
    rel = path.relative_to(ROOT).as_posix()
    if rel == BRIDGE_PATH:
        return False

    original = path.read_text(encoding="utf-8")
    text = original
    changed = False

    texture_rules = [
        (
            re.compile(r"\bglIsEnabled\s*\(\s*GL_TEXTURE_2D\s*\)"),
            "elite::render::core_legacy::texture2DEnabled()",
        ),
        (
            re.compile(r"\bglEnable\s*\(\s*GL_TEXTURE_2D\s*\)"),
            "elite::render::core_legacy::enableTexture2D(true)",
        ),
        (
            re.compile(r"\bglDisable\s*\(\s*GL_TEXTURE_2D\s*\)"),
            "elite::render::core_legacy::enableTexture2D(false)",
        ),
    ]
    for pattern, replacement in texture_rules:
        text, count = pattern.subn(replacement, text)
        changed = changed or count > 0

    for old, new in CALL_REPLACEMENTS.items():
        pattern = re.compile(rf"\b{re.escape(old)}\s*\(")
        text, count = pattern.subn(new + "(", text)
        changed = changed or count > 0

    had_matrix_query = "GL_MATRIX_MODE" in text
    had_current_color = "GL_CURRENT_COLOR" in text

    for old, new in CONSTANT_REPLACEMENTS.items():
        pattern = re.compile(rf"\b{re.escape(old)}\b")
        text, count = pattern.subn(new, text)
        changed = changed or count > 0

    if had_matrix_query:
        text, count = re.subn(
            r"\bglGetIntegerv\s*\(",
            "elite::render::core_legacy::getIntegerv(",
            text,
        )
        changed = changed or count > 0

    if had_current_color:
        text, count = re.subn(
            r"\bglGetFloatv\s*\(",
            "elite::render::core_legacy::getFloatv(",
            text,
        )
        changed = changed or count > 0

    if changed:
        text = insert_bridge_include(text)
        path.write_text(text, encoding="utf-8")
    return changed


def migrate_window() -> None:
    path = ROOT / "src/window/Window.cpp"
    text = path.read_text(encoding="utf-8")
    text = text.replace(
        "glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);",
        "glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);",
    )
    text = text.replace(
        "// GPU modernization baseline. Compatibility profile is intentional for\n"
        "    // the transition: a few legacy render paths still use fixed-function\n"
        "    // calls, while OpenGL 4.3 already exposes compute shaders and SSBOs.\n",
        "// OpenGL 4.3 Core is the production client baseline. Legacy presentation\n"
        "    // syntax is translated by CoreGlLegacyBridge into GLSL/VAO/VBO submission;\n"
        "    // the driver no longer exposes or owns fixed-function state.\n",
    )
    text = text.replace(
        "// Load the complete 4.3 compatibility entry-point set before any\n"
        "        // client renderer starts. The capability gate prints the exact\n"
        "        // driver/runtime limits and fails early below the supported floor.\n",
        "// Load the OpenGL 4.3 Core entry-point set before any client renderer\n"
        "        // starts. The capability gate prints the exact driver/runtime limits\n"
        "        // and fails early below the supported floor.\n",
    )
    path.write_text(text, encoding="utf-8")


def write_final_contract() -> None:
    path = ROOT / "tests/architecture_contracts/check_gl43_modernization_boundary.py"
    path.write_text(r'''#!/usr/bin/env python3
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
        r"\bgl(?:Enable|Disable|IsEnabled)\s*\(\s*GL_TEXTURE_2D\s*\)"
    ),
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

bridge = (ROOT / "src/render/legacy/CoreGlLegacyBridge.h").read_text(encoding="utf-8")
assert "#version 430 core" in bridge
assert "glDrawArrays" in bridge
assert "QuadsToken" in bridge

print("GL43 CORE MODERNIZATION BOUNDARY: PASS")
print(" - GLFW requests OpenGL 4.3 Core Profile")
print(" - bundled GLAD is generated for gl:core=4.3")
print(" - production src/ has zero forbidden compatibility-only API tokens")
print(" - legacy presentation syntax is translated by the Core GLSL/VAO/VBO bridge")
''', encoding="utf-8")


def main() -> None:
    changed_files = []
    for path in SRC.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        if migrate_source(path):
            changed_files.append(path.relative_to(ROOT).as_posix())

    migrate_window()
    write_final_contract()

    workflow = ROOT / ".github/workflows/gl43-core-migration.yml"
    if workflow.exists():
        workflow.unlink()

    print("GL43 core source migration completed")
    for path in changed_files:
        print(" -", path)


if __name__ == "__main__":
    main()
