#include "src/game/system_map/SystemMapRenderer.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <fstream>
#include <GLFW/glfw3.h>
#include <chrono>
#include <cmath>
#include <limits>
#include <filesystem>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/HUD/TextRenderer.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/render/bitmap/TextureLoader.h"


namespace
{
    constexpr double AU_KM = 149597870.7;
    
    std::unordered_map<GLFWwindow*, double*> g_systemMapScrollTargets;

    void systemMapScrollCallback(
        GLFWwindow* window,
        double,
        double yoffset
    )
    {
        auto it =
            g_systemMapScrollTargets.find(
                window
            );

        if (it == g_systemMapScrollTargets.end() ||
            it->second == nullptr)
        {
            return;
        }

        *it->second +=
            yoffset;
    }


    constexpr double AU_PER_LIGHT_YEAR = 63241.077084266;

    std::string fmtDistanceLy(double ly)
    {
        std::ostringstream ss;

        if (ly < 0.01)
        {
            ss << std::fixed << std::setprecision(4) << ly << " ly";
        }
        else if (ly < 10.0)
        {
            ss << std::fixed << std::setprecision(2) << ly << " ly";
        }
        else
        {
            ss << std::fixed << std::setprecision(1) << ly << " ly";
        }

        return ss.str();
    }
    

    

    std::string fmt2(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << v;
        return ss.str();
    }

    std::string fmt4(double v)
    {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(4) << v;
        return ss.str();
    }


    std::string normalizeGeneratedIdentityToken(
        const std::string& s
    )
    {
        std::string out;
        out.reserve(s.size());

        for (unsigned char c : s)
        {
            if (c < 128)
            {
                if (std::isalnum(c))
                {
                    out.push_back(
                        static_cast<char>(
                            std::tolower(c)
                        )
                    );
                }

                continue;
            }

            // Preserve UTF-8 bytes for exact non-ASCII matching.
            out.push_back(static_cast<char>(c));
        }

        return out;
    }

    void addGeneratedIdentityToken(
        std::vector<std::string>& out,
        const std::string& raw
    )
    {
        const std::string token =
            normalizeGeneratedIdentityToken(raw);

        if (token.empty())
            return;

        if (std::find(out.begin(), out.end(), token) == out.end())
            out.push_back(token);
    }

    std::vector<std::string> splitGeneratedIdentityPath(
        const std::string& s
    )
    {
        std::vector<std::string> out;
        std::string current;

        for (char c : s)
        {
            if (c == '.' || c == '/' || c == '\\')
            {
                if (!current.empty())
                {
                    out.push_back(current);
                    current.clear();
                }

                continue;
            }

            current.push_back(c);
        }

        if (!current.empty())
            out.push_back(current);

        return out;
    }

    std::string lastGeneratedIdentityPathPart(
        const std::string& s
    )
    {
        const std::vector<std::string> parts =
            splitGeneratedIdentityPath(s);

        if (parts.empty())
            return "";

        return parts.back();
    }

    const std::unordered_map<std::string, std::vector<std::string>>&
    knownGeneratedSolAliases()
    {
        static const std::unordered_map<std::string, std::vector<std::string>> aliases =
        {
            { "mercury",   { "Mercury", "Меркурий" } },
            { "venus",     { "Venus", "Венера" } },
            { "earth",     { "Earth", "Terra", "Gaia", "Земля" } },
            { "moon",      { "Moon", "Luna", "Луна" } },
            { "mars",      { "Mars", "Ares", "Марс" } },
            { "phobos",    { "Phobos", "Фобос" } },
            { "deimos",    { "Deimos", "Деймос" } },

            { "jupiter",   { "Jupiter", "Zeus", "Юпитер" } },
            { "ganymede",  { "Ganymede", "Ганимед" } },
            { "callisto",  { "Callisto", "Каллисто" } },
            { "io",        { "Io", "Ио" } },
            { "europa",    { "Europa", "Европа" } },
            { "amalthea",  { "Amalthea", "Амальтея" } },
            { "himalia",   { "Himalia", "Гималия" } },
            { "elara",     { "Elara", "Элара" } },
            { "pasiphae",  { "Pasiphae", "Пасифе" } },

            { "saturn",    { "Saturn", "Cronus", "Сатурн" } },
            { "titan",     { "Titan", "Титан" } },
            { "rhea",      { "Rhea", "Рея" } },
            { "iapetus",   { "Iapetus", "Япет" } },
            { "dione",     { "Dione", "Диона" } },
            { "tethys",    { "Tethys", "Тетис" } },
            { "enceladus", { "Enceladus", "Энцелад" } },
            { "mimas",     { "Mimas", "Мимас" } },
            { "phoebe",    { "Phoebe", "Феба" } },

            { "uranus",    { "Uranus", "Ouranos", "Уран" } },
            { "titania",   { "Titania", "Титания" } },
            { "oberon",    { "Oberon", "Оберон" } },
            { "umbriel",   { "Umbriel", "Умбриэль" } },
            { "ariel",     { "Ariel", "Ариэль" } },
            { "miranda",   { "Miranda", "Миранда" } },

            { "neptune",   { "Neptune", "Poseidon", "Нептун" } },
            { "triton",    { "Triton", "Тритон" } },
            { "proteus",   { "Proteus", "Протей" } },
            { "nereid",    { "Nereid", "Нереида" } },
            { "larissa",   { "Larissa", "Ларисса" } }
        };

        return aliases;
    }

    std::vector<std::string> systemMapBodyTokens(
        const world::celestial::SystemMapBody& body
    )
    {
        std::vector<std::string> out;

        addGeneratedIdentityToken(out, body.name);
        addGeneratedIdentityToken(out, lastGeneratedIdentityPathPart(body.id));

        for (const auto& alt : body.alternativeNames)
            addGeneratedIdentityToken(out, alt.name);

        return out;
    }

    std::vector<std::string> generatedAssetTokens(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    )
    {
        std::vector<std::string> out;

        addGeneratedIdentityToken(out, asset.bodyFolderName);
        addGeneratedIdentityToken(out, asset.displayName);

        const std::string folderKey =
            normalizeGeneratedIdentityToken(asset.bodyFolderName);

        const auto& aliases =
            knownGeneratedSolAliases();

        auto it =
            aliases.find(folderKey);

        if (it != aliases.end())
        {
            for (const std::string& alias : it->second)
                addGeneratedIdentityToken(out, alias);
        }

        return out;
    }




    std::vector<std::string> generatedBodyIdentityTokens(
        const std::string& bodyId,
        const std::string& displayName
    )
    {
        std::vector<std::string> out;

        addGeneratedIdentityToken(out, displayName);

        const std::string lastIdPart =
            lastGeneratedIdentityPathPart(bodyId);

        addGeneratedIdentityToken(out, lastIdPart);

        const std::string aliasKey =
            normalizeGeneratedIdentityToken(lastIdPart);

        const auto& aliases =
            knownGeneratedSolAliases();

        auto it =
            aliases.find(aliasKey);

        if (it != aliases.end())
        {
            for (const std::string& alias : it->second)
                addGeneratedIdentityToken(out, alias);
        }

        return out;
    }

    std::string generatedSystemFolderForSystemId(
        int systemId
    )
    {
        if (systemId == 0)
            return "sol";

        return "";
    }



















    bool anyGeneratedTokenMatches(
        const std::vector<std::string>& a,
        const std::vector<std::string>& b
    )
    {
        for (const std::string& x : a)
        {
            for (const std::string& y : b)
            {
                if (x == y)
                    return true;
            }
        }

        return false;
    }

    std::string generatedAssetKey(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    )
    {
        return asset.systemFolderName + "/" + asset.bodyFolderName;
    }





    double degToRadD(
        double deg
    )
    {
        return deg *
            glm::pi<double>() /
            180.0;
    }

    glm::dvec3 safeNormalizeD(
        const glm::dvec3& v,
        const glm::dvec3& fallback
    )
    {
        const double len =
            glm::length(v);

        if (len <= 1e-12)
            return fallback;

        return v / len;
    }

    glm::dvec3 planetNorthAxisWorld(
        const world::celestial::PlanetMapSnapshot& planet
    )
    {
        const double tilt =
            degToRadD(
                planet.planetAxialTiltDeg
            );

        const double node =
            degToRadD(
                planet.planetAxisNodeDeg
            );

        // axialTiltDeg = 0 means north is +Y.
        // axisNodeDeg chooses the tilt direction in XZ plane.
        return safeNormalizeD(
            glm::dvec3(
                std::sin(tilt) * std::cos(node),
                std::cos(tilt),
                std::sin(tilt) * std::sin(node)
            ),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    glm::dvec3 planetPrimeAxisWorld(
        const glm::dvec3& north
    )
    {
        glm::dvec3 ref(1.0, 0.0, 0.0);

        if (std::abs(glm::dot(ref, north)) > 0.92)
            ref = glm::dvec3(0.0, 0.0, 1.0);

        return safeNormalizeD(
            ref - north * glm::dot(ref, north),
            glm::dvec3(1.0, 0.0, 0.0)
        );
    }

    glm::dvec3 planetEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return safeNormalizeD(
            glm::cross(north, prime),
            glm::dvec3(0.0, 0.0, 1.0)
        );
    }

    glm::dvec3 planetSurfacePointMeters(
        const world::celestial::PlanetMapSnapshot& planet,
        double latitudeRad,
        double textureLongitudeRad,
        double radiusScale = 1.0
    )
    {
        const double radius =
            planet.planetRadiusMeters *
            radiusScale;

        const glm::dvec3 north =
            planetNorthAxisWorld(planet);

        const glm::dvec3 prime0 =
            planetPrimeAxisWorld(north);

        const glm::dvec3 east0 =
            planetEastAxisWorld(
                north,
                prime0
            );

        const double textureOffset =
            degToRadD(
                planet.planetTextureLongitudeOffsetDeg
            );

        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            planet.planetRotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 localWorld =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return
            planet.planetCenterMeters +
            localWorld;
    }

    






    glm::dvec3 systemBodyNorthAxisWorld(
        const world::celestial::SystemMapBody& body
    )
    {
        const double tilt =
            degToRadD(
                body.axialTiltDeg
            );

        const double node =
            degToRadD(
                body.axisNodeDeg
            );

        return safeNormalizeD(
            glm::dvec3(
                std::sin(tilt) * std::cos(node),
                std::cos(tilt),
                std::sin(tilt) * std::sin(node)
            ),
            glm::dvec3(0.0, 1.0, 0.0)
        );
    }

    glm::vec3 systemBodySurfacePoint(
        const world::celestial::SystemMapBody& body,
        const glm::vec3& center,
        float radius,
        double latitudeRad,
        double textureLongitudeRad
    )
    {
        const glm::dvec3 north =
            systemBodyNorthAxisWorld(
                body
            );

        const glm::dvec3 prime0 =
            planetPrimeAxisWorld(
                north
            );

        const glm::dvec3 east0 =
            planetEastAxisWorld(
                north,
                prime0
            );

        const double textureOffset =
            degToRadD(
                body.textureLongitudeOffsetDeg
            );

        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            body.rotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 local =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return center +
            glm::vec3(
                static_cast<float>(local.x),
                static_cast<float>(local.y),
                static_cast<float>(local.z)
            );
    }
















    std::filesystem::path resolveSystemMapAssetPath(
        const std::string& assetPath
    )
    {
        namespace fs = std::filesystem;

        const fs::path raw(assetPath);

        if (raw.is_absolute())
            return raw.lexically_normal();

        const fs::path cwd =
            fs::current_path();

        const std::vector<fs::path> candidates =
        {
            cwd / raw,
            cwd.parent_path() / raw,
            cwd.parent_path().parent_path() / raw,
            fs::path("D:/__elite/work") / raw
        };

        for (const fs::path& p : candidates)
        {
            if (fs::exists(p))
                return p.lexically_normal();
        }

        return (cwd.parent_path() / raw).lexically_normal();
    }




    constexpr float SYSTEM_MAP_FOV_DEG =
        48.0f;

    
    constexpr float SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT =
        0.0000005f;

    constexpr float SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT =
        5000.0f;

    constexpr float SYSTEM_MAP_ORTHO_EYE_DISTANCE =
        50000.0f;

    constexpr float SYSTEM_MAP_ORTHO_FAR_PLANE =
        120000.0f;

        double systemMapWorldUnitsPerPixel(
        double cameraHalfHeight,
        int viewportHeight
    )
    {
        const double safeHeight =
            static_cast<double>(
                std::max(
                    viewportHeight,
                    1
                )
            );

        const double halfHeight =
            std::clamp(
                cameraHalfHeight,
                static_cast<double>(SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT),
                static_cast<double>(SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT)
            );

        return
            (halfHeight * 2.0) /
            safeHeight;
    }

    glm::vec3 systemMapViewRight(
        const glm::mat4& view
    )
    {
        return glm::normalize(
            glm::vec3(
                view[0][0],
                view[1][0],
                view[2][0]
            )
        );
    }

    glm::vec3 systemMapViewUp(
        const glm::mat4& view
    )
    {
        return glm::normalize(
            glm::vec3(
                view[0][1],
                view[1][1],
                view[2][1]
            )
        );
    }

    glm::dvec3 systemMapTargetPlanePointFromScreen(
        const Viewport& vp,
        const glm::mat4& view,
        const glm::dvec3& target,
        double cameraHalfHeight,
        double localMouseX,
        double localMouseY
    )
    {
        const double safeWidth =
            static_cast<double>(
                std::max(
                    vp.width,
                    1
                )
            );

        const double safeHeight =
            static_cast<double>(
                std::max(
                    vp.height,
                    1
                )
            );

        const double aspect =
            safeWidth /
            safeHeight;

        const double halfHeightWorld =
            std::clamp(
                cameraHalfHeight,
                static_cast<double>(SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT),
                static_cast<double>(SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT)
            );

        const double halfWidthWorld =
            halfHeightWorld *
            aspect;

        const double ndcX =
            localMouseX / safeWidth * 2.0 - 1.0;

        const double ndcY =
            1.0 -
            localMouseY / safeHeight * 2.0;

        const glm::vec3 rightF =
            systemMapViewRight(
                view
            );

        const glm::vec3 upF =
            systemMapViewUp(
                view
            );

        const glm::dvec3 right(
            rightF.x,
            rightF.y,
            rightF.z
        );

        const glm::dvec3 up(
            upF.x,
            upF.y,
            upF.z
        );

        return
            target +
            right * ndcX * halfWidthWorld +
            up    * ndcY * halfHeightWorld;
    }




    

    

    

    double niceSystemMapScaleNumber(
        double value
    )
    {
        if (value <= 0.0 ||
            !std::isfinite(value))
        {
            return 1.0;
        }

        const double exponent =
            std::floor(
                std::log10(value)
            );

        const double base =
            std::pow(
                10.0,
                exponent
            );

        const double normalized =
            value / base;

        double nice =
            1.0;

        if (normalized <= 1.0)
            nice = 1.0;
        else if (normalized <= 2.0)
            nice = 2.0;
        else if (normalized <= 5.0)
            nice = 5.0;
        else
            nice = 10.0;

        return
            nice * base;
    }

    std::string fmtSystemMapScaleDistance(
        double km
    )
    {
        std::ostringstream ss;

        if (km >= AU_KM * 0.1)
        {
            ss
                << std::fixed
                << std::setprecision(3)
                << (km / AU_KM)
                << " AU";
        }
        else if (km >= 1000000.0)
        {
            ss
                << std::fixed
                << std::setprecision(2)
                << (km / 1000000.0)
                << " M km";
        }
        else if (km >= 1000.0)
        {
            ss
                << std::fixed
                << std::setprecision(0)
                << km
                << " km";
        }
        else
        {
            ss
                << std::fixed
                << std::setprecision(1)
                << km
                << " km";
        }

        return ss.str();
    }









    float wrapAngleRadF(
        float a
    )
    {
        const float twoPi =
            glm::two_pi<float>();

        while (a > glm::pi<float>())
            a -= twoPi;

        while (a < -glm::pi<float>())
            a += twoPi;

        return a;
    }

    double wrapAngleRadD(
        double a
    )
    {
        const double twoPi =
            glm::two_pi<double>();

        while (a > glm::pi<double>())
            a -= twoPi;

        while (a < -glm::pi<double>())
            a += twoPi;

        return a;
    }

    glm::vec3 orbitCameraDirectionFromYawPitch(
        float yaw,
        float pitch
    )
    {
        const float cp =
            std::cos(pitch);

        const float sp =
            std::sin(pitch);

        const float cy =
            std::cos(yaw);

        const float sy =
            std::sin(yaw);

        return glm::vec3(
            cp * sy,
            sp,
            cp * cy
        );
    }

    glm::vec3 orbitCameraUpFromYawPitch(
        float yaw,
        float pitch
    )
    {
        const float cp =
            std::cos(pitch);

        const float sp =
            std::sin(pitch);

        const float cy =
            std::cos(yaw);

        const float sy =
            std::sin(yaw);

        // Это производная direction по pitch.
        // Она всегда перпендикулярна направлению взгляда,
        // даже когда камера переваливает через верх/низ.
        glm::vec3 up(
            -sp * sy,
            cp,
            -sp * cy
        );

        if (glm::length(up) < 0.000001f)
            return glm::vec3(0.0f, 1.0f, 0.0f);

        return glm::normalize(up);
    }








}

void SystemMapRenderer::init()
{
    ensureGlObjects();
    ensureShader();

    ensureTexturedGlObjects();
    ensureTexturedShader();

    ensureBackground();

    ensureGeneratedCelestialAssets();

    GLFWwindow* window = glfwGetCurrentContext();

    if (window)
    {
        g_systemMapScrollTargets[window] = &m_pendingScrollY;

        glfwSetScrollCallback(
            window,
            systemMapScrollCallback
        );
    }

    m_initialized = true;
}

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







void SystemMapRenderer::ensureTexturedGlObjects()
{
    if (m_texturedVao && m_texturedVbo)
        return;

    glGenVertexArrays(1, &m_texturedVao);
    glGenBuffers(1, &m_texturedVbo);

    glBindVertexArray(m_texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texturedVbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, pos))
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, uv))
    );

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(
        2,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TexturedVertex),
        reinterpret_cast<void*>(offsetof(TexturedVertex, color))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
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







void SystemMapRenderer::ensureGeneratedCelestialAssets()
{
    if (m_generatedCelestialAssetsAttempted)
        return;

    m_generatedCelestialAssetsAttempted = true;

    world::celestial::visual::CelestialGeneratedAssetLibraryOptions options;

    options.generatedRoot =
        "assets/generated/celestial";

    // Renderer path should be tolerant.
    // Full validation is done by:
    //   --check-generated-celestial-assets
    options.validateFiles = false;
    options.verbose = false;

    m_generatedCelestialAssetsLoaded =
    m_generatedCelestialAssets.load(options);

    if (m_generatedCelestialAssetsLoaded)
    {
        std::cout
            << "[SystemMapRenderer] generated celestial assets loaded: "
            << m_generatedCelestialAssets.assets().size()
            << "\n";
    }
    else
    {
        std::cerr
            << "[SystemMapRenderer] generated celestial assets unavailable; "
            << "system map will use color fallback bodies.\n";
    }
}







void SystemMapRenderer::resetView()
{
    m_mode = Mode::Galaxy;
    m_galaxyCamera = GalaxyCamera{};
    m_systemCamera = SystemCamera{};
    m_planetCamera = DetailCamera{};
    m_hubCamera = DetailCamera{};
    m_selectedSystemId = -1;
    m_focusedSystemId = -1;
    m_selectedBodyId.clear();
    m_comboOpen = false;
    m_pendingScrollY = 0.0;
    m_systemOrbitPivotAbsolute = glm::dvec3(0.0, 0.0, 0.0);
    m_systemOrbitPivotActive = false;
    m_galaxyOrbitPivotWorld = glm::vec3(0.0f, 0.0f, 0.0f);
    m_galaxyOrbitPivotActive = false;
    m_galaxyMouseDownX = 0.0;
    m_galaxyMouseDownY = 0.0;
    m_hubMapOrbitPivotLocalMeters = glm::dvec3(0.0, 0.0, 0.0);
    m_hubMapOrbitPivotScreenPx = glm::dvec2(0.0, 0.0);
    m_lastHubMapScale = 1.0;
    m_lastHubMapCenterPx = glm::dvec2(0.0, 0.0);
    m_lastGalaxyScreenPoints.clear();
    m_lastSystemBodyScreenPoints.clear();
    m_lastHubMapPickables.clear();
    m_lastSystemBodyAbsolutePosById.clear();
    m_smoothBodyPositions.clear();
    m_smoothObjectPositions.clear();
    m_lastSmoothTimeSeconds = 0.0;
    m_lastSystemScale = 1.0f;
}




