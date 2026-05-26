#include "GalaxyStarfieldRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <random>

#include <glm/gtc/type_ptr.hpp>
#include <glm/geometric.hpp>
#include <nlohmann/json.hpp>

#include "src/render/ShaderLibrary.h"

using json = nlohmann::json;

namespace
{
constexpr float kMinDirectionLength = 0.0001f;
constexpr float kParsecInLightYears = 3.261563777f;
constexpr float kStarAngularScale = 1.35f;   // было ~1.0–1.15 → делаем чуть крупнее
constexpr float kStarAlphaBoost   = 1.12f;   // лёгкое усиление альфы

float clamp01(float v)
{
    return std::max(0.0f, std::min(1.0f, v));
}

float apparentMagnitudeFromAbsolute(float absoluteMagnitude, float distanceLy)
{
    const float safeDistanceLy = std::max(distanceLy, 0.001f);
    const float distancePc = safeDistanceLy / kParsecInLightYears;

    return absoluteMagnitude + 5.0f * std::log10(distancePc) - 5.0f;
}

float starAlphaFromMagnitude(float apparentMagnitude)
{
    const float normalized = 1.0f - ((apparentMagnitude + 1.5f) / 8.0f);
    const float clamped = std::max(0.0f, std::min(1.0f, normalized));

    // Реальные звёзды иначе выглядят как пыль.
    return std::max(0.075f, std::pow(clamped, 1.15f));
}

float starSizeFromMagnitude(float apparentMagnitude, bool isGameSystem)
{
    const float normalized = 1.0f - ((apparentMagnitude + 1.5f) / 8.0f);
    const float clamped = std::max(0.0f, std::min(1.0f, normalized));

    float size = 1.35f + std::pow(clamped, 0.75f) * 6.8f;
    // float size = 1.15f + std::pow(clamped, 0.75f) * 6.5f;

    if (isGameSystem)
        size *= 1.35f;

    return size;
}

float randomRange(std::mt19937& rng, float a, float b)
{
    std::uniform_real_distribution<float> dist(a, b);
    return dist(rng);
}

float randomNormal(std::mt19937& rng, float mean, float sigma)
{
    std::normal_distribution<float> dist(mean, sigma);
    return dist(rng);
}




glm::vec3 makeDirFromAngles(float yaw, float pitch)
{
    const float cp = std::cos(pitch);

    return glm::normalize(glm::vec3(
        std::cos(yaw) * cp,
        std::sin(pitch),
        std::sin(yaw) * cp
    ));
}



bool matrixAlmostEqual(const glm::mat4& a, const glm::mat4& b, float eps = 0.0001f)
{
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            if (std::abs(a[c][r] - b[c][r]) > eps)
                return false;
        }
    }

    return true;
}



}

GalaxyStarfieldRenderer::GalaxyStarfieldRenderer() = default;

GalaxyStarfieldRenderer::~GalaxyStarfieldRenderer()
{
    shutdown();
}

