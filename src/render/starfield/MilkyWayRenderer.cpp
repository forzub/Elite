#include "MilkyWayRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "src/render/ShaderLibrary.h"

using json = nlohmann::json;

namespace
{
struct SkyVertex
{
    glm::vec3 position;
    glm::vec2 uv;
};

float readFloat(const json& item, const char* key, float fallback)
{
    if (!item.contains(key) || !item[key].is_number())
        return fallback;

    return static_cast<float>(item[key].get<double>());
}

glm::vec3 readVec3(const json& item, const char* key, const glm::vec3& fallback)
{
    if (!item.contains(key) || !item[key].is_array())
        return fallback;

    const auto& a = item[key];

    if (a.size() < 3)
        return fallback;

    if (!a[0].is_number() || !a[1].is_number() || !a[2].is_number())
        return fallback;

    return glm::vec3(
        static_cast<float>(a[0].get<double>()),
        static_cast<float>(a[1].get<double>()),
        static_cast<float>(a[2].get<double>())
    );
}
}

MilkyWayRenderer::~MilkyWayRenderer()
{
    shutdown();
}

bool MilkyWayRenderer::initialize(const std::string& path)
{
    if (m_initialized)
        return true;

    bool loaded = loadConfig(path);

    if (!loaded && path == "assets/data/galaxy/milky_way.json")
    {
        loaded = loadConfig("../assets/data/galaxy/milky_way.json");
    }

    if (!loaded)
    {
        std::cerr << "[MilkyWay] failed to load milky_way.json, using defaults" << std::endl;
    }

    const SkyVertex vertices[] =
    {
        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 1.0f}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}},

        {{-1.0f, -1.0f, 1.0f}, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f, 1.0f}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 1.0f}, {0.0f, 1.0f}},
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(vertices),
        vertices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkyVertex),
        reinterpret_cast<void*>(offsetof(SkyVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(SkyVertex),
        reinterpret_cast<void*>(offsetof(SkyVertex, uv))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_initialized = true;

    std::cout
        << "[MilkyWay] initialized as full-screen galactic sky field"
        << " center=("
        << m_galacticCenterDir.x << ", "
        << m_galacticCenterDir.y << ", "
        << m_galacticCenterDir.z << ")"
        << " north=("
        << m_galacticNorthDir.x << ", "
        << m_galacticNorthDir.y << ", "
        << m_galacticNorthDir.z << ")"
        << std::endl;

    return true;
}

void MilkyWayRenderer::shutdown()
{
    if (m_vbo != 0)
    {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }

    if (m_vao != 0)
    {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }

    m_initialized = false;
}

void MilkyWayRenderer::setObserverPositionLy(const glm::vec3& observerLy)
{
    // Пока направление на галактический центр считаем практически бесконечно дальним.
    // Для перемещений внутри локального пузыря это правильно: небо не должно прыгать.
    m_observerLy = observerLy;
}

bool MilkyWayRenderer::loadConfig(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        std::cerr << "[MilkyWay] config not found: " << path << std::endl;
        return false;
    }

    std::cout << "[MilkyWay] loading config: " << path << std::endl;

    json root;

    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[MilkyWay] failed to parse JSON: " << e.what() << std::endl;
        return false;
    }

    if (root.contains("orientation") && root["orientation"].is_object())
    {
        const auto& o = root["orientation"];

        m_galacticCenterDir = readVec3(
            o,
            "galactic_center_dir",
            m_galacticCenterDir
        );

        m_galacticNorthDir = readVec3(
            o,
            "galactic_north_dir",
            m_galacticNorthDir
        );
    }

    if (glm::length(m_galacticCenterDir) < 0.001f)
        m_galacticCenterDir = glm::vec3(-0.0549f, -0.8734f, -0.4838f);

    if (glm::length(m_galacticNorthDir) < 0.001f)
        m_galacticNorthDir = glm::vec3(-0.8676f, -0.1981f, 0.4559f);

    m_galacticCenterDir = glm::normalize(m_galacticCenterDir);
    m_galacticNorthDir = glm::normalize(m_galacticNorthDir);

    // Убираем возможную неортогональность из JSON.
    m_galacticNorthDir = glm::normalize(
        m_galacticNorthDir -
        m_galacticCenterDir * glm::dot(m_galacticNorthDir, m_galacticCenterDir)
    );

    if (root.contains("appearance") && root["appearance"].is_object())
    {
        const auto& a = root["appearance"];

        m_bandWidthDeg = readFloat(a, "band_width_deg", m_bandWidthDeg);
        m_coreWidthDeg = readFloat(a, "core_width_deg", m_coreWidthDeg);
        m_intensity = readFloat(a, "intensity", m_intensity);
        m_dustStrength = readFloat(a, "dust_strength", m_dustStrength);
    }

    m_bandWidthDeg = std::max(3.0f, m_bandWidthDeg);
    m_coreWidthDeg = std::max(4.0f, m_coreWidthDeg);
    m_intensity = std::max(0.0f, m_intensity);
    m_dustStrength = std::max(0.0f, std::min(1.0f, m_dustStrength));

    return true;
}

void MilkyWayRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    float skyRadius
)
{
    (void)skyRadius;

    if (!m_initialized || m_vao == 0 || m_vbo == 0)
        return;

    const GLuint shader = ShaderLibrary::instance().get("galaxy_haze");
    if (shader == 0)
        return;

    const glm::mat4 viewRotation = glm::mat4(glm::mat3(view));
    const glm::mat4 invViewRotation = glm::inverse(viewRotation);
    const glm::mat4 invProjection = glm::inverse(projection);

    glUseProgram(shader);

    const GLint locInvProjection = glGetUniformLocation(shader, "uInvProjection");
    if (locInvProjection >= 0)
    {
        glUniformMatrix4fv(
            locInvProjection,
            1,
            GL_FALSE,
            glm::value_ptr(invProjection)
        );
    }

    const GLint locInvViewRotation = glGetUniformLocation(shader, "uInvViewRotation");
    if (locInvViewRotation >= 0)
    {
        glUniformMatrix4fv(
            locInvViewRotation,
            1,
            GL_FALSE,
            glm::value_ptr(invViewRotation)
        );
    }

    const GLint locGalacticCenterDir = glGetUniformLocation(shader, "uGalacticCenterDir");
    if (locGalacticCenterDir >= 0)
    {
        glUniform3fv(
            locGalacticCenterDir,
            1,
            glm::value_ptr(m_galacticCenterDir)
        );
    }

    const GLint locGalacticNorthDir = glGetUniformLocation(shader, "uGalacticNorthDir");
    if (locGalacticNorthDir >= 0)
    {
        glUniform3fv(
            locGalacticNorthDir,
            1,
            glm::value_ptr(m_galacticNorthDir)
        );
    }

    const GLint locBandWidthDeg = glGetUniformLocation(shader, "uBandWidthDeg");
    if (locBandWidthDeg >= 0)
        glUniform1f(locBandWidthDeg, m_bandWidthDeg);

    const GLint locCoreWidthDeg = glGetUniformLocation(shader, "uCoreWidthDeg");
    if (locCoreWidthDeg >= 0)
        glUniform1f(locCoreWidthDeg, m_coreWidthDeg);

    const GLint locIntensity = glGetUniformLocation(shader, "uIntensity");
    if (locIntensity >= 0)
        glUniform1f(locIntensity, m_intensity);

    const GLint locDustStrength = glGetUniformLocation(shader, "uDustStrength");
    if (locDustStrength >= 0)
        glUniform1f(locDustStrength, m_dustStrength);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glUseProgram(0);
}