void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    for (const auto& s : galaxy.systems)
    {
        if (s.id != systemId)
            continue;

        m_selectedSystemId = s.id;
        m_focusedSystemId = s.id;
        m_galaxyCamera.target = galaxyStarPosition(s);

        // Не зумим насильно, но если камера слишком далеко —
        // подводим к рабочей дистанции.
        if (m_galaxyCamera.distance > 130.0f)
            m_galaxyCamera.distance = 130.0f;

        return;
    }
}

int SystemMapRenderer::selectedSystemId() const
{
    return m_selectedSystemId;
}


int SystemMapRenderer::focusedSystemId() const
{
    return m_focusedSystemId;
}


void SystemMapRenderer::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    m_mode = mode;

    if (m_mode == Mode::Planet)
    {
        m_planetCamera =
            DetailCamera{};
    }

    if (m_mode == Mode::Hub)
    {
        m_hubCamera =
            DetailCamera{};

        m_hubMapOrbitPivotLocalMeters =
            glm::dvec3(0.0, 0.0, 0.0);

        m_hubMapOrbitPivotScreenPx =
            glm::dvec2(0.0, 0.0);
    }
}



SystemMapRenderer::Mode SystemMapRenderer::mode() const
{
    return m_mode;
}




SystemMapRenderer::DetailCamera&
SystemMapRenderer::activeDetailCamera()
{
    if (m_mode == Mode::Hub)
        return m_hubCamera;

    return m_planetCamera;
}

const SystemMapRenderer::DetailCamera&
SystemMapRenderer::activeDetailCamera() const
{
    if (m_mode == Mode::Hub)
        return m_hubCamera;

    return m_planetCamera;
}

const SystemMapRenderer::DetailControlSettings&
SystemMapRenderer::activeDetailControls() const
{
    if (m_mode == Mode::Hub)
        return m_hubControls;

    return m_planetControls;
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

void SystemMapRenderer::addCircleYZ(
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

        glm::vec3 p0 = center + glm::vec3(0.0f, std::cos(a0) * radius, std::sin(a0) * radius);
        glm::vec3 p1 = center + glm::vec3(0.0f, std::cos(a1) * radius, std::sin(a1) * radius);

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












void SystemMapRenderer::addSphereWire(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color
)
{
    addCircleXZ(center, radius, color, 64);
    addCircleXY(center, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.65f), 64);
    addCircleYZ(center, radius, glm::vec4(color.r, color.g, color.b, color.a * 0.65f), 64);
}





void SystemMapRenderer::beginSolids()
{
    m_solidVertices.clear();
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
        batch.vertices.clear();
    }
}

void SystemMapRenderer::addTexturedBillboard(
    GLuint texture,
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view
)
{
    if (texture == 0 || radius <= 0.0f)
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

    const glm::vec3 p0 =
        center + (-right - up) * radius;

    const glm::vec3 p1 =
        center + ( right - up) * radius;

    const glm::vec3 p2 =
        center + ( right + up) * radius;

    const glm::vec3 p3 =
        center + (-right + up) * radius;

    TexturedBatch* batch = nullptr;

    for (auto& b : m_texturedBatches)
    {
        if (b.texture == texture)
        {
            batch = &b;
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

    batch->vertices.reserve(
        batch->vertices.size() + 6u
    );

   

    // TextureLoader currently flips loaded images vertically.
    // This UV layout matches the existing OpenGL texture convention.
    batch->vertices.push_back({ p0, glm::vec2(0.0f, 0.0f), color });
    batch->vertices.push_back({ p1, glm::vec2(1.0f, 0.0f), color });
    batch->vertices.push_back({ p2, glm::vec2(1.0f, 1.0f), color });

    batch->vertices.push_back({ p0, glm::vec2(0.0f, 0.0f), color });
    batch->vertices.push_back({ p2, glm::vec2(1.0f, 1.0f), color });
    batch->vertices.push_back({ p3, glm::vec2(0.0f, 1.0f), color });
}










void SystemMapRenderer::addTexturedSystemBodySphere(
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

    latSegments =
        std::max(
            latSegments,
            8
        );

    lonSegments =
        std::max(
            lonSegments,
            16
        );

    TexturedBatch* batch = nullptr;

    const glm::dvec3 north =
    systemBodyNorthAxisWorld(
        body
    );

const glm::dvec3 prime0 =
    planetPrimeAxisWorld(
        north
    );

const glm::dvec3 east0 =
    planetEastAxisWorld(
        north,
        prime0
    );

const double textureOffset =
    degToRadD(
        body.textureLongitudeOffsetDeg
    );

auto bodyPoint =
    [&](double latitudeRad, double textureLongitudeRad) -> glm::vec3
    {
        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            body.rotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 local =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return center +
            glm::vec3(
                static_cast<float>(local.x),
                static_cast<float>(local.y),
                static_cast<float>(local.z)
            );
    };

    for (auto& b : m_texturedBatches)
    {
        if (b.texture == texture)
        {
            batch = &b;
            break;
        }
    }

    if (!batch)
    {
        TexturedBatch newBatch;
        newBatch.texture = texture;

        m_texturedBatches.push_back(
            std::move(newBatch)
        );

        batch =
            &m_texturedBatches.back();
    }



    const std::size_t vertexCountToAdd =
        static_cast<std::size_t>(latSegments) *
        static_cast<std::size_t>(lonSegments) *
        6u;

    batch->vertices.reserve(
        batch->vertices.size() + vertexCountToAdd
    );









    for (int iy = 0; iy < latSegments; ++iy)
    {
        const float v0 =
            static_cast<float>(iy) /
            static_cast<float>(latSegments);

        const float v1 =
            static_cast<float>(iy + 1) /
            static_cast<float>(latSegments);

        const double lat0 =
            -glm::half_pi<double>() +
            static_cast<double>(v0) *
            glm::pi<double>();

        const double lat1 =
            -glm::half_pi<double>() +
            static_cast<double>(v1) *
            glm::pi<double>();

        for (int ix = 0; ix < lonSegments; ++ix)
        {
            const float u0 =
                static_cast<float>(ix) /
                static_cast<float>(lonSegments);

            const float u1 =
                static_cast<float>(ix + 1) /
                static_cast<float>(lonSegments);

            const double lon0 =
                -glm::pi<double>() +
                static_cast<double>(u0) *
                glm::two_pi<double>();

            const double lon1 =
                -glm::pi<double>() +
                static_cast<double>(u1) *
                glm::two_pi<double>();

            const glm::vec3 p00 =
                bodyPoint(
                    lat0,
                    lon0
                );

            const glm::vec3 p10 =
                bodyPoint(
                    lat0,
                    lon1
                );

            const glm::vec3 p11 =
                bodyPoint(
                    lat1,
                    lon1
                );

            const glm::vec3 p01 =
                bodyPoint(
                    lat1,
                    lon0
                );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p10, glm::vec2(u1, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p01, glm::vec2(u0, v1), color }
            );
        }
    }
}











void SystemMapRenderer::addTexturedSphere(
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

    latSegments =
        std::max(
            latSegments,
            8
        );

    lonSegments =
        std::max(
            lonSegments,
            16
        );

    TexturedBatch* batch = nullptr;

    for (auto& b : m_texturedBatches)
    {
        if (b.texture == texture)
        {
            batch = &b;
            break;
        }
    }

    if (!batch)
    {
        TexturedBatch newBatch;
        newBatch.texture = texture;

        m_texturedBatches.push_back(
            std::move(newBatch)
        );

        batch =
            &m_texturedBatches.back();
    }


    const std::size_t vertexCountToAdd =
        static_cast<std::size_t>(latSegments) *
        static_cast<std::size_t>(lonSegments) *
        6u;

    batch->vertices.reserve(
        batch->vertices.size() + vertexCountToAdd
    );



    auto spherePoint =
        [&](float lat, float lon) -> glm::vec3
        {
            const float cosLat =
                std::cos(lat);

            return center +
                glm::vec3(
                    std::cos(lon) * cosLat,
                    std::sin(lat),
                    std::sin(lon) * cosLat
                ) * radius;
        };

    for (int iy = 0; iy < latSegments; ++iy)
    {
        const float v0 =
            static_cast<float>(iy) /
            static_cast<float>(latSegments);

        const float v1 =
            static_cast<float>(iy + 1) /
            static_cast<float>(latSegments);

        const float lat0 =
            -glm::half_pi<float>() +
            v0 * glm::pi<float>();

        const float lat1 =
            -glm::half_pi<float>() +
            v1 * glm::pi<float>();

        for (int ix = 0; ix < lonSegments; ++ix)
        {
            const float u0 =
                static_cast<float>(ix) /
                static_cast<float>(lonSegments);

            const float u1 =
                static_cast<float>(ix + 1) /
                static_cast<float>(lonSegments);

            const float lon0 =
                -glm::pi<float>() +
                u0 * glm::two_pi<float>();

            const float lon1 =
                -glm::pi<float>() +
                u1 * glm::two_pi<float>();


            const glm::vec3 p00 =
                spherePoint(
                    lat0,
                    lon0
                );

            const glm::vec3 p10 =
                spherePoint(
                    lat0,
                    lon1
                );

            const glm::vec3 p11 =
                spherePoint(
                    lat1,
                    lon1
                );

            const glm::vec3 p01 =
                spherePoint(
                    lat1,
                    lon0
                );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p10, glm::vec2(u1, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p01, glm::vec2(u0, v1), color }
            );
        }
    }
}













void SystemMapRenderer::flushTexturedBodies(
    const glm::mat4& mvp
)
{
    if (!m_texturedShader ||
        !m_texturedVao ||
        !m_texturedVbo ||
        m_texturedBatches.empty())
    {
        return;
    }

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    GLboolean depthMaskWasEnabled = GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldDepthFunc = GL_LESS;

    glGetIntegerv(
        GL_DEPTH_FUNC,
        &oldDepthFunc
    );

    GLint oldCullFaceMode = GL_BACK;

    glGetIntegerv(
        GL_CULL_FACE_MODE,
        &oldCullFaceMode
    );

    GLint oldFrontFaceMode = GL_CCW;

    glGetIntegerv(
        GL_FRONT_FACE,
        &oldFrontFaceMode
    );




    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Textured system bodies are real 3D spheres.
    // At strong orthographic zoom their front/back depth difference is tiny
    // relative to the system-map far plane. Rendering both sides causes
    // z-fighting stripes. Cull backfaces and draw only the visible shell.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Планеты и луны рисуем как opaque geometry.
    // Alpha-канал generated texture не должен резать сферу полосами.
    glDisable(GL_BLEND);

    glUseProgram(m_texturedShader);

    glUniformMatrix4fv(
        m_texturedMvpLoc,
        1,
        GL_FALSE,
        glm::value_ptr(mvp)
    );

    glUniform1i(m_texturedSamplerLoc, 0);

    glBindVertexArray(m_texturedVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_texturedVbo);

    glActiveTexture(GL_TEXTURE0);

    for (const TexturedBatch& batch : m_texturedBatches)
    {
        if (batch.texture == 0 ||
            batch.vertices.empty())
        {
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, batch.texture);

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                batch.vertices.size() * sizeof(TexturedVertex)
            ),
            batch.vertices.data(),
            GL_DYNAMIC_DRAW
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            static_cast<GLsizei>(batch.vertices.size())
        );
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    glDepthFunc(
        oldDepthFunc
    );

    glDepthMask(
        depthMaskWasEnabled
    );




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







const world::celestial::visual::CelestialGeneratedAssetSet*
SystemMapRenderer::generatedAssetForBody(
    const world::celestial::SystemMapBody& body
) const
{
    using world::celestial::BodyType;

    if (!m_generatedCelestialAssetsLoaded)
        return nullptr;

    if (body.type == BodyType::Star ||
        body.type == BodyType::AsteroidBelt)
    {
        return nullptr;
    }

    const std::vector<std::string> bodyTokens =
        systemMapBodyTokens(body);

    for (const auto& asset : m_generatedCelestialAssets.assets())
    {
        const std::vector<std::string> assetTokens =
            generatedAssetTokens(asset);

        if (anyGeneratedTokenMatches(bodyTokens, assetTokens))
            return &asset;
    }

    return nullptr;
}




const world::celestial::visual::CelestialGeneratedAssetSet*
SystemMapRenderer::generatedAssetForIdentity(
    int systemId,
    const std::string& bodyId,
    const std::string& displayName
) const
{
    if (!m_generatedCelestialAssetsLoaded)
        return nullptr;

    const std::string systemFolder =
        generatedSystemFolderForSystemId(systemId);

    if (systemFolder.empty())
        return nullptr;

    const std::vector<std::string> bodyTokens =
        generatedBodyIdentityTokens(
            bodyId,
            displayName
        );

    for (const auto& asset : m_generatedCelestialAssets.assets())
    {
        if (normalizeGeneratedIdentityToken(asset.systemFolderName) !=
            normalizeGeneratedIdentityToken(systemFolder))
        {
            continue;
        }

        const std::vector<std::string> assetTokens =
            generatedAssetTokens(asset);

        if (anyGeneratedTokenMatches(bodyTokens, assetTokens))
            return &asset;
    }

    return nullptr;
}

GLuint SystemMapRenderer::mapPreviewTextureForGeneratedAsset(
    const world::celestial::visual::CelestialGeneratedAssetSet& asset
)
{
    if (asset.map.preview512Path.empty())
        return 0;

    const std::string key =
        generatedAssetKey(asset);

    auto existing =
        m_mapPreviewTextureByAssetKey.find(key);

    if (existing != m_mapPreviewTextureByAssetKey.end())
        return existing->second;

    const std::filesystem::path resolvedPath =
        resolveSystemMapAssetPath(asset.map.preview512Path);

    const GLuint tex =
        TextureLoader::load2D(
            resolvedPath.generic_string(),
            false
        );

    m_mapPreviewTextureByAssetKey[key] =
        tex;

    if (tex == 0)
    {
        std::cerr
            << "[SystemMapRenderer] failed to load map preview texture for "
            << key
            << " path="
            << resolvedPath.generic_string()
            << "\n";
    }

    return tex;
}

GLuint SystemMapRenderer::mapPreviewTextureForPlanetSnapshot(
    const world::celestial::PlanetMapSnapshot& planet
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset)
        return 0;

    return mapPreviewTextureForGeneratedAsset(*asset);
}

GLuint SystemMapRenderer::mapPreviewTextureForHubSnapshot(
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* asset =
        generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (!asset)
        return 0;

    return mapPreviewTextureForGeneratedAsset(*asset);
}







GLuint SystemMapRenderer::globalAlbedoTextureForGeneratedAsset(
    const world::celestial::visual::CelestialGeneratedAssetSet& asset
)
{
    if (asset.global.albedoPath.empty())
        return 0;

    const std::string key =
        generatedAssetKey(asset);

    auto existing =
        m_globalAlbedoTextureByAssetKey.find(key);

    if (existing != m_globalAlbedoTextureByAssetKey.end())
        return existing->second;

    const std::filesystem::path resolvedPath =
        resolveSystemMapAssetPath(
            asset.global.albedoPath
        );

    const GLuint tex =
        TextureLoader::load2D(
            resolvedPath.generic_string(),
            false
        );

    m_globalAlbedoTextureByAssetKey[key] =
        tex;

    if (tex == 0)
    {
        std::cerr
            << "[SystemMapRenderer] failed to load global albedo texture for "
            << key
            << " path="
            << resolvedPath.generic_string()
            << "\n";
    }

    return tex;
}

GLuint SystemMapRenderer::globalAlbedoTextureForPlanetSnapshot(
    const world::celestial::PlanetMapSnapshot& planet
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset)
        return 0;

    return globalAlbedoTextureForGeneratedAsset(
        *asset
    );
}




