#include "ConstellationOverlayRenderer.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <utility>

#include <glm/gtc/type_ptr.hpp>
#include <nlohmann/json.hpp>

#include "src/render/ShaderLibrary.h"

namespace
{
    using json = nlohmann::json;

    constexpr float kMinimumDirectionLength = 0.000001f;
    constexpr float kConstellationRadiusScale = 0.992f;

    const glm::vec4 kConstellationLineColor(
        0.34f,
        0.66f,
        0.90f,
        0.34f
    );
}

ConstellationOverlayRenderer::~ConstellationOverlayRenderer()
{
    shutdown();
}

bool ConstellationOverlayRenderer::initialize(
    const std::string& path
)
{
    if (m_initialized)
        return true;

    if (!loadDefinitions(path))
        return false;

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(LineVertex),
        reinterpret_cast<void*>(offsetof(LineVertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_initialized = true;

    std::cout
        << "[Constellations] definitions="
        << m_definitions.size()
        << " source="
        << path
        << std::endl;

    return true;
}

void ConstellationOverlayRenderer::shutdown()
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

    m_vertices.clear();
    m_definitions.clear();
    m_initialized = false;
}

bool ConstellationOverlayRenderer::loadDefinitions(
    const std::string& path
)
{
    std::ifstream input(path);
    if (!input.is_open())
        return false;

    json root;

    try
    {
        input >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[Constellations] failed to parse "
            << path
            << ": "
            << e.what()
            << std::endl;

        return false;
    }

    if (!root.contains("constellations") ||
        !root["constellations"].is_array())
    {
        std::cerr
            << "[Constellations] missing constellations array: "
            << path
            << std::endl;

        return false;
    }

    std::vector<ConstellationDefinition> loaded;
    loaded.reserve(root["constellations"].size());

    std::size_t skipped = 0;

    for (const auto& item : root["constellations"])
    {
        if (!item.is_object() ||
            !item.contains("id") ||
            !item["id"].is_string() ||
            !item.contains("polylines_hr") ||
            !item["polylines_hr"].is_array())
        {
            ++skipped;
            continue;
        }

        ConstellationDefinition definition;
        definition.id = item["id"].get<std::string>();

        if (item.contains("name") && item["name"].is_string())
            definition.name = item["name"].get<std::string>();
        else
            definition.name = definition.id;

        for (const auto& polylineJson : item["polylines_hr"])
        {
            if (!polylineJson.is_array())
                continue;

            std::vector<int> polyline;
            polyline.reserve(polylineJson.size());

            bool valid = true;

            for (const auto& starId : polylineJson)
            {
                if (!starId.is_number_integer())
                {
                    valid = false;
                    break;
                }

                const int hr = starId.get<int>();
                if (hr <= 0)
                {
                    valid = false;
                    break;
                }

                polyline.push_back(hr);
            }

            if (valid && polyline.size() >= 2)
                definition.polylinesHr.push_back(std::move(polyline));
        }

        if (definition.id.empty() || definition.polylinesHr.empty())
        {
            ++skipped;
            continue;
        }

        loaded.push_back(std::move(definition));
    }

    if (loaded.empty())
    {
        std::cerr
            << "[Constellations] no valid definitions in "
            << path
            << std::endl;

        return false;
    }

    m_definitions = std::move(loaded);

    if (skipped > 0)
    {
        std::cerr
            << "[Constellations] skipped invalid definitions="
            << skipped
            << std::endl;
    }

    return true;
}

void ConstellationOverlayRenderer::rebuild(
    const std::vector<StarReference>& stars,
    const glm::vec3& observerPositionLy,
    float skyRadius
)
{
    if (!m_initialized)
        return;

    std::unordered_map<int, glm::vec3> positionsByHr;
    positionsByHr.reserve(stars.size());

    for (const StarReference& star : stars)
    {
        if (star.brightStarCatalogId <= 0)
            continue;

        positionsByHr.emplace(
            star.brightStarCatalogId,
            star.positionLy
        );
    }

    m_vertices.clear();

    std::size_t sourceSegments = 0;
    std::size_t missingSegments = 0;

    for (const ConstellationDefinition& definition : m_definitions)
    {
        for (const std::vector<int>& polyline : definition.polylinesHr)
        {
            for (std::size_t i = 1; i < polyline.size(); ++i)
            {
                ++sourceSegments;

                const auto aIt = positionsByHr.find(polyline[i - 1]);
                const auto bIt = positionsByHr.find(polyline[i]);

                if (aIt == positionsByHr.end() ||
                    bIt == positionsByHr.end())
                {
                    ++missingSegments;
                    continue;
                }

                const glm::vec3 relativeA =
                    aIt->second - observerPositionLy;

                const glm::vec3 relativeB =
                    bIt->second - observerPositionLy;

                const float lengthA = glm::length(relativeA);
                const float lengthB = glm::length(relativeB);

                if (lengthA < kMinimumDirectionLength ||
                    lengthB < kMinimumDirectionLength)
                {
                    ++missingSegments;
                    continue;
                }

                const glm::vec3 pointA =
                    (relativeA / lengthA) *
                    skyRadius *
                    kConstellationRadiusScale;

                const glm::vec3 pointB =
                    (relativeB / lengthB) *
                    skyRadius *
                    kConstellationRadiusScale;

                m_vertices.push_back(
                    LineVertex{pointA, kConstellationLineColor}
                );

                m_vertices.push_back(
                    LineVertex{pointB, kConstellationLineColor}
                );
            }
        }
    }

    uploadVertices();

    std::cout
        << "[Constellations] segments="
        << segmentCount()
        << "/"
        << sourceSegments
        << " missing="
        << missingSegments
        << " observerLy=("
        << observerPositionLy.x
        << ", "
        << observerPositionLy.y
        << ", "
        << observerPositionLy.z
        << ")"
        << std::endl;
}

void ConstellationOverlayRenderer::uploadVertices()
{
    if (m_vbo == 0)
        return;

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            m_vertices.size() * sizeof(LineVertex)
        ),
        m_vertices.empty() ? nullptr : m_vertices.data(),
        GL_DYNAMIC_DRAW
    );
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void ConstellationOverlayRenderer::render(
    const glm::mat4& mvp
) const
{
    if (!m_initialized || m_vertices.empty())
        return;

    const GLuint shader =
        ShaderLibrary::instance().get("system_map_lines");

    if (shader == 0)
        return;

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glLineWidth(1.0f);

    glUseProgram(shader);

    const GLint mvpLocation =
        glGetUniformLocation(shader, "uMVP");

    if (mvpLocation >= 0)
    {
        glUniformMatrix4fv(
            mvpLocation,
            1,
            GL_FALSE,
            glm::value_ptr(mvp)
        );
    }

    glBindVertexArray(m_vao);
    glDrawArrays(
        GL_LINES,
        0,
        static_cast<GLsizei>(m_vertices.size())
    );
    glBindVertexArray(0);

    glUseProgram(0);
    glLineWidth(1.0f);
}
