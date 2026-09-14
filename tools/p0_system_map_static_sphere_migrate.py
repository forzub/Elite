#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, text: str) -> None:
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_between(text: str, start: str, end: str, replacement: str) -> str:
    i = text.index(start)
    j = text.index(end, i)
    return text[:i] + replacement + text[j:]


header_path = "src/game/system_map/SystemMapRenderer.h"
header = read(header_path)
header = header.replace(
'''    struct TexturedVertex
    {
        glm::vec3 pos;
        glm::vec2 uv;
        glm::vec4 color;
    };

    struct TexturedBatch
    {
        GLuint texture = 0;
        std::vector<TexturedVertex> vertices;
    };
''',
'''    struct TexturedSphereVertex
    {
        glm::vec3 unitPosition;
        glm::vec2 uv;
    };

    struct TexturedSphereGpuMesh
    {
        GLuint vao = 0;
        GLuint vertexBuffer = 0;
        GLuint indexBuffer = 0;
        GLsizei indexCount = 0;
    };

    struct TexturedBodyDraw
    {
        glm::vec3 center { 0.0f };
        float radius = 0.0f;
        glm::vec3 primeAxis { 1.0f, 0.0f, 0.0f };
        glm::vec3 northAxis { 0.0f, 1.0f, 0.0f };
        glm::vec3 eastAxis { 0.0f, 0.0f, -1.0f };
        glm::vec4 color { 1.0f };
        bool highResolution = false;
    };

    struct TexturedBatch
    {
        GLuint texture = 0;
        std::vector<TexturedBodyDraw> bodies;
    };
''')
header = header.replace(
'''    void ensureTexturedGlObjects();
    void ensureTexturedShader();
''',
'''    void ensureTexturedGlObjects();
    void createTexturedSphereMesh(
        TexturedSphereGpuMesh& mesh,
        int latitudeSegments,
        int longitudeSegments
    );
    void ensureTexturedShader();
''')
header = header.replace(
'''    GLuint m_texturedVao = 0;
    GLuint m_texturedVbo = 0;
    GLuint m_texturedShader = 0;
    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;
''',
'''    TexturedSphereGpuMesh m_texturedSphereLow;
    TexturedSphereGpuMesh m_texturedSphereHigh;
    GLuint m_texturedShader = 0;
    GLint  m_texturedMvpLoc = -1;
    GLint  m_texturedSamplerLoc = -1;
    GLint  m_texturedCenterLoc = -1;
    GLint  m_texturedRadiusLoc = -1;
    GLint  m_texturedPrimeAxisLoc = -1;
    GLint  m_texturedNorthAxisLoc = -1;
    GLint  m_texturedEastAxisLoc = -1;
    GLint  m_texturedColorLoc = -1;
''')
write(header_path, header)