bool SystemMapRenderer::drawPlanetShapeModelDetail(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset ||
        !asset->hasShapeModel())
    {
        return false;
    }

    if (planet.planetRadiusMeters <= 1.0)
        return false;

    const std::string objPath =
        resolveSystemMapAssetPath(
            asset->base.shapeModelPath
        ).generic_string();

    std::string albedoPath;

    if (!asset->global.albedoPath.empty())
    {
        albedoPath =
            resolveSystemMapAssetPath(
                asset->global.albedoPath
            ).generic_string();
    }
    else if (!asset->base.albedoPath.empty())
    {
        albedoPath =
            resolveSystemMapAssetPath(
                asset->base.albedoPath
            ).generic_string();
    }

    const render::celestial::CelestialShapeMesh* mesh =
        m_celestialShapeMeshes.load(
            objPath,
            albedoPath,
            true
        );

    if (!mesh ||
        !mesh->valid())
    {
        return false;
    }

    const double meshToMeters =
        planet.planetRadiusMeters /
        static_cast<double>(
            mesh->boundRadius
        );

    const glm::dvec3 north =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 prime =
        planetPrimeAxisWorld(
            north
        );

    const glm::dvec3 east =
        planetEastAxisWorld(
            north,
            prime
        );

    const double cp =
        std::cos(
            planet.planetRotationPhaseRad
        );

    const double sp =
        std::sin(
            planet.planetRotationPhaseRad
        );

    auto meshPointToRelativeMeters =
        [&](const glm::vec3& p) -> glm::dvec3
        {
            const glm::dvec3 local(
                static_cast<double>(p.x - mesh->boundCenter.x) * meshToMeters,
                static_cast<double>(p.y - mesh->boundCenter.y) * meshToMeters,
                static_cast<double>(p.z - mesh->boundCenter.z) * meshToMeters
            );

            const glm::dvec3 spun(
                local.x * cp - local.z * sp,
                local.y,
                local.x * sp + local.z * cp
            );

            return
                prime * spun.x +
                north * spun.y +
                east  * spun.z;
        };

    struct ProjectedTri
    {
        glm::dvec2 aScreen {0.0};
        glm::dvec2 bScreen {0.0};
        glm::dvec2 cScreen {0.0};

        glm::vec2 aUv {0.0f};
        glm::vec2 bUv {0.0f};
        glm::vec2 cUv {0.0f};

        double depth = 0.0;
    };

    std::vector<ProjectedTri> projected;
    projected.reserve(
        mesh->triangles.size()
    );

    for (const render::celestial::CelestialShapeTriangle& tri : mesh->triangles)
    {
        const glm::dvec3 aRel =
            meshPointToRelativeMeters(
                tri.a.pos
            );

        const glm::dvec3 bRel =
            meshPointToRelativeMeters(
                tri.b.pos
            );

        const glm::dvec3 cRel =
            meshPointToRelativeMeters(
                tri.c.pos
            );

        const glm::dvec3 aCam =
            planetMapCameraSpaceRelative(
                aRel
            );

        const glm::dvec3 bCam =
            planetMapCameraSpaceRelative(
                bRel
            );

        const glm::dvec3 cCam =
            planetMapCameraSpaceRelative(
                cRel
            );

        const glm::dvec3 midCam =
            (
                aCam +
                bCam +
                cCam
            ) / 3.0;

        const glm::dvec3 faceNormal =
            glm::cross(
                bCam - aCam,
                cCam - aCam
            );

        if (glm::length(faceNormal) <= 1e-9)
            continue;

        glm::dvec3 outwardNormal =
            glm::normalize(
                faceNormal
            );

        // OBJ winding может быть любым.
        // Для тела, центрированного около нуля, midCam задаёт наружное направление.
        // Если normal смотрит внутрь, разворачиваем его.
        if (glm::dot(
                outwardNormal,
                midCam
            ) < 0.0)
        {
            outwardNormal =
                -outwardNormal;
        }

        // В planet detail convention видимая сторона имеет +Z.
        //
        // ВАЖНО:
        // Для irregular OBJ нельзя резать силуэт строго по normal.z <= 0.
        // Большие треугольники на краю могут быть частично видимыми.
        // Если выбрасывать их целиком, появляется "рваный" зубчатый край.
        //
        // Поэтому оставляем небольшой допуск за линию силуэта.
        constexpr double kSilhouetteNormalTolerance =
            -0.08;

        if (outwardNormal.z < kSilhouetteNormalTolerance)
            continue;

        const glm::dvec3 aWorld =
            planet.planetCenterMeters +
            aRel;

        const glm::dvec3 bWorld =
            planet.planetCenterMeters +
            bRel;

        const glm::dvec3 cWorld =
            planet.planetCenterMeters +
            cRel;

        ProjectedTri out;

        out.aScreen =
            planetMapProject(
                aWorld,
                planet,
                scale,
                centerPx
            );

        out.bScreen =
            planetMapProject(
                bWorld,
                planet,
                scale,
                centerPx
            );

        out.cScreen =
            planetMapProject(
                cWorld,
                planet,
                scale,
                centerPx
            );




        const double screenArea =
        (
            out.bScreen.x - out.aScreen.x
        ) *
        (
            out.cScreen.y - out.aScreen.y
        ) -
        (
            out.bScreen.y - out.aScreen.y
        ) *
        (
            out.cScreen.x - out.aScreen.x
        );

    if (std::abs(screenArea) < 0.05)
        continue;







        out.aUv =
            tri.a.uv;

        out.bUv =
            tri.b.uv;

        out.cUv =
            tri.c.uv;

        out.depth =
            midCam.z;

        projected.push_back(
            out
        );
    }

    if (projected.empty())
        return false;

    // Рисуем дальние front-facing треугольники раньше,
    // ближние позже. Это заменяет depth buffer для 2D-проекции.
    std::sort(
        projected.begin(),
        projected.end(),
        [](const ProjectedTri& a, const ProjectedTri& b)
        {
            return a.depth < b.depth;
        }
    );

   
    
    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    GLboolean alphaTestWasEnabled =
        glIsEnabled(
            GL_ALPHA_TEST
        );







    GLint oldTextureBinding =
        0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(
        0
    );

    // Shape model должен быть opaque.
    // Blend/alpha-test дают эффект дыр, просвечивания и "внутренней" текстуры.
    glDisable(
        GL_BLEND
    );

    glDisable(
        GL_ALPHA_TEST
    );

    // Shape model должен быть opaque.
    // Blend даёт эффект "текстура видна изнутри".
    glDisable(
        GL_BLEND
    );

    if (mesh->albedoTexture != 0)
    {
        glEnable(
            GL_TEXTURE_2D
        );

        glBindTexture(
            GL_TEXTURE_2D,
            mesh->albedoTexture
        );

        glColor4f(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );
    }
    else
    {
        glDisable(
            GL_TEXTURE_2D
        );

        glColor4f(
            0.62f,
            0.60f,
            0.56f,
            1.0f
        );
    }

    glBegin(
        GL_TRIANGLES
    );

    for (const ProjectedTri& tri : projected)
    {
        glTexCoord2f(
            tri.aUv.x,
            tri.aUv.y
        );

        glVertex2d(
            tri.aScreen.x,
            tri.aScreen.y
        );

        glTexCoord2f(
            tri.bUv.x,
            tri.bUv.y
        );

        glVertex2d(
            tri.bScreen.x,
            tri.bScreen.y
        );

        glTexCoord2f(
            tri.cUv.x,
            tri.cUv.y
        );

        glVertex2d(
            tri.cScreen.x,
            tri.cScreen.y
        );
    }

    glEnd();

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            oldTextureBinding
        )
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);



    if (alphaTestWasEnabled)
        glEnable(GL_ALPHA_TEST);
    else
        glDisable(GL_ALPHA_TEST);







    return true;
}

























GLuint SystemMapRenderer::globalAlbedoTextureForBody(
    const world::celestial::SystemMapBody& body
)
{
    const auto* asset =
        generatedAssetForBody(
            body
        );

    if (!asset)
        return 0;

    return globalAlbedoTextureForGeneratedAsset(
        *asset
    );
}










GLuint SystemMapRenderer::mapPreviewTextureForBody(
    const world::celestial::SystemMapBody& body
)
{
    const auto* asset =
        generatedAssetForBody(body);

    if (!asset)
        return 0;

    return mapPreviewTextureForGeneratedAsset(*asset);
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

glm::vec4 SystemMapRenderer::colorForStarType(const std::string& starType) const
{
    if (starType.empty())
        return { 1.0f, 0.86f, 0.65f, 1.0f };

    const char t = static_cast<char>(std::toupper(starType[0]));

    switch (t)
    {
        case 'O': return { 0.61f, 0.69f, 1.00f, 1.0f };
        case 'B': return { 0.66f, 0.75f, 1.00f, 1.0f };
        case 'A': return { 0.86f, 0.91f, 1.00f, 1.0f };
        case 'F': return { 0.97f, 0.97f, 1.00f, 1.0f };
        case 'G': return { 1.00f, 0.92f, 0.62f, 1.0f };
        case 'K': return { 1.00f, 0.73f, 0.45f, 1.0f };
        case 'M': return { 1.00f, 0.43f, 0.31f, 1.0f };
        default:  return { 1.00f, 0.86f, 0.65f, 1.0f };
    }
}

glm::vec4 SystemMapRenderer::colorForBodyType(world::celestial::BodyType type) const
{
    using world::celestial::BodyType;

    switch (type)
    {
        case BodyType::Star:         return { 1.00f, 0.93f, 0.62f, 1.00f };
        case BodyType::Planet:       return { 0.36f, 0.68f, 1.00f, 1.00f };
        case BodyType::Moon:         return { 0.70f, 0.72f, 0.78f, 1.00f };
        case BodyType::AsteroidBelt: return { 0.45f, 0.45f, 0.45f, 0.65f };
        default:                     return { 0.60f, 0.82f, 1.00f, 1.00f };
    }
}




float SystemMapRenderer::bodyVisualRadius(
    const world::celestial::SystemMapBody& body,
    float distanceScale
) const
{
    if (body.radiusKm <= 0.0)
        return 0.0f;

    const double radiusAu =
        body.radiusKm / AU_KM;

    return
        static_cast<float>(
            radiusAu *
            static_cast<double>(distanceScale)
        );
}











void SystemMapRenderer::render(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::PlanetMapSnapshot& planet,
    const world::celestial::HubMapSnapshot& hub,
    const world::celestial::PlayerNavigationState& nav
)
{
    
        
    if (!m_initialized)
        init();
    const Viewport& vp = viewport;

    glViewport(vp.x, vp.y, vp.width, vp.height);
    glScissor(vp.x, vp.y, vp.width, vp.height);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);

    drawBackground();

    // System map рисует свою 3D-сцену поверх игрового кадра.
    // Поэтому depth buffer надо очистить, иначе карта может
    // наследовать глубину от предыдущего рендера.
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    


    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    
    

    if (m_mode == Mode::Planet)
    {
        renderPlanetMap(
            viewport,
            planet
        );
    }
    else if (m_mode == Mode::Hub)
    {
        renderHubMap(
            viewport,
            hub
        );
    }
    else if (m_mode == Mode::Galaxy)
    {
        renderGalaxy(
            vp,
            galaxy,
            nav
        );
    }
    else if (m_mode == Mode::System)
    {
        renderSystem(
            vp,
            system,
            nav
        );
    }








    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}




glm::vec3 SystemMapRenderer::galaxyStarPosition(
    const world::celestial::GalaxyMapSystem& s
) const
{
    const float scale = 2.2f;

    return {
        static_cast<float>(s.positionLy.x * scale),
        static_cast<float>(s.positionLy.y * scale),
        static_cast<float>(s.positionLy.z * scale)
    };
}






glm::mat4 SystemMapRenderer::galaxyViewMatrix() const
{
    const glm::vec3 dir =
        orbitCameraDirectionFromYawPitch(
            m_galaxyCamera.yaw,
            m_galaxyCamera.pitch
        );

    const glm::vec3 up =
        orbitCameraUpFromYawPitch(
            m_galaxyCamera.yaw,
            m_galaxyCamera.pitch
        );

    const glm::vec3 eye =
        m_galaxyCamera.target +
        dir * m_galaxyCamera.distance;

    return glm::lookAt(
        eye,
        m_galaxyCamera.target,
        up
    );
}







glm::mat4 SystemMapRenderer::galaxyProjectionMatrix(const Viewport& vp) const
{
    const float aspect =
        vp.height > 0
            ? static_cast<float>(vp.width) / static_cast<float>(vp.height)
            : 1.0f;

    return glm::perspective(
        glm::radians(48.0f),
        aspect,
        0.1f,
        2000.0f
    );
}




glm::mat4 SystemMapRenderer::systemViewMatrix() const
{
    const glm::vec3 dir =
        orbitCameraDirectionFromYawPitch(
            m_systemCamera.yaw,
            m_systemCamera.pitch
        );

    const glm::vec3 up =
        orbitCameraUpFromYawPitch(
            m_systemCamera.yaw,
            m_systemCamera.pitch
        );

    // В system map все render-позиции уже будут relative:
    // renderPosition = absolutePosition - m_systemCamera.target.
    //
    // Поэтому камера смотрит в локальный ноль.
    const glm::vec3 eye =
        dir * SYSTEM_MAP_ORTHO_EYE_DISTANCE;

    return glm::lookAt(
        eye,
        glm::vec3(0.0f, 0.0f, 0.0f),
        up
    );
}












glm::mat4 SystemMapRenderer::systemProjectionMatrix(
    const Viewport& vp
) const
{
    const float aspect =
        vp.height > 0
            ? static_cast<float>(vp.width) /
              static_cast<float>(vp.height)
            : 1.0f;

    const float halfHeight =
        std::clamp(
            m_systemCamera.distance,
            SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
            SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
        );

    const float halfWidth =
        halfHeight *
        aspect;

    return glm::ortho(
        -halfWidth,
         halfWidth,
        -halfHeight,
         halfHeight,
        0.001f,
        SYSTEM_MAP_ORTHO_FAR_PLANE
    );
}








glm::vec2 SystemMapRenderer::projectToScreen(
    const glm::vec3& world,
    const glm::mat4& mvp,
    const Viewport& vp,
    bool& visible,
    float& depth
) const
{
    const glm::vec4 clip = mvp * glm::vec4(world, 1.0f);

    visible = false;
    depth = 1.0f;

    if (std::abs(clip.w) < 0.00001f)
        return {0.0f, 0.0f};

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    visible =
        ndc.x >= -1.0f && ndc.x <= 1.0f &&
        ndc.y >= -1.0f && ndc.y <= 1.0f &&
        ndc.z >= -1.0f && ndc.z <= 1.0f;

    depth = ndc.z;

    return {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(vp.width),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(vp.height)
    };
}

int SystemMapRenderer::pickGalaxySystem(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    double mouseX,
    double mouseY
) const
{
    const glm::mat4 mvp =
        galaxyProjectionMatrix(vp) * galaxyViewMatrix();

    int bestId = -1;
    float bestDist = 999999.0f;

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            galaxyStarPosition(s),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const float dx = screen.x - static_cast<float>(mouseX);
        const float dy = screen.y - static_cast<float>(mouseY);
        const float d = std::sqrt(dx * dx + dy * dy);

        const float pickRadius = m_galaxyControls.systemPickRadiusPx;

        if (d < pickRadius && d < bestDist)
        {
            bestDist = d;
            bestId = s.id;
        }
    }

    return bestId;
}













void SystemMapRenderer::handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    if (m_mode != Mode::Galaxy &&
        m_mode != Mode::System &&
        m_mode != Mode::Planet &&
        m_mode != Mode::Hub)
    {
        return;
    }

    GLFWwindow* window =
        glfwGetCurrentContext();

    if (!window)
        return;

    double mx = 0.0;
    double my = 0.0;

    glfwGetCursorPos(
        window,
        &mx,
        &my
    );

    const double localMx =
        mx - static_cast<double>(vp.x);

    const double localMy =
        my - static_cast<double>(vp.y);

    const bool inside =
        mx >= static_cast<double>(vp.x) &&
        my >= static_cast<double>(vp.y) &&
        mx <= static_cast<double>(vp.x + vp.width) &&
        my <= static_cast<double>(vp.y + vp.height);

    const bool leftDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_LEFT
        ) == GLFW_PRESS;

    const bool rightDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_RIGHT
        ) == GLFW_PRESS;

    if (m_mode == Mode::System)
    {
        handleSystemInput(
            vp,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown
        );

        return;
    }

    if (m_mode == Mode::Planet ||
        m_mode == Mode::Hub)
    {
        handleDetailInput(
            vp,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown
        );

        return;
    }

    if (m_mode == Mode::Galaxy)
    {
        handleGalaxyInput(
            vp,
            galaxy,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown
        );

        return;
    }
}









void SystemMapRenderer::handleSystemInput(
    const Viewport& vp,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown
)
{
    // =========================================================
    // SYSTEM MODE INPUT
    // =========================================================
    
        const SystemControlSettings& controls = m_systemControls;
        const double dx = mx - m_systemCamera.lastMouseX;
        const double dy = my - m_systemCamera.lastMouseY;
        bool leftStartedThisFrame = false;
        bool rightStartedThisFrame = false;

        auto projectSystemAbsoluteToScreen =
            [&](const glm::dvec3& absolutePos,
                bool& visible,
                float& depth) -> glm::vec2
            {
                const glm::dvec3 relative =
                    absolutePos -
                    m_systemCamera.target;

                const glm::vec3 renderPos(
                    static_cast<float>(relative.x),
                    static_cast<float>(relative.y),
                    static_cast<float>(relative.z)
                );

                const glm::mat4 mvp =
                    systemProjectionMatrix(vp) *
                    systemViewMatrix();

                return projectToScreen(
                    renderPos,
                    mvp,
                    vp,
                    visible,
                    depth
                );
            };

        auto captureSystemOrbitPivot =
            [&]()
            {
                m_systemOrbitPivotActive =
                    false;

                const int pickedIndex =
                    pickSystemOrbitPivotBody(
                        localMx,
                        localMy,
                        vp
                    );

                if (pickedIndex >= 0 &&
                    pickedIndex < static_cast<int>(m_lastSystemBodyScreenPoints.size()))
                {
                    const std::string& pickedBodyId =
                        m_lastSystemBodyScreenPoints[pickedIndex].bodyId;

                    auto absIt =
                        m_lastSystemBodyAbsolutePosById.find(
                            pickedBodyId
                        );

                    if (absIt != m_lastSystemBodyAbsolutePosById.end())
                    {
                        m_systemOrbitPivotAbsolute =
                            absIt->second;

                        m_systemOrbitPivotActive =
                            true;

                        return;
                    }
                }

                // Пустота — это не новая точка вращения.
                // В пустоте вращаем вокруг текущего центра карты.
                m_systemOrbitPivotAbsolute =
                    m_systemCamera.target;

                m_systemOrbitPivotActive =
                    true;
            };

        if (inside &&
            leftDown &&
            !m_systemCamera.leftWasDown)
        {
            leftStartedThisFrame =
                true;

            m_systemCamera.mouseDownX =
                mx;

            m_systemCamera.mouseDownY =
                my;

            m_systemCamera.rotating =
                true;

            m_systemCamera.lastMouseX =
                mx;

            m_systemCamera.lastMouseY =
                my;

            captureSystemOrbitPivot();
        }

        if (!leftDown &&
            m_systemCamera.leftWasDown)
        {
            if (inside)
            {
                const double move =
                    std::abs(mx - m_systemCamera.mouseDownX) +
                    std::abs(my - m_systemCamera.mouseDownY);

                if (move < controls.clickMoveThresholdPx)
                {
                    const int pickedIndex =
                        pickSystemBody(
                            localMx,
                            localMy
                        );

                    if (pickedIndex >= 0 &&
                        pickedIndex < static_cast<int>(m_lastSystemBodyScreenPoints.size()))
                    {
                        m_selectedBodyId =
                            m_lastSystemBodyScreenPoints[pickedIndex].bodyId;
                    }
                    else
                    {
                        m_selectedBodyId.clear();
                    }
                }
            }

            m_systemCamera.rotating =
                false;

            m_systemOrbitPivotActive =
                false;
        }

        if (!leftDown)
        {
            m_systemCamera.rotating =
                false;

            m_systemOrbitPivotActive =
                false;
        }

        if (inside &&
            rightDown &&
            !m_systemCamera.rightWasDown)
        {
            rightStartedThisFrame =
                true;

            m_systemCamera.panning =
                true;

            m_systemCamera.lastMouseX =
                mx;

            m_systemCamera.lastMouseY =
                my;
        }

        if (!rightDown)
        {
            m_systemCamera.panning =
                false;
        }

        if (m_systemCamera.rotating &&
            leftDown &&
            !leftStartedThisFrame)
        {
            bool beforeVisible =
                false;

            float beforeDepth =
                1.0f;

            const glm::vec2 pivotBefore =
                projectSystemAbsoluteToScreen(
                    m_systemOrbitPivotAbsolute,
                    beforeVisible,
                    beforeDepth
                );

            m_systemCamera.yaw -=
                static_cast<float>(dx) *
                controls.rotateSensitivity;

            m_systemCamera.pitch +=
                static_cast<float>(dy) *
                controls.rotateSensitivity;

            m_systemCamera.yaw =
                wrapAngleRadF(
                    m_systemCamera.yaw
                );

            m_systemCamera.pitch =
                std::clamp(
                    m_systemCamera.pitch,
                    -controls.pitchLimitRad,
                    controls.pitchLimitRad
                );

            if (m_systemOrbitPivotActive)
            {
                bool afterVisible =
                    false;

                float afterDepth =
                    1.0f;

                const glm::vec2 pivotAfter =
                    projectSystemAbsoluteToScreen(
                        m_systemOrbitPivotAbsolute,
                        afterVisible,
                        afterDepth
                    );

                const glm::vec2 screenDelta =
                    pivotBefore -
                    pivotAfter;

                if (std::isfinite(screenDelta.x) &&
                    std::isfinite(screenDelta.y))
                {
                    const glm::mat4 viewAfter =
                        systemViewMatrix();

                    const glm::vec3 rightF =
                        systemMapViewRight(
                            viewAfter
                        );

                    const glm::vec3 upF =
                        systemMapViewUp(
                            viewAfter
                        );

                    const glm::dvec3 right(
                        rightF.x,
                        rightF.y,
                        rightF.z
                    );

                    const glm::dvec3 up(
                        upF.x,
                        upF.y,
                        upF.z
                    );

                    const double worldUnitsPerPixel =
                        systemMapWorldUnitsPerPixel(
                            static_cast<double>(m_systemCamera.distance),
                            vp.height
                        );

                    m_systemCamera.target -=
                        right *
                        static_cast<double>(screenDelta.x) *
                        worldUnitsPerPixel;

                    m_systemCamera.target +=
                        up *
                        static_cast<double>(screenDelta.y) *
                        worldUnitsPerPixel;
                }
            }
        }

        if (m_systemCamera.panning &&
            rightDown &&
            !rightStartedThisFrame)
        {
            const glm::mat4 view =
                systemViewMatrix();

            const glm::vec3 rightF =
                systemMapViewRight(
                    view
                );

            const glm::vec3 upF =
                systemMapViewUp(
                    view
                );

            const glm::dvec3 right(
                rightF.x,
                rightF.y,
                rightF.z
            );

            const glm::dvec3 up(
                upF.x,
                upF.y,
                upF.z
            );

            const double worldUnitsPerPixel =
                systemMapWorldUnitsPerPixel(
                    static_cast<double>(m_systemCamera.distance),
                    vp.height
                );

            m_systemCamera.target -=
                right *
                dx *
                worldUnitsPerPixel;

            m_systemCamera.target +=
                up *
                dy *
                worldUnitsPerPixel;
        }

        if (inside)
        {
            float zoom = 0.0f;

            if (m_pendingScrollY != 0.0)
            {
                zoom += static_cast<float>(m_pendingScrollY);

                m_pendingScrollY = 0.0;
            }

            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                zoom += 1.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                zoom -= 1.0f;
            }

            if (zoom != 0.0f)
            {
                const glm::mat4 viewBefore =
                    systemViewMatrix();

                const glm::dvec3 anchorBefore =
                    systemMapTargetPlanePointFromScreen(
                        vp,
                        viewBefore,
                        m_systemCamera.target,
                        static_cast<double>(m_systemCamera.distance),
                        localMx,
                        localMy
                    );

                const float zoomStep = controls.zoomStep;

                const float factor =
                    std::pow(
                        zoomStep,
                        -zoom
                    );

                m_systemCamera.distance *= factor;

                const float minHalfHeightFor20KmPerPx =
                    static_cast<float>(
                        (controls.minKmPerPixel *
                         static_cast<double>(m_lastSystemScale) /
                         AU_KM) *
                        static_cast<double>(vp.height) *
                        0.5
                    );

                const float dynamicMinHalfHeight =
                    std::max(
                        SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                        minHalfHeightFor20KmPerPx
                    );

                m_systemCamera.distance =
                    std::clamp(
                        m_systemCamera.distance,
                        dynamicMinHalfHeight,
                        SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
                    );

                const glm::mat4 viewAfter =
                    systemViewMatrix();

                const glm::dvec3 anchorAfter =
                    systemMapTargetPlanePointFromScreen(
                        vp,
                        viewAfter,
                        m_systemCamera.target,
                        static_cast<double>(m_systemCamera.distance),
                        localMx,
                        localMy
                    );

                m_systemCamera.target +=
                    anchorBefore -
                    anchorAfter;
            }
        }

        m_systemCamera.leftWasDown = leftDown;
        m_systemCamera.rightWasDown = rightDown;
        m_systemCamera.lastMouseX = mx;
        m_systemCamera.lastMouseY = my;

        
}




