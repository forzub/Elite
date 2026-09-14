#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    if text.count(old) != 1:
        raise RuntimeError(f"{path}: expected exactly one match, got {text.count(old)}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")

# Reuse the old, already runtime-proven shader interface. The optimization is
# static GPU geometry; it does not require a new shader ABI. Per-body transforms
# are folded into uMVP instead.
vert = ROOT / "src/assets/shaders/system_map/map_body_preview.vert"
vert.write_text("""#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColor;

uniform mat4 uMVP;

out vec2 vUv;
out vec4 vColor;

void main()
{
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUv = aUv;
    vColor = aColor;
}
""", encoding="utf-8")

header = ROOT / "src/game/system_map/SystemMapRenderer.h"
replace_once(
    header,
    """    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;
    GLint  m_texturedCenterLoc = -1;
    GLint  m_texturedRadiusLoc = -1;
    GLint  m_texturedPrimeAxisLoc = -1;
    GLint  m_texturedNorthAxisLoc = -1;
    GLint  m_texturedEastAxisLoc = -1;
    GLint  m_texturedColorLoc = -1;
""",
    """    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;
""",
)

cpp = ROOT / "src/game/system_map/SystemMapRenderer.cpp"
replace_once(
    cpp,
    """    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedSphereVertex),
        reinterpret_cast<void*>(
            offsetof(TexturedSphereVertex, uv)
        )
    );

    mesh.indexCount = static_cast<GLsizei>(indices.size());
""",
    """    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedSphereVertex),
        reinterpret_cast<void*>(
            offsetof(TexturedSphereVertex, uv)
        )
    );

    // The proven map-body shader receives color at attribute 2. The static
    // sphere has no per-vertex color payload, so keep the array disabled and
    // supply one constant generic attribute per body draw.
    glDisableVertexAttribArray(2);

    mesh.indexCount = static_cast<GLsizei>(indices.size());
""",
)

replace_once(
    cpp,
    """    m_texturedMvpLoc =
        glGetUniformLocation(m_texturedShader, \"uMVP\");
    m_texturedSamplerLoc =
        glGetUniformLocation(m_texturedShader, \"uTexture\");
    m_texturedCenterLoc =
        glGetUniformLocation(m_texturedShader, \"uBodyCenter\");
    m_texturedRadiusLoc =
        glGetUniformLocation(m_texturedShader, \"uBodyRadius\");
    m_texturedPrimeAxisLoc =
        glGetUniformLocation(m_texturedShader, \"uPrimeAxis\");
    m_texturedNorthAxisLoc =
        glGetUniformLocation(m_texturedShader, \"uNorthAxis\");
    m_texturedEastAxisLoc =
        glGetUniformLocation(m_texturedShader, \"uEastAxis\");
    m_texturedColorLoc =
        glGetUniformLocation(m_texturedShader, \"uColor\");
""",
    """    m_texturedMvpLoc =
        glGetUniformLocation(m_texturedShader, \"uMVP\");
    m_texturedSamplerLoc =
        glGetUniformLocation(m_texturedShader, \"uTexture\");
""",
)

replace_once(
    cpp,
    """    glUseProgram(m_texturedShader);
    glUniformMatrix4fv(
        m_texturedMvpLoc,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );
    glUniform1i(m_texturedSamplerLoc, 0);
""",
    """    glUseProgram(m_texturedShader);
    glUniform1i(m_texturedSamplerLoc, 0);
""",
)