cpp_path = "src/game/system_map/SystemMapRenderer.cpp"
cpp = read(cpp_path)
new_gl_objects = r'''void SystemMapRenderer::createTexturedSphereMesh(
    TexturedSphereGpuMesh& mesh,
    int latitudeSegments,
    int longitudeSegments
)
{
    if (mesh.vao != 0)
        return;

    latitudeSegments = std::max(latitudeSegments, 8);
    longitudeSegments = std::max(longitudeSegments, 16);

    std::vector<TexturedSphereVertex> vertices;
    vertices.reserve(
        static_cast<std::size_t>(latitudeSegments + 1) *
        static_cast<std::size_t>(longitudeSegments + 1)
    );

    for (int latitudeIndex = 0;
         latitudeIndex <= latitudeSegments;
         ++latitudeIndex)
    {
        const float v =
            static_cast<float>(latitudeIndex) /
            static_cast<float>(latitudeSegments);
        const float latitude =
            -glm::half_pi<float>() + v * glm::pi<float>();
        const float cosLatitude = std::cos(latitude);
        const float sinLatitude = std::sin(latitude);

        for (int longitudeIndex = 0;
             longitudeIndex <= longitudeSegments;
             ++longitudeIndex)
        {
            const float u =
                static_cast<float>(longitudeIndex) /
                static_cast<float>(longitudeSegments);
            const float longitude =
                -glm::pi<float>() + u * glm::two_pi<float>();

            TexturedSphereVertex vertex;
            vertex.unitPosition = glm::vec3(
                cosLatitude * std::cos(longitude),
                sinLatitude,
                cosLatitude * std::sin(longitude)
            );
            vertex.uv = glm::vec2(u, v);
            vertices.push_back(vertex);
        }
    }

    std::vector<std::uint32_t> indices;
    indices.reserve(
        static_cast<std::size_t>(latitudeSegments) *
        static_cast<std::size_t>(longitudeSegments) * 6u
    );

    const int rowStride = longitudeSegments + 1;
    for (int latitudeIndex = 0;
         latitudeIndex < latitudeSegments;
         ++latitudeIndex)
    {
        for (int longitudeIndex = 0;
             longitudeIndex < longitudeSegments;
             ++longitudeIndex)
        {
            const std::uint32_t i00 =
                static_cast<std::uint32_t>(
                    latitudeIndex * rowStride + longitudeIndex
                );
            const std::uint32_t i10 = i00 + 1u;
            const std::uint32_t i01 =
                static_cast<std::uint32_t>(
                    (latitudeIndex + 1) * rowStride + longitudeIndex
                );
            const std::uint32_t i11 = i01 + 1u;

            // Preserve the old System Map winding exactly. The authored
            // prime/north/east basis is left-handed, so this canonical order
            // becomes the same outward CCW shell after the body transform.
            indices.push_back(i00);
            indices.push_back(i10);
            indices.push_back(i11);
            indices.push_back(i00);
            indices.push_back(i11);
            indices.push_back(i01);
        }
    }

    GLint previousVao = 0;
    GLint previousArrayBuffer = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vertexBuffer);
    glGenBuffers(1, &mesh.indexBuffer);

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vertexBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            vertices.size() * sizeof(TexturedSphereVertex)
        ),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.indexBuffer);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            indices.size() * sizeof(std::uint32_t)
        ),
        indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedSphereVertex),
        reinterpret_cast<void*>(
            offsetof(TexturedSphereVertex, unitPosition)
        )
    );

    glEnableVertexAttribArray(1);
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

    glBindBuffer(
        GL_ARRAY_BUFFER,
        static_cast<GLuint>(previousArrayBuffer)
    );
    glBindVertexArray(static_cast<GLuint>(previousVao));
}


void SystemMapRenderer::ensureTexturedGlObjects()
{
    if (m_texturedSphereLow.vao != 0 &&
        m_texturedSphereHigh.vao != 0)
    {
        return;
    }

    // Keep the exact two tessellation levels used by the previous CPU path,
    // but build/upload them only once.
    createTexturedSphereMesh(
        m_texturedSphereLow,
        24,
        48
    );
    createTexturedSphereMesh(
        m_texturedSphereHigh,
        64,
        128
    );
}


'''
cpp = replace_between(
    cpp,
    "void SystemMapRenderer::ensureTexturedGlObjects()",
    "void SystemMapRenderer::ensureShader()",
    new_gl_objects
)
new_shader_init = r'''void SystemMapRenderer::ensureTexturedShader()
{
    if (m_texturedShader)
        return;

    m_texturedShader =
        ShaderLibrary::instance().get("system_map_body_preview");

    if (!m_texturedShader)
    {
        static bool warned = false;

        if (!warned)
        {
            warned = true;
            std::cerr
                << "[SystemMapRenderer] shader system_map_body_preview not available; "
                << "map body previews disabled.\n";
        }
        return;
    }

    m_texturedMvpLoc =
        glGetUniformLocation(m_texturedShader, "uMVP");
    m_texturedSamplerLoc =
        glGetUniformLocation(m_texturedShader, "uTexture");
    m_texturedCenterLoc =
        glGetUniformLocation(m_texturedShader, "uBodyCenter");
    m_texturedRadiusLoc =
        glGetUniformLocation(m_texturedShader, "uBodyRadius");
    m_texturedPrimeAxisLoc =
        glGetUniformLocation(m_texturedShader, "uPrimeAxis");
    m_texturedNorthAxisLoc =
        glGetUniformLocation(m_texturedShader, "uNorthAxis");
    m_texturedEastAxisLoc =
        glGetUniformLocation(m_texturedShader, "uEastAxis");
    m_texturedColorLoc =
        glGetUniformLocation(m_texturedShader, "uColor");
}


'''
cpp = replace_between(
    cpp,
    "void SystemMapRenderer::ensureTexturedShader()",
    "void SystemMapRenderer::resetView()",
    new_shader_init
)
new_begin = r'''void SystemMapRenderer::beginTexturedBodies()
{
    for (auto& batch : m_texturedBatches)
    {
        batch.bodies.clear();
    }
}


'''
cpp = replace_between(
    cpp,
    "void SystemMapRenderer::beginTexturedBodies()",
    "void SystemMapRenderer::flushTexturedBodies(",
    new_begin
)
new_flush = r'''void SystemMapRenderer::flushTexturedBodies(
    const glm::mat4& mvp
)
{
    if (!m_texturedShader ||
        m_texturedSphereLow.vao == 0 ||
        m_texturedSphereHigh.vao == 0 ||
        m_texturedBatches.empty())
    {
        return;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWasEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

    GLint oldDepthFunc = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &oldDepthFunc);

    GLint oldCullFaceMode = GL_BACK;
    glGetIntegerv(GL_CULL_FACE_MODE, &oldCullFaceMode);

    GLint oldFrontFaceMode = GL_CCW;
    glGetIntegerv(GL_FRONT_FACE, &oldFrontFaceMode);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glDisable(GL_BLEND);

    glUseProgram(m_texturedShader);
    glUniformMatrix4fv(
        m_texturedMvpLoc,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );
    glUniform1i(m_texturedSamplerLoc, 0);
    glActiveTexture(GL_TEXTURE0);

    for (const TexturedBatch& batch : m_texturedBatches)
    {
        if (batch.texture == 0 || batch.bodies.empty())
            continue;

        glBindTexture(GL_TEXTURE_2D, batch.texture);

        for (const TexturedBodyDraw& draw : batch.bodies)
        {
            const TexturedSphereGpuMesh& mesh =
                draw.highResolution
                    ? m_texturedSphereHigh
                    : m_texturedSphereLow;

            glUniform3fv(
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
                GL_TRIANGLES,
                mesh.indexCount,
                GL_UNSIGNED_INT,
                nullptr
            );
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthFunc(oldDepthFunc);
    glDepthMask(depthMaskWasEnabled);
    glCullFace(oldCullFaceMode);
    glFrontFace(oldFrontFaceMode);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}


'''
cpp = replace_between(
    cpp,
    "void SystemMapRenderer::flushTexturedBodies(",
    "void SystemMapRenderer::addCross(",
    new_flush
)
write(cpp_path, cpp)