bool GalaxyStarfieldRenderer::initialize(const std::string& atlasPath)
{
    if (m_initialized)
        return true;

    bool realCatalogLoaded = loadRealStarCatalog("assets/data/galaxy/real_star_catalog.json");

    if (!realCatalogLoaded)
    {
        // When EliteGame.exe is launched from build/, assets may still live one level above.
        realCatalogLoaded = loadRealStarCatalog("../assets/data/galaxy/real_star_catalog.json");
    }

    if (realCatalogLoaded)
    {
        m_milkyWayRenderer.initialize();
        rebuildVerticesFromRealCatalog();
    }
    else
    {
        std::cerr
            << "[Starfield] real_star_catalog.json not found. "
            << "Falling back to old atlas/procedural starfield."
            << std::endl;

        bool atlasLoaded = loadAtlasStars(atlasPath);

        if (!atlasLoaded && atlasPath == "assets/data/galaxy/star_systems.json")
        {
            atlasLoaded = loadAtlasStars("../assets/data/galaxy/star_systems.json");
        }

        generateProceduralField();
        m_milkyWayRenderer.initialize();
        rebuildVertices();
    }

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(StarVertex)),
        m_vertices.empty() ? nullptr : m_vertices.data(),
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StarVertex),
        reinterpret_cast<void*>(offsetof(StarVertex, position))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StarVertex),
        reinterpret_cast<void*>(offsetof(StarVertex, color))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        1,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StarVertex),
        reinterpret_cast<void*>(offsetof(StarVertex, size))
    );

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(
        3,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(StarVertex),
        reinterpret_cast<void*>(offsetof(StarVertex, uv))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // Diagnostic starfield shader test: five clip-space points.
    // If these are visible, the shader/pass is alive and the problem is world/view/projection placement.
    // If these are not visible, something after this pass overwrites the framebuffer or the shader path is wrong.
    

    // Dynamic screen-space quad buffer. This avoids driver-dependent GL_POINT
    // behaviour and makes the starfield pass visible and debuggable.
    glGenVertexArrays(1, &m_screenVao);
    glGenBuffers(1, &m_screenVbo);
    glBindVertexArray(m_screenVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_screenVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(StarVertex), reinterpret_cast<void*>(offsetof(StarVertex, position)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(StarVertex), reinterpret_cast<void*>(offsetof(StarVertex, color)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(StarVertex), reinterpret_cast<void*>(offsetof(StarVertex, size)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(StarVertex), reinterpret_cast<void*>(offsetof(StarVertex, uv)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);



    







    m_initialized = true;

    std::cout
    << "[Starfield] initialized"
    << " realStars=" << m_realStars.size()
    << " oldSources=" << m_sources.size()
    << " vertices=" << m_vertices.size()
    << " vao=" << m_vao
    << " vbo=" << m_vbo
    << std::endl;

    return true;
}

void GalaxyStarfieldRenderer::shutdown()
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

    

    if (m_screenVbo != 0)
    {
        glDeleteBuffers(1, &m_screenVbo);
        m_screenVbo = 0;
    }

    if (m_screenVao != 0)
    {
        glDeleteVertexArrays(1, &m_screenVao);
        m_screenVao = 0;
    }

    m_milkyWayRenderer.shutdown();

    m_initialized = false;
}






void GalaxyStarfieldRenderer::setObserverPositionLy(const glm::vec3& positionLy)
{
    const float observerDelta = glm::length(positionLy - m_observerPositionLy);
    if (observerDelta < 0.0001f)
        return;

    m_observerPositionLy = positionLy;
    m_milkyWayRenderer.setObserverPositionLy(m_observerPositionLy);

    const float rebuildDelta =
        glm::length(m_observerPositionLy - m_lastRebuildObserverPositionLy);

    if (rebuildDelta < m_rebuildThresholdLy)
        return;

    if (!m_realStars.empty())
        rebuildVerticesFromRealCatalog();
    else
        rebuildVertices();

    m_lastRebuildObserverPositionLy = m_observerPositionLy;

    m_starScreenCacheDirty = true;
    

    if (m_vbo != 0)
    {
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(m_vertices.size() * sizeof(StarVertex)),
            m_vertices.empty() ? nullptr : m_vertices.data(),
            GL_DYNAMIC_DRAW
        );
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}









bool GalaxyStarfieldRenderer::loadAtlasStars(const std::string& atlasPath)
{
    std::ifstream in(atlasPath);
    if (!in.is_open())
    {
        std::cerr << "[Starfield] atlas not found: " << atlasPath << std::endl;
        return false;
    }

    std::cout << "[Starfield] loading atlas: " << atlasPath << std::endl;

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Starfield] failed to parse atlas: " << e.what() << std::endl;
        return false;
    }

    const json* arr = nullptr;
    if (root.contains("star_systems") && root["star_systems"].is_array())
        arr = &root["star_systems"];
    else if (root.is_array())
        arr = &root;

    if (!arr)
    {
        std::cerr << "[Starfield] atlas has no star_systems array" << std::endl;
        return false;
    }

    for (const auto& item : *arr)
    {
        const float x = item.value("x_ly", 0.0f);
        const float y = item.value("y_ly", 0.0f);
        const float z = item.value("z_ly", 0.0f);

        glm::vec3 pos(x, y, z);
        if (glm::length(pos) < kMinDirectionLength)
            continue;

        const std::string type = item.value("star_type", std::string("G"));
        glm::vec3 color = colorForStarType(type);

        const float distance = std::max(0.1f, item.value("distance_ly", glm::length(pos)));
        const int starsCount = std::max(1, item.value("stars_count", 1));

        float brightness = 1.0f / std::sqrt(std::max(1.0f, distance));
        brightness = std::max(
            0.72f,
            std::min(1.0f, brightness * 2.8f + 0.12f * static_cast<float>(starsCount - 1))
        );

        const float size =
            2.4f
            + 0.45f * static_cast<float>(starsCount - 1)
            + brightness * 3.2f;
        {
            StarSource s;
            s.positionLy = pos;
            s.color = color;
            s.magnitude = brightness;
            s.size = size;
            s.atlasStar = true;
            m_sources.push_back(s);
        }
    }

    std::cout << "[Starfield] atlas stars loaded=" << m_sources.size() << std::endl;
    return true;
}




bool GalaxyStarfieldRenderer::loadRealStarCatalog(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        std::cerr << "[Starfield] real catalog not found: " << path << std::endl;
        return false;
    }

    std::cout << "[Starfield] loading real catalog: " << path << std::endl;

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Starfield] failed to parse real catalog: " << e.what() << std::endl;
        return false;
    }

    if (!root.contains("stars") || !root["stars"].is_array())
    {
        std::cerr << "[Starfield] real catalog has no stars array" << std::endl;
        return false;
    }

    m_realStars.clear();

    for (const auto& item : root["stars"])
{
    if (!item.is_object())
        continue;

    // --- position ---
    if (!item.contains("position_ly") || !item["position_ly"].is_array())
        continue;

    const auto& p = item["position_ly"];
    if (p.size() < 3)
        continue;

    if (p[0].is_null() || p[1].is_null() || p[2].is_null())
        continue;

    if (!p[0].is_number() || !p[1].is_number() || !p[2].is_number())
        continue;

    // --- color ---
    if (!item.contains("color") || !item["color"].is_array())
        continue;

    const auto& c = item["color"];
    if (c.size() < 3)
        continue;

    if (c[0].is_null() || c[1].is_null() || c[2].is_null())
        continue;

    if (!c[0].is_number() || !c[1].is_number() || !c[2].is_number())
        continue;

    RealStar s;

    s.id.clear();
    if (item.contains("id") && item["id"].is_string())
    {
        s.id = item["id"].get<std::string>();
    }

    s.name.clear();
    if (item.contains("name") && item["name"].is_string())
    {
        s.name = item["name"].get<std::string>();
    }

    s.positionLy = glm::vec3(
        static_cast<float>(p[0].get<double>()),
        static_cast<float>(p[1].get<double>()),
        static_cast<float>(p[2].get<double>())
    );

    s.color = glm::vec3(
        static_cast<float>(c[0].get<double>()),
        static_cast<float>(c[1].get<double>()),
        static_cast<float>(c[2].get<double>())
    );

    // --- magnitude ---
    float visualMag = 6.0f;
    if (item.contains("visual_magnitude") && item["visual_magnitude"].is_number())
        visualMag = static_cast<float>(item["visual_magnitude"].get<double>());

    float absMag = visualMag;
    if (item.contains("absolute_magnitude") && item["absolute_magnitude"].is_number())
        absMag = static_cast<float>(item["absolute_magnitude"].get<double>());

    s.visualMagnitudeFromSol = visualMag;
    s.absoluteMagnitude = absMag;

    // --- flags ---
    s.isGameSystem = false;
    if (item.contains("isGameSystem") && item["isGameSystem"].is_boolean())
    {
        s.isGameSystem = item["isGameSystem"].get<bool>();
    }

    s.gameSystemId = -1;
    if (item.contains("gameSystemId") && item["gameSystemId"].is_number_integer())
    {
        s.gameSystemId = item["gameSystemId"].get<int>();
    }

        m_realStars.push_back(s);
    }

    std::cout
        << "[Starfield] real stars loaded="
        << m_realStars.size()
        << std::endl;

    return !m_realStars.empty();
}