replace_once(
    cpp,
    """            glUniform3fv(
                m_texturedCenterLoc,
                1,
                glm::value_ptr(draw.center)
            );
            glUniform1f(
                m_texturedRadiusLoc,
                draw.radius
            );
            glUniform3fv(
                m_texturedPrimeAxisLoc,
                1,
                glm::value_ptr(draw.primeAxis)
            );
            glUniform3fv(
                m_texturedNorthAxisLoc,
                1,
                glm::value_ptr(draw.northAxis)
            );
            glUniform3fv(
                m_texturedEastAxisLoc,
                1,
                glm::value_ptr(draw.eastAxis)
            );
            glUniform4fv(
                m_texturedColorLoc,
                1,
                glm::value_ptr(draw.color)
            );

            glBindVertexArray(mesh.vao);
            glDrawElements(
""",
    """            glm::mat4 bodyModel(1.0f);
            bodyModel[0] = glm::vec4(
                draw.primeAxis * draw.radius,
                0.0f
            );
            bodyModel[1] = glm::vec4(
                draw.northAxis * draw.radius,
                0.0f
            );
            bodyModel[2] = glm::vec4(
                draw.eastAxis * draw.radius,
                0.0f
            );
            bodyModel[3] = glm::vec4(
                draw.center,
                1.0f
            );

            const glm::mat4 bodyMvp =
                mvp * bodyModel;

            glUniformMatrix4fv(
                m_texturedMvpLoc,
                1,
                GL_FALSE,
                glm::value_ptr(bodyMvp)
            );

            glBindVertexArray(mesh.vao);
            // Explicit rebind is redundant for a valid VAO but makes the
            // indexed resource ownership unambiguous across surrounding passes.
            glBindBuffer(
                GL_ELEMENT_ARRAY_BUFFER,
                mesh.indexBuffer
            );
            glVertexAttrib4fv(
                2,
                glm::value_ptr(draw.color)
            );
            glDrawElements(
""",
)

# Strengthen the architecture contract around the runtime-proven shader ABI.
test = ROOT / "tests/architecture_contracts/check_system_map_static_sphere.py"
text = test.read_text(encoding="utf-8")
old = """for token in (
    \"#version 430 core\",
    \"aUnitPosition\",
    \"uBodyCenter\",
    \"uBodyRadius\",
    \"uPrimeAxis\",
    \"uNorthAxis\",
    \"uEastAxis\",
    \"uColor\",
):
    assert token in vert, token

print(\"SYSTEM MAP STATIC TEXTURED SPHERE: PASS\")
print(\" - 24x48 and 64x128 indexed sphere meshes are one-time GL_STATIC_DRAW resources\")
print(\" - per-frame textured body path records only body parameters\")
print(\" - draw path uses glDrawElements and performs no dynamic full-sphere upload\")
print(\" - rotation phase / longitude offset are folded into the per-body basis\")
"""
new = """for token in (
    \"#version 330 core\",
    \"aPos\",
    \"aUv\",
    \"aColor\",
    \"uniform mat4 uMVP\",
):
    assert token in vert, token

for retired in (
    \"uBodyCenter\",
    \"uBodyRadius\",
    \"uPrimeAxis\",
    \"uNorthAxis\",
    \"uEastAxis\",
    \"uColor\",
):
    assert retired not in vert, retired

assert \"bodyMvp\" in flush_body
assert \"mvp * bodyModel\" in flush_body
assert \"glVertexAttrib4fv\" in flush_body
assert \"GL_ELEMENT_ARRAY_BUFFER\" in flush_body

print(\"SYSTEM MAP STATIC TEXTURED SPHERE: PASS\")
print(\" - 24x48 and 64x128 indexed sphere meshes are one-time GL_STATIC_DRAW resources\")
print(\" - per-frame textured body path records only body parameters\")
print(\" - draw path uses glDrawElements and performs no dynamic full-sphere upload\")
print(\" - runtime-proven map-body shader ABI is preserved; body transform is folded into uMVP\")
print(\" - rotation phase / longitude offset are folded into the per-body basis\")
"""
if text.count(old) != 1:
    raise RuntimeError("static sphere test: expected contract block not found exactly once")
test.write_text(text.replace(old, new, 1), encoding="utf-8")

print("Applied P0 System Map static-sphere shader-contract fix")
