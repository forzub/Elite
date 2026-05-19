#include "src/render/planet/PlanetWireRenderer.h"

#include <cmath>
#include <iostream>

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderLibrary.h"

namespace
{
constexpr float EarthRadiusM = 6371000.0f;
constexpr float Pi = 3.14159265358979323846f;

float degToRad(float deg)
{
    return deg * Pi / 180.0f;
}

float fract(float x)
{
    return x - std::floor(x);
}
}

bool PlanetWireRenderer::initialize()
{
    if (m_initialized)
        return true;

    if (!gladLoadGL(glfwGetProcAddress))
    {
        std::cerr << "[PlanetWireRenderer] failed to load OpenGL functions" << std::endl;
        return false;
    }

    m_shader = ShaderLibrary::instance().get("debug_lines");

    if (m_shader == 0)
    {
        std::cerr << "[PlanetWireRenderer] debug_lines shader not found" << std::endl;
        return false;
    }

    m_mvpLocation = glGetUniformLocation(m_shader, "MVP");

    buildMeshes();

    m_initialized = true;

    std::cerr
        << "[PlanetWireRenderer] initialized. staticVertices="
        << m_staticVertexCount
        << " cloudVertices="
        << m_cloudVertexCount
        << std::endl;

    return true;
}

void PlanetWireRenderer::buildMeshes()
{
    std::vector<PlanetLineVertex> staticLines;
    std::vector<PlanetLineVertex> cloudLines;

    staticLines.reserve(20000);
    cloudLines.reserve(8000);

    const glm::vec4 oceanGridColor  {0.08f, 0.32f, 0.55f, 0.40f};
    const glm::vec4 majorGridColor  {0.18f, 0.65f, 0.95f, 0.55f};
    const glm::vec4 continentColor  {0.45f, 0.95f, 0.70f, 0.78f};
    const glm::vec4 atmosphereColor {0.20f, 0.62f, 1.00f, 0.28f};

    // Широты.
    for (float lat = -80.0f; lat <= 80.0f; lat += 10.0f)
    {
        const bool major = std::abs(lat) < 0.01f || std::fmod(std::abs(lat), 30.0f) < 0.01f;

        addLatitudeLine(
            staticLines,
            EarthRadiusM,
            lat,
            192,
            major ? majorGridColor : oceanGridColor
        );
    }

    // Долготы.
    for (float lon = 0.0f; lon < 360.0f; lon += 10.0f)
    {
        const bool major = std::fmod(lon, 30.0f) < 0.01f;

        addLongitudeLine(
            staticLines,
            EarthRadiusM,
            lon,
            192,
            major ? majorGridColor : oceanGridColor
        );
    }

    // Процедурные контуры материков.
    buildProceduralContinents(
        staticLines,
        EarthRadiusM * 1.002f
    );

    // Атмосфера — не шар, а несколько слабых оболочек-окружностей.
    addGreatCircleXz(staticLines, EarthRadiusM * 1.035f, 256, atmosphereColor);
    addGreatCircleXy(staticLines, EarthRadiusM * 1.035f, 256, atmosphereColor);
    addGreatCircleYz(staticLines, EarthRadiusM * 1.035f, 256, atmosphereColor);

    addGreatCircleXz(staticLines, EarthRadiusM * 1.075f, 256, glm::vec4(0.10f, 0.40f, 0.95f, 0.12f));
    addGreatCircleXy(staticLines, EarthRadiusM * 1.075f, 256, glm::vec4(0.10f, 0.40f, 0.95f, 0.12f));
    addGreatCircleYz(staticLines, EarthRadiusM * 1.075f, 256, glm::vec4(0.10f, 0.40f, 0.95f, 0.12f));

    // Облака — отдельный слой, он будет вращаться.
    buildCloudLayer(
        cloudLines,
        EarthRadiusM * 1.018f
    );

    uploadBuffer(
        m_staticVao,
        m_staticVbo,
        staticLines,
        m_staticVertexCount
    );

    uploadBuffer(
        m_cloudVao,
        m_cloudVbo,
        cloudLines,
        m_cloudVertexCount
    );
}