void GalaxyStarfieldRenderer::generateProceduralField()
{
    std::mt19937 rng(0x5A17F13D);

    // Sparse halo / ordinary background stars.
    // Keep this restrained: real sky is mostly black, not wallpaper glitter.
    for (int i = 0; i < 260; ++i)
    {
        const float z = randomRange(rng, -1.0f, 1.0f);
        const float a = randomRange(rng, 0.0f, 6.2831853f);
        const float r = std::sqrt(std::max(0.0f, 1.0f - z * z));

        glm::vec3 dir(std::cos(a) * r, std::sin(a) * r, z);
        const float pseudoDistance = randomRange(rng, 120.0f, 1200.0f);
        const float tint = randomRange(rng, 0.72f, 0.96f);
        
        const float colorRoll = randomRange(rng, 0.0f, 1.0f);

        glm::vec3 color;

        if (colorRoll < 0.16f)
        {
            // Cold blue-white stars
            color = glm::vec3(0.45f, 0.66f, 1.0f);
        }
        else if (colorRoll < 0.34f)
        {
            // Pale cyan / young hot stars
            color = glm::vec3(0.62f, 0.86f, 1.0f);
        }
        else if (colorRoll < 0.68f)
        {
            // Neutral white
            color = glm::vec3(0.90f, 0.94f, 1.0f);
        }
        else if (colorRoll < 0.88f)
        {
            // Warm yellow
            color = glm::vec3(1.0f, 0.86f, 0.52f);
        }
        else
        {
            // Rare red/orange stars
            color = glm::vec3(1.0f, 0.38f, 0.22f);
        }

        StarSource s;
        s.positionLy = dir * pseudoDistance;
        s.color = color;
        // Skew hard toward dim stars. A few bright points are enough.
        const float u = randomRange(rng, 0.0f, 1.0f);
        s.magnitude = 0.025f + std::pow(u, 6.0f) * 0.22f;
        s.size = 0.28f + std::pow(randomRange(rng, 0.0f, 1.0f), 5.0f) * 0.70f;
        s.atlasStar = false;
        m_sources.push_back(s);
    }


    // Rare bright navigation stars.
    // These are not "dust"; they are visual anchors for orientation.
auto addConstellation = [&](float yaw, float pitch, float scale, int pattern)
{
    std::vector<glm::vec2> points;

    if (pattern == 0)
    {
        // Broken hook
        points = {
            {-1.00f, -0.20f},
            {-0.55f,  0.10f},
            {-0.10f,  0.02f},
            { 0.32f,  0.28f},
            { 0.72f,  0.18f},
            { 1.00f, -0.10f}
        };
    }
    else if (pattern == 1)
    {
        // Triangle with tail
        points = {
            {-0.65f, -0.35f},
            { 0.05f,  0.48f},
            { 0.70f, -0.22f},
            { 0.05f,  0.48f},
            { 0.18f,  0.95f}
        };
    }
    else if (pattern == 2)
    {
        // Long bent chain
        points = {
            {-1.10f, -0.12f},
            {-0.70f,  0.00f},
            {-0.32f,  0.16f},
            { 0.04f,  0.06f},
            { 0.46f,  0.24f},
            { 0.92f,  0.38f}
        };
    }
    else
    {
        // Compact asymmetric cluster
        points = {
            {-0.38f, -0.22f},
            {-0.08f,  0.18f},
            { 0.28f,  0.06f},
            { 0.52f, -0.31f},
            { 0.04f, -0.48f}
        };
    }

    for (size_t i = 0; i < points.size(); ++i)
    {
        const glm::vec2 p = points[i];

        const float localYaw =
            yaw + p.x * scale + randomRange(rng, -0.012f, 0.012f);

        const float localPitch =
            pitch + p.y * scale + randomRange(rng, -0.010f, 0.010f);

        glm::vec3 dir = makeDirFromAngles(localYaw, localPitch);

        const float colorRoll = randomRange(rng, 0.0f, 1.0f);

        glm::vec3 color;

        if (colorRoll < 0.18f)
        {
            color = glm::vec3(0.36f, 0.58f, 1.0f); // blue
        }
        else if (colorRoll < 0.36f)
        {
            color = glm::vec3(0.55f, 0.82f, 1.0f); // cyan-blue
        }
        else if (colorRoll < 0.62f)
        {
            color = glm::vec3(0.86f, 0.92f, 1.0f); // cold white
        }
        else if (colorRoll < 0.82f)
        {
            color = glm::vec3(1.0f, 0.82f, 0.46f); // amber
        }
        else
        {
            color = glm::vec3(1.0f, 0.28f, 0.14f); // red-orange
        }

        StarSource s;
        s.positionLy = dir * randomRange(rng, 300.0f, 1800.0f);
        s.color = color;
        s.magnitude = randomRange(rng, 0.78f, 1.0f);
        s.size = randomRange(rng, 3.6f, 6.8f);

        if (i == 1 || i == 2)
        {
            s.magnitude = randomRange(rng, 0.92f, 1.0f);
            s.size *= 1.75f;

            // Make main constellation stars more color-readable.
            if (colorRoll < 0.30f)
                s.color = glm::vec3(0.42f, 0.68f, 1.0f);
            else if (colorRoll > 0.86f)
                s.color = glm::vec3(1.0f, 0.30f, 0.16f);
        }

        s.atlasStar = false;
        m_sources.push_back(s);
    }
};

// Hand-placed fake constellations.
// These are visual landmarks, not physical catalog stars.
addConstellation(-1.45f,  0.18f, 0.230f, 0);
addConstellation(-0.62f, -0.02f, 0.205f, 1);
addConstellation( 0.20f,  0.20f, 0.225f, 2);
addConstellation( 0.98f, -0.12f, 0.195f, 3);
addConstellation( 1.76f,  0.10f, 0.215f, 0);

    // Milky Way / spiral-arm edge.
    // Not a full ring. This is a broken north-south river across the sky with
    // dense named-looking knots and dark rifts. It is fixed in global space.
    const glm::vec3 armAxis = glm::normalize(glm::vec3(0.10f, 0.93f, 0.35f));
    const glm::vec3 armCenter = glm::normalize(glm::vec3(-0.42f, 0.24f, 0.88f));
    const glm::vec3 armSide = glm::normalize(glm::cross(armAxis, armCenter));

    auto addArmCloud = [&](int count, float tCenter, float tSigma, float width, float sideBias, float brightnessMul, float sizeMul)
    {
        for (int i = 0; i < count; ++i)
        {
            float t = randomNormal(rng, tCenter, tSigma);
            t = std::max(-1.36f, std::min(1.36f, t));

            // Several dark rifts. We skip stars in these zones instead of drawing
            // an even procedural belt. The gaps are what make the pattern readable.
            const float riftA = std::exp(-std::pow((t + 0.58f) / 0.105f, 2.0f));
            const float riftB = std::exp(-std::pow((t - 0.03f) / 0.075f, 2.0f));
            const float riftC = std::exp(-std::pow((t - 0.72f) / 0.115f, 2.0f));
            const float rift = std::max(riftA * 0.68f, std::max(riftB * 0.82f, riftC * 0.55f));
            if (randomRange(rng, 0.0f, 1.0f) < rift)
                continue;

            const float alongWave = 0.065f * std::sin(t * 5.4f + 1.7f) + 0.035f * std::sin(t * 11.0f);
            const float side = randomNormal(rng, sideBias + alongWave, width);
            const float verticalDustSplit = (randomRange(rng, 0.0f, 1.0f) < 0.52f) ? -0.045f : 0.045f;
            const float split = verticalDustSplit * std::exp(-std::pow(t / 0.82f, 2.0f));

            glm::vec3 dir = glm::normalize(
                armCenter * std::cos(t) +
                armAxis   * std::sin(t) +
                armSide   * (side + split)
            );

            const float pseudoDistance = randomRange(rng, 320.0f, 2200.0f);
            const float knot =
                0.85f * std::exp(-std::pow((t + 0.96f) / 0.18f, 2.0f)) +
                1.00f * std::exp(-std::pow((t - 0.31f) / 0.14f, 2.0f)) +
                0.72f * std::exp(-std::pow((t - 1.02f) / 0.20f, 2.0f));

            glm::vec3 color = glm::mix(
                glm::vec3(0.50f, 0.58f, 0.76f),
                glm::vec3(1.0f, 0.82f, 0.55f),
                randomRange(rng, 0.18f, 0.72f)
            );

            StarSource s;
            s.positionLy = dir * pseudoDistance;
            s.color = color;
            s.magnitude = (0.06f + std::pow(randomRange(rng, 0.0f, 1.0f), 2.7f) * 0.36f + knot * 0.13f) * brightnessMul;
            s.size = (0.35f + std::pow(randomRange(rng, 0.0f, 1.0f), 3.0f) * 1.35f + knot * 0.35f) * sizeMul;
            s.atlasStar = false;
            m_sources.push_back(s);
        }
    };

    // One broad river and three recognizable knots. The asymmetry is deliberate.
    addArmCloud(360,  -0.15f, 0.82f, 0.060f,  0.000f, 0.55f, 0.70f);
    addArmCloud(120,  -0.98f, 0.16f, 0.050f, -0.025f, 0.78f, 0.82f);
    addArmCloud(170,   0.31f, 0.13f, 0.043f,  0.030f, 0.86f, 0.88f);
    addArmCloud(110,   1.02f, 0.19f, 0.055f, -0.018f, 0.68f, 0.78f);
}

