void SystemMapRenderer::handleDetailInput(
    const Viewport& vp,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown
)
{

    // =========================================================
    // DETAILS MODE INPUT: PLANET + HUB
    // =========================================================
    
        DetailCamera& camera = activeDetailCamera();
        const DetailControlSettings& controls = activeDetailControls();
        double wheel = 0.0;

        if (inside &&
            m_pendingScrollY != 0.0)
        {
            wheel = m_pendingScrollY;

            m_pendingScrollY = 0.0;
        }

        bool leftStartedThisFrame = false;

        bool rightStartedThisFrame = false;

        if (inside &&
            leftDown &&
            !camera.rotating)
        {
            leftStartedThisFrame = true;

            camera.rotating = true;

            camera.lastMouseX = mx;

            camera.lastMouseY = my;

            if (m_mode == Mode::Hub)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const glm::dvec2 mousePx(
                    localMx,
                    localMy
                );

                const double safeScale =
                    std::max(
                        0.000001,
                        m_lastHubMapScale
                    );

                glm::dvec3 pickedPivotLocalMeters(
                    0.0,
                    0.0,
                    0.0
                );

                const bool pickedObject =
                    pickHubMapOrbitPivot(
                        mousePx,
                        pickedPivotLocalMeters
                    );

                if (pickedObject)
                {
                    m_hubMapOrbitPivotLocalMeters =
                        pickedPivotLocalMeters;
                }
                else
                {
                    m_hubMapOrbitPivotLocalMeters =
                        hubMapUnprojectCursorToLocal(
                            mousePx,
                            safeScale,
                            centerPx
                        );
                }

                m_hubMapOrbitPivotScreenPx =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );
            }
        }

        if (!leftDown)
        {
            camera.rotating = false;
        }

        if (inside &&
            rightDown &&
            !camera.panning)
        {
            rightStartedThisFrame = true;
            camera.panning = true;
            camera.lastMouseX = mx;
            camera.lastMouseY = my;
        }

        if (!rightDown)
        {
            camera.panning = false;
        }

        const double dx = mx - camera.lastMouseX;
        const double dy = my - camera.lastMouseY;

        if (camera.rotating &&
            leftDown &&
            !leftStartedThisFrame)
        {
            if (m_mode == Mode::Hub)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const double safeScale =
                    std::max(
                        0.000001,
                        m_lastHubMapScale
                    );

                const glm::dvec2 pivotScreenBefore =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );

                camera.yaw += dx * controls.rotateSensitivity;
                camera.pitch += dy * controls.rotateSensitivity;

                camera.yaw =
                    wrapAngleRadD(
                        camera.yaw
                    );

                camera.pitch =
                    wrapAngleRadD(
                        camera.pitch
                    );

                const glm::dvec2 pivotScreenAfter =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );

                camera.pan +=
                    pivotScreenBefore -
                    pivotScreenAfter;
            }
            else
            {
                camera.yaw += dx * controls.rotateSensitivity;
                camera.pitch += dy * controls.rotateSensitivity;

                camera.yaw =
                    wrapAngleRadD(
                        camera.yaw
                    );

                camera.pitch =
                    wrapAngleRadD(
                        camera.pitch
                    );
            }
        }

        if (camera.panning &&
            rightDown &&
            !rightStartedThisFrame)
        {
            camera.pan.x += dx;
            camera.pan.y += dy;
        }

        if (std::abs(wheel) > 0.001)
        {
            const double oldZoom = camera.zoom;
            const double zoomStep = controls.zoomStep;
        

            const double newZoom =
                std::clamp(
                    oldZoom *
                    std::pow(zoomStep, wheel),
                    controls.minZoom,
                    controls.maxZoom
                );

            if (std::abs(newZoom - oldZoom) > 0.000001)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const glm::dvec2 mousePx(
                    localMx,
                    localMy
                );

                const double zoomFactor =
                    newZoom /
                    oldZoom;

                camera.pan =
                    mousePx -
                    centerPx -
                    (mousePx - centerPx - camera.pan) *
                    zoomFactor;

                camera.zoom = newZoom;
            }
        }

        camera.lastMouseX = mx;
        camera.lastMouseY = my;

}







void SystemMapRenderer::handleGalaxyInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown
)
{

    // =========================================================
    // GALAXY MODE INPUT
    // =========================================================
        const GalaxyControlSettings& controls = m_galaxyControls;
        const double dx = mx - m_galaxyCamera.lastMouseX;
        const double dy = my - m_galaxyCamera.lastMouseY;

        bool leftStartedThisFrame = false;

        if (inside &&
            leftDown &&
            !m_galaxyCamera.leftWasDown)
        {
            leftStartedThisFrame = true;
            m_galaxyMouseDownX = mx;
            m_galaxyMouseDownY = my;
            bool pivotFound = false;

            const glm::vec3 pivot =
                nearestVisibleStarToScreenPoint(
                    vp,
                    galaxy,
                    localMx,
                    localMy,
                    controls.pivotPickRadiusPx,
                    pivotFound
                );

            if (pivotFound)
            {
                m_galaxyOrbitPivotWorld =
                    pivot;

                m_galaxyOrbitPivotActive =
                    true;
            }
            else
            {
                m_galaxyOrbitPivotWorld =
                    m_galaxyCamera.target;

                m_galaxyOrbitPivotActive =
                    false;
            }

            // Важно:
            // здесь больше не делаем:
            // m_galaxyCamera.target = pivot;
            //
            // Target — центр камеры.
            // Pivot — отдельная точка вращения.
            m_galaxyCamera.rotating =
                true;
        }

        if (!leftDown &&
            m_galaxyCamera.leftWasDown)
        {
            if (inside)
            {
                const double move =
                    std::abs(mx - m_galaxyMouseDownX) +
                    std::abs(my - m_galaxyMouseDownY);

                if (move < controls.clickMoveThresholdPx)
                {
                    const int picked =
                        pickGalaxySystem(
                            vp,
                            galaxy,
                            localMx,
                            localMy
                        );

                    if (picked >= 0)
                    {
                        m_selectedSystemId =
                            picked;

                        m_focusedSystemId =
                            picked;
                    }
                }
            }

            m_galaxyCamera.rotating = false;
            m_galaxyOrbitPivotActive = false;
        }

        if (!leftDown)
        {
            m_galaxyCamera.rotating = false;
            m_galaxyOrbitPivotActive = false;
        }

        if (inside &&
            rightDown &&
            !m_galaxyCamera.rightWasDown)
        {
            m_galaxyCamera.panning = true;
            m_galaxyCamera.lastMouseX = mx;
            m_galaxyCamera.lastMouseY = my;
        }

        if (!rightDown)
        {
            m_galaxyCamera.panning = false;
        }

        if (m_galaxyCamera.rotating &&
            leftDown &&
            !leftStartedThisFrame)
        {
            bool beforeVisible =
                false;

            float beforeDepth =
                1.0f;

            const glm::mat4 mvpBefore =
                galaxyProjectionMatrix(vp) *
                galaxyViewMatrix();

            const glm::vec2 pivotBefore =
                projectToScreen(
                    m_galaxyOrbitPivotWorld,
                    mvpBefore,
                    vp,
                    beforeVisible,
                    beforeDepth
                );

            m_galaxyCamera.yaw -=
                static_cast<float>(dx) * controls.rotateSensitivity;

            m_galaxyCamera.pitch +=
                static_cast<float>(dy) * controls.rotateSensitivity;

            m_galaxyCamera.yaw =
                wrapAngleRadF(
                    m_galaxyCamera.yaw
                );

            m_galaxyCamera.pitch =
                std::clamp(
                    m_galaxyCamera.pitch,
                    -controls.pitchLimitRad,
                    controls.pitchLimitRad
                );

            if (m_galaxyOrbitPivotActive)
            {
                bool afterVisible =
                    false;

                float afterDepth =
                    1.0f;

                const glm::mat4 viewAfter =
                    galaxyViewMatrix();

                const glm::mat4 mvpAfter =
                    galaxyProjectionMatrix(vp) *
                    viewAfter;

                const glm::vec2 pivotAfter =
                    projectToScreen(
                        m_galaxyOrbitPivotWorld,
                        mvpAfter,
                        vp,
                        afterVisible,
                        afterDepth
                    );

                const glm::vec2 screenDelta =
                    pivotBefore -
                    pivotAfter;

                if (std::isfinite(screenDelta.x) &&
                    std::isfinite(screenDelta.y))
                {
                    const glm::vec3 right(
                        viewAfter[0][0],
                        viewAfter[1][0],
                        viewAfter[2][0]
                    );

                    const glm::vec3 up(
                        viewAfter[0][1],
                        viewAfter[1][1],
                        viewAfter[2][1]
                    );

                    const glm::vec3 dir =
                        orbitCameraDirectionFromYawPitch(
                            m_galaxyCamera.yaw,
                            m_galaxyCamera.pitch
                        );

                    const glm::vec3 eye =
                        m_galaxyCamera.target +
                        dir *
                        m_galaxyCamera.distance;

                    const float pivotDepth =
                        std::max(
                            1.0f,
                            glm::dot(
                                m_galaxyOrbitPivotWorld - eye,
                                -dir
                            )
                        );

                    const float fovRad =
                        glm::radians(
                            48.0f
                        );

                    const float worldUnitsPerPixel =
                        2.0f *
                        pivotDepth *
                        std::tan(fovRad * 0.5f) /
                        static_cast<float>(
                            std::max(vp.height, 1)
                        );

                    m_galaxyCamera.target -=
                        right *
                        screenDelta.x *
                        worldUnitsPerPixel;

                    m_galaxyCamera.target +=
                        up *
                        screenDelta.y *
                        worldUnitsPerPixel;
                }
            }
        }

        if (m_galaxyCamera.panning && rightDown)
        {
            const glm::mat4 view =
                galaxyViewMatrix();

            const glm::vec3 right(
                view[0][0],
                view[1][0],
                view[2][0]
            );

            const glm::vec3 up(
                view[0][1],
                view[1][1],
                view[2][1]
            );

            const float panScale =
                m_galaxyCamera.distance *
                controls.panScaleByDistance;

            m_galaxyCamera.target -=
                right *
                static_cast<float>(dx) *
                panScale;

            m_galaxyCamera.target +=
                up *
                static_cast<float>(dy) *
                panScale;
        }

        if (inside)
        {
            float zoom = 0.0f;

            if (m_pendingScrollY != 0.0)
            {
                zoom += static_cast<float>(m_pendingScrollY);
                m_pendingScrollY = 0.0;
            }

            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                zoom += 1.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                zoom -= 1.0f;
            }

            if (zoom != 0.0f)
            {
                const float factor =
                    zoom > 0.0f
                        ? controls.zoomInFactor
                        : controls.zoomOutFactor;

                m_galaxyCamera.distance *=
                    factor;

                m_galaxyCamera.distance =
                    std::clamp(
                        m_galaxyCamera.distance,
                        controls.minDistance,
                        controls.maxDistance
                    );
            }
        }

        m_galaxyCamera.leftWasDown = leftDown;
        m_galaxyCamera.rightWasDown = rightDown;
        m_galaxyCamera.lastMouseX = mx;
        m_galaxyCamera.lastMouseY = my;

        

    
}













   




void SystemMapRenderer::renderGalaxy(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav
)
{
    const auto& systems = galaxy.systems;
    if (m_selectedSystemId < 0)
        m_selectedSystemId = nav.currentSystemId;

    const glm::mat4 proj = galaxyProjectionMatrix(vp);
    const glm::mat4 view = galaxyViewMatrix();
    const glm::mat4 mvp = proj * view;

    beginLines();

    const glm::vec4 gridColor { 0.10f, 0.28f, 0.43f, 0.22f };

    for (int i = -20; i <= 20; ++i)
    {
        const float v = static_cast<float>(i) * 5.0f;

        addLine({ -100.0f, 0.0f, v }, { 100.0f, 0.0f, v }, gridColor);
        addLine({ v, 0.0f, -100.0f }, { v, 0.0f, 100.0f }, gridColor);
    }

    // Пустой будущий слой навигации.
    // Здесь позже будут гравитационные складки, трассы, маяки, опасные зоны.
    drawNavigationLayerPlaceholder();

    m_lastGalaxyScreenPoints.clear();

    for (const auto& s : systems)
    {
        const glm::vec3 p = galaxyStarPosition(s);

        const bool isCurrent = s.id == nav.currentSystemId;
        const bool isSelected = s.id == m_selectedSystemId;

        glm::vec4 c =
            isSelected
                ? glm::vec4(0.55f, 0.95f, 1.00f, 1.0f)
                : isCurrent
                    ? glm::vec4(1.0f, 0.86f, 0.45f, 1.0f)
                    : colorForStarType(s.starType);

        const float size =
            isSelected ? 1.25f :
            isCurrent  ? 0.95f :
                         0.48f;

        addCross(p, size, c);

        if (isCurrent)
            addCircleXZ(p, 1.45f, { 1.0f, 0.82f, 0.35f, 0.9f }, 48);

        if (isSelected)
        {
            addCircleXZ(p, 2.10f, { 0.40f, 0.95f, 1.00f, 0.95f }, 64);
            addCircleXZ(p, 2.80f, { 0.40f, 0.95f, 1.00f, 0.35f }, 64);
        }

        ScreenPoint sp;
        sp.systemId = s.id;
        sp.name = s.name;
        sp.world = p;
        sp.screen = projectToScreen(p, mvp, vp, sp.visible, sp.depth);
        m_lastGalaxyScreenPoints.push_back(sp);
    }

    flushLines(mvp);
    drawGalaxyLabels(
        vp,
        galaxy,
        mvp
    );
}


void SystemMapRenderer::drawNavigationLayerPlaceholder()
{
    // Пока намеренно пусто.
    // Это точка расширения под:
    // - fold lanes;
    // - beacon coverage;
    // - gravity distortion fields;
    // - restricted jump zones.
}






void SystemMapRenderer::drawGalaxyLabels(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const glm::mat4& mvp
)
{
    if (m_galaxyCamera.distance > 130.0f)
        return;

   

    struct Label
    {
        std::string title;
        std::string subtitle;
        glm::vec2 screen;
        glm::vec2 lineEnd;
        glm::vec2 textPos;
        bool selected = false;
    };


    std::vector<Label> labels;
    std::vector<glm::vec4> occupied;

    const float screenFactor =
        std::clamp(static_cast<float>(vp.height) / 768.0f, 0.85f, 1.45f);

    const int titlePx =
        std::clamp(static_cast<int>(15.0f * screenFactor), 13, 20);

    const int selectedTitlePx =
        std::clamp(static_cast<int>(18.0f * screenFactor), 15, 25);

    const int subtitlePx =
        std::clamp(static_cast<int>(12.0f * screenFactor), 10, 16);

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            galaxyStarPosition(s),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const bool selected = s.id == m_selectedSystemId;

        // Пока игрок находится у текущей системы. Для galaxy-map этого достаточно.
        // Если позже игрок будет в межзвездном пространстве — добавим точную playerPositionLy.
        double distanceLy = 0.0;

        for (const auto& cur : galaxy.systems)
        {
            // У тебя текущая система сейчас визуально совпадает с Sol.
            // До передачи nav сюда считаем от системы id=0, если она есть.
            if (cur.id == 0)
            {
                const double dx = s.positionLy.x - cur.positionLy.x;
                const double dy = s.positionLy.y - cur.positionLy.y;
                const double dz = s.positionLy.z - cur.positionLy.z;

                distanceLy = std::sqrt(dx * dx + dy * dy + dz * dz);
                break;
            }
        }

        const std::string subtitle =
            (s.starType.empty() ? "Unknown" : s.starType) +
            std::string("  /  ") +
            fmtDistanceLy(distanceLy);

        const int px = selected ? selectedTitlePx : titlePx;

        const float titleW =
            static_cast<float>(s.name.size()) * static_cast<float>(px) * 0.58f;

        const float subtitleW =
            static_cast<float>(subtitle.size()) * static_cast<float>(subtitlePx) * 0.50f;

        const float w = std::max(titleW, subtitleW);
        const float h =
            static_cast<float>(px + subtitlePx) + 8.0f;

        // Смещение метки от звезды.
        // Чем выше разрешение — тем чуть дальше, но без дикости.


    // pos.y — это baseline текста, поэтому учитываем высоту блока.
    const float gap =
        std::clamp(static_cast<float>(vp.height) * 0.018f, 14.0f, 24.0f);

    glm::vec2 lineEnd {
        screen.x + gap,
        screen.y - gap
    };

    glm::vec2 textPos {
        lineEnd.x + 6.0f,
        lineEnd.y - 4.0f
    };

    // if (pos.x + w > vp.x + vp.width - 12.0f)
    //     pos.x = screen.x - w - gap;

    // if (pos.y - h < vp.y + 12.0f)
    //     pos.y = screen.y + h + gap;

    // debugLabelTraceToFile(s.name, screen, textPos);


    

        const glm::vec4 rect {
            textPos.x,
            textPos.y - static_cast<float>(px),
            textPos.x + w,
            textPos.y + static_cast<float>(subtitlePx) + 10.0f
        };

        bool overlap = false;

        for (const auto& r : occupied)
        {
            if (rect.x < r.z && rect.z > r.x &&
                rect.y < r.w && rect.w > r.y)
            {
                overlap = true;
                break;
            }
        }

        if (overlap && !selected)
            continue;

        occupied.push_back(rect);



        labels.push_back({
                s.name,
                subtitle,
                screen,
                lineEnd,
                textPos,
                selected
            });



        

    }

beginLines();

static int debugFrame = 0;
const bool logThisFrame = debugFrame < 20;

std::ofstream dbg;

if (logThisFrame)
{
    dbg.open("system_map_label_debug.txt", std::ios::app);

    dbg
        << "\nFRAME " << debugFrame
        << " vp=(" << vp.x << "," << vp.y
        << "," << vp.width << "," << vp.height << ")"
        << " cameraDistance=" << m_galaxyCamera.distance
        << "\n";
}

