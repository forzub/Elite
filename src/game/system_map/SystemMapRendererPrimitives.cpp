// SystemMapRenderer: GL resources, shaders and the batched primitives (lines, circles, solids, billboards, volume sprites, textured bodies).

#include "src/game/system_map/SystemMapRendererInternal.h"
























void SystemMapRenderer::ensureGlObjects()
{
    if (m_vao && m_vbo)
        return;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, pos))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}







void SystemMapRenderer::createTexturedSphereMesh(
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

    // The proven map-body shader receives color at attribute 2. The static
    // sphere has no per-vertex color payload, so keep the array disabled and
    // supply one constant generic attribute per body draw.
    glDisableVertexAttribArray(2);

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


void SystemMapRenderer::ensureShader()
{
    if (m_shader)
        return;

    m_shader = ShaderLibrary::instance().get("system_map_lines");

    if (!m_shader)
    {

        return;
    }



    m_mvpLoc = glGetUniformLocation(m_shader, "uMVP");
}






void SystemMapRenderer::ensureTexturedShader()
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
}










void SystemMapRenderer::beginLines()
{
    m_vertices.clear();
}

void SystemMapRenderer::addLine(
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec4& color
)
{
    m_vertices.push_back({ a, color });
    m_vertices.push_back({ b, color });
}

void SystemMapRenderer::addCircleXZ(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments = std::max(12, segments);

    for (int i = 0; i < segments; ++i)
    {
        const float a0 = float(i) / float(segments) * glm::two_pi<float>();
        const float a1 = float(i + 1) / float(segments) * glm::two_pi<float>();

        glm::vec3 p0 =
            center + glm::vec3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);

        glm::vec3 p1 =
            center + glm::vec3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);

        addLine(p0, p1, color);
    }
}




void SystemMapRenderer::addCircleXY(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments = std::max(12, segments);

    for (int i = 0; i < segments; ++i)
    {
        const float a0 = float(i) / float(segments) * glm::two_pi<float>();
        const float a1 = float(i + 1) / float(segments) * glm::two_pi<float>();

        glm::vec3 p0 = center + glm::vec3(std::cos(a0) * radius, std::sin(a0) * radius, 0.0f);
        glm::vec3 p1 = center + glm::vec3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f);

        addLine(p0, p1, color);
    }
}



void SystemMapRenderer::addOrbitCircle3D(
    const glm::vec3& center,
    float radius,
    double inclinationDeg,
    double longitudeOfAscendingNodeDeg,
    double argumentOfPeriapsisDeg,
    const glm::vec4& color,
    int segments
)
{
    if (radius <= 0.0f)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glm::dmat4 rot(
        1.0
    );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                longitudeOfAscendingNodeDeg
            ),
            glm::dvec3(
                0.0,
                1.0,
                0.0
            )
        );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                inclinationDeg
            ),
            glm::dvec3(
                1.0,
                0.0,
                0.0
            )
        );

    rot =
        glm::rotate(
            rot,
            degToRadD(
                argumentOfPeriapsisDeg
            ),
            glm::dvec3(
                0.0,
                1.0,
                0.0
            )
        );

    for (int i = 0; i < segments; ++i)
    {
        const double a0 =
            static_cast<double>(i) /
            static_cast<double>(segments) *
            glm::two_pi<double>();

        const double a1 =
            static_cast<double>(i + 1) /
            static_cast<double>(segments) *
            glm::two_pi<double>();

        const glm::dvec3 local0(
            std::cos(a0) *
                static_cast<double>(radius),
            0.0,
            std::sin(a0) *
                static_cast<double>(radius)
        );

        const glm::dvec3 local1(
            std::cos(a1) *
                static_cast<double>(radius),
            0.0,
            std::sin(a1) *
                static_cast<double>(radius)
        );

        const glm::dvec3 rotated0 =
            glm::dvec3(
                rot *
                glm::dvec4(
                    local0,
                    0.0
                )
            );

        const glm::dvec3 rotated1 =
            glm::dvec3(
                rot *
                glm::dvec4(
                    local1,
                    0.0
                )
            );

        addLine(
            center +
                glm::vec3(
                    static_cast<float>(rotated0.x),
                    static_cast<float>(rotated0.y),
                    static_cast<float>(rotated0.z)
                ),
            center +
                glm::vec3(
                    static_cast<float>(rotated1.x),
                    static_cast<float>(rotated1.y),
                    static_cast<float>(rotated1.z)
                ),
            color
        );
    }
}