void PlanetWireRenderer::render(
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& planetCenter,
    float timeSeconds
)
{
    if (!m_initialized)
        return;

    GLboolean depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    GLfloat oldLineWidth = 1.0f;
    glGetFloatv(GL_LINE_WIDTH, &oldLineWidth);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 planetModel =
        glm::translate(glm::mat4(1.0f), planetCenter);

    glm::mat4 planetMvp =
        proj * view * planetModel;

    drawBuffer(
        m_staticVao,
        m_staticVertexCount,
        planetMvp,
        1.0f
    );

    // Облака вращаем отдельно, чуть медленнее и под наклоном.
    glm::mat4 cloudModel =
        glm::translate(glm::mat4(1.0f), planetCenter) *
        glm::rotate(glm::mat4(1.0f), degToRad(23.4f), glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::rotate(glm::mat4(1.0f), timeSeconds * 0.015f, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 cloudMvp =
        proj * view * cloudModel;

    drawBuffer(
        m_cloudVao,
        m_cloudVertexCount,
        cloudMvp,
        1.0f
    );

    glLineWidth(oldLineWidth);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthTestWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glUseProgram(0);
}

void PlanetWireRenderer::uploadBuffer(
    GLuint& vao,
    GLuint& vbo,
    const std::vector<PlanetLineVertex>& vertices,
    GLsizei& outVertexCount
)
{
    outVertexCount =
        static_cast<GLsizei>(vertices.size());

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(PlanetLineVertex),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(PlanetLineVertex),
        reinterpret_cast<void*>(offsetof(PlanetLineVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(PlanetLineVertex),
        reinterpret_cast<void*>(offsetof(PlanetLineVertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void PlanetWireRenderer::drawBuffer(
    GLuint vao,
    GLsizei vertexCount,
    const glm::mat4& mvp,
    float lineWidth
)
{
    if (vao == 0 || vertexCount <= 0)
        return;

    glUseProgram(m_shader);
    glUniformMatrix4fv(
        m_mvpLocation,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );

    glLineWidth(lineWidth);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, vertexCount);
    glBindVertexArray(0);
}

void PlanetWireRenderer::addLine(
    std::vector<PlanetLineVertex>& out,
    const glm::vec3& a,
    const glm::vec3& b,
    const glm::vec4& color
)
{
    out.push_back({a, color});
    out.push_back({b, color});
}

glm::vec3 PlanetWireRenderer::spherePoint(
    float radius,
    float latitudeDeg,
    float longitudeDeg
) const
{
    const float lat = degToRad(latitudeDeg);
    const float lon = degToRad(longitudeDeg);

    const float c = std::cos(lat);

    return glm::vec3(
        radius * c * std::cos(lon),
        radius * std::sin(lat),
        radius * c * std::sin(lon)
    );
}

void PlanetWireRenderer::addLatitudeLine(
    std::vector<PlanetLineVertex>& out,
    float radius,
    float latitudeDeg,
    int segments,
    const glm::vec4& color
)
{
    for (int i = 0; i < segments; ++i)
    {
        const float lon0 = 360.0f * float(i) / float(segments);
        const float lon1 = 360.0f * float(i + 1) / float(segments);

        addLine(
            out,
            spherePoint(radius, latitudeDeg, lon0),
            spherePoint(radius, latitudeDeg, lon1),
            color
        );
    }
}

void PlanetWireRenderer::addLongitudeLine(
    std::vector<PlanetLineVertex>& out,
    float radius,
    float longitudeDeg,
    int segments,
    const glm::vec4& color
)
{
    for (int i = 0; i < segments; ++i)
    {
        const float lat0 = -90.0f + 180.0f * float(i) / float(segments);
        const float lat1 = -90.0f + 180.0f * float(i + 1) / float(segments);

        addLine(
            out,
            spherePoint(radius, lat0, longitudeDeg),
            spherePoint(radius, lat1, longitudeDeg),
            color
        );
    }
}

void PlanetWireRenderer::addGreatCircleXz(
    std::vector<PlanetLineVertex>& out,
    float radius,
    int segments,
    const glm::vec4& color
)
{
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = 2.0f * Pi * float(i) / float(segments);
        const float a1 = 2.0f * Pi * float(i + 1) / float(segments);

        addLine(
            out,
            {std::cos(a0) * radius, 0.0f, std::sin(a0) * radius},
            {std::cos(a1) * radius, 0.0f, std::sin(a1) * radius},
            color
        );
    }
}

void PlanetWireRenderer::addGreatCircleXy(
    std::vector<PlanetLineVertex>& out,
    float radius,
    int segments,
    const glm::vec4& color
)
{
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = 2.0f * Pi * float(i) / float(segments);
        const float a1 = 2.0f * Pi * float(i + 1) / float(segments);

        addLine(
            out,
            {std::cos(a0) * radius, std::sin(a0) * radius, 0.0f},
            {std::cos(a1) * radius, std::sin(a1) * radius, 0.0f},
            color
        );
    }
}

void PlanetWireRenderer::addGreatCircleYz(
    std::vector<PlanetLineVertex>& out,
    float radius,
    int segments,
    const glm::vec4& color
)
{
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = 2.0f * Pi * float(i) / float(segments);
        const float a1 = 2.0f * Pi * float(i + 1) / float(segments);

        addLine(
            out,
            {0.0f, std::cos(a0) * radius, std::sin(a0) * radius},
            {0.0f, std::cos(a1) * radius, std::sin(a1) * radius},
            color
        );
    }
}

float PlanetWireRenderer::planetNoise(const glm::vec3& p) const
{
    // Дешёвый процедурный шум без зависимостей.
    // Не Perlin, не магия NASA, но для проволочных материков хватает.
    const glm::vec3 n = glm::normalize(p);

    float v = 0.0f;

    v += std::sin(n.x * 8.1f  + n.y * 3.7f  + n.z * 5.3f) * 0.55f;
    v += std::sin(n.x * 17.4f - n.y * 11.2f + n.z * 9.1f) * 0.28f;
    v += std::sin(n.x * 31.7f + n.y * 23.5f - n.z * 19.8f) * 0.14f;
    v += std::sin((n.x + n.z) * 47.0f + n.y * 13.0f) * 0.08f;

    return v;
}

void PlanetWireRenderer::buildProceduralContinents(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 continentColor {0.45f, 0.95f, 0.70f, 0.78f};

    const float threshold = 0.10f;

    const float latStep = 3.0f;
    const float lonStep = 3.0f;

    for (float lat = -78.0f; lat <= 78.0f; lat += latStep)
    {
        for (float lon = 0.0f; lon < 360.0f; lon += lonStep)
        {
            const glm::vec3 p00 = spherePoint(radius, lat, lon);
            const glm::vec3 p10 = spherePoint(radius, lat + latStep, lon);
            const glm::vec3 p01 = spherePoint(radius, lat, lon + lonStep);

            const bool land00 = planetNoise(p00) > threshold;
            const bool land10 = planetNoise(p10) > threshold;
            const bool land01 = planetNoise(p01) > threshold;

            // Рисуем границу там, где соседние точки меняют состояние суша/океан.
            if (land00 != land10)
            {
                addLine(
                    out,
                    p00,
                    p10,
                    continentColor
                );
            }

            if (land00 != land01)
            {
                addLine(
                    out,
                    p00,
                    p01,
                    continentColor
                );
            }
        }
    }
}

void PlanetWireRenderer::buildCloudLayer(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 cloudColor {0.75f, 0.92f, 1.00f, 0.34f};

    const float latStep = 5.0f;
    const float lonStep = 5.0f;

    for (float lat = -70.0f; lat <= 70.0f; lat += latStep)
    {
        for (float lon = 0.0f; lon < 360.0f; lon += lonStep)
        {
            const glm::vec3 p = spherePoint(radius, lat, lon);

            float v =
                std::sin(glm::normalize(p).x * 22.0f + glm::normalize(p).z * 15.0f) +
                std::sin(glm::normalize(p).y * 37.0f - glm::normalize(p).x * 11.0f);

            if (v < 1.0f)
                continue;

            const glm::vec3 p0 = spherePoint(radius, lat, lon);
            const glm::vec3 p1 = spherePoint(radius, lat + latStep * 0.45f, lon + lonStep * 1.8f);

            addLine(
                out,
                p0,
                p1,
                cloudColor
            );
        }
    }
}