for (const auto& l : labels)
{
    const glm::vec4 lineColor =
        l.selected
            ? glm::vec4(0.98f, 0.72f, 0.34f, 0.55f)
            : glm::vec4(0.46f, 0.78f, 1.00f, 0.32f);

    

    addLine(
        glm::vec3(l.screen.x, l.screen.y, 0.0f),
        glm::vec3(l.lineEnd.x, l.lineEnd.y, 0.0f),
        lineColor
    );
}

if (logThisFrame)
    ++debugFrame;

// Label coordinates are local to the system-map viewport:
// x = 0..vp.width
// y = 0..vp.height
//
// Do NOT switch to full-window viewport here.
// TextRenderer also normalizes text using StateContext viewport size,
// so lines and text must stay in the same map-local coordinate space.
const glm::mat4 labelOrtho =
    glm::ortho(
        0.0f,
        static_cast<float>(vp.width),
        static_cast<float>(vp.height),
        0.0f,
        -1.0f,
        1.0f
    );






flushLines(labelOrtho);







auto& text = TextRenderer::instance();

text.beginFrameForViewport(
    vp.width,
    vp.height
);

for (const auto& l : labels)
{
    const int px = l.selected ? selectedTitlePx : titlePx;

    const glm::vec4 titleColor =
        l.selected
            ? glm::vec4(0.98f, 0.72f, 0.34f, 0.92f)
            : glm::vec4(0.46f, 0.78f, 1.00f, 0.68f);

    const glm::vec4 subtitleColor =
        l.selected
            ? glm::vec4(0.70f, 0.86f, 1.00f, 0.68f)
            : glm::vec4(0.38f, 0.64f, 0.90f, 0.46f);







    text.textDrawPx(
        l.title,
        l.textPos.x,
        l.textPos.y,
        px,
        titleColor
    );
























    text.textDrawPx(
        l.subtitle,
        l.textPos.x,
        l.textPos.y + static_cast<float>(px) + 2.0f,
        subtitlePx,
        subtitleColor
    );
}

text.endFrame();
    
}






void SystemMapRenderer::drawSystemLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById
)
{
    using world::celestial::BodyType;

    auto& text = TextRenderer::instance();

        text.beginFrameForViewport(
            vp.width,
            vp.height
        );

        const double worldUnitsPerPixel =
            systemMapWorldUnitsPerPixel(
                static_cast<double>(m_systemCamera.distance),
                vp.height
            );

    for (const auto& b : system.bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::Moon &&
            b.type != BodyType::AsteroidBelt)
        {
            continue;
        }

        auto posIt = posById.find(b.id);
        if (posIt == posById.end())
            continue;

        auto radiusIt = drawRadiusById.find(b.id);

        const float r =
            radiusIt != drawRadiusById.end()
                ? radiusIt->second
                : 0.1f;

        const bool selected = b.id == m_selectedBodyId;
        

        const float screenRadiusPx =
            worldUnitsPerPixel > 0.0
                ? static_cast<float>(
                    static_cast<double>(r) /
                    worldUnitsPerPixel
                )
                : 0.0f;

       if (!selected)
        {
            // Планеты показываем всегда.
            // В true-scale физический диск может быть меньше пикселя,
            // но подпись нужна как навигационный маяк.

            const double kmPerPixel =
            static_cast<double>(worldUnitsPerPixel) *
            AU_KM /
            std::max(
                static_cast<double>(m_lastSystemScale),
                0.000001
            );

            if (b.type == BodyType::Moon &&
                kmPerPixel > 200.0)
            {
                continue;
            }

            if (b.type == BodyType::AsteroidBelt &&
                screenRadiusPx < 2.0f)
            {
                continue;
            }
        }

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen =
            projectToScreen(posIt->second, mvp, vp, visible, depth);

        if (!visible)
            continue;

        std::string subtitle;

        for (size_t i = 0; i < b.alternativeNames.size(); ++i)
        {
            const auto& alt = b.alternativeNames[i];

            if (alt.name.empty())
                continue;

            if (!subtitle.empty())
                subtitle += ", ";

            subtitle += alt.name;

            if (!alt.actors.empty())
            {
                subtitle += " (";

                for (size_t a = 0; a < alt.actors.size(); ++a)
                {
                    if (a > 0)
                        subtitle += ", ";

                    subtitle += alt.actors[a];
                }

                subtitle += ")";
            }
        }

        const float labelOffsetPx =
            std::max(
                screenRadiusPx + 10.0f,
                14.0f
            );

        const float x = screen.x + labelOffsetPx;
        const float y = screen.y - 6.0f;

        text.textDrawPx(
            b.name,
            x,
            y,
            14,
            glm::vec4(0.62f, 0.84f, 1.0f, 0.88f)
        );

        if (!subtitle.empty() &&
            (selected || screenRadiusPx >= 10.0f))
        {
            text.textDrawPx(
                "(" + subtitle + ")",
                x,
                y + 16.0f,
                10,
                glm::vec4(0.55f, 0.67f, 0.78f, 0.62f)
            );
        }
    }

    text.endFrame();
}




float SystemMapRenderer::smoothingAlpha() const
{
    using Clock = std::chrono::steady_clock;

    static double lastTime = 0.0;

    const auto now = Clock::now().time_since_epoch();
    const double nowSec =
        std::chrono::duration<double>(now).count();

    if (lastTime <= 0.0)
    {
        lastTime = nowSec;
        return 1.0f;
    }

    const double dt = std::clamp(nowSec - lastTime, 0.0, 0.1);
    lastTime = nowSec;

    // Чем больше число, тем быстрее карта догоняет новую позицию.
    constexpr double smoothSpeed = 6.0;

    return static_cast<float>(
        1.0 - std::exp(-smoothSpeed * dt)
    );
}

glm::vec3 SystemMapRenderer::smoothVec3(
    SmoothPoint& point,
    const glm::vec3& target,
    float alpha
)
{
    if (!point.initialized)
    {
        point.visual = target;
        point.initialized = true;
        return target;
    }

    point.visual += (target - point.visual) * alpha;
    return point.visual;
}






void SystemMapRenderer::renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
)
{
    ensureTexturedGlObjects();
    ensureTexturedShader();
    ensureGeneratedCelestialAssets();

    const auto& bodies = system.bodies;

    m_lastSystemBodyScreenPoints.clear();

    if (bodies.empty())
        return;

    double maxAu = 1.0;

    for (const auto& b : bodies)
    {
        const double r =
            std::sqrt(
                b.positionAu.x * b.positionAu.x +
                b.positionAu.y * b.positionAu.y +
                b.positionAu.z * b.positionAu.z
            );

        maxAu = std::max(maxAu, r);

        
    }

    const float systemScale = 70.0f / static_cast<float>(maxAu);
    m_lastSystemScale = systemScale;

    const glm::mat4 proj =
        systemProjectionMatrix(
            vp
        );

    const glm::mat4 view =
        systemViewMatrix();

    const glm::mat4 mvp =
        proj * view;

    const double systemWorldUnitsPerPixel =
        systemMapWorldUnitsPerPixel(
            static_cast<double>(
                m_systemCamera.distance
            ),
            vp.height
        );

    using world::celestial::BodyType;

    // =========================================================
    // Static lookup data.
    // Эти таблицы нужны дальше для выбора, радиусов, орбит,
    // подписей и selection overlay.
    // =========================================================
    std::unordered_map<
        std::string,
        const world::celestial::SystemMapBody*
    > bodyById;

    std::unordered_map<
        std::string,
        float
    > drawRadiusById;

    for (const auto& b : bodies)
    {
        bodyById[b.id] =
            &b;

        drawRadiusById[b.id] =
            bodyVisualRadius(
                b,
                systemScale
            );
    }

// Если выбранная цель исчезла или это звезда — сбрасываем выбор.
if (!m_selectedBodyId.empty())
{
    auto selectedIt =
        bodyById.find(
            m_selectedBodyId
        );

    if (selectedIt == bodyById.end() ||
        selectedIt->second->type == BodyType::Star)
    {
        m_selectedBodyId.clear();
    }
}

// =========================================================
// Floating origin для system map.
//
// absolutePosById хранит точные double-позиции в map units.
// posById хранит render-relative float-позиции около нуля.
// В GPU отдаём только relative position.
// =========================================================
std::unordered_map<
    std::string,
    glm::dvec3
> absolutePosById;

std::unordered_map<
    std::string,
    glm::vec3
> posById;

const glm::dvec3 systemCameraOrigin =
    m_systemCamera.target;

auto auToMapUnits =
    [&](const glm::dvec3& au) -> glm::dvec3
    {
        return glm::dvec3(
            au.x * static_cast<double>(systemScale),
            au.y * static_cast<double>(systemScale),
            au.z * static_cast<double>(systemScale)
        );
    };

auto toRenderPos =
    [&](const glm::dvec3& absoluteMapUnits) -> glm::vec3
    {
        const glm::dvec3 relative =
            absoluteMapUnits -
            systemCameraOrigin;

        return glm::vec3(
            static_cast<float>(relative.x),
            static_cast<float>(relative.y),
            static_cast<float>(relative.z)
        );
    };

for (const auto& b : bodies)
{
    const glm::dvec3 absolutePos =
        auToMapUnits(
            b.positionAu
        );

    absolutePosById[b.id] =
        absolutePos;

    posById[b.id] =
        toRenderPos(
            absolutePos
        );
}


m_lastSystemBodyAbsolutePosById = absolutePosById;















    beginLines();
    beginSolids();
    beginTexturedBodies();


    



    const glm::vec4 gridColor { 0.10f, 0.28f, 0.43f, 0.18f };

    constexpr double gridStep =
        5.0;

    constexpr int gridHalfLines =
        24;

    const double gridHalfSize =
        gridStep *
        static_cast<double>(gridHalfLines);

    const double gridCenterX =
        std::floor(systemCameraOrigin.x / gridStep) *
        gridStep;

    const double gridCenterZ =
        std::floor(systemCameraOrigin.z / gridStep) *
        gridStep;

    for (int i = -gridHalfLines; i <= gridHalfLines; ++i)
    {
        const double xAbs =
            gridCenterX +
            static_cast<double>(i) * gridStep;

        const double zAbs =
            gridCenterZ +
            static_cast<double>(i) * gridStep;

        addLine(
            toRenderPos(glm::dvec3(gridCenterX - gridHalfSize, 0.0, zAbs)),
            toRenderPos(glm::dvec3(gridCenterX + gridHalfSize, 0.0, zAbs)),
            gridColor
        );

        addLine(
            toRenderPos(glm::dvec3(xAbs, 0.0, gridCenterZ - gridHalfSize)),
            toRenderPos(glm::dvec3(xAbs, 0.0, gridCenterZ + gridHalfSize)),
            gridColor
        );
    }

    

    // Орбиты планет, лун и астероидных поясов.
    for (const auto& b : bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::Moon)
        {
            continue;
        }

        
        if (!b.drawOrbit || b.orbitRadiusAu <= 0.0)
            continue;

       


        const glm::dvec3 orbitCenterAbsolute =
            auToMapUnits(
                b.orbitCenterAu
            );

        const glm::vec3 center =
            toRenderPos(
                orbitCenterAbsolute
            );

        float orbitR =
            static_cast<float>(b.orbitRadiusAu) * systemScale;


        

        




        const glm::vec4 orbitColor =
            b.type == BodyType::Moon
                ? glm::vec4(0.72f, 0.78f, 0.86f, 0.24f)
                : b.type == BodyType::AsteroidBelt
                    ? glm::vec4(0.62f, 0.66f, 0.72f, 0.30f)
                    : glm::vec4(0.48f, 0.76f, 1.00f, 0.34f);

        addCircleXZ(
            center,
            orbitR,
            orbitColor,
            b.type == BodyType::Moon ? 64 : 160
        );
    }




 




    // Тела, кольца, пояса.
    for (const auto& b : bodies)
    {
        const glm::vec3 p = posById[b.id];
        const glm::vec4 c = colorForBodyType(b.type);
        const float r = drawRadiusById[b.id];


        
        float bodyDrawRadius = r;

        if (b.type == BodyType::Moon &&
            b.radiusKm > 0.0 &&
            b.radiusKm < 40.0)
        {
            bodyDrawRadius =
                std::max(
                    bodyDrawRadius,
                    static_cast<float>(
                        systemWorldUnitsPerPixel *
                        static_cast<double>(
                            m_systemControls.tinyMoonProxyRadiusPx
                        )
                    )
                );
        }



        const float screenRadiusPx =
            systemWorldUnitsPerPixel > 0.0
                ? static_cast<float>(
                    static_cast<double>(bodyDrawRadius) /
                    systemWorldUnitsPerPixel
                )
                : 0.0f;

        if (b.type == BodyType::Planet ||
            b.type == BodyType::Moon)
        {
            BodyScreenPoint bp;

            bp.bodyId = b.id;
            bp.name = b.name;
            bp.screenRadiusPx = screenRadiusPx;

            bp.screen =
                projectToScreen(
                    p,
                    mvp,
                    vp,
                    bp.visible,
                    bp.depth
                );

            m_lastSystemBodyScreenPoints.push_back(bp);
        }


        
        


        if (b.type == BodyType::AsteroidBelt)
        {
            const glm::dvec3 orbitCenterAbsolute =
                auToMapUnits(
                    b.orbitCenterAu
                );

            const glm::vec3 center =
                toRenderPos(
                    orbitCenterAbsolute
                );

            const float beltR =
                static_cast<float>(b.orbitRadiusAu) *
                systemScale;

            addCircleXZ(center, beltR - 0.12f, {0.65f, 0.68f, 0.72f, 0.12f}, 160);
            addCircleXZ(center, beltR,         {0.65f, 0.68f, 0.72f, 0.24f}, 160);
            addCircleXZ(center, beltR + 0.12f, {0.65f, 0.68f, 0.72f, 0.12f}, 160);

            continue;
        }

        
        

        GLuint bodyAlbedoTexture = 0;

       if (b.type != BodyType::Star)
        {
            bodyAlbedoTexture =
                globalAlbedoTextureForBody(
                    b
                );
        }

        if (bodyAlbedoTexture != 0 &&
            m_texturedShader != 0 &&
            m_texturedVao != 0 &&
            m_texturedVbo != 0)
        {

        const bool largeBody =
            b.type == BodyType::Planet &&
            bodyDrawRadius > 0.16f;

        const int latSeg = largeBody ? 64 : 24;
        const int lonSeg = largeBody ? 128 : 48;

        addTexturedSystemBodySphere(
                b,
                bodyAlbedoTexture,
                p,
                bodyDrawRadius,
                glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
                latSeg,
                lonSeg
            );
        }
        else
        {
            // Fallback для звёзд и тел без generated-текстуры.
            addBillboardBall(
                p,
                bodyDrawRadius,
                c,
                view,
                32
            );
        }

        





        
    }

    if (system.systemId == nav.currentSystemId)
    {
        const glm::dvec3 playerAbsolute(
                nav.systemLocalAu.x * static_cast<double>(systemScale),
                nav.systemLocalAu.y * static_cast<double>(systemScale),
                nav.systemLocalAu.z * static_cast<double>(systemScale)
            );

        const glm::dvec3 playerRelative =
            playerAbsolute -
            systemCameraOrigin;

        glm::vec3 player {
            static_cast<float>(playerRelative.x),
            static_cast<float>(playerRelative.y),
            static_cast<float>(playerRelative.z)
        };

        const float playerCrossSize =
            systemWorldUnitsPerPixel *
            10.0f;

        const float playerCircleRadius =
            systemWorldUnitsPerPixel *
            17.0f;

        addCross(
            player,
            playerCrossSize,
            glm::vec4(1.0f, 0.82f, 0.35f, 1.0f)
        );

        addCircleXZ(
            player,
            playerCircleRadius,
            glm::vec4(1.0f, 0.82f, 0.35f, 0.55f),
            48
        );
    }


    std::unordered_map<uint32_t, glm::vec3> objectVisualPosById;










    for (const auto& obj : system.objects)
    {
        const glm::dvec3 objectAbsolute =
            auToMapUnits(
                obj.positionAu
            );

        const glm::vec3 p =
            toRenderPos(
                objectAbsolute
            );

        objectVisualPosById[obj.id.value] =
            p;
    }

    





    flushLines(mvp);

    // Draw old solid bodies first.
    // Textured bodies are drawn after them.
    flushSolids(mvp);
    flushTexturedBodies(mvp);


    

    // Selection overlay must be drawn AFTER bodies,
    // otherwise the planet texture overpaints the marker.
    if (!m_selectedBodyId.empty())
    {
        auto posIt =
            posById.find(m_selectedBodyId);

        auto radiusIt =
            drawRadiusById.find(m_selectedBodyId);

        if (posIt != posById.end() &&
            radiusIt != drawRadiusById.end())
        {
            beginLines();

            const glm::vec3 selectedPos =
                posIt->second;

            const float selectedRadius =
                radiusIt->second;

            addCircleXZ(
                selectedPos,
                selectedRadius * 1.95f,
                glm::vec4(1.0f, 0.82f, 0.25f, 0.98f),
                96
            );

            addCircleXY(
                selectedPos,
                selectedRadius * 2.10f,
                glm::vec4(1.0f, 0.82f, 0.25f, 0.34f),
                96
            );

            flushLines(mvp);
        }
    }



    drawSystemObjectOverlays(
        system,
        view,
        mvp,
        objectVisualPosById,
        posById,
        drawRadiusById,
        systemWorldUnitsPerPixel,
        systemScale
    );









    drawSystemLabels(
        vp,
        system,
        mvp,
        posById,
        drawRadiusById
    );




    drawSystemObjectLabels(
        vp,
        system,
        mvp,
        view,
        objectVisualPosById,
        posById,
        drawRadiusById
    );
   



    {
        const double worldUnitsPerPixel =
            systemMapWorldUnitsPerPixel(
                static_cast<double>(m_systemCamera.distance),
                vp.height
            );

        const double kmPerPixel =
            static_cast<double>(worldUnitsPerPixel) /
            static_cast<double>(systemScale) *
            AU_KM;

        if (kmPerPixel > 0.0 &&
            std::isfinite(kmPerPixel))
        {
            const double desiredBarPx =
                150.0;

            const double niceKm =
                niceSystemMapScaleNumber(
                    kmPerPixel * desiredBarPx
                );

            const double barPx =
                niceKm / kmPerPixel;

            const float x0 =
                28.0f;

            const float y0 =
                static_cast<float>(vp.height) -
                42.0f;

            const float x1 =
                x0 +
                static_cast<float>(
                    std::clamp(
                        barPx,
                        48.0,
                        260.0
                    )
                );

            const glm::mat4 scaleOrtho =
                glm::ortho(
                    0.0f,
                    static_cast<float>(vp.width),
                    static_cast<float>(vp.height),
                    0.0f,
                    -1.0f,
                    1.0f
                );

            beginLines();

            const glm::vec4 scaleColor(
                0.48f,
                0.78f,
                1.0f,
                0.72f
            );

            addLine(
                glm::vec3(x0, y0, 0.0f),
                glm::vec3(x1, y0, 0.0f),
                scaleColor
            );

            addLine(
                glm::vec3(x0, y0 - 5.0f, 0.0f),
                glm::vec3(x0, y0 + 5.0f, 0.0f),
                scaleColor
            );

            addLine(
                glm::vec3(x1, y0 - 5.0f, 0.0f),
                glm::vec3(x1, y0 + 5.0f, 0.0f),
                scaleColor
            );

            flushLines(
                scaleOrtho
            );

            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                vp.width,
                vp.height
            );

            text.textDrawPx(
                fmtSystemMapScaleDistance(
                    niceKm
                ),
                x0,
                y0 - 24.0f,
                12,
                glm::vec4(
                    0.58f,
                    0.82f,
                    1.0f,
                    0.78f
                )
            );

            std::ostringstream pxLabel;

            pxLabel
                << "1 px = "
                << fmtSystemMapScaleDistance(
                    kmPerPixel
                );

            text.textDrawPx(
                pxLabel.str(),
                x0,
                y0 + 10.0f,
                10,
                glm::vec4(
                    0.42f,
                    0.62f,
                    0.82f,
                    0.58f
                )
            );

            text.endFrame();
        }
    }








}   