void SystemMapRenderer::beginSolids()
{
    m_solidVertices.clear();
}

void SystemMapRenderer::beginVolumes()
{
    m_volumeVertices.clear();
}

void SystemMapRenderer::addVolumeSprite(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view
)
{
    if (radius <= 0.0f || color.a <= 0.0f)
        return;

    const glm::vec3 right {view[0][0], view[1][0], view[2][0]};
    const glm::vec3 up {view[0][1], view[1][1], view[2][1]};
    const glm::vec4 rim {color.r, color.g, color.b, 0.0f};

    // Soft disc: full alpha at the centre, zero at the rim (6-segment fan).
    constexpr int segments = 6;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = 6.28318530718f * static_cast<float>(i) / segments;
        const float a1 = 6.28318530718f * static_cast<float>(i + 1) / segments;
        m_volumeVertices.push_back({center, color});
        m_volumeVertices.push_back({center + (std::cos(a0) * right + std::sin(a0) * up) * radius, rim});
        m_volumeVertices.push_back({center + (std::cos(a1) * right + std::sin(a1) * up) * radius, rim});
    }
}

void SystemMapRenderer::flushVolumes(const glm::mat4& mvp)
{
    if (!m_shader || !m_vao || !m_vbo || m_volumeVertices.empty())
        return;

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthMaskWasEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_volumeVertices.size() * sizeof(Vertex)),
        m_volumeVertices.data(),
        GL_DYNAMIC_DRAW
    );

    // Unsorted translucent sprites: no depth test against each other, no
    // depth writes; everything drawn afterwards stays on top.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_volumeVertices.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthMask(depthMaskWasEnabled);
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    if (!blendWasEnabled)
        glDisable(GL_BLEND);

    m_volumeVertices.clear();
}

void SystemMapRenderer::drawMediumShells(
    const world::celestial::GalaxyMapMediumAtlas& atlas,
    const std::vector<world::celestial::GalaxyMapMediumAtlas::Range>& ranges,
    const glm::mat4& mvpLy
)
{
    if (!m_shader || ranges.empty() || atlas.vertices.empty())
        return;

    using AtlasVertex = world::celestial::GalaxyMapMediumAtlas::Vertex;
    if (!m_mediumVao)
    {
        glGenVertexArrays(1, &m_mediumVao);
        glGenBuffers(1, &m_mediumVbo);
        glBindVertexArray(m_mediumVao);
        glBindBuffer(GL_ARRAY_BUFFER, m_mediumVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(AtlasVertex),
                              reinterpret_cast<void*>(offsetof(AtlasVertex, positionLy)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(AtlasVertex),
                              reinterpret_cast<void*>(offsetof(AtlasVertex, rgba)));
        glBindVertexArray(0);
        m_mediumAtlasUid = 0;
    }
    if (m_mediumAtlasUid != atlas.uid)
    {
        // Once per catalog snapshot, never per frame.
        glBindBuffer(GL_ARRAY_BUFFER, m_mediumVbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(atlas.vertices.size() * sizeof(AtlasVertex)),
                     atlas.vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        m_mediumAtlasUid = atlas.uid;
    }

    m_mediumFirst.clear();
    m_mediumCount.clear();
    for (const auto& range : ranges)
        if (range.count > 0)
        {
            m_mediumFirst.push_back(static_cast<int>(range.first));
            m_mediumCount.push_back(static_cast<int>(range.count));
        }
    if (m_mediumFirst.empty())
        return;

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWasEnabled = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskWasEnabled);

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvpLy));
    glBindVertexArray(m_mediumVao);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); // premultiplied
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glMultiDrawArrays(GL_TRIANGLES, m_mediumFirst.data(), m_mediumCount.data(),
                      static_cast<GLsizei>(m_mediumFirst.size()));

    glBindVertexArray(0);
    glUseProgram(0);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (!cullWasEnabled)
        glDisable(GL_CULL_FACE);
    glDepthMask(depthMaskWasEnabled);
    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    if (!blendWasEnabled)
        glDisable(GL_BLEND);
}