inl_path = "src/game/system_map/SystemMapRendererSystem.inl"
inl = read(inl_path)
inl = inl.replace(
'''        m_texturedShader != 0 &&
        m_texturedVao != 0 &&
        m_texturedVbo != 0)
''',
'''        m_texturedShader != 0 &&
        m_texturedSphereLow.vao != 0 &&
        m_texturedSphereHigh.vao != 0)
''')
new_add_sphere = r'''void SystemMapRenderer::addTexturedSystemBodySphere(
    const world::celestial::SystemMapBody& body,
    GLuint texture,
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int latSegments,
    int lonSegments
)
{
    if (texture == 0 || radius <= 0.0f)
        return;

    latSegments = std::max(latSegments, 8);
    lonSegments = std::max(lonSegments, 16);

    TexturedBatch* batch = nullptr;
    for (auto& candidate : m_texturedBatches)
    {
        if (candidate.texture == texture)
        {
            batch = &candidate;
            break;
        }
    }

    if (!batch)
    {
        TexturedBatch newBatch;
        newBatch.texture = texture;
        m_texturedBatches.push_back(std::move(newBatch));
        batch = &m_texturedBatches.back();
    }

    const glm::dvec3 north =
        systemBodyNorthAxisWorld(body);
    const glm::dvec3 prime0 =
        systemBodyPrimeAxisWorld(north);
    const glm::dvec3 east0 =
        systemBodyEastAxisWorld(north, prime0);

    // The old CPU tessellator added texture longitude offset and rotation phase
    // to every vertex longitude. Fold the same rotation into the per-body basis
    // once, then let the vertex shader transform the resident unit sphere.
    const double longitudePhase =
        degToRadD(body.textureLongitudeOffsetDeg) +
        body.rotationPhaseRad;
    const double phaseCos = std::cos(longitudePhase);
    const double phaseSin = std::sin(longitudePhase);

    const glm::dvec3 prime =
        prime0 * phaseCos +
        east0 * phaseSin;
    const glm::dvec3 east =
        -prime0 * phaseSin +
        east0 * phaseCos;

    TexturedBodyDraw draw;
    draw.center = center;
    draw.radius = radius;
    draw.primeAxis = glm::vec3(prime);
    draw.northAxis = glm::vec3(north);
    draw.eastAxis = glm::vec3(east);
    draw.color = color;

    const std::size_t requestedCells =
        static_cast<std::size_t>(latSegments) *
        static_cast<std::size_t>(lonSegments);
    constexpr std::size_t lowResolutionCells = 24u * 48u;
    draw.highResolution = requestedCells > lowResolutionCells;

    batch->bodies.push_back(draw);
}


'''
inl = replace_between(
    inl,
    "void SystemMapRenderer::addTexturedSystemBodySphere(",
    "// ============================================================================\n// System body visual metrics",
    new_add_sphere
)
write(inl_path, inl)