void SystemMapRenderer::drawPanelText(
    const Viewport& vp,
    const std::string& title,
    const std::vector<std::string>& lines
)
{
    auto& text = TextRenderer::instance();

    text.beginFrame();

    const float panelX = static_cast<float>(vp.width) - 386.0f;
    float y = 48.0f;

    // Title: same family as loading screen — orange, thin, technical.
    text.textDraw(
        title,
        panelX,
        y,
        0.34f,
        glm::vec3(1.0f, 0.68f, 0.38f)
    );

    y += 42.0f;

    for (const auto& line : lines)
    {
        if (line.empty())
        {
            y += 12.0f;
            continue;
        }

        const bool isHint =
            line.find("F11") != std::string::npos ||
            line.find("Next") != std::string::npos;

        const glm::vec3 color =
            isHint
                ? glm::vec3(0.38f, 0.58f, 0.78f)
                : glm::vec3(0.78f, 0.86f, 0.96f);

        text.textDraw(
            line,
            panelX,
            y,
            0.24f,
            color
        );

        y += 24.0f;
    }

    text.endFrame();
}





void SystemMapRenderer::ensureBackground()
{
    if (m_bgVao && m_bgVbo && m_bgShader)
        return;

    m_bgShader = ShaderLibrary::instance().get("system_map_background");

    if (!m_bgShader)
    {
        
        return;
    }

    const float verts[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_bgVao);
    glGenBuffers(1, &m_bgVbo);

    glBindVertexArray(m_bgVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_bgVbo);

    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(float) * 2,
        reinterpret_cast<void*>(0)
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}


void SystemMapRenderer::drawBackground()
{
    if (!m_bgShader || !m_bgVao)
        return;

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bgShader);
    glBindVertexArray(m_bgVao);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}


void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}


glm::vec3 SystemMapRenderer::nearestVisibleStarToScreenPoint(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    double localMouseX,
    double localMouseY,
    float maxRadiusPx,
    bool& found
) const
{
    found = false;

    const glm::mat4 mvp =
        galaxyProjectionMatrix(vp) * galaxyViewMatrix();

    const glm::vec2 mouse {
        static_cast<float>(localMouseX),
        static_cast<float>(localMouseY)
    };

    const float maxDist2 =
        maxRadiusPx * maxRadiusPx;

    float bestDist2 =
        maxDist2;

    glm::vec3 bestWorld {0.0f};

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec3 world =
            galaxyStarPosition(s);

        const glm::vec2 screen =
            projectToScreen(
                world,
                mvp,
                vp,
                visible,
                depth
            );

        if (!visible)
            continue;

        const glm::vec2 d =
            screen - mouse;

        const float dist2 =
            glm::dot(d, d);

        if (dist2 < bestDist2)
        {
            bestDist2 = dist2;
            bestWorld = world;
            found = true;
        }
    }

    return bestWorld;
}

void SystemMapRenderer::debugLabelTraceToFile(
    const std::string& name,
    const glm::vec2& screen,
    const glm::vec2& pos
) const
{
    if (name.find("Tau") == std::string::npos &&
        name.find("Ceti") == std::string::npos)
    {
        return;
    }

    std::ofstream out(
        "galaxy_label_trace.txt",
        std::ios::app
    );

    out
        << name
        << " screen=(" << screen.x << "," << screen.y << ")"
        << " pos=(" << pos.x << "," << pos.y << ")"
        << " delta=(" << (pos.x - screen.x) << "," << (pos.y - screen.y) << ")"
        << " cameraDistance=" << m_galaxyCamera.distance
        << " target=("
        << m_galaxyCamera.target.x << ","
        << m_galaxyCamera.target.y << ","
        << m_galaxyCamera.target.z << ")"
        << "\n";
}





glm::vec3 SystemMapRenderer::systemObjectVisualPosition(
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::SystemMapObject& obj,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById,
    float systemScale
) const
{
    (void)system;
    (void)posById;
    (void)drawRadiusById;

    return glm::vec3 {
        static_cast<float>(obj.positionAu.x) * systemScale,
        static_cast<float>(obj.positionAu.y) * systemScale,
        static_cast<float>(obj.positionAu.z) * systemScale
    };
}








float SystemMapRenderer::systemObjectOcclusionAlpha(
    const world::celestial::SystemMapObject& obj,
    const glm::vec3& objectVisualPos,
    const glm::mat4& view,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById
) const
{
    constexpr float kFrontAlpha =
        0.96f;

    constexpr float kBehindAlpha =
        0.28f;

    if (obj.parentBodyId.empty())
        return kFrontAlpha;

    auto bodyPosIt =
        bodyVisualPosById.find(
            obj.parentBodyId
        );

    auto bodyRadiusIt =
        drawRadiusById.find(
            obj.parentBodyId
        );

    if (bodyPosIt == bodyVisualPosById.end() ||
        bodyRadiusIt == drawRadiusById.end())
    {
        return kFrontAlpha;
    }

    const float bodyRadius =
        bodyRadiusIt->second;

    if (bodyRadius <= 0.0f)
        return kFrontAlpha;

    const glm::vec4 objectView =
        view *
        glm::vec4(
            objectVisualPos,
            1.0f
        );

    const glm::vec4 bodyView =
        view *
        glm::vec4(
            bodyPosIt->second,
            1.0f
        );

    const float dx =
        objectView.x -
        bodyView.x;

    const float dy =
        objectView.y -
        bodyView.y;

    const float lateral2 =
        dx * dx +
        dy * dy;

    const float radius2 =
        bodyRadius *
        bodyRadius;

    // Объект не проецируется на диск планеты.
    // Значит планета его визуально не перекрывает.
    if (lateral2 >= radius2)
        return kFrontAlpha;

    // В view-space камера смотрит вдоль -Z.
    // Передняя поверхность сферы имеет z больше, чем центр.
    const float frontSurfaceZ =
        bodyView.z +
        std::sqrt(
            std::max(
                0.0f,
                radius2 - lateral2
            )
        );

    // Если объект дальше передней поверхности,
    // значит он находится за диском планеты.
    if (objectView.z < frontSurfaceZ)
        return kBehindAlpha;

    return kFrontAlpha;
}







void SystemMapRenderer::drawSystemObjectOverlays(
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& view,
    const glm::mat4& mvp,
    const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById,
    double worldUnitsPerPixel,
    float systemScale
)
{
    beginLines();

    for (const auto& obj : system.objects)
    {
        auto objectPosIt =
            objectVisualPosById.find(
                obj.id.value
            );

        if (objectPosIt == objectVisualPosById.end())
            continue;

        const glm::vec3 objectPos =
            objectPosIt->second;

        const float alpha =
            systemObjectOcclusionAlpha(
                obj,
                objectPos,
                view,
                bodyVisualPosById,
                drawRadiusById
            );



    if (obj.hasOrbit &&
        obj.orbitRadiusAu > 0.0 &&
        !obj.parentBodyId.empty())
    {
        auto parentPosIt =
            bodyVisualPosById.find(
                obj.parentBodyId
            );

        if (parentPosIt != bodyVisualPosById.end())
        {
            const glm::vec3 orbitCenter =
                parentPosIt->second;

            const float orbitRadius =
                static_cast<float>(
                    obj.orbitRadiusAu *
                    static_cast<double>(systemScale)
                );

            addOrbitCircle3D(
                orbitCenter,
                orbitRadius,
                obj.orbitInclinationDeg,
                obj.orbitLongitudeOfAscendingNodeDeg,
                obj.orbitArgumentOfPeriapsisDeg,
                glm::vec4(
                    1.0f,
                    0.78f,
                    0.30f,
                    alpha * 0.34f
                ),
                160
            );
        }
    }







        const float markerSize =
            static_cast<float>(
                std::max(
                    worldUnitsPerPixel * 7.0,
                    worldUnitsPerPixel
                )
            );

        addMapObjectCube(
            objectPos,
            markerSize,
            glm::vec4(
                1.0f,
                0.78f,
                0.30f,
                alpha
            )
        );
    }

    flushLines(
        mvp
    );
}








void SystemMapRenderer::addMapObjectCube(
    const glm::vec3& center,
    float size,
    const glm::vec4& color
)
{
    const glm::vec3 p000 = center + glm::vec3(-size, -size, -size);
    const glm::vec3 p001 = center + glm::vec3(-size, -size,  size);
    const glm::vec3 p010 = center + glm::vec3(-size,  size, -size);
    const glm::vec3 p011 = center + glm::vec3(-size,  size,  size);

    const glm::vec3 p100 = center + glm::vec3( size, -size, -size);
    const glm::vec3 p101 = center + glm::vec3( size, -size,  size);
    const glm::vec3 p110 = center + glm::vec3( size,  size, -size);
    const glm::vec3 p111 = center + glm::vec3( size,  size,  size);

    addLine(p000, p001, color);
    addLine(p001, p011, color);
    addLine(p011, p010, color);
    addLine(p010, p000, color);

    addLine(p100, p101, color);
    addLine(p101, p111, color);
    addLine(p111, p110, color);
    addLine(p110, p100, color);

    addLine(p000, p100, color);
    addLine(p001, p101, color);
    addLine(p010, p110, color);
    addLine(p011, p111, color);
}





void SystemMapRenderer::drawSystemObjectLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const glm::mat4& view,
    const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById
)
{
    auto& text =
        TextRenderer::instance();

    text.beginFrameForViewport(
        vp.width,
        vp.height
    );

    for (const auto& obj : system.objects)
    {
        auto posIt =
            objectVisualPosById.find(
                obj.id.value
            );

        if (posIt == objectVisualPosById.end())
            continue;

        const glm::vec3 p =
            posIt->second;

        bool visible =
            false;

        float depth =
            1.0f;

        const glm::vec2 screen =
            projectToScreen(
                p,
                mvp,
                vp,
                visible,
                depth
            );

        if (!visible)
            continue;

        const float alpha =
            systemObjectOcclusionAlpha(
                obj,
                p,
                view,
                bodyVisualPosById,
                drawRadiusById
            );

        text.textDrawPx(
            obj.name,
            screen.x + 8.0f,
            screen.y - 7.0f,
            13,
            glm::vec4(
                1.0f,
                0.86f,
                0.42f,
                alpha
            )
        );

        if (!obj.owner.empty())
        {
            text.textDrawPx(
                "(" + obj.owner + ")",
                screen.x + 8.0f,
                screen.y + 9.0f,
                10,
                glm::vec4(
                    0.75f,
                    0.72f,
                    0.58f,
                    alpha * 0.70f
                )
            );
        }
    }

    text.endFrame();
}








glm::dvec3 SystemMapRenderer::planetMapCameraSpaceRelative(
    const glm::dvec3& relativeMeters
) const
{
    const double cy =
        std::cos(activeDetailCamera().yaw);

    const double sy =
        std::sin(activeDetailCamera().yaw);

    const double cp =
        std::cos(activeDetailCamera().pitch);

    const double sp =
        std::sin(activeDetailCamera().pitch);

    // yaw around Y
    glm::dvec3 a;
    a.x = relativeMeters.x * cy - relativeMeters.z * sy;
    a.y = relativeMeters.y;
    a.z = relativeMeters.x * sy + relativeMeters.z * cy;

    // pitch around X
    glm::dvec3 b;
    b.x = a.x;
    b.y = a.y * cp - a.z * sp;
    b.z = a.y * sp + a.z * cp;

    return b;
}












glm::dvec2 SystemMapRenderer::planetMapProject(
    const glm::dvec3& worldMeters,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
) const
{
    const glm::dvec3 relative =
        worldMeters -
        planet.planetCenterMeters;

    const glm::dvec3 cameraSpace =
        planetMapCameraSpaceRelative(
            relative
        );

    const double finalScale =
        scale *
        activeDetailCamera().zoom;

    return glm::dvec2(
        centerPx.x + activeDetailCamera().pan.x + cameraSpace.x * finalScale,
        centerPx.y + activeDetailCamera().pan.y - cameraSpace.y * finalScale
    );
}





void SystemMapRenderer::drawPlanetMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCross(
    const glm::dvec2& p,
    float size
)
{
    glBegin(GL_LINES);
    glVertex2d(p.x - size, p.y);
    glVertex2d(p.x + size, p.y);
    glVertex2d(p.x, p.y - size);
    glVertex2d(p.x, p.y + size);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    if (segments < 8)
        segments = 8;

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        glVertex2d(
            center.x + std::cos(a) * radiusPx,
            center.y + std::sin(a) * radiusPx
        );
    }

    glEnd();
}