void SystemMapRenderer::addBillboardBall(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view,
    int segments
)
{
    if (radius <= 0.0f || segments < 8)
        return;

    const glm::vec3 right {
        view[0][0],
        view[1][0],
        view[2][0]
    };

    const glm::vec3 up {
        view[0][1],
        view[1][1],
        view[2][1]
    };

    const glm::vec4 coreColor {
        color.r,
        color.g,
        color.b,
        std::min(color.a, 0.92f)
    };

    const glm::vec4 edgeColor {
        color.r,
        color.g,
        color.b,
        std::min(color.a * 0.55f, 0.55f)
    };

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);

        const float a1 =
            6.28318530718f * static_cast<float>(i + 1) / static_cast<float>(segments);

        const glm::vec3 p0 =
            center + (std::cos(a0) * right + std::sin(a0) * up) * radius;

        const glm::vec3 p1 =
            center + (std::cos(a1) * right + std::sin(a1) * up) * radius;

        m_solidVertices.push_back({ center, coreColor });
        m_solidVertices.push_back({ p0, edgeColor });
        m_solidVertices.push_back({ p1, edgeColor });
    }
}























void SystemMapRenderer::flushSolids(const glm::mat4& mvp)
{
    if (!m_shader || !m_vao || !m_vbo || m_solidVertices.empty())
        return;

    GLboolean depthWasEnabled =
    glIsEnabled(GL_DEPTH_TEST);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    GLboolean depthMaskWasEnabled =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldDepthFunc =
        GL_LESS;

    glGetIntegerv(
        GL_DEPTH_FUNC,
        &oldDepthFunc
    );

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_solidVertices.size() * sizeof(Vertex)),
        m_solidVertices.data(),
        GL_DYNAMIC_DRAW
    );

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);



    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_solidVertices.size()));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthFunc(
        oldDepthFunc
    );

    glDepthMask(
        depthMaskWasEnabled
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




void SystemMapRenderer::beginTexturedBodies()
{
    for (auto& batch : m_texturedBatches)
    {
        batch.bodies.clear();
    }
}


void SystemMapRenderer::flushTexturedBodies(
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

            glm::mat4 bodyModel(1.0f);
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


void SystemMapRenderer::addCross(
    const glm::vec3& center,
    float size,
    const glm::vec4& color
)
{
    addLine(
        center + glm::vec3(-size, 0.0f, 0.0f),
        center + glm::vec3( size, 0.0f, 0.0f),
        color
    );

    addLine(
        center + glm::vec3(0.0f, -size, 0.0f),
        center + glm::vec3(0.0f,  size, 0.0f),
        color
    );

    addLine(
        center + glm::vec3(0.0f, 0.0f, -size),
        center + glm::vec3(0.0f, 0.0f,  size),
        color
    );
}

void SystemMapRenderer::flushLines(const glm::mat4& mvp)
{
    if (!m_shader || !m_vao || !m_vbo || m_vertices.empty())
        return;

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    GLfloat oldLineWidth = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &oldLineWidth);

    glUseProgram(m_shader);
    glUniformMatrix4fv(m_mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
        m_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDisable(GL_DEPTH_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.5f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertices.size()));

    glLineWidth(oldLineWidth);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}
