#include "src/render/planet/PlanetWireRenderer.h"

#include <algorithm>
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

    float wrapLonDeg(float lon)
    {
        while (lon < -180.0f)
            lon += 360.0f;

        while (lon > 180.0f)
            lon -= 360.0f;

        return lon;
    }

    float lonDistanceDeg(float a, float b)
    {
        return wrapLonDeg(a - b);
    }

    float gaussian2D(
        float lat,
        float lon,
        float centerLat,
        float centerLon,
        float latScale,
        float lonScale,
        float amplitude
    )
    {
        const float dLat = (lat - centerLat) / latScale;
        const float dLon = lonDistanceDeg(lon, centerLon) / lonScale;

        return amplitude * std::exp(-(dLat * dLat + dLon * dLon));
    }

    float ridge2D(
        float lat,
        float lon,
        float centerLat,
        float centerLon,
        float latScale,
        float lonScale,
        float amplitude
    )
    {
        const float dLat = std::abs((lat - centerLat) / latScale);
        const float dLon = std::abs(lonDistanceDeg(lon, centerLon) / lonScale);

        const float d = dLat + dLon * 0.35f;

        return amplitude * std::exp(-d * d);
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

    const glm::vec4 oceanGridColor  {0.34f, 0.34f, 0.34f, 0.18f};
    const glm::vec4 majorGridColor  {0.68f, 0.68f, 0.68f, 0.32f};
    const glm::vec4 atmosphereColor {0.95f, 0.95f, 0.95f, 0.22f};

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


    // Внутренние топографические линии: равнины, плато, горы.
    buildEarthTopography(
        staticLines,
        EarthRadiusM * 1.004f
    );

    // Основные горные хребты.
    buildEarthMountainRanges(
        staticLines,
        EarthRadiusM * 1.007f
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
    const glm::vec4 coastColor {0.92f, 0.92f, 0.92f, 0.88f};

    const float threshold = 0.0f;

    const float latStep = 2.0f;
    const float lonStep = 2.0f;

    for (float lat = -82.0f; lat <= 82.0f; lat += latStep)
    {
        for (float lon = -180.0f; lon < 180.0f; lon += lonStep)
        {
            const bool land00 = isEarthLand(lat, lon);
            const bool land10 = isEarthLand(lat + latStep, lon);
            const bool land01 = isEarthLand(lat, lon + lonStep);

            const glm::vec3 p00 = spherePoint(radius, lat, lon);
            const glm::vec3 p10 = spherePoint(radius, lat + latStep, lon);
            const glm::vec3 p01 = spherePoint(radius, lat, lon + lonStep);

            // Вертикальная граница ячейки.
            if (land00 != land10)
            {
                addLine(
                    out,
                    p00,
                    p10,
                    coastColor
                );
            }

            // Горизонтальная граница ячейки.
            if (land00 != land01)
            {
                addLine(
                    out,
                    p00,
                    p01,
                    coastColor
                );
            }
        }
    }
}



float PlanetWireRenderer::earthField(
    float latitudeDeg,
    float longitudeDeg
) const
{
    const float lat = latitudeDeg;
    const float lon = wrapLonDeg(longitudeDeg);

    float v = -0.42f;

    // ============================================================
    // Большие узнаваемые массы суши.
    // Это не реальная карта, а процедурная карикатура Земли.
    // ============================================================

    // Северная Америка.
    v += gaussian2D(lat, lon, 48.0f, -105.0f, 24.0f, 38.0f, 1.15f);
    v += gaussian2D(lat, lon, 33.0f,  -95.0f, 17.0f, 28.0f, 0.55f);
    v += gaussian2D(lat, lon, 61.0f, -135.0f, 16.0f, 28.0f, 0.62f);

    // Центральная Америка.
    v += gaussian2D(lat, lon, 17.0f, -88.0f,  7.0f, 24.0f, 0.44f);

    // Южная Америка.
    v += gaussian2D(lat, lon, -14.0f, -60.0f, 27.0f, 20.0f, 1.05f);
    v += gaussian2D(lat, lon, -42.0f, -68.0f, 19.0f, 13.0f, 0.48f);

    // Европа.
    v += gaussian2D(lat, lon, 51.0f,  15.0f, 15.0f, 24.0f, 0.72f);
    v += gaussian2D(lat, lon, 60.0f,  25.0f, 12.0f, 20.0f, 0.34f);

    // Африка.
    v += gaussian2D(lat, lon,  5.0f,  20.0f, 30.0f, 23.0f, 1.10f);
    v += gaussian2D(lat, lon, -25.0f, 25.0f, 16.0f, 16.0f, 0.54f);

    // Азия.
    v += gaussian2D(lat, lon, 48.0f,  80.0f, 25.0f, 55.0f, 1.25f);
    v += gaussian2D(lat, lon, 25.0f,  78.0f, 17.0f, 25.0f, 0.62f);
    v += gaussian2D(lat, lon, 58.0f, 120.0f, 19.0f, 32.0f, 0.62f);

    // Индия.
    v += gaussian2D(lat, lon, 19.0f, 78.0f, 10.0f, 9.0f, 0.46f);

    // Юго-восточная Азия.
    v += gaussian2D(lat, lon, 10.0f, 105.0f, 12.0f, 18.0f, 0.35f);

    // Австралия.
    v += gaussian2D(lat, lon, -25.0f, 134.0f, 13.0f, 22.0f, 0.72f);

    // Гренландия.
    v += gaussian2D(lat, lon, 73.0f, -42.0f, 10.0f, 18.0f, 0.46f);

    // Антарктида — кольцо внизу.
    v += gaussian2D(lat, lon, -83.0f, 0.0f, 10.0f, 220.0f, 0.90f);

    // ============================================================
    // Океанические разрывы, чтобы материки не слипались.
    // ============================================================

    // Атлантика.
    v -= gaussian2D(lat, lon,  20.0f, -35.0f, 42.0f, 18.0f, 0.65f);
    v -= gaussian2D(lat, lon, -25.0f, -25.0f, 32.0f, 20.0f, 0.45f);

    // Индийский океан.
    v -= gaussian2D(lat, lon, -18.0f,  78.0f, 24.0f, 26.0f, 0.56f);

    // Тихий океан.
    v -= gaussian2D(lat, lon,  10.0f, 170.0f, 55.0f, 42.0f, 0.80f);
    v -= gaussian2D(lat, lon,   0.0f,-155.0f, 45.0f, 35.0f, 0.65f);

    // Средиземноморский разрез.
    v -= gaussian2D(lat, lon, 36.0f, 18.0f, 5.0f, 22.0f, 0.32f);

    // Немного рваности береговой линии.
    const glm::vec3 p = glm::normalize(spherePoint(1.0f, lat, lon));

    v += planetNoise(p) * 0.09f;
    v += std::sin(degToRad(lat * 7.0f + lon * 1.3f)) * 0.035f;
    v += std::sin(degToRad(lon * 5.0f - lat * 2.0f)) * 0.025f;

    return v;
}

float PlanetWireRenderer::earthHeight(
    float latitudeDeg,
    float longitudeDeg
) const
{
    const float field = earthField(latitudeDeg, longitudeDeg);

    if (field <= 0.0f)
        return 0.0f;

    float h = field;

    // Крупные горные зоны.
    h += ridge2D(latitudeDeg, longitudeDeg, -22.0f, -70.0f, 40.0f,  7.0f, 0.90f); // Анды
    h += ridge2D(latitudeDeg, longitudeDeg,  44.0f,-115.0f, 28.0f,  9.0f, 0.45f); // Скалистые
    h += ridge2D(latitudeDeg, longitudeDeg,  46.0f,  10.0f,  8.0f, 18.0f, 0.50f); // Альпы
    h += ridge2D(latitudeDeg, longitudeDeg,  31.0f,  82.0f,  8.0f, 30.0f, 0.90f); // Гималаи
    h += ridge2D(latitudeDeg, longitudeDeg,  10.0f,  38.0f, 18.0f,  8.0f, 0.45f); // Восточная Африка
    h += ridge2D(latitudeDeg, longitudeDeg, -24.0f, 132.0f, 12.0f, 18.0f, 0.22f); // Австралийское плато

    const glm::vec3 p = glm::normalize(spherePoint(1.0f, latitudeDeg, longitudeDeg));

    h += planetNoise(p * 2.0f) * 0.08f;

    return std::clamp(h, 0.0f, 1.8f);
}

bool PlanetWireRenderer::isEarthLand(
    float latitudeDeg,
    float longitudeDeg
) const
{
    return earthField(latitudeDeg, longitudeDeg) > 0.0f;
}





void PlanetWireRenderer::buildEarthTopography(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 lowContourColor  {0.50f, 0.50f, 0.50f, 0.24f};
    const glm::vec4 midContourColor  {0.70f, 0.70f, 0.70f, 0.34f};
    const glm::vec4 highContourColor {0.92f, 0.92f, 0.92f, 0.52f};

    const float latStep = 2.0f;
    const float lonStep = 2.0f;

    const float levels[] =
    {
        0.18f,
        0.30f,
        0.45f,
        0.62f,
        0.82f,
        1.05f,
        1.30f
    };

    for (float level : levels)
    {
        glm::vec4 color = lowContourColor;

        if (level > 0.55f)
            color = midContourColor;

        if (level > 0.95f)
            color = highContourColor;

        for (float lat = -80.0f; lat <= 80.0f; lat += latStep)
        {
            for (float lon = -180.0f; lon < 180.0f; lon += lonStep)
            {
                const float h00 = earthHeight(lat, lon);
                const float h10 = earthHeight(lat + latStep, lon);
                const float h01 = earthHeight(lat, lon + lonStep);

                const bool crossLat = (h00 < level && h10 >= level) ||
                                      (h00 >= level && h10 < level);

                const bool crossLon = (h00 < level && h01 >= level) ||
                                      (h00 >= level && h01 < level);

                const float elevationLift =
                    1.0f + level * 0.004f;

                if (crossLat)
                {
                    addLine(
                        out,
                        spherePoint(radius * elevationLift, lat, lon),
                        spherePoint(radius * elevationLift, lat + latStep, lon),
                        color
                    );
                }

                if (crossLon)
                {
                    addLine(
                        out,
                        spherePoint(radius * elevationLift, lat, lon),
                        spherePoint(radius * elevationLift, lat, lon + lonStep),
                        color
                    );
                }
            }
        }
    }
}





void PlanetWireRenderer::buildEarthMountainRanges(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 mountainColor {1.0f, 1.0f, 1.0f, 0.72f};

    auto addRange =
        [&](float lat0, float lon0, float lat1, float lon1, int segments)
        {
            for (int i = 0; i < segments; ++i)
            {
                const float t0 = float(i) / float(segments);
                const float t1 = float(i + 1) / float(segments);

                const float latA = lat0 + (lat1 - lat0) * t0;
                const float lonA = lon0 + (lon1 - lon0) * t0;

                const float latB = lat0 + (lat1 - lat0) * t1;
                const float lonB = lon0 + (lon1 - lon0) * t1;

                const float wiggleA =
                    std::sin(t0 * Pi * 9.0f) * 1.8f;

                const float wiggleB =
                    std::sin(t1 * Pi * 9.0f) * 1.8f;

                addLine(
                    out,
                    spherePoint(radius * 1.010f, latA + wiggleA, lonA),
                    spherePoint(radius * 1.010f, latB + wiggleB, lonB),
                    mountainColor
                );
            }
        };

    // Анды.
    addRange(  8.0f, -78.0f, -53.0f, -71.0f, 42);

    // Скалистые горы.
    addRange( 58.0f, -132.0f, 30.0f, -106.0f, 28);

    // Альпы.
    addRange( 45.0f, 5.0f, 47.0f, 18.0f, 16);

    // Кавказ / Передняя Азия.
    addRange( 42.0f, 38.0f, 35.0f, 58.0f, 18);

    // Гималаи.
    addRange( 30.0f, 70.0f, 34.0f, 100.0f, 30);

    // Восточно-Африканский разлом.
    addRange( 12.0f, 36.0f, -16.0f, 34.0f, 24);

    // Большой водораздел Австралии.
    addRange(-15.0f, 145.0f, -38.0f, 148.0f, 20);
}







void PlanetWireRenderer::buildCloudLayer(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 cloudColor {0.86f, 0.86f, 0.86f, 0.22f};

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