void SystemMapRenderer::drawPlanetMapAxes(
    const glm::dvec3& originMeters,
    const world::celestial::PlanetMapAxisSet& axes,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double axisLenMeters
)
{
    const glm::dvec2 o =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 x =
        planetMapProject(
            originMeters + axes.x * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 y =
        planetMapProject(
            originMeters + axes.y * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 z =
        planetMapProject(
            originMeters + axes.z * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.25f, 0.25f, 0.9f);
    drawPlanetMapLine(o, x);

    glColor4f(0.25f, 1.0f, 0.25f, 0.9f);
    drawPlanetMapLine(o, y);

    glColor4f(0.25f, 0.55f, 1.0f, 0.9f);
    drawPlanetMapLine(o, z);
}


void SystemMapRenderer::drawPlanetMapVelocityArrow(
    const glm::dvec3& originMeters,
    const glm::dvec3& velocityMps,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double lenMeters
)
{
    const double speed =
        glm::length(velocityMps);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocityMps / speed;

    const glm::dvec2 a =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 b =
        planetMapProject(
            originMeters + dir * lenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.92f, 0.25f, 0.95f);
    drawPlanetMapLine(a, b);

    drawPlanetMapCross(
        b,
        4.0f
    );
}


void SystemMapRenderer::renderPlanetMap(
    const Viewport& viewport,
    const world::celestial::PlanetMapSnapshot& planet
)
{
    ensureGeneratedCelestialAssets();


    glViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glScissor(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(0.02f, 0.025f, 0.035f, 1.0f);

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!planet.valid)
        return;

    const glm::dvec2 centerPx(
        static_cast<double>(viewport.width) * 0.5,
        static_cast<double>(viewport.height) * 0.5
    );

    double maxRadiusMeters =
        planet.planetRadiusMeters;

    for (const auto& o : planet.hubOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    for (const auto& o : planet.playerOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    const double mapHalfPx =
        std::min(
            viewport.width,
            viewport.height
        ) * 0.42;

    const double scale =
        mapHalfPx / std::max(1.0, maxRadiusMeters);

    



    const bool shapeModelDrawn =
        drawPlanetShapeModelDetail(
            planet,
            scale,
            centerPx
        );

    if (!shapeModelDrawn)
    {
        // Always draw fallback first.
        // If there is no generated texture for this system/body,
        // planet detail still remains visible as disk + grid.
        drawPlanetFilledDisk(
            planet,
            scale,
            centerPx
        );

        // Full detail texture, not preview_512.
        // Uses lod/global/albedo_2048.tga when available.
        drawPlanetTexturedGlobe(
            planet,
            scale,
            centerPx
        );

        // Grid/orientation overlay.
        drawPlanetSphereGrid(
            planet,
            scale,
            centerPx
        );
    }    










    // Орбиты хабов.
    glColor4f(0.45f, 0.78f, 1.0f, 0.75f);

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    // Орбита игрока.
    glColor4f(1.0f, 0.75f, 0.25f, 0.9f);

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    const double axisLenMeters =
        std::max(
            50000.0,
            maxRadiusMeters * 0.035
        );

    const double velocityArrowLenMeters =
        std::max(
            70000.0,
            maxRadiusMeters * 0.05
        );

    // Хабы.
    for (const auto& hub : planet.hubs)
    {
        if (!hub.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                hub.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.3f, 0.9f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 7.0f);

        drawPlanetMapAxes(
            hub.positionMeters,
            hub.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            hub.positionMeters,
            hub.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    // Станции.
    for (const auto& station : planet.stations)
    {
        if (!station.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                station.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.8f, 0.95f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 6.0f);

        drawPlanetMapAxes(
            station.positionMeters,
            station.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            station.positionMeters,
            station.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters * 0.8
        );
    }

    // Корабли.
    for (const auto& ship : planet.ships)
    {
        if (!ship.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                ship.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(1.0f, 0.85f, 0.25f, 1.0f);
        drawPlanetMapCross(p, 8.0f);

        drawPlanetMapAxes(
            ship.positionMeters,
            ship.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        drawPlanetMapVelocityArrow(
            ship.positionMeters,
            ship.velocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    glEnable(GL_DEPTH_TEST);
}



const std::string& SystemMapRenderer::selectedBodyId() const
{
    return m_selectedBodyId;
}



int SystemMapRenderer::pickSystemBody(
    double mouseX,
    double mouseY
) const
{
    int bestIndex =
        -1;

    float bestScore =
        std::numeric_limits<float>::max();

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    for (int i = 0;
         i < static_cast<int>(m_lastSystemBodyScreenPoints.size());
         ++i)
    {
        const BodyScreenPoint& p =
            m_lastSystemBodyScreenPoints[i];

        if (!std::isfinite(p.screen.x) ||
            !std::isfinite(p.screen.y) ||
            !std::isfinite(p.screenRadiusPx))
        {
            continue;
        }

        // Центр тела может быть за экраном, когда тело сильно увеличено.
        // Это нормально: видимый диск всё ещё может занимать viewport.
        const bool depthOk =
            p.depth >= -1.0f &&
            p.depth <= 1.0f;

        if (!p.visible &&
            !depthOk)
        {
            continue;
        }

        const glm::vec2 delta =
            p.screen -
            mouse;

        const float centerDistance =
            glm::length(
                delta
            );

        // ВАЖНО:
        // Для проверки попадания в диск используем реальный экранный радиус.
        // Нельзя ограничивать его 8000 px, иначе при большом zoom тело
        // перестаёт выбираться за пределами зоны вокруг центра.
        const float realBodyRadiusPx =
            std::max(
                0.0f,
                p.screenRadiusPx
            );

        // А вот для размера halo радиус можно ограничить,
        // чтобы огромная планета не создавала бесконечную зону липкости.
        const float haloBodyRadiusPx =
            std::clamp(
                realBodyRadiusPx,
                0.0f,
                m_systemControls.pickMaxBodyRadiusPx
            );

        const float distanceToRealDisk =
            std::max(
                0.0f,
                centerDistance -
                realBodyRadiusPx
            );

        const float pickHaloPx =
            std::clamp(
                haloBodyRadiusPx *
                    m_systemControls.pickHaloRadiusFactor +
                    m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloMaxPx
            );

        if (distanceToRealDisk > pickHaloPx)
        {
            continue;
        }

        const bool mouseInsideRealDisk =
            centerDistance <= realBodyRadiusPx;

        float score =
            0.0f;

        if (mouseInsideRealDisk)
        {
            // Если мышь реально внутри диска тела, это сильный hit.
            // При нескольких вложенных дисках выигрывает тело,
            // чей центр ближе к курсору. Так Луна может перебить Землю,
            // если курсор возле Луны.
            score =
                centerDistance *
                0.001f;
        }
        else
        {
            // Halo-hit слабее, чем реальное попадание в диск.
            score =
                1000000.0f +
                distanceToRealDisk *
                    m_systemControls.pickScoreDiskWeight +
                centerDistance;
        }

        if (score < bestScore)
        {
            bestScore =
                score;

            bestIndex =
                i;
        }
    }

    return bestIndex;
}





int SystemMapRenderer::pickSystemOrbitPivotBody(
    double mouseX,
    double mouseY,
    const Viewport& vp
) const
{
    int strictBestIndex =
        -1;

    float strictBestScore =
        std::numeric_limits<float>::max();

    int sparseVisibleCount =
        0;

    int sparseBestIndex =
        -1;

    float sparseBestScore =
        std::numeric_limits<float>::max();

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    const float padding =
        m_systemControls.sparsePivotViewportPaddingPx;

    const float viewMinX =
        -padding;

    const float viewMinY =
        -padding;

    const float viewMaxX =
        static_cast<float>(vp.width) +
        padding;

    const float viewMaxY =
        static_cast<float>(vp.height) +
        padding;

    for (int i = 0;
         i < static_cast<int>(m_lastSystemBodyScreenPoints.size());
         ++i)
    {
        const BodyScreenPoint& p =
            m_lastSystemBodyScreenPoints[i];

        if (!std::isfinite(p.screen.x) ||
            !std::isfinite(p.screen.y) ||
            !std::isfinite(p.screenRadiusPx))
        {
            continue;
        }

        const bool depthOk =
            p.depth >= -1.0f &&
            p.depth <= 1.0f;

        if (!p.visible &&
            !depthOk)
        {
            continue;
        }

        const float realBodyRadiusPx =
            std::max(
                0.0f,
                p.screenRadiusPx
            );

        const bool diskTouchesViewport =
            p.screen.x + realBodyRadiusPx >= viewMinX &&
            p.screen.x - realBodyRadiusPx <= viewMaxX &&
            p.screen.y + realBodyRadiusPx >= viewMinY &&
            p.screen.y - realBodyRadiusPx <= viewMaxY;

        if (!diskTouchesViewport)
        {
            continue;
        }

        ++sparseVisibleCount;

        const glm::vec2 delta =
            p.screen -
            mouse;

        const float centerDistance =
            glm::length(
                delta
            );

        const float distanceToRealDisk =
            std::max(
                0.0f,
                centerDistance -
                realBodyRadiusPx
            );

        // Sparse score:
        // если тело на экране одно/два, хотим выбирать ближайшее
        // по видимому диску, а не требовать попадание в маленькую область.
        const float sparseScore =
            distanceToRealDisk *
            100000.0f +
            centerDistance;

        if (sparseScore < sparseBestScore)
        {
            sparseBestScore =
                sparseScore;

            sparseBestIndex =
                i;
        }

        // Strict score:
        // это нормальный hit-test по диску/halo.
        const float haloBodyRadiusPx =
            std::clamp(
                realBodyRadiusPx,
                0.0f,
                m_systemControls.pickMaxBodyRadiusPx
            );

        const float pickHaloPx =
            std::clamp(
                haloBodyRadiusPx *
                    m_systemControls.pickHaloRadiusFactor +
                    m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloMaxPx
            );

        if (distanceToRealDisk > pickHaloPx)
        {
            continue;
        }

        const bool mouseInsideRealDisk =
            centerDistance <= realBodyRadiusPx;

        float strictScore =
            0.0f;

        if (mouseInsideRealDisk)
        {
            strictScore =
                centerDistance *
                0.001f;
        }
        else
        {
            strictScore =
                1000000.0f +
                distanceToRealDisk *
                    m_systemControls.pickScoreDiskWeight +
                centerDistance;
        }

        if (strictScore < strictBestScore)
        {
            strictBestScore =
                strictScore;

            strictBestIndex =
                i;
        }
    }

    if (strictBestIndex >= 0)
    {
        return strictBestIndex;
    }

    if (sparseVisibleCount > 0 &&
        sparseVisibleCount <= m_systemControls.sparsePivotMaxVisibleBodies)
    {
        return sparseBestIndex;
    }

    return -1;
}









void SystemMapRenderer::drawPlanetSphereGrid(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters;

    if (r <= 1.0)
        return;

    // Чем больше segments — тем чище линия горизонта.
    constexpr int segments = 96;

    // Шаг сетки. Если будет слишком густо — поставь 30.
    constexpr int gridStepDeg = 30;

    auto cameraSpaceForWorldPoint =
        [&](const glm::dvec3& worldPoint) -> glm::dvec3
        {
            const glm::dvec3 relative =
                worldPoint -
                planet.planetCenterMeters;

            return planetMapCameraSpaceRelative(
                relative
            );
        };

    auto emitVisibleSurfaceSegment =
        [&](const glm::dvec3& aWorld, const glm::dvec3& bWorld)
        {
            const glm::dvec3 aCam =
                cameraSpaceForWorldPoint(
                    aWorld
                );

            const glm::dvec3 bCam =
                cameraSpaceForWorldPoint(
                    bWorld
                );

            // В planet-map convention:
            // +Z — сторона, обращённая к камере.
            //
            // Если оба конца за планетой — сегмент не рисуем.
            if (aCam.z < 0.0 && bCam.z < 0.0)
                return;

            glm::dvec3 a =
                aWorld;

            glm::dvec3 b =
                bWorld;

            // Если сегмент пересекает горизонт, обрезаем его по плоскости z = 0.
            // Так сетка не вылезает с обратной стороны даже на краю диска.
            if ((aCam.z < 0.0 && bCam.z >= 0.0) ||
                (aCam.z >= 0.0 && bCam.z < 0.0))
            {
                const double denom =
                    aCam.z - bCam.z;

                if (std::abs(denom) < 1e-12)
                    return;

                const double t =
                    std::clamp(
                        aCam.z / denom,
                        0.0,
                        1.0
                    );

                const glm::dvec3 hit =
                    aWorld +
                    (bWorld - aWorld) * t;

                if (aCam.z < 0.0)
                    a = hit;
                else
                    b = hit;
            }

            const glm::dvec2 sa =
                planetMapProject(
                    a,
                    planet,
                    scale,
                    centerPx
                );

            const glm::dvec2 sb =
                planetMapProject(
                    b,
                    planet,
                    scale,
                    centerPx
                );

            glVertex2d(sa.x, sa.y);
            glVertex2d(sb.x, sb.y);
        };

    glColor4f(
        0.18f,
        0.42f,
        0.85f,
        0.72f
    );

    glBegin(GL_LINES);

    // =========================================================
    // Параллели: полные кольца вокруг планеты.
    // Задняя часть каждого кольца будет отрезана камерой.
    // =========================================================
    for (int latDeg = -75; latDeg <= 75; latDeg += gridStepDeg)
    {
        const double lat =
            glm::radians(
                static_cast<double>(latDeg)
            );

        for (int i = 0; i < segments; ++i)
        {
            const double lon0 =
                -glm::pi<double>() +
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double lon1 =
                -glm::pi<double>() +
                glm::two_pi<double>() *
                static_cast<double>(i + 1) /
                static_cast<double>(segments);

            const glm::dvec3 p0 =
                planetSurfacePointMeters(
                    planet,
                    lat,
                    lon0,
                    1.004
                );

            const glm::dvec3 p1 =
                planetSurfacePointMeters(
                    planet,
                    lat,
                    lon1,
                    1.004
                );

            emitVisibleSurfaceSegment(
                p0,
                p1
            );
        }
    }

    // =========================================================
    // Меридианы: теперь 0..360, а не 0..180.
    // Это возвращает полноценную сетку.
    // =========================================================
    for (int lonDeg = 0; lonDeg < 360; lonDeg += gridStepDeg)
    {
        const double lon =
            glm::radians(
                static_cast<double>(lonDeg)
            );

        for (int i = 0; i < segments; ++i)
        {
            const double lat0 =
                -glm::half_pi<double>() +
                glm::pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double lat1 =
                -glm::half_pi<double>() +
                glm::pi<double>() *
                static_cast<double>(i + 1) /
                static_cast<double>(segments);

            const glm::dvec3 p0 =
                planetSurfacePointMeters(
                    planet,
                    lat0,
                    lon,
                    1.004
                );

            const glm::dvec3 p1 =
                planetSurfacePointMeters(
                    planet,
                    lat1,
                    lon,
                    1.004
                );

            emitVisibleSurfaceSegment(
                p0,
                p1
            );
        }
    }

    glEnd();

    // =========================================================
    // Полюса. Показываем только тот, который на видимой стороне.
    // =========================================================
    const glm::dvec3 northAxis =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 northWorld =
        planet.planetCenterMeters +
        northAxis * r * 1.018;

    const glm::dvec3 southWorld =
        planet.planetCenterMeters -
        northAxis * r * 1.018;

    glColor4f(
        0.55f,
        0.8f,
        1.0f,
        0.95f
    );

    if (cameraSpaceForWorldPoint(northWorld).z >= 0.0)
    {
        const glm::dvec2 north =
            planetMapProject(
                northWorld,
                planet,
                scale,
                centerPx
            );

        drawPlanetMapCross(
            north,
            5.0f
        );
    }

    if (cameraSpaceForWorldPoint(southWorld).z >= 0.0)
    {
        const glm::dvec2 south =
            planetMapProject(
                southWorld,
                planet,
                scale,
                centerPx
            );

        drawPlanetMapCross(
            south,
            5.0f
        );
    }
}















void SystemMapRenderer::drawPlanetFilledDisk(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const glm::dvec2 c {
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    };

    glColor4f(0.035f, 0.09f, 0.18f, 0.92f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2d(c.x, c.y);

    for (int i = 0; i <= 192; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            192.0;

        glVertex2d(
            c.x + std::cos(a) * r,
            c.y + std::sin(a) * r
        );
    }

    glEnd();
}









void SystemMapRenderer::drawPlanetTexturedGlobe(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (planet.planetRadiusMeters <= 1.0)
        return;

    const GLuint texture =
        globalAlbedoTextureForPlanetSnapshot(
            planet
        );

    if (texture == 0)
        return;

    constexpr int latSegments = 48;
    constexpr int lonSegments = 96;

    GLboolean textureWasEnabled =
        glIsEnabled(GL_TEXTURE_2D);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glColor4f(
        1.0f,
        1.0f,
        1.0f,
        1.0f
    );

    glBegin(GL_TRIANGLES);

    for (int iy = 0; iy < latSegments; ++iy)
    {
        const double v0 =
            static_cast<double>(iy) /
            static_cast<double>(latSegments);

        const double v1 =
            static_cast<double>(iy + 1) /
            static_cast<double>(latSegments);

        const double lat0 =
            -glm::half_pi<double>() +
            v0 * glm::pi<double>();

        const double lat1 =
            -glm::half_pi<double>() +
            v1 * glm::pi<double>();

        for (int ix = 0; ix < lonSegments; ++ix)
        {
            const double u0 =
                static_cast<double>(ix) /
                static_cast<double>(lonSegments);

            const double u1 =
                static_cast<double>(ix + 1) /
                static_cast<double>(lonSegments);

            const double lon0 =
                -glm::pi<double>() +
                u0 * glm::two_pi<double>();

            const double lon1 =
                -glm::pi<double>() +
                u1 * glm::two_pi<double>();

            const double latMid =
                0.5 * (lat0 + lat1);

            const double lonMid =
                0.5 * (lon0 + lon1);

            const glm::dvec3 midWorld =
                planetSurfacePointMeters(
                    planet,
                    latMid,
                    lonMid
                );

            const glm::dvec3 midRelative =
                midWorld -
                planet.planetCenterMeters;

            const glm::dvec3 midCamera =
                planetMapCameraSpaceRelative(
                    midRelative
                );

            // Draw only the visible hemisphere.
            // In the current planet-map camera convention +Z is front.
            if (midCamera.z < 0.0)
                continue;

            const glm::dvec3 p00 =
                planetSurfacePointMeters(
                    planet,
                    lat0,
                    lon0
                );

            const glm::dvec3 p10 =
                planetSurfacePointMeters(
                    planet,
                    lat0,
                    lon1
                );

            const glm::dvec3 p11 =
                planetSurfacePointMeters(
                    planet,
                    lat1,
                    lon1
                );

            const glm::dvec3 p01 =
                planetSurfacePointMeters(
                    planet,
                    lat1,
                    lon0
                );

            const glm::dvec2 s00 =
                planetMapProject(
                    p00,
                    planet,
                    scale,
                    centerPx
                );

            const glm::dvec2 s10 =
                planetMapProject(
                    p10,
                    planet,
                    scale,
                    centerPx
                );

            const glm::dvec2 s11 =
                planetMapProject(
                    p11,
                    planet,
                    scale,
                    centerPx
                );

            const glm::dvec2 s01 =
                planetMapProject(
                    p01,
                    planet,
                    scale,
                    centerPx
                );

            // Triangle 1
            glTexCoord2d(u0, v0);
            glVertex2d(s00.x, s00.y);

            glTexCoord2d(u1, v0);
            glVertex2d(s10.x, s10.y);

            glTexCoord2d(u1, v1);
            glVertex2d(s11.x, s11.y);

            // Triangle 2
            glTexCoord2d(u0, v0);
            glVertex2d(s00.x, s00.y);

            glTexCoord2d(u1, v1);
            glVertex2d(s11.x, s11.y);

            glTexCoord2d(u0, v1);
            glVertex2d(s01.x, s01.y);
        }
    }

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}












void SystemMapRenderer::drawTexturedDisk2D(
    GLuint texture,
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& color,
    int segments
)
{
    if (texture == 0 ||
        radiusPx <= 1.0)
    {
        return;
    }

    segments =
        std::max(segments, 32);

    GLboolean textureWasEnabled =
        glIsEnabled(GL_TEXTURE_2D);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    glBegin(GL_TRIANGLE_FAN);

    glTexCoord2d(0.5, 0.5);
    glVertex2d(centerPx.x, centerPx.y);

    for (int i = 0; i <= segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const double ca =
            std::cos(a);

        const double sa =
            std::sin(a);

        const double u =
            0.5 + ca * 0.5;

        const double v =
            0.5 + sa * 0.5;

        glTexCoord2d(u, v);

        glVertex2d(
            centerPx.x + ca * radiusPx,
            centerPx.y + sa * radiusPx
        );
    }

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}









void SystemMapRenderer::drawPlanetTexturedDisk(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const GLuint texture =
        mapPreviewTextureForPlanetSnapshot(planet);

    if (texture == 0)
        return;

    const double radiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const glm::dvec2 screenCenter {
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    };

    drawTexturedDisk2D(
        texture,
        screenCenter,
        radiusPx,
        glm::vec4(1.0f, 1.0f, 1.0f, 0.96f),
        192
    );
}













void SystemMapRenderer::drawPlanetMapOrbit3D(
    const world::celestial::PlanetMapOrbit& orbit,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (!orbit.valid || orbit.radiusMeters <= 1.0)
        return;

    segments =
        std::max(segments, 32);

    glm::dvec3 radial =
        orbit.radialAxis;

    glm::dvec3 prograde =
        orbit.progradeAxis;

    if (glm::length(radial) < 0.001)
        radial = glm::dvec3(1.0, 0.0, 0.0);

    if (glm::length(prograde) < 0.001)
        prograde = glm::dvec3(0.0, 0.0, 1.0);

    radial =
        glm::normalize(radial);

    prograde =
        glm::normalize(
            prograde -
            radial * glm::dot(prograde, radial)
        );

    GLfloat baseColor[4] =
    {
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    glGetFloatv(
        GL_CURRENT_COLOR,
        baseColor
    );

    auto orbitPoint =
        [&](int i) -> glm::dvec3
        {
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            return
                orbit.centerMeters +
                radial * std::cos(a) * orbit.radiusMeters +
                prograde * std::sin(a) * orbit.radiusMeters;
        };

    auto isHiddenBehindPlanet =
        [&](const glm::dvec3& worldPoint) -> bool
        {
            const glm::dvec3 relative =
                worldPoint -
                planet.planetCenterMeters;

            const glm::dvec3 cameraSpace =
                planetMapCameraSpaceRelative(
                    relative
                );

            const double projectedDistance2 =
                cameraSpace.x * cameraSpace.x +
                cameraSpace.y * cameraSpace.y;

            const double planetRadius =
                planet.planetRadiusMeters;

            const bool behindPlanetCenter =
                cameraSpace.z < 0.0;

            const bool insidePlanetDisc =
                projectedDistance2 <
                planetRadius * planetRadius;

            return
                behindPlanetCenter &&
                insidePlanetDisc;
        };

    glBegin(GL_LINES);

    for (int i = 0; i < segments; ++i)
    {
        const glm::dvec3 p0 =
            orbitPoint(i);

        const glm::dvec3 p1 =
            orbitPoint((i + 1) % segments);

        const glm::dvec3 mid =
            (p0 + p1) * 0.5;

        const bool hidden =
            isHiddenBehindPlanet(mid);

        const float alpha =
            hidden
                ? baseColor[3] * 0.16f
                : baseColor[3];

        glColor4f(
            baseColor[0],
            baseColor[1],
            baseColor[2],
            alpha
        );

        const glm::dvec2 s0 =
            planetMapProject(
                p0,
                planet,
                scale,
                centerPx
            );

        const glm::dvec2 s1 =
            planetMapProject(
                p1,
                planet,
                scale,
                centerPx
            );

        glVertex2d(s0.x, s0.y);
        glVertex2d(s1.x, s1.y);
    }

    glEnd();

    glColor4f(
        baseColor[0],
        baseColor[1],
        baseColor[2],
        baseColor[3]
    );
}














glm::dvec2 SystemMapRenderer::hubMapProject(
    const glm::dvec3& localMeters,
    double scale,
    const glm::dvec2& centerPx
) const
{
    glm::dvec3 p =
        localMeters;

    const double cy = std::cos(activeDetailCamera().yaw);
    const double sy = std::sin(activeDetailCamera().yaw);
    const double cp = std::cos(activeDetailCamera().pitch);
    const double sp = std::sin(activeDetailCamera().pitch);

    glm::dvec3 a;
    a.x = p.x * cy - p.z * sy;
    a.y = p.y;
    a.z = p.x * sy + p.z * cy;

    glm::dvec3 b;
    b.x = a.x;
    b.y = a.y * cp - a.z * sp;
    b.z = a.y * sp + a.z * cp;

    const double finalScale =
        scale * activeDetailCamera().zoom;

    return glm::dvec2(
        centerPx.x + activeDetailCamera().pan.x + b.x * finalScale,
        centerPx.y + activeDetailCamera().pan.y - b.y * finalScale
    );
}





    bool SystemMapRenderer::pickHubMapOrbitPivot(
        const glm::dvec2& mousePx,
        glm::dvec3& outPivotLocalMeters
    ) const
    {
        if (m_lastHubMapPickables.empty())
            return false;

        const HubMapPickable* best = nullptr;

        double bestScore =
            std::numeric_limits<double>::max();

        for (const auto& p : m_lastHubMapPickables)
        {
            const double dx =
                mousePx.x - p.screenCenterPx.x;

            const double dy =
                mousePx.y - p.screenCenterPx.y;

            const double dist =
                std::sqrt(dx * dx + dy * dy);

            // Не даём огромной станции захватывать весь экран.
            // Но и не требуем попадания в пиксель.
            const double pickRadius =
                std::clamp(
                    p.screenRadiusPx,
                    18.0,
                    140.0
                );

            // Разрешаем брать ближайший объект чуть за пределами радиуса,
            // иначе при wireframe-модели будет раздражающе трудно попасть.
            const double softLimit =
                pickRadius + 80.0;

            if (dist > softLimit)
                continue;

            // priority слегка улучшает счёт.
            // Игрок/корабли выигрывают у станции при близком расстоянии.
            const double priorityBonus =
                static_cast<double>(p.priority) * 18.0;

            const double score =
                dist - priorityBonus;

            if (score < bestScore)
            {
                bestScore = score;
                best = &p;
            }
        }

        if (!best)
            return false;

        outPivotLocalMeters =
            best->localCenterMeters;

        return true;
    }









glm::dvec3 SystemMapRenderer::hubMapUnprojectCursorToLocal(
    const glm::dvec2& mousePx,
    double scale,
    const glm::dvec2& centerPx
) const
{
    const double finalScale =
        scale * activeDetailCamera().zoom;

    if (std::abs(finalScale) < 0.000001)
        return glm::dvec3(0.0);

    // hubMapProject:
    //
    // screen.x = center.x + pan.x + b.x * finalScale
    // screen.y = center.y + pan.y - b.y * finalScale
    //
    // Reverse that first.
    const double bx =
        (mousePx.x - centerPx.x - activeDetailCamera().pan.x) /
        finalScale;

    const double by =
        -(mousePx.y - centerPx.y - activeDetailCamera().pan.y) /
        finalScale;

    // No real depth information under cursor yet.
    // We choose the current view plane depth = 0.
    const glm::dvec3 b(
        bx,
        by,
        0.0
    );

    const double cy = std::cos(activeDetailCamera().yaw);
    const double sy = std::sin(activeDetailCamera().yaw);
    const double cp = std::cos(activeDetailCamera().pitch);
    const double sp = std::sin(activeDetailCamera().pitch);

    // Inverse pitch.
    glm::dvec3 a;
    a.x = b.x;
    a.y = b.y * cp + b.z * sp;
    a.z = -b.y * sp + b.z * cp;

    // Inverse yaw.
    glm::dvec3 p;
    p.x = a.x * cy + a.z * sy;
    p.y = a.y;
    p.z = -a.x * sy + a.z * cy;

    return p;
}















void SystemMapRenderer::drawHubMapBox(
    const glm::dvec3& center,
    const world::celestial::PlanetMapAxisSet& axes,
    const glm::dvec3& size,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec3 hx = axes.x * (size.x * 0.5);
    const glm::dvec3 hy = axes.y * (size.y * 0.5);
    const glm::dvec3 hz = axes.z * (size.z * 0.5);

    glm::dvec3 p[8];

    p[0] = center - hx - hy - hz;
    p[1] = center + hx - hy - hz;
    p[2] = center + hx + hy - hz;
    p[3] = center - hx + hy - hz;

    p[4] = center - hx - hy + hz;
    p[5] = center + hx - hy + hz;
    p[6] = center + hx + hy + hz;
    p[7] = center - hx + hy + hz;

    auto line =
        [&](int a, int b)
        {
            const glm::dvec2 sa =
                hubMapProject(p[a], scale, centerPx);

            const glm::dvec2 sb =
                hubMapProject(p[b], scale, centerPx);

            drawPlanetMapLine(sa, sb);
        };

    line(0, 1);
    line(1, 2);
    line(2, 3);
    line(3, 0);

    line(4, 5);
    line(5, 6);
    line(6, 7);
    line(7, 4);

    line(0, 4);
    line(1, 5);
    line(2, 6);
    line(3, 7);
}



void SystemMapRenderer::drawHubMapAxes(
    const glm::dvec3& center,
    const world::celestial::PlanetMapAxisSet& axes,
    double axisLenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec2 o =
        hubMapProject(center, scale, centerPx);

    glColor4f(1.0f, 0.2f, 0.2f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.x * axisLenMeters, scale, centerPx)
    );

    glColor4f(0.2f, 1.0f, 0.2f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.y * axisLenMeters, scale, centerPx)
    );

    glColor4f(0.25f, 0.55f, 1.0f, 0.95f);
    drawPlanetMapLine(
        o,
        hubMapProject(center + axes.z * axisLenMeters, scale, centerPx)
    );
}



void SystemMapRenderer::drawHubMapVelocityArrow(
    const glm::dvec3& center,
    const glm::dvec3& velocity,
    double lenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double speed =
        glm::length(velocity);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocity / speed;

    glColor4f(1.0f, 0.9f, 0.25f, 0.95f);

    drawPlanetMapLine(
        hubMapProject(center, scale, centerPx),
        hubMapProject(center + dir * lenMeters, scale, centerPx)
    );
}







glm::dvec3 SystemMapRenderer::hubMapObjectLocalToHubLocal(
    const glm::dvec3& objectCenter,
    const world::celestial::PlanetMapAxisSet& objectAxes,
    const glm::dvec3& localPoint
) const
{
    return
        objectCenter +
        objectAxes.x * localPoint.x +
        objectAxes.y * localPoint.y +
        objectAxes.z * localPoint.z;
}



bool SystemMapRenderer::drawHubMapAssemblyWire(
    ObjectType typeId,
    const glm::dvec3& objectCenter,
    const world::celestial::PlanetMapAxisSet& objectAxes,
    double scale,
    const glm::dvec2& centerPx
)
{
    using game::ship::geometry::AssemblyMeshLibrary;

    if (typeId == ObjectType::None)
        return false;

    if (!AssemblyMeshLibrary::has(typeId))
        return false;

    const auto& assembly =
        AssemblyMeshLibrary::get(typeId);

    bool drewAnything = false;

    // Для маленьких кораблей лучше цельный proxy, если он есть.
    // Это быстрее и чище на карте.
    if (assembly.hasWholeShipProxy)
    {
        const auto& mesh =
            assembly.wholeShipProxyMesh;

        for (const auto& edge : mesh.edges)
        {
            const glm::dvec3 a =
                hubMapObjectLocalToHubLocal(
                    objectCenter,
                    objectAxes,
                    glm::dvec3(edge.a)
                );

            const glm::dvec3 b =
                hubMapObjectLocalToHubLocal(
                    objectCenter,
                    objectAxes,
                    glm::dvec3(edge.b)
                );

            drawPlanetMapLine(
                hubMapProject(a, scale, centerPx),
                hubMapProject(b, scale, centerPx)
            );

            drewAnything = true;
        }

        if (drewAnything)
            return true;
    }

    // Для станции рисуем модульную LOD1-сборку.
    // Это как раз "цветок со стеблем", а не кирпич.
    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            const auto& mesh =
                part.lod1Mesh.edges.empty()
                    ? part.lod0Mesh
                    : part.lod1Mesh;

            for (const auto& edge : mesh.edges)
            {
                const glm::dvec3 localA =
                    glm::dvec3(module.localPosition) +
                    glm::dvec3(part.localOffset) +
                    glm::dvec3(edge.a);

                const glm::dvec3 localB =
                    glm::dvec3(module.localPosition) +
                    glm::dvec3(part.localOffset) +
                    glm::dvec3(edge.b);

                const glm::dvec3 a =
                    hubMapObjectLocalToHubLocal(
                        objectCenter,
                        objectAxes,
                        localA
                    );

                const glm::dvec3 b =
                    hubMapObjectLocalToHubLocal(
                        objectCenter,
                        objectAxes,
                        localB
                    );

                drawPlanetMapLine(
                    hubMapProject(a, scale, centerPx),
                    hubMapProject(b, scale, centerPx)
                );

                drewAnything = true;
            }
        }
    }

    return drewAnything;
}















void SystemMapRenderer::drawHubMapCircleLocalXY(
    const glm::dvec3& center,
    double radiusMeters,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (radiusMeters <= 0.0)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const glm::dvec3 p =
            center +
            glm::dvec3(
                std::cos(a) * radiusMeters,
                std::sin(a) * radiusMeters,
                0.0
            );

        const glm::dvec2 s =
            hubMapProject(
                p,
                scale,
                centerPx
            );

        glVertex2d(
            s.x,
            s.y
        );
    }

    glEnd();
}



void SystemMapRenderer::drawHubMapPlanetSurfaceHint(
    const world::celestial::HubMapSnapshot& hub,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (hub.parentPlanetRadiusMeters <= 0.0 ||
        hub.hubOrbitRadiusMeters <= 0.0)
    {
        return;
    }

    const glm::dvec3 planetCenter =
        hub.parentPlanetCenterLocalMeters;

    const glm::dvec2 planetCenterPx =
        hubMapProject(
            planetCenter,
            scale,
            centerPx
        );

    const double planetRadiusPx =
        hub.parentPlanetRadiusMeters *
        scale *
        activeDetailCamera().zoom;


        const GLuint planetTexture =
        mapPreviewTextureForHubSnapshot(hub);

    if (planetTexture != 0)
    {
        drawTexturedDisk2D(
            planetTexture,
            planetCenterPx,
            planetRadiusPx,
            glm::vec4(1.0f, 1.0f, 1.0f, 0.28f),
            192
        );
    }

    // Если планета слишком огромная и центр далеко за экраном,
    // всё равно рисуем только геометрически корректную дугу/край.
    // Это дает глазу "низ", не превращая карту в синий блин.
    glColor4f(
        0.08f,
        0.22f,
        0.32f,
        0.22f
    );

    glBegin(GL_TRIANGLE_FAN);
    glVertex2d(
        planetCenterPx.x,
        planetCenterPx.y
    );

    constexpr int fillSegments = 160;

    for (int i = 0; i <= fillSegments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(fillSegments);

        glVertex2d(
            planetCenterPx.x + std::cos(a) * planetRadiusPx,
            planetCenterPx.y + std::sin(a) * planetRadiusPx
        );
    }

    glEnd();

    // Контур поверхности.
    glColor4f(
        0.22f,
        0.58f,
        0.78f,
        0.75f
    );

    drawPlanetMapCircle(
        planetCenterPx,
        planetRadiusPx,
        192
    );

    // Орбита хаба вокруг планеты.
    glColor4f(
        0.95f,
        0.82f,
        0.32f,
        0.42f
    );

    drawHubMapCircleLocalXY(
        planetCenter,
        hub.hubOrbitRadiusMeters,
        scale,
        centerPx,
        256
    );

    // Радиальная линия: центр планеты -> поверхность -> хаб.
    const glm::dvec3 surfacePoint =
        planetCenter +
        glm::dvec3(
            0.0,
            hub.parentPlanetRadiusMeters,
            0.0
        );

    glColor4f(
        0.35f,
        0.85f,
        1.0f,
        0.45f
    );

    drawPlanetMapLine(
        hubMapProject(surfacePoint, scale, centerPx),
        hubMapProject(glm::dvec3(0.0), scale, centerPx)
    );

    // Маленькая отметка точки под хабом на поверхности.
    glColor4f(
        0.65f,
        0.95f,
        1.0f,
        0.85f
    );

    drawPlanetMapCross(
        hubMapProject(surfacePoint, scale, centerPx),
        5.0f
    );
}



void SystemMapRenderer::drawHubMapAdaptiveGrid(
    const Viewport& viewport,
    double scale,
    const glm::dvec2& centerPx,
    double worldRadiusMeters
)
{
    if (scale <= 0.0)
        return;

    const double visibleMeters =
        std::max(
            1000.0,
            worldRadiusMeters
        );

    double gridStep =
        100.0;

    while ((gridStep * scale * activeDetailCamera().zoom) < 28.0)
        gridStep *= 2.0;

    while ((gridStep * scale * activeDetailCamera().zoom) > 90.0 &&
           gridStep > 25.0)
    {
        gridStep *= 0.5;
    }

    const int gridN =
        static_cast<int>(
            std::ceil(
                visibleMeters / gridStep
            )
        ) + 2;

    glColor4f(
        0.12f,
        0.28f,
        0.38f,
        0.30f
    );

    for (int i = -gridN; i <= gridN; ++i)
    {
        const double v =
            static_cast<double>(i) *
            gridStep;

        drawPlanetMapLine(
            hubMapProject(glm::dvec3(-gridN * gridStep, 0.0, v), scale, centerPx),
            hubMapProject(glm::dvec3( gridN * gridStep, 0.0, v), scale, centerPx)
        );

        drawPlanetMapLine(
            hubMapProject(glm::dvec3(v, 0.0, -gridN * gridStep), scale, centerPx),
            hubMapProject(glm::dvec3(v, 0.0,  gridN * gridStep), scale, centerPx)
        );
    }

    // Главные оси плоскости хаба.
    glColor4f(
        0.20f,
        0.55f,
        0.75f,
        0.45f
    );

    drawPlanetMapLine(
        hubMapProject(glm::dvec3(-gridN * gridStep, 0.0, 0.0), scale, centerPx),
        hubMapProject(glm::dvec3( gridN * gridStep, 0.0, 0.0), scale, centerPx)
    );

    drawPlanetMapLine(
        hubMapProject(glm::dvec3(0.0, 0.0, -gridN * gridStep), scale, centerPx),
        hubMapProject(glm::dvec3(0.0, 0.0,  gridN * gridStep), scale, centerPx)
    );
}



glm::dvec3 SystemMapRenderer::visualSizeForHubShip(
    const world::celestial::HubMapShip& ship,
    double scale
) const
{
    // Реальный корабль можно держать маленьким, но на карте он не должен
    // превращаться в субатомную пыль. Поэтому размер физический,
    // но с минимальным экранным размером.
    glm::dvec3 physicalSizeMeters(
        90.0,
        35.0,
        160.0
    );

    if (ship.player)
    {
        physicalSizeMeters =
            glm::dvec3(
                130.0,
                50.0,
                210.0
            );
    }

    const double pixelsPerMeter =
        scale *
        activeDetailCamera().zoom;

    if (pixelsPerMeter <= 0.0)
        return physicalSizeMeters;

    const double longestPx =
        std::max(
            physicalSizeMeters.x,
            std::max(
                physicalSizeMeters.y,
                physicalSizeMeters.z
            )
        ) * pixelsPerMeter;

    constexpr double minPlayerLongestPx = 18.0;
    constexpr double minOtherLongestPx = 11.0;

    const double minPx =
        ship.player
            ? minPlayerLongestPx
            : minOtherLongestPx;

    if (longestPx >= minPx)
        return physicalSizeMeters;

    const double factor =
        minPx /
        std::max(
            1.0,
            longestPx
        );

    return physicalSizeMeters * factor;
}







void SystemMapRenderer::renderHubMap(
    const Viewport& viewport,
    const world::celestial::HubMapSnapshot& hub
)
{
    ensureGeneratedCelestialAssets();

    glViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glScissor(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(
        0.015f,
        0.020f,
        0.030f,
        1.0f
    );

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!hub.valid)
        return;

    const glm::dvec2 centerPx(
        static_cast<double>(viewport.width) * 0.5,
        static_cast<double>(viewport.height) * 0.5
    );

    double maxDist =
        1000.0;

    for (const auto& m : hub.modules)
    {
        if (!m.valid)
            continue;

        maxDist =
            std::max(
                maxDist,
                glm::length(m.localPositionMeters) +
                glm::length(m.sizeMeters)
            );
    }

    for (const auto& s : hub.ships)
    {
        if (!s.valid)
            continue;

        maxDist =
            std::max(
                maxDist,
                glm::length(s.localPositionMeters) +
                800.0
            );
    }

    // Планета и орбита не должны раздувать масштаб карты до состояния:
    // "станция стала точкой, поздравляем, вы снова ничего не видите".
    //
    // Поэтому масштаб выбираем по объектам хаба, а планету рисуем как
    // ориентир в том же масштабе. Если центр планеты далеко за экраном,
    // видна будет дуга поверхности/орбиты — именно это и нужно.
    maxDist =
        std::max(
            maxDist,
            2500.0
        );

    const double halfPx =
        std::min(
            viewport.width,
            viewport.height
        ) * 0.38;

    const double scale =
        halfPx /
        std::max(
            1.0,
            maxDist
        );

    m_lastHubMapScale = scale;
    m_lastHubMapCenterPx = centerPx;
    m_lastHubMapPickables.clear();

    drawHubMapPlanetSurfaceHint(
        hub,
        scale,
        centerPx
    );

    drawHubMapAdaptiveGrid(
        viewport,
        scale,
        centerPx,
        maxDist
    );

    // Оси хаба.
    drawHubMapAxes(
        glm::dvec3(0.0),
        hub.hubAxes,
        900.0,
        scale,
        centerPx
    );

    // Центр хаба / текущая орбитальная точка.
    glColor4f(
        1.0f,
        0.86f,
        0.35f,
        0.95f
    );

    drawPlanetMapCross(
        hubMapProject(glm::dvec3(0.0), scale, centerPx),
        6.0f
    );

    // Модули станции.
    for (const auto& mod : hub.modules)
    {
        if (!mod.valid)
            continue;

        {
            const glm::dvec2 screen =
                hubMapProject(
                    mod.localPositionMeters,
                    scale,
                    centerPx
                );

            const double objectRadiusMeters =
                glm::length(mod.sizeMeters) * 0.5;

            HubMapPickable pickable;
            pickable.localCenterMeters =
                mod.localPositionMeters;

            pickable.screenCenterPx =
                screen;

            pickable.screenRadiusPx =
                objectRadiusMeters *
                scale *
                activeDetailCamera().zoom;

            pickable.priority =
                mod.prime ? 20 : 10;

            pickable.label =
                mod.name;

            m_lastHubMapPickables.push_back(
                pickable
            );
        }







        if (mod.prime || mod.kind == "station")
        {
            glColor4f(
                0.65f,
                0.92f,
                1.0f,
                0.95f
            );
        }
        else
        {
            glColor4f(
                0.45f,
                0.65f,
                0.85f,
                0.75f
            );
        }


        const bool modelDrawn =
            drawHubMapAssemblyWire(
                mod.typeId,
                mod.localPositionMeters,
                mod.localAxes,
                scale,
                centerPx
            );

        if (!modelDrawn)
        {
            drawHubMapBox(
                mod.localPositionMeters,
                mod.localAxes,
                mod.sizeMeters,
                scale,
                centerPx
            );
        }

        const double moduleAxisLen =
            std::max(
                350.0,
                glm::length(mod.sizeMeters) * 0.08
            );

        drawHubMapAxes(
            mod.localPositionMeters,
            mod.localAxes,
            moduleAxisLen,
            scale,
            centerPx
        );


        
    }

    // Игрок / корабли.
    for (const auto& ship : hub.ships)
    {
        if (!ship.valid)
            continue;

    {
        const glm::dvec2 screen =
            hubMapProject(
                ship.localPositionMeters,
                scale,
                centerPx
            );

        const double objectRadiusMeters =
            glm::length(ship.sizeMeters) * 0.5;

        HubMapPickable pickable;
        pickable.localCenterMeters =
            ship.localPositionMeters;

        pickable.screenCenterPx =
            screen;

        pickable.screenRadiusPx =
            std::max(
                18.0,
                objectRadiusMeters *
                scale *
                activeDetailCamera().zoom
            );

        pickable.priority =
            ship.player ? 100 : 50;

        pickable.label =
            ship.player
                ? "PLAYER"
                : ship.name;

        m_lastHubMapPickables.push_back(
            pickable
        );
    }


        if (ship.player)
        {
            glColor4f(
                1.0f,
                0.78f,
                0.25f,
                1.0f
            );
        }
        else
        {
            glColor4f(
                0.95f,
                0.65f,
                0.35f,
                0.85f
            );
        }

        const glm::dvec3 shipSize =
            visualSizeForHubShip(
                ship,
                scale
            );


        const bool shipModelDrawn =
            drawHubMapAssemblyWire(
                ship.typeId,
                ship.localPositionMeters,
                ship.localAxes,
                scale,
                centerPx
            );

        if (!shipModelDrawn)
        {
            drawHubMapBox(
                ship.localPositionMeters,
                ship.localAxes,
                ship.sizeMeters,
                scale,
                centerPx
            );
        }

        const double shipAxisLen =
            std::max(
                12.0,
                glm::length(ship.sizeMeters) * 0.85
            );

        drawHubMapAxes(
            ship.localPositionMeters,
            ship.localAxes,
            shipAxisLen,
            scale,
            centerPx
        );

        drawHubMapVelocityArrow(
            ship.localPositionMeters,
            ship.localVelocityMps,
            std::max(80.0, shipAxisLen * 2.0),
            scale,
            centerPx
        );
    }







        {
            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                viewport.width,
                viewport.height
            );

            for (const auto& mod : hub.modules)
            {
                if (!mod.valid)
                    continue;

                const glm::dvec2 p =
                    hubMapProject(
                        mod.localPositionMeters,
                        scale,
                        centerPx
                    );

                text.textDrawPx(
                    mod.name,
                    static_cast<float>(p.x + 10.0),
                    static_cast<float>(p.y - 8.0),
                    13,
                    glm::vec4(0.65f, 0.92f, 1.0f, 0.88f)
                );

                if (!mod.kind.empty())
                {
                    text.textDrawPx(
                        mod.kind,
                        static_cast<float>(p.x + 10.0),
                        static_cast<float>(p.y + 8.0),
                        10,
                        glm::vec4(0.55f, 0.72f, 0.82f, 0.62f)
                    );
                }
            }

            for (const auto& ship : hub.ships)
            {
                if (!ship.valid)
                    continue;

                const glm::dvec2 p =
                    hubMapProject(
                        ship.localPositionMeters,
                        scale,
                        centerPx
                    );

                const std::string label =
                    ship.player
                        ? "PLAYER"
                        : ship.name;

                text.textDrawPx(
                    label,
                    static_cast<float>(p.x + 10.0),
                    static_cast<float>(p.y - 8.0),
                    13,
                    glm::vec4(1.0f, 0.78f, 0.25f, 0.92f)
                );
            }

            text.endFrame();
        }












    glEnable(GL_DEPTH_TEST);
}