void GalaxyStarfieldRenderer::rebuildVertices()
{
    m_vertices.clear();
    m_vertices.reserve(m_sources.size() * 6);

    auto pushStarQuad = [&](const glm::vec3& dir,
                            const glm::vec4& color,
                            float visualSize)
    {
        glm::vec3 upRef = std::abs(dir.y) > 0.92f
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 right = glm::normalize(glm::cross(upRef, dir));
        glm::vec3 up    = glm::normalize(glm::cross(dir, right));

        const float angularSize = 0.00095f * visualSize * kStarAngularScale;
        const glm::vec3 center = dir * m_renderRadius;

        const glm::vec3 dx = right * (m_renderRadius * angularSize);
        const glm::vec3 dy = up    * (m_renderRadius * angularSize);

        auto makeVertex = [&](const glm::vec3& pos, float u, float v)
        {
            StarVertex sv;
            sv.position = pos;
            sv.color = color;
            sv.size = visualSize;
            sv.uv = glm::vec2(u, v);
            return sv;
        };

        StarVertex a = makeVertex(center - dx - dy, 0.0f, 0.0f);
        StarVertex b = makeVertex(center + dx - dy, 1.0f, 0.0f);
        StarVertex c = makeVertex(center + dx + dy, 1.0f, 1.0f);
        StarVertex d = makeVertex(center - dx + dy, 0.0f, 1.0f);

        m_vertices.push_back(a);
        m_vertices.push_back(b);
        m_vertices.push_back(c);

        m_vertices.push_back(a);
        m_vertices.push_back(c);
        m_vertices.push_back(d);
    };

    for (const auto& source : m_sources)
    {
        glm::vec3 relative = source.positionLy - m_observerPositionLy;
        const float len = glm::length(relative);

        if (len < kMinDirectionLength)
            continue;

        const glm::vec3 dir = relative / len;

        float alpha = source.magnitude;

        if (source.atlasStar)
            alpha = std::max(alpha, 0.68f);

        const glm::vec4 color(source.color, clamp01(alpha));
        const float size = source.atlasStar
            ? source.size * 1.15f
            : source.size;

        pushStarQuad(dir, color, size);
    }
}