shader_path = "src/assets/shaders/system_map/map_body_preview.vert"
write(shader_path, '''#version 430 core

layout(location = 0) in vec3 aUnitPosition;
layout(location = 1) in vec2 aUv;

uniform mat4 uMVP;
uniform vec3 uBodyCenter;
uniform float uBodyRadius;
uniform vec3 uPrimeAxis;
uniform vec3 uNorthAxis;
uniform vec3 uEastAxis;
uniform vec4 uColor;

out vec2 vUv;
out vec4 vColor;

void main()
{
    vec3 bodyOffset =
        uPrimeAxis * aUnitPosition.x +
        uNorthAxis * aUnitPosition.y +
        uEastAxis * aUnitPosition.z;

    vec3 worldPosition =
        uBodyCenter + bodyOffset * uBodyRadius;

    gl_Position = uMVP * vec4(worldPosition, 1.0);
    vUv = aUv;
    vColor = uColor;
}
''')

test_path = ROOT / "tests/architecture_contracts/check_system_map_static_sphere.py"
test_path.write_text(r'''#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src/game/system_map/SystemMapRenderer.h").read_text(encoding="utf-8")
cpp = (ROOT / "src/game/system_map/SystemMapRenderer.cpp").read_text(encoding="utf-8")
inl = (ROOT / "src/game/system_map/SystemMapRendererSystem.inl").read_text(encoding="utf-8")
vert = (ROOT / "src/assets/shaders/system_map/map_body_preview.vert").read_text(encoding="utf-8")

assert "TexturedSphereGpuMesh" in header
assert "TexturedBodyDraw" in header
assert "m_texturedSphereLow" in header
assert "m_texturedSphereHigh" in header

create_start = cpp.index("void SystemMapRenderer::createTexturedSphereMesh(")
create_end = cpp.index("void SystemMapRenderer::ensureTexturedGlObjects()", create_start)
create_body = cpp[create_start:create_end]
assert "GL_STATIC_DRAW" in create_body
assert "glDraw" not in create_body
assert "64" in cpp[create_end:cpp.index("void SystemMapRenderer::ensureShader()", create_end)]
assert "128" in cpp[create_end:cpp.index("void SystemMapRenderer::ensureShader()", create_end)]

flush_start = cpp.index("void SystemMapRenderer::flushTexturedBodies(")
flush_end = cpp.index("void SystemMapRenderer::addCross(", flush_start)
flush_body = cpp[flush_start:flush_end]
assert "glDrawElements" in flush_body
assert "GL_DYNAMIC_DRAW" not in flush_body
assert "glBufferData" not in flush_body
assert "batch.vertices" not in flush_body

sphere_start = inl.index("void SystemMapRenderer::addTexturedSystemBodySphere(")
sphere_end = inl.index("// ============================================================================\n// System body visual metrics", sphere_start)
sphere_body = inl[sphere_start:sphere_end]
assert "bodyPoint" not in sphere_body
assert "for (int iy" not in sphere_body
assert "for (int ix" not in sphere_body
assert "rotationPhaseRad" in sphere_body
assert "textureLongitudeOffsetDeg" in sphere_body
assert "batch->bodies.push_back" in sphere_body

for token in (
    "#version 430 core",
    "aUnitPosition",
    "uBodyCenter",
    "uBodyRadius",
    "uPrimeAxis",
    "uNorthAxis",
    "uEastAxis",
    "uColor",
):
    assert token in vert, token

print("SYSTEM MAP STATIC TEXTURED SPHERE: PASS")
print(" - 24x48 and 64x128 indexed sphere meshes are one-time GL_STATIC_DRAW resources")
print(" - per-frame textured body path records only body parameters")
print(" - draw path uses glDrawElements and performs no dynamic full-sphere upload")
print(" - rotation phase / longitude offset are folded into the per-body basis")
''', encoding="utf-8")

# Remove one-shot migration plumbing from the committed tree.
for transient in (
    ROOT / ".github/workflows/p0-system-map-static-sphere-migration.yml",
    ROOT / "tools/p0_system_map_static_sphere_migrate.py",
):
    if transient.exists():
        transient.unlink()

print("P0 System Map static sphere migration applied")
