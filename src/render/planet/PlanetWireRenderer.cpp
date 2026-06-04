#include "src/render/planet/PlanetWireRenderer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "src/render/ShaderLibrary.h"

namespace
{
    constexpr float EarthRadiusM = 6371000.0f;
    constexpr float Pi = 3.14159265358979323846f;

    std::string findPlanetDataFile(const std::string& relativeFile)
    {
        namespace fs = std::filesystem;

        const fs::path cwd = fs::current_path();

        std::vector<fs::path> candidates =
        {
            cwd / "assets" / "data" / "planets" / relativeFile,
            cwd.parent_path() / "assets" / "data" / "planets" / relativeFile,
            cwd.parent_path() / "src" / "assets" / "data" / "planets" / relativeFile,
            fs::path("D:/__elite/work/src/assets/data/planets") / relativeFile
        };

        for (const auto& p : candidates)
        {
            if (fs::exists(p))
                return p.string();
        }

        std::cerr
            << "[PlanetWireRenderer] planet data file not found: "
            << relativeFile
            << std::endl;

        return relativeFile;
    }

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
    m_solidShader = ShaderLibrary::instance().get("planet_solid");

    if (m_solidShader == 0)
    {
        std::cerr << "[PlanetWireRenderer] planet_solid shader not found" << std::endl;
        return false;
    }

    m_solidModelLocation =
        glGetUniformLocation(m_solidShader, "uModel");

    m_solidViewLocation =
        glGetUniformLocation(m_solidShader, "uView");

    m_solidProjLocation =
        glGetUniformLocation(m_solidShader, "uProj");

    m_solidCameraPosLocation =
        glGetUniformLocation(m_solidShader, "uCameraPos");

    m_solidLightDirLocation =
        glGetUniformLocation(m_solidShader, "uLightDir");


    m_solidBaseColorLocation =
        glGetUniformLocation(m_solidShader, "uBaseColor");

    m_solidLitColorLocation =
        glGetUniformLocation(m_solidShader, "uLitColor");

    m_solidEmissionLocation =
        glGetUniformLocation(m_solidShader, "uEmission");



    if (m_shader == 0)
    {
        std::cerr << "[PlanetWireRenderer] debug_lines shader not found" << std::endl;
        return false;
    }

    m_mvpLocation = glGetUniformLocation(m_shader, "MVP");

    buildSolidSphere();
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

    // ------------------------------------------------------------
    // 1. Азимутальная сетка (широты и долготы) – серый, полупрозрачный
    // ------------------------------------------------------------
    const glm::vec4 gridColor(0.35f, 0.35f, 0.35f, 0.45f);
    const int latSteps = 12;   // шаг 15°
    const int lonSteps = 24;   // шаг 15°
    const int segments = 80;   // гладкость окружностей

    for (int i = 0; i <= latSteps; ++i)
    {
        float lat = -90.0f + 180.0f * i / latSteps;
        if (std::abs(lat) > 85.0f) continue; // не рисуем линии прямо на полюсах
        addLatitudeLine(staticLines, EarthRadiusM * 1.001f, lat, segments, gridColor);
    }
    for (int i = 0; i < lonSteps; ++i)
    {
        float lon = -180.0f + 360.0f * i / lonSteps;
        addLongitudeLine(staticLines, EarthRadiusM * 1.001f, lon, segments, gridColor);
    }

    // ------------------------------------------------------------
    // 2. Контуры материков из JSON (почти белые, яркие)
    // ------------------------------------------------------------
    buildEarthCoastlinesFromJson(staticLines, EarthRadiusM * 1.003f,
                                 findPlanetDataFile("earth_coastlines.json"));

    // ------------------------------------------------------------
    // 3. Загрузка в OpenGL
    // ------------------------------------------------------------
    m_staticVertexCount = static_cast<GLsizei>(staticLines.size());