void GalaxyStarfieldRenderer::rebuildVerticesFromRealCatalog()
{
    m_vertices.clear();
    m_vertices.reserve(m_realStars.size() * 6);

    auto pushStarQuad = [&](const glm::vec3& dir,
                            const glm::vec4& color,
                            float visualSize)
    {
        glm::vec3 upRef = std::abs(dir.y) > 0.92f
            ? glm::vec3(1.0f, 0.0f, 0.0f)
            : glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 right = glm::normalize(glm::cross(upRef, dir));
        glm::vec3 up    = glm::normalize(glm::cross(dir, right));

        const float angularSize = 0.00055f * visualSize;
        const glm::vec3 center = dir * m_renderRadius;

        const glm::vec3 dx = right * (m_renderRadius * angularSize);
        const glm::vec3 dy = up    * (m_renderRadius * angularSize);

        auto makeVertex = [&](const glm::vec3& pos, float u, float v)
        {
            StarVertex sv;
            sv.position = pos;
            sv.color = color;
            sv.size = visualSize;
            sv.uv = glm::vec2(u, v);
            return sv;
        };

        StarVertex a = makeVertex(center - dx - dy, 0.0f, 0.0f);
        StarVertex b = makeVertex(center + dx - dy, 1.0f, 0.0f);
        StarVertex c = makeVertex(center + dx + dy, 1.0f, 1.0f);
        StarVertex d = makeVertex(center - dx + dy, 0.0f, 1.0f);

        m_vertices.push_back(a);
        m_vertices.push_back(b);
        m_vertices.push_back(c);

        m_vertices.push_back(a);
        m_vertices.push_back(c);
        m_vertices.push_back(d);
    };

    for (const RealStar& star : m_realStars)
    {
        const glm::vec3 relative = star.positionLy - m_observerPositionLy;
        const float distanceLy = glm::length(relative);

        // Если наблюдатель находится прямо в этой системе,
        // сама звезда не рисуется как точка на небесной сфере.
        if (distanceLy < kMinDirectionLength)
            continue;

        const glm::vec3 dir = relative / distanceLy;

        // ВАЖНО:
        // visual_magnitude в каталоге — яркость из Sol.
        // Для чужих систем надо пересчитывать яркость от absolute_magnitude и текущей дистанции.
        const float apparentMagnitude =
            apparentMagnitudeFromAbsolute(star.absoluteMagnitude, distanceLy);

        float alpha = starAlphaFromMagnitude(apparentMagnitude);
        alpha = std::min(1.0f, alpha * kStarAlphaBoost);
        float size = starSizeFromMagnitude(apparentMagnitude, star.isGameSystem);

        if (star.isGameSystem)
        {
            alpha = std::min(1.0f, alpha * 1.18f);
        }

        // Очень слабые звёзды оставляем, но почти прозрачными.
        // Так каталог не превращается в обои, а остаётся реальным небом.
        const glm::vec4 color(star.color, clamp01(alpha));

        pushStarQuad(dir, color, size);
    }

    std::cout
        << "[Starfield] rebuilt from real catalog"
        << " stars=" << m_realStars.size()
        << " vertices=" << m_vertices.size()
        << " observerLy=("
        << m_observerPositionLy.x << ", "
        << m_observerPositionLy.y << ", "
        << m_observerPositionLy.z << ")"
        << std::endl;
}