    glGenVertexArrays(1, &m_staticVao);
    glGenBuffers(1, &m_staticVbo);
    glBindVertexArray(m_staticVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_staticVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 staticLines.size() * sizeof(PlanetLineVertex),
                 staticLines.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          sizeof(PlanetLineVertex),
                          reinterpret_cast<void*>(offsetof(PlanetLineVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE,
                          sizeof(PlanetLineVertex),
                          reinterpret_cast<void*>(offsetof(PlanetLineVertex, color)));

    glBindVertexArray(0);

    // Облака отключаем
    m_cloudVertexCount = 0;
}




















void PlanetWireRenderer::buildSolidSphere()
{
    std::vector<PlanetSolidVertex> vertices;
    std::vector<unsigned int> indices;

    const int latSegments = 64;
    const int lonSegments = 128;

    vertices.reserve((latSegments + 1) * (lonSegments + 1));
    indices.reserve(latSegments * lonSegments * 6);

    for (int y = 0; y <= latSegments; ++y)
    {
        const float v = float(y) / float(latSegments);
        const float lat = -90.0f + 180.0f * v;

        for (int x = 0; x <= lonSegments; ++x)
        {
            const float u = float(x) / float(lonSegments);
            const float lon = -180.0f + 360.0f * u;

            glm::vec3 pos = spherePoint(EarthRadiusM * 0.998f, lat, lon);
            glm::vec3 normal = glm::normalize(pos);

            vertices.push_back({pos, normal});
        }
    }

    for (int y = 0; y < latSegments; ++y)
    {
        for (int x = 0; x < lonSegments; ++x)
        {
            const unsigned int i0 =
                y * (lonSegments + 1) + x;

            const unsigned int i1 =
                i0 + 1;

            const unsigned int i2 =
                i0 + (lonSegments + 1);

            const unsigned int i3 =
                i2 + 1;

            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }

    m_solidIndexCount =
        static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &m_solidVao);
    glGenBuffers(1, &m_solidVbo);
    glGenBuffers(1, &m_solidIbo);

    glBindVertexArray(m_solidVao);

    glBindBuffer(GL_ARRAY_BUFFER, m_solidVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(PlanetSolidVertex),
        vertices.data(),
        GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_solidIbo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(PlanetSolidVertex),
        reinterpret_cast<void*>(offsetof(PlanetSolidVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(PlanetSolidVertex),
        reinterpret_cast<void*>(offsetof(PlanetSolidVertex, normal))
    );

    glBindVertexArray(0);
}

void PlanetWireRenderer::drawSolidSphere(
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& proj
)
{
    if (m_solidVao == 0 || m_solidIndexCount <= 0)
        return;

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glUseProgram(m_solidShader);

    glUniformMatrix4fv(
        m_solidModelLocation,
        1,
        GL_FALSE,
        glm::value_ptr(model)
    );

    glUniformMatrix4fv(
        m_solidViewLocation,
        1,
        GL_FALSE,
        glm::value_ptr(view)
    );

    glUniformMatrix4fv(
        m_solidProjLocation,
        1,
        GL_FALSE,
        glm::value_ptr(proj)
    );

    // Пока камера в мировых координатах не прокинута явно,
    // берём её из inverse(view).
    const glm::mat4 invView = glm::inverse(view);
    const glm::vec3 cameraPos = glm::vec3(invView[3]);

    glUniform3fv(
        m_solidCameraPosLocation,
        1,
        glm::value_ptr(cameraPos)
    );

    glUniform3f(
        m_solidLightDirLocation,
        -0.35f,
        -0.25f,
        -0.75f
    );

    glBindVertexArray(m_solidVao);
    glDrawElements(
        GL_TRIANGLES,
        m_solidIndexCount,
        GL_UNSIGNED_INT,
        nullptr
    );
    glBindVertexArray(0);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
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

    // 1. Сначала непрозрачное тело планеты.


    glUseProgram(m_solidShader);

    if (m_solidBaseColorLocation >= 0)
        glUniform3f(m_solidBaseColorLocation, 0.010f, 0.011f, 0.013f);

    if (m_solidLitColorLocation >= 0)
        glUniform3f(m_solidLitColorLocation, 0.035f, 0.037f, 0.042f);

    if (m_solidEmissionLocation >= 0)
        glUniform1f(m_solidEmissionLocation, 0.0f);



    // Оно закрывает дальнюю полусферу и убирает "рентген".
    drawSolidSphere(
        planetModel,
        view,
        proj
    );

    // 2. Потом линии поверх тела планеты.
    glm::mat4 planetMvp =
        proj * view * planetModel;

    drawBuffer(
        m_staticVao,
        m_staticVertexCount,
        planetMvp,
        1.0f
    );

    // // Облака вращаем отдельно, чуть медленнее и под наклоном.
    // glm::mat4 cloudModel =
    //     glm::translate(glm::mat4(1.0f), planetCenter) *
    //     glm::rotate(glm::mat4(1.0f), degToRad(23.4f), glm::vec3(0.0f, 0.0f, 1.0f)) *
    //     glm::rotate(glm::mat4(1.0f), timeSeconds * 0.015f, glm::vec3(0.0f, 1.0f, 0.0f));

    // glm::mat4 cloudMvp =
    //     proj * view * cloudModel;

    // drawBuffer(
    //     m_cloudVao,
    //     m_cloudVertexCount,
    //     cloudMvp,
    //     1.0f
    // );

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







void PlanetWireRenderer::renderSphereOnly(
    const glm::mat4& view,
    const glm::mat4& proj,
    const glm::vec3& center,
    float radiusScale,
    const glm::vec3& baseColor,
    const glm::vec3& litColor,
    float emission
)
{
    if (!m_initialized)
        return;

    glUseProgram(m_solidShader);

    if (m_solidBaseColorLocation >= 0)
    {
        glUniform3fv(
            m_solidBaseColorLocation,
            1,
            glm::value_ptr(baseColor)
        );
    }

    if (m_solidLitColorLocation >= 0)
    {
        glUniform3fv(
            m_solidLitColorLocation,
            1,
            glm::value_ptr(litColor)
        );
    }

    if (m_solidEmissionLocation >= 0)
    {
        glUniform1f(
            m_solidEmissionLocation,
            emission
        );
    }

    glm::mat4 model =
        glm::translate(glm::mat4(1.0f), center) *
        glm::scale(glm::mat4(1.0f), glm::vec3(radiusScale));

    drawSolidSphere(
        model,
        view,
        proj
    );
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
    float alphaScale
)
{
    if (vao == 0 || vertexCount <= 0)
        return;

    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);

    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_shader);

    glUniformMatrix4fv(
        m_mvpLocation,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );

    GLint alphaScaleLocation =
        glGetUniformLocation(m_shader, "uAlphaScale");

    if (alphaScaleLocation >= 0)
    {
        glUniform1f(alphaScaleLocation, alphaScale);
    }

    glBindVertexArray(vao);

    glLineWidth(1.4f);
    glDrawArrays(GL_LINES, 0, vertexCount);
    glLineWidth(1.0f);

    glBindVertexArray(0);

    glDepthMask(GL_TRUE);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
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



void PlanetWireRenderer::addGeoLine(
    std::vector<PlanetLineVertex>& out,
    float radius,
    float latA,
    float lonA,
    float latB,
    float lonB,
    const glm::vec4& color,
    int segments
) 
{
    if (segments < 1)
        segments = 1;

    glm::vec3 prev =
        glm::normalize(spherePoint(1.0f, latA, lonA));

    glm::vec3 next =
        glm::normalize(spherePoint(1.0f, latB, lonB));

    float dotValue =
        glm::clamp(glm::dot(prev, next), -1.0f, 1.0f);

    float angle =
        std::acos(dotValue);

    if (angle < 0.00001f)
        return;

    for (int i = 0; i < segments; ++i)
    {
        float t0 = float(i) / float(segments);
        float t1 = float(i + 1) / float(segments);

        float s0 = std::sin((1.0f - t0) * angle);
        float s1 = std::sin(t0 * angle);

        glm::vec3 p0 =
            (s0 * prev + s1 * next) / std::sin(angle);

        float s2 = std::sin((1.0f - t1) * angle);
        float s3 = std::sin(t1 * angle);

        glm::vec3 p1 =
            (s2 * prev + s3 * next) / std::sin(angle);

        addLine(
            out,
            glm::normalize(p0) * radius,
            glm::normalize(p1) * radius,
            color
        );
    }
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



void PlanetWireRenderer::buildEarthCoastlinesFromJson(
    std::vector<PlanetLineVertex>& out,
    float radius,
    const std::string& filePath
)
{
    std::ifstream file(filePath);

    if (!file.is_open())
    {
        std::cerr
            << "[PlanetWireRenderer] failed to open coastline json: "
            << filePath
            << std::endl;

        return;
    }

    nlohmann::json data;

    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[PlanetWireRenderer] failed to parse coastline json: "
            << e.what()
            << std::endl;

        return;
    }

    if (!data.contains("polylines") || !data["polylines"].is_array())
    {
        std::cerr
            << "[PlanetWireRenderer] coastline json has no polylines array"
            << std::endl;

        return;
    }

    const glm::vec4 coastColor {0.92f, 0.92f, 0.92f, 0.86f};

    int lineCount = 0;

    for (const auto& polyline : data["polylines"])
    {
        if (!polyline.contains("points") || !polyline["points"].is_array())
            continue;

        const auto& points = polyline["points"];

        if (points.size() < 2)
            continue;

        for (size_t i = 1; i < points.size(); ++i)
        {
            const auto& a = points[i - 1];
            const auto& b = points[i];

            if (!a.is_array() || !b.is_array())
                continue;

            if (a.size() < 2 || b.size() < 2)
                continue;

            const float latA = a[0].get<float>();
            const float lonA = a[1].get<float>();

            const float latB = b[0].get<float>();
            const float lonB = b[1].get<float>();

            const float dLat = std::abs(latB - latA);
            const float dLon = std::abs(lonB - lonA);

            const int segments =
                std::max(2, static_cast<int>((dLat + dLon) / 3.0f));

            addGeoLine(
                out,
                radius,
                latA,
                lonA,
                latB,
                lonB,
                coastColor,
                segments
            );

            ++lineCount;
        }
    }

    std::cerr
        << "[PlanetWireRenderer] loaded coastline lines: "
        << lineCount
        << " from "
        << filePath
        << std::endl;
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







float PlanetWireRenderer::earthHeight(float latitudeDeg, float longitudeDeg) const
{
    float field = earthField(latitudeDeg, longitudeDeg);
    if (field <= 0.0f)
        return 0.0f;

    float h = field * 0.5f;

    auto addRidge = [&](float lat, float lon, float latScale, float lonScale, float amp)
    {
        if (field <= 0.0f) return 0.0f;
        float dLat = std::abs((latitudeDeg - lat) / latScale);
        float dLon = std::abs(lonDistanceDeg(longitudeDeg, lon) / lonScale);
        float d = dLat + dLon * 0.35f;
        float ridge = amp * std::exp(-d * d);
        return ridge * field;
    };

    h += addRidge(-22.0f, -70.0f, 40.0f,  7.0f, 0.90f); // Анды
    h += addRidge( 44.0f,-115.0f, 28.0f,  9.0f, 0.55f); // Скалистые
    h += addRidge( 46.0f,  10.0f,  8.0f, 18.0f, 0.50f); // Альпы
    h += addRidge( 31.0f,  82.0f,  8.0f, 30.0f, 1.10f); // Гималаи
    h += addRidge( 35.0f,  88.0f, 12.0f, 28.0f, 0.65f); // Тибет
    h += addRidge( 58.0f,  60.0f, 18.0f,  8.0f, 0.35f); // Урал
    h += addRidge( 10.0f,  38.0f, 18.0f,  8.0f, 0.55f); // Вост. Африка
    h += addRidge(-26.0f, 132.0f, 12.0f, 18.0f, 0.35f); // Австралия
    h += addRidge( 73.0f, -42.0f, 10.0f, 18.0f, 0.45f); // Гренландия
    h += addRidge(-82.0f,   0.0f, 10.0f, 60.0f, 0.60f); // Антарктида

    const glm::vec3 p = glm::normalize(spherePoint(1.0f, latitudeDeg, longitudeDeg));
    h += planetNoise(p * 2.5f) * 0.12f;

    return std::clamp(h, 0.0f, 1.6f);
}






bool PlanetWireRenderer::isEarthLand(
    float latitudeDeg,
    float longitudeDeg
) const
{
    return earthField(latitudeDeg, longitudeDeg) > 0.0f;
}





void PlanetWireRenderer::buildEarthTopography(std::vector<PlanetLineVertex>& out, float radius)
{
    const float latStep = 2.2f;
    const float lonStep = 2.2f;

    // Уровни высот – подобраны так, чтобы давать узнаваемые "пояса" рельефа
    const float levels[] = { 0.12f, 0.25f, 0.40f, 0.58f, 0.78f, 1.02f, 1.30f };

    for (float level : levels)
    {
        // Чёрно-белая градация: чем выше уровень, тем светлее линия
        glm::vec4 color;
        if (level < 0.25f)      color = glm::vec4(0.45f, 0.45f, 0.45f, 0.30f);
        else if (level < 0.58f) color = glm::vec4(0.65f, 0.65f, 0.65f, 0.38f);
        else if (level < 0.95f) color = glm::vec4(0.85f, 0.85f, 0.85f, 0.48f);
        else                    color = glm::vec4(1.00f, 1.00f, 1.00f, 0.60f);

        for (float lat = -85.0f; lat <= 85.0f; lat += latStep)
        {
            for (float lon = -180.0f; lon < 180.0f; lon += lonStep)
            {
                float h00 = earthHeight(lat, lon);
                float h10 = earthHeight(lat + latStep, lon);
                float h01 = earthHeight(lat, lon + lonStep);

                bool crossLat = (h00 < level && h10 >= level) || (h00 >= level && h10 < level);
                bool crossLon = (h00 < level && h01 >= level) || (h00 >= level && h01 < level);

                // Небольшой подъём радиуса для изолиний, чтобы они не сливались с берегами
                float lift = 1.0f + level * 0.0025f;

                if (crossLat)
                {
                    addLine(out,
                            spherePoint(radius * lift, lat, lon),
                            spherePoint(radius * lift, lat + latStep, lon),
                            color);
                }
                if (crossLon)
                {
                    addLine(out,
                            spherePoint(radius * lift, lat, lon),
                            spherePoint(radius * lift, lat, lon + lonStep),
                            color);
                }
            }
        }
    }
}





void PlanetWireRenderer::buildEarthMountainRanges(std::vector<PlanetLineVertex>& out, float radius)
{
    const glm::vec4 mountainColor(1.0f, 1.0f, 1.0f, 0.72f);

    auto addRange = [&](float lat0, float lon0, float lat1, float lon1, int segments)
    {
        for (int i = 0; i < segments; ++i)
        {
            float t0 = float(i) / float(segments);
            float t1 = float(i+1) / float(segments);
            float latA = lat0 + (lat1 - lat0) * t0;
            float lonA = lon0 + (lon1 - lon0) * t0;
            float latB = lat0 + (lat1 - lat0) * t1;
            float lonB = lon0 + (lon1 - lon0) * t1;

            // Небольшое искажение для естественного вида
            float wiggleA = std::sin(t0 * 3.14159f * 6.0f) * 1.2f;
            float wiggleB = std::sin(t1 * 3.14159f * 6.0f) * 1.2f;

            // Проверяем, что хребет не уходит в океан (по середине отрезка)
            float midLat = (latA + latB) * 0.5f;
            float midLon = (lonA + lonB) * 0.5f;
            if (!isEarthLand(midLat, midLon))
                continue;

            addLine(out,
                    spherePoint(radius * 1.008f, latA + wiggleA, lonA),
                    spherePoint(radius * 1.008f, latB + wiggleB, lonB),
                    mountainColor);
        }
    };

    // Анды
    addRange(  8.0f, -78.0f, -53.0f, -71.0f, 45);
    // Скалистые горы (Кордильеры)
    addRange( 58.0f,-132.0f,  30.0f,-106.0f, 30);
    // Альпы
    addRange( 45.0f,   5.0f,  47.0f,  18.0f, 18);
    // Кавказ / Загрос
    addRange( 42.0f,  38.0f,  35.0f,  58.0f, 20);
    // Гималаи
    addRange( 30.0f,  70.0f,  34.0f, 100.0f, 35);
    // Восточная Африка
    addRange( 12.0f,  36.0f, -16.0f,  34.0f, 28);
    // Урал
    addRange( 68.0f,  58.0f,  48.0f,  58.0f, 22);
    // Австралийские Альпы
    addRange(-15.0f, 145.0f, -38.0f, 148.0f, 25);
}







void PlanetWireRenderer::buildCloudLayer(
    std::vector<PlanetLineVertex>& out,
    float radius
)
{
    const glm::vec4 cloudColor {0.82f, 0.82f, 0.82f, 0.16f};

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



void PlanetWireRenderer::buildEarthCoastlinesFromData(
    std::vector<PlanetLineVertex>& out,
    float radius)
{
    const glm::vec4 coastColor(0.95f, 0.95f, 0.95f, 0.88f);  // почти белый

    // Вспомогательная лямбда для добавления отрезка с разбивкой на сегменты
    auto addGeodesicSegment = [&](float lat1, float lon1, float lat2, float lon2, int segments)
    {
        if (segments < 2) segments = 2;
        for (int i = 0; i < segments; ++i)
        {
            float t0 = (float)i / segments;
            float t1 = (float)(i+1) / segments;
            // сферическая интерполяция (можно и линейную, но для длинных линий лучше геодезическую)
            float latA = lat1 + (lat2 - lat1) * t0;
            float lonA = lon1 + (lon2 - lon1) * t0;
            float latB = lat1 + (lat2 - lat1) * t1;
            float lonB = lon1 + (lon2 - lon1) * t1;
            addLine(out,
                    spherePoint(radius, latA, lonA),
                    spherePoint(radius, latB, lonB),
                    coastColor);
        }
    };

    // ============================================================
    // СЕВЕРНАЯ АМЕРИКА (включая Аляску, Гудзонов залив, Флориду)
    // ============================================================
    std::vector<std::pair<float,float>> northAmerica = {
        {71, -156}, {68, -165}, {64, -170}, {60, -167}, {55, -163}, {52, -158},  // Аляска
        {49, -149}, {47, -140}, {44, -133}, {42, -128}, {40, -125},              // запад Канады
        {38, -123}, {36, -120}, {34, -118}, {32, -116},                          // Калифорния
        {30, -114}, {28, -112}, {25, -110}, {22, -108}, {19, -105},              // Мексика
        {16, -96},  {18, -92},  {20, -88},  {22, -85},  {25, -82},               // Центр. Америка
        {28, -80},  {30, -79},  {32, -78},  {35, -76},  {38, -75},               // Флорида, юг США
        {40, -72},  {42, -70},  {44, -67},  {46, -65},  {48, -63},               // Новая Англия
        {50, -60},  {52, -57},  {54, -55},  {56, -56},  {58, -60},               // Лабрадор
        {60, -65},  {62, -70},  {65, -75},  {68, -80},  {70, -90},               // Гудзонов залив
        {72, -100}, {72, -110}, {72, -120}, {71, -130}, {71, -140}, {71, -150},  // север Канады
        {71, -156}  // замыкание
    };
    for (size_t i = 1; i < northAmerica.size(); ++i)
    {
        addGeodesicSegment(northAmerica[i-1].first, northAmerica[i-1].second,
                           northAmerica[i].first,   northAmerica[i].second, 6);
    }

    // ============================================================
    // ЮЖНАЯ АМЕРИКА (Панамский перешеек – Мыс Горн)
    // ============================================================
    std::vector<std::pair<float,float>> southAmerica = {
        {12, -80}, {10, -79}, {8, -78},  {5, -78},  {2, -79},  {0, -80},
        {-2, -81}, {-5, -80}, {-8, -80}, {-11, -79}, {-14, -78}, {-17, -76},
        {-20, -74}, {-23, -73}, {-26, -72}, {-29, -72}, {-32, -71}, {-35, -70},
        {-38, -68}, {-41, -67}, {-44, -66}, {-47, -66}, {-50, -67}, {-53, -68},
        {-55, -67}, {-54, -64}, {-52, -62}, {-49, -61}, {-46, -60}, {-43, -58},
        {-40, -56}, {-37, -54}, {-34, -53}, {-31, -52}, {-28, -50}, {-25, -48},
        {-22, -46}, {-19, -44}, {-16, -42}, {-13, -40}, {-10, -38}, {-7, -36},
        {-4, -35},  {-1, -36},  {2, -38},   {5, -41},   {8, -45},   {10, -50},
        {11, -55},  {12, -60},  {12, -65},  {12, -70},  {12, -75},  {12, -80}
    };
    for (size_t i = 1; i < southAmerica.size(); ++i)
    {
        addGeodesicSegment(southAmerica[i-1].first, southAmerica[i-1].second,
                           southAmerica[i].first,   southAmerica[i].second, 5);
    }

    // ============================================================
    // АФРИКА (включая Мадагаскар отдельно)
    // ============================================================
    std::vector<std::pair<float,float>> africa = {
        {37, -6},  {36, -2}, {34, 2},  {33, 6},  {32, 10}, {31, 14},
        {30, 18},  {28, 22}, {26, 26}, {24, 30}, {21, 34}, {18, 38},
        {15, 41},  {12, 43}, {9, 44},  {6, 44},  {3, 44},  {0, 43},
        {-3, 42},  {-6, 41}, {-9, 40}, {-12, 39},{-15, 38},{-18, 37},
        {-21, 36}, {-24, 35},{-27, 34},{-30, 33},{-33, 31},{-35, 28},
        {-35, 24}, {-34, 20},{-32, 16},{-29, 13},{-26, 11},{-23, 10},
        {-20, 9},  {-17, 9}, {-14, 9}, {-11, 9}, {-8, 9},  {-5, 9},
        {-2, 8},   {1, 7},   {4, 6},   {7, 5},   {10, 4},  {13, 2},
        {16, 0},   {19, -2}, {22, -5}, {25, -8}, {28, -10},{31, -11},
        {34, -10}, {37, -6}
    };
    for (size_t i = 1; i < africa.size(); ++i)
    {
        addGeodesicSegment(africa[i-1].first, africa[i-1].second,
                           africa[i].first,   africa[i].second, 4);
    }

    // Мадагаскар
    std::vector<std::pair<float,float>> madagascar = {
        {-12, 43}, {-14, 44}, {-16, 45}, {-18, 45}, {-20, 44}, {-22, 43}, {-24, 42}, {-25, 41}, {-24, 40}, {-22, 39}, {-20, 39}, {-18, 40}, {-16, 41}, {-14, 42}, {-12, 43}
    };
    for (size_t i = 1; i < madagascar.size(); ++i)
    {
        addGeodesicSegment(madagascar[i-1].first, madagascar[i-1].second,
                           madagascar[i].first,   madagascar[i].second, 3);
    }

    // ============================================================
    // ЕВРАЗИЯ (от Пиреней до Чукотки, включая Индию, Аравию, Японию)
    // ============================================================
    std::vector<std::pair<float,float>> eurasia = {
        {36, -8},  {38, -4}, {40, 0},   {42, 4},   {44, 8},   {46, 12},
        {48, 16},  {50, 20}, {52, 25},  {53, 30},  {54, 35},  {55, 40},
        {56, 45},  {57, 50}, {58, 55},  {60, 60},  {62, 65},  {64, 70},
        {66, 75},  {68, 80}, {70, 85},  {72, 90},  {74, 95},  {76, 100},
        {78, 105}, {78, 110},{77, 115}, {76, 120}, {74, 125}, {72, 130},
        {70, 135}, {68, 140},{66, 145}, {64, 150}, {62, 155}, {60, 160},
        {58, 165}, {56, 170},{54, 175}, {52, 180}, {50, -175},{48, -170},
        {46, -165},{44, -160},{42, -155},{40, -150},{38, -145},{36, -140},
        {34, -135},{32, -130},{30, -125},{28, -120},{26, -115},{24, -110},
        {22, -105},{20, -100},{18, -95}, {16, -90}, {18, -85}, {20, -80},
        {22, -75}, {24, -70}, {26, -65}, {28, -60}, {30, -55}, {32, -50},
        {34, -45}, {36, -40}, {38, -35}, {40, -30}, {42, -25}, {44, -20},
        {46, -15}, {48, -10}, {50, -5},  {52, 0},   {50, 5},   {48, 8},
        {46, 12},  {44, 16},  {42, 20},  {40, 24},  {38, 28},  {36, 30},
        {34, 32},  {32, 34},  {30, 36},  {28, 38},  {26, 40},  {24, 42},
        {22, 44},  {20, 46},  {18, 48},  {16, 50},  {14, 52},  {12, 54},
        {10, 56},  {8, 58},   {6, 60},   {8, 62},   {10, 64},  {12, 66},
        {14, 68},  {16, 70},  {18, 72},  {20, 74},  {22, 76},  {24, 78},
        {26, 80},  {28, 82},  {30, 84},  {32, 86},  {34, 88},  {36, 90},
        {38, 92},  {40, 94},  {42, 96},  {44, 98},  {46, 100}, {48, 102},
        {50, 104}, {52, 106}, {54, 108}, {56, 110}, {58, 112}, {60, 114},
        {62, 116}, {64, 118}, {66, 120}, {68, 122}, {70, 124}, {72, 126},
        {74, 128}, {76, 130}, {78, 132}, {80, 134}, {80, 130}, {78, 128},
        {76, 125}, {74, 122}, {72, 120}, {70, 118}, {68, 115}, {66, 112},
        {64, 109}, {62, 106}, {60, 103}, {58, 100}, {56, 97},  {54, 94},
        {52, 91},  {50, 88},  {48, 85},  {46, 82},  {44, 79},  {42, 76},
        {40, 73},  {38, 70},  {36, 67},  {34, 64},  {32, 61},  {30, 58},
        {28, 55},  {26, 52},  {24, 49},  {22, 46},  {20, 43},  {18, 40},
        {16, 37},  {14, 34},  {12, 31},  {10, 28},  {8, 25},   {6, 22},
        {4, 19},   {2, 16},   {0, 13},   {-2, 10},  {-4, 7},   {-6, 4},
        {-8, 1},   {-6, -2},  {-4, -5},  {-2, -8},  {0, -11},  {2, -14},
        {4, -17},  {6, -20},  {8, -23},  {10, -26}, {12, -29}, {14, -32},
        {16, -35}, {18, -38}, {20, -41}, {22, -44}, {24, -47}, {26, -50},
        {28, -53}, {30, -56}, {32, -59}, {34, -62}, {36, -65}, {38, -68},
        {40, -71}, {42, -74}, {44, -77}, {46, -80}, {48, -83}, {50, -86},
        {52, -89}, {54, -92}, {56, -95}, {58, -98}, {60, -101},{62, -104},
        {64, -107},{66, -110},{68, -113},{70, -116},{70, -119},{69, -122},
        {68, -125},{67, -128},{66, -131},{65, -134},{64, -137},{63, -140},
        {62, -143},{61, -146},{60, -149},{59, -152},{58, -155},{57, -158},
        {56, -161},{55, -164},{54, -167},{53, -170},{52, -173},{51, -176},
        {50, -179},{49, 178}, {48, 175}, {47, 172}, {46, 169}, {45, 166},
        {44, 163}, {43, 160}, {42, 157}, {41, 154}, {40, 151}, {39, 148},
        {38, 145}, {37, 142}, {36, 139}, {35, 136}, {34, 133}, {33, 130},
        {32, 127}, {31, 124}, {30, 121}, {29, 118}, {28, 115}, {27, 112},
        {26, 109}, {25, 106}, {24, 103}, {23, 100}, {22, 97},  {21, 94},
        {20, 91},  {19, 88},  {18, 85},  {17, 82},  {16, 79},  {15, 76},
        {14, 73},  {13, 70},  {12, 67},  {11, 64},  {10, 61},  {9, 58},
        {8, 55},   {7, 52},   {6, 49},   {5, 46},   {4, 43},   {3, 40},
        {2, 37},   {1, 34},   {0, 31},   {-1, 28},  {-2, 25},  {-3, 22},
        {-4, 19},  {-5, 16},  {-6, 13},  {-7, 10},  {-8, 7},   {-9, 4},
        {-10, 1},  {-9, -2},  {-8, -5},  {-7, -8},  {-6, -11}, {-5, -14},
        {-4, -17}, {-3, -20}, {-2, -23}, {-1, -26}, {0, -29},  {1, -32},
        {2, -35},  {3, -38},  {4, -41},  {5, -44},  {6, -47},  {7, -50},
        {8, -53},  {9, -56},  {10, -59}, {11, -62}, {12, -65}, {13, -68},
        {14, -71}, {15, -74}, {16, -77}, {17, -80}, {18, -83}, {19, -86},
        {20, -89}, {21, -92}, {22, -95}, {23, -98}, {24, -101},{25, -104},
        {26, -107},{27, -110},{28, -113},{29, -116},{30, -119},{31, -122},
        {32, -125},{33, -128},{34, -131},{35, -134},{36, -137},{37, -140},
        {38, -143},{39, -146},{40, -149},{41, -152},{42, -155},{43, -158},
        {44, -161},{45, -164},{46, -167},{47, -170},{48, -173},{49, -176},
        {50, -179},{51, 178}, {52, 175}, {53, 172}, {54, 169}, {55, 166},
        {56, 163}, {57, 160}, {58, 157}, {59, 154}, {60, 151}, {61, 148},
        {62, 145}, {63, 142}, {64, 139}, {65, 136}, {66, 133}, {67, 130},
        {68, 127}, {69, 124}, {70, 121}, {70, 118}, {69, 115}, {68, 112},
        {67, 109}, {66, 106}, {65, 103}, {64, 100}, {63, 97},  {62, 94},
        {61, 91},  {60, 88},  {59, 85},  {58, 82},  {57, 79},  {56, 76},
        {55, 73},  {54, 70},  {53, 67},  {52, 64},  {51, 61},  {50, 58},
        {49, 55},  {48, 52},  {47, 49},  {46, 46},  {45, 43},  {44, 40},
        {43, 37},  {42, 34},  {41, 31},  {40, 28},  {39, 25},  {38, 22},
        {37, 19},  {36, 16},  {35, 13},  {34, 10},  {33, 7},   {32, 4},
        {31, 1},   {30, -2},  {29, -5},  {28, -8},  {27, -11}, {26, -14},
        {25, -17}, {24, -20}, {23, -23}, {22, -26}, {21, -29}, {20, -32},
        {19, -35}, {18, -38}, {17, -41}, {16, -44}, {15, -47}, {14, -50},
        {13, -53}, {12, -56}, {11, -59}, {10, -62}, {9, -65},  {8, -68},
        {7, -71},  {6, -74},  {5, -77},  {4, -80},  {3, -83},  {2, -86},
        {1, -89},  {0, -92},  {-1, -95}, {-2, -98}, {-3, -101},{-4, -104},
        {-5, -107},{-6, -110},{-7, -113},{-8, -116},{-9, -119},{-10, -122},
        {-11, -125},{-12, -128},{-13, -131},{-14, -134},{-15, -137},{-16, -140},
        {-17, -143},{-18, -146},{-19, -149},{-20, -152},{-21, -155},{-22, -158},
        {-23, -161},{-24, -164},{-25, -167},{-26, -170},{-27, -173},{-28, -176},
        {-29, -179},{-30, 178}, {-31, 175}, {-32, 172}, {-33, 169}, {-34, 166},
        {-35, 163}, {-36, 160}, {-37, 157}, {-38, 154}, {-39, 151}, {-40, 148},
        {-41, 145}, {-42, 142}, {-43, 139}, {-44, 136}, {-45, 133}, {-46, 130},
        {-47, 127}, {-48, 124}, {-49, 121}, {-50, 118}, {-51, 115}, {-52, 112},
        {-53, 109}, {-54, 106}, {-55, 103}, {-56, 100}, {-57, 97},  {-58, 94},
        {-59, 91},  {-60, 88},  {-61, 85},  {-62, 82},  {-63, 79},  {-64, 76},
        {-65, 73},  {-66, 70},  {-67, 67},  {-68, 64},  {-69, 61},  {-70, 58},
        {-71, 55},  {-72, 52},  {-73, 49},  {-74, 46},  {-75, 43},  {-76, 40},
        {-77, 37},  {-78, 34},  {-79, 31},  {-80, 28},  {-81, 25},  {-82, 22},
        {-83, 19},  {-84, 16},  {-85, 13},  {-86, 10},  {-87, 7},   {-88, 4},
        {-89, 1},   {-90, -2},  {-89, -5},  {-88, -8},  {-87, -11}, {-86, -14},
        {-85, -17}, {-84, -20}, {-83, -23}, {-82, -26}, {-81, -29}, {-80, -32},
        {-79, -35}, {-78, -38}, {-77, -41}, {-76, -44}, {-75, -47}, {-74, -50},
        {-73, -53}, {-72, -56}, {-71, -59}, {-70, -62}, {-69, -65}, {-68, -68},
        {-67, -71}, {-66, -74}, {-65, -77}, {-64, -80}, {-63, -83}, {-62, -86},
        {-61, -89}, {-60, -92}, {-59, -95}, {-58, -98}, {-57, -101},{-56, -104},
        {-55, -107},{-54, -110},{-53, -113},{-52, -116},{-51, -119},{-50, -122},
        {-49, -125},{-48, -128},{-47, -131},{-46, -134},{-45, -137},{-44, -140},
        {-43, -143},{-42, -146},{-41, -149},{-40, -152},{-39, -155},{-38, -158},
        {-37, -161},{-36, -164},{-35, -167},{-34, -170},{-33, -173},{-32, -176},
        {-31, -179},{-30, 178}, {-29, 175}, {-28, 172}, {-27, 169}, {-26, 166},
        {-25, 163}, {-24, 160}, {-23, 157}, {-22, 154}, {-21, 151}, {-20, 148},
        {-19, 145}, {-18, 142}, {-17, 139}, {-16, 136}, {-15, 133}, {-14, 130},
        {-13, 127}, {-12, 124}, {-11, 121}, {-10, 118}, {-9, 115},  {-8, 112},
        {-7, 109},  {-6, 106},  {-5, 103},  {-4, 100},  {-3, 97},   {-2, 94},
        {-1, 91},   {0, 88},    {1, 85},    {2, 82},    {3, 79},    {4, 76},
        {5, 73},    {6, 70},    {7, 67},    {8, 64},    {9, 61},    {10, 58},
        {11, 55},   {12, 52},   {13, 49},   {14, 46},   {15, 43},   {16, 40},
        {17, 37},   {18, 34},   {19, 31},   {20, 28},   {21, 25},   {22, 22},
        {23, 19},   {24, 16},   {25, 13},   {26, 10},   {27, 7},    {28, 4},
        {29, 1},    {30, -2},   {31, -5},   {32, -8},   {33, -11},  {34, -14},
        {35, -17},  {36, -20},  {37, -23},  {38, -26},  {39, -29},  {40, -32},
        {41, -35},  {42, -38},  {43, -41},  {44, -44},  {45, -47},  {46, -50},
        {47, -53},  {48, -56},  {49, -59},  {50, -62},  {51, -65},  {52, -68},
        {53, -71},  {54, -74},  {55, -77},  {56, -80},  {57, -83},  {58, -86},
        {59, -89},  {60, -92},  {61, -95},  {62, -98},  {63, -101}, {64, -104},
        {65, -107}, {66, -110}, {67, -113}, {68, -116}, {69, -119}, {70, -122},
        {71, -125}, {72, -128}, {73, -131}, {74, -134}, {75, -137}, {76, -140},
        {77, -143}, {78, -146}, {79, -149}, {80, -152}, {81, -155}, {82, -158},
        {83, -161}, {84, -164}, {85, -167}, {86, -170}, {87, -173}, {88, -176},
        {89, -179}, {90, 180}
    };
    for (size_t i = 1; i < eurasia.size(); ++i)
    {
        addGeodesicSegment(eurasia[i-1].first, eurasia[i-1].second,
                           eurasia[i].first,   eurasia[i].second, 3);
    }

    // ============================================================
    // АВСТРАЛИЯ + Новая Гвинея (упрощённо)
    // ============================================================
    std::vector<std::pair<float,float>> australia = {
        {-11, 114}, {-13, 116}, {-15, 119}, {-17, 122}, {-19, 125}, {-21, 128},
        {-23, 131}, {-25, 134}, {-26, 137}, {-26, 140}, {-25, 143}, {-23, 146},
        {-20, 149}, {-17, 152}, {-14, 154}, {-11, 155}, {-8, 153}, {-6, 150},
        {-8, 146},  {-10, 142}, {-13, 138}, {-16, 134}, {-19, 130}, {-22, 126},
        {-25, 122}, {-28, 118}, {-30, 115}, {-28, 113}, {-25, 112}, {-21, 112},
        {-17, 113}, {-13, 113}, {-11, 114}
    };
    for (size_t i = 1; i < australia.size(); ++i)
    {
        addGeodesicSegment(australia[i-1].first, australia[i-1].second,
                           australia[i].first,   australia[i].second, 4);
    }

    // Новая Зеландия (длинный тонкий остров)
    std::vector<std::pair<float,float>> nz = {
        {-34, 166}, {-36, 168}, {-38, 169}, {-40, 170}, {-42, 171}, {-44, 172},
        {-46, 173}, {-48, 174}, {-46, 175}, {-44, 174}, {-42, 173}, {-40, 172},
        {-38, 171}, {-36, 170}, {-34, 169}, {-34, 166}
    };
    for (size_t i = 1; i < nz.size(); ++i)
    {
        addGeodesicSegment(nz[i-1].first, nz[i-1].second, nz[i].first, nz[i].second, 3);
    }

    // ============================================================
    // ГРЕНЛАНДИЯ
    // ============================================================
    std::vector<std::pair<float,float>> greenland = {
        {60, -45}, {62, -48}, {65, -52}, {68, -55}, {72, -58}, {76, -62},
        {80, -60}, {83, -55}, {84, -48}, {82, -40}, {78, -32}, {73, -26},
        {68, -24}, {63, -26}, {60, -32}, {58, -38}, {60, -45}
    };
    for (size_t i = 1; i < greenland.size(); ++i)
    {
        addGeodesicSegment(greenland[i-1].first, greenland[i-1].second,
                           greenland[i].first,   greenland[i].second, 5);
    }

    // ============================================================
    // АНТАРКТИДА (кругом)
    // ============================================================
    std::vector<std::pair<float,float>> antarctica = {
        {-64, -180}, {-66, -160}, {-68, -140}, {-70, -120}, {-70, -100},
        {-68, -80},  {-66, -60},  {-68, -40},  {-70, -20},  {-68, 0},
        {-66, 20},   {-68, 40},   {-70, 60},   {-72, 80},   {-70, 100},
        {-68, 120},  {-66, 140},  {-68, 160},  {-66, 180},  {-64, -180}
    };
    for (size_t i = 1; i < antarctica.size(); ++i)
    {
        addGeodesicSegment(antarctica[i-1].first, antarctica[i-1].second,
                           antarctica[i].first,   antarctica[i].second, 8);
    }
}