void GalaxyStarfieldRenderer::render(
    const glm::mat4& view,
    const glm::mat4& projection,
    float sizeScale
)
{
    
    static bool s_loggedEmpty = false;
    static bool s_loggedMissingShader = false;

    if (!m_initialized)
    {
        if (!s_loggedEmpty)
        {
            std::cerr << "[Starfield] render skipped: renderer is not initialized" << std::endl;
            s_loggedEmpty = true;
        }
        return;
    }

    if (m_vertices.empty())
    {
        if (!s_loggedEmpty)
        {
            std::cerr << "[Starfield] render skipped: vertices=0" << std::endl;
            s_loggedEmpty = true;
        }
        return;
    }

    const GLuint shader = ShaderLibrary::instance().get("galaxy_starfield");
    if (shader == 0)
    {
        if (!s_loggedMissingShader)
        {
            std::cerr << "[Starfield] render skipped: shader 'galaxy_starfield' is missing or failed to load" << std::endl;
            s_loggedMissingShader = true;
        }
        return;
    }

    

    // Убираем перемещение камеры.
    // Небо должно только вращаться, но не иметь параллакса от движения корабля.
    const glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
    const glm::mat4 mvp = projection * viewNoTranslation;

    // Состояния задаём явно.
    // Важно: тут больше нет glGetIntegerv/glGetBooleanv/glIsEnabled.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glDisable(GL_CULL_FACE);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Сначала мягкая дымка Млечного Пути.
    m_milkyWayRenderer.render(view, projection, m_renderRadius);

    // Потом острые звёзды.
    glUseProgram(shader);

    const GLint locMvp = glGetUniformLocation(shader, "uMVP");
    if (locMvp >= 0)
        glUniformMatrix4fv(locMvp, 1, GL_FALSE, glm::value_ptr(mvp));

    const GLint locSizeScale = glGetUniformLocation(shader, "uSizeScale");
    if (locSizeScale >= 0)
        glUniform1f(locSizeScale, sizeScale);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(m_vertices.size()));
    glBindVertexArray(0);

    glUseProgram(0);

    // Возвращаем базовое состояние движка.
    // Если где-то дальше нужен другой state — тот pass должен сам его выставлять.
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}





glm::vec3 GalaxyStarfieldRenderer::colorForStarType(const std::string& type)
{
    if (type.find('O') != std::string::npos) return glm::vec3(0.55f, 0.68f, 1.0f);
    if (type.find('B') != std::string::npos) return glm::vec3(0.64f, 0.75f, 1.0f);
    if (type.find('A') != std::string::npos) return glm::vec3(0.78f, 0.86f, 1.0f);
    if (type.find('F') != std::string::npos) return glm::vec3(0.94f, 0.96f, 1.0f);
    if (type.find('G') != std::string::npos) return glm::vec3(1.0f, 0.92f, 0.74f);
    if (type.find('K') != std::string::npos) return glm::vec3(1.0f, 0.74f, 0.46f);
    if (type.find('M') != std::string::npos) return glm::vec3(1.0f, 0.48f, 0.34f);
    if (type.find("WD") != std::string::npos || type.find("white") != std::string::npos) return glm::vec3(0.82f, 0.9f, 1.0f);

    return glm::vec3(0.92f, 0.94f, 1.0f);
}
