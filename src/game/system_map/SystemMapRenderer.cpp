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
#include <cctype>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "render/HUD/TextRenderer.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/render/bitmap/TextureLoader.h"
#include <nlohmann/json.hpp>

namespace
{
    constexpr double AU_KM = 149597870.7;
    
    double cloudWindTimeScale()
    {
        static const double cachedValue =
            []() -> double
            {
                namespace fs = std::filesystem;

                fs::path searchRoot =
                    fs::current_path();

                /*
                    Запуск обычно происходит из build/.
                    Поднимаемся вверх и ищем project/src/assets.
                */
                for (int level = 0;
                    level < 6;
                    ++level)
                {
                    const fs::path candidate =
                        searchRoot /
                        "src/assets/data/celestial/runtime/environment_debug.json";

                    if (fs::exists(candidate))
                    {
                        try
                        {
                            std::ifstream input(
                                candidate
                            );

                            nlohmann::json root;
                            input >> root;

                            const auto windScale =
                                root.value(
                                    "wind_time_scale",
                                    nlohmann::json::object()
                                );

                            const double value =
                                windScale.value(
                                    "default_debug",
                                    600.0
                                );

                            std::cout
                                << "[CloudWind]"
                                << " runtime scale="
                                << value
                                << " source="
                                << candidate.generic_string()
                                << "\n";

                            return
                                std::max(
                                    0.0,
                                    value
                                );
                        }
                        catch (const std::exception& error)
                        {
                            std::cerr
                                << "[CloudWind]"
                                << " failed to read runtime config: "
                                << error.what()
                                << "\n";

                            return 600.0;
                        }
                    }

                    if (!searchRoot.has_parent_path())
                        break;

                    const fs::path parent =
                        searchRoot.parent_path();

                    if (parent == searchRoot)
                        break;

                    searchRoot =
                        parent;
                }

                std::cerr
                    << "[CloudWind]"
                    << " environment_debug.json not found;"
                    << " fallback scale=600"
                    << "\n";

                return 600.0;
            }();

        return cachedValue;
    }








    std::string normalizeCloudToken(
        const std::string& value
    )
    {
        std::string result;
        result.reserve(
            value.size()
        );

        for (const unsigned char character : value)
        {
            if (std::isalnum(character))
            {
                result.push_back(
                    static_cast<char>(
                        std::tolower(character)
                    )
                );
            }
            else
            {
                result.push_back('_');
            }
        }

        return result;
    }












    
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

        if (systemId == 8)
            return "tau_ceti";

        return "";
    }





    std::uint32_t hashEnvironmentString32(
        const std::string& text
    )
    {
        std::uint32_t hash = 2166136261u;

        for (unsigned char c : text)
        {
            hash ^= static_cast<std::uint32_t>(c);
            hash *= 16777619u;
        }

        return hash;
    }

    std::uint32_t makeEnvironmentRuntimeSeed()
    {
        static std::uint32_t counter = 0u;

        ++counter;

        const auto now =
            std::chrono::high_resolution_clock::now()
                .time_since_epoch()
                .count();

        std::uint64_t mixed =
            static_cast<std::uint64_t>(now);

        mixed ^=
            static_cast<std::uint64_t>(counter) *
            0x9E3779B97F4A7C15ull;

        mixed ^= mixed >> 32;

        return static_cast<std::uint32_t>(mixed);
    }




    render::celestial::ProceduralCloudPattern toProceduralCloudPattern(
        world::celestial::visual::EnvironmentCloudPattern pattern
    )
    {
        using EnvPattern =
            world::celestial::visual::EnvironmentCloudPattern;

        switch (pattern)
        {
            case EnvPattern::ScatteredCumulus:
                return render::celestial::ProceduralCloudPattern::ScatteredCumulus;

            case EnvPattern::DenseOvercast:
                return render::celestial::ProceduralCloudPattern::DenseOvercast;

            case EnvPattern::Banded:
                return render::celestial::ProceduralCloudPattern::Banded;

            case EnvPattern::BrokenFields:
            case EnvPattern::None:
            default:
                return render::celestial::ProceduralCloudPattern::BrokenFields;
        }
    }

    glm::vec4 alphaScaled(
        glm::vec4 color,
        float intensity
    )
    {
        color.a *=
            std::max(
                0.0f,
                intensity
            );

        return color;
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
    ensureEnvironmentProfiles();

    if (!m_mapStarfieldInitialized)
    {
        m_mapStarfieldInitialized =
            m_mapStarfieldRenderer.initialize(
                "assets/data/galaxy/star_systems.json"
            );

        if (!m_mapStarfieldInitialized)
        {
            std::cerr
                << "[SystemMapRenderer]"
                << " failed to initialize map starfield"
                << "\n";
        }
    }

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






void SystemMapRenderer::drawMapStarfield(
    const Viewport& viewport,
    const glm::dvec3& observerPositionLy
)
{
    if (!m_mapStarfieldInitialized ||
        viewport.width <= 0 ||
        viewport.height <= 0)
    {
        return;
    }

    const float aspect =
        static_cast<float>(
            viewport.width
        ) /
        static_cast<float>(
            std::max(
                viewport.height,
                1
            )
        );

    const glm::mat4 projection =
        glm::perspective(
            glm::radians(
                60.0f
            ),
            aspect,
            0.1f,
            500.0f
        );

    /*
        Небо вращается вместе с detail camera,
        но не зависит от zoom и pan.
    */
    glm::mat4 view(1.0f);

    view =
        glm::rotate(
            view,
            static_cast<float>(
                -activeDetailCamera().pitch
            ),
            glm::vec3(
                1.0f,
                0.0f,
                0.0f
            )
        );

    view =
        glm::rotate(
            view,
            static_cast<float>(
                -activeDetailCamera().yaw
            ),
            glm::vec3(
                0.0f,
                1.0f,
                0.0f
            )
        );

    m_mapStarfieldRenderer.setObserverPositionLy(
        glm::vec3(
            static_cast<float>(
                observerPositionLy.x
            ),
            static_cast<float>(
                observerPositionLy.y
            ),
            static_cast<float>(
                observerPositionLy.z
            )
        )
    );

    m_mapStarfieldRenderer.render(
        view,
        projection,
        0.88f
    );

    /*
        GalaxyStarfieldRenderer в конце включает depth test.
        Дальнейший map renderer работает как 2D overlay.
    */
    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_TRUE
    );

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

    glMatrixMode(
        GL_PROJECTION
    );

    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(
        GL_MODELVIEW
    );

    glLoadIdentity();

    glUseProgram(
        0
    );
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











void SystemMapRenderer::ensureEnvironmentProfiles()
{
    if (m_environmentProfilesAttempted)
        return;

    m_environmentProfilesAttempted = true;

    // Runtime path после копирования assets в build.
    m_environmentProfilesLoaded =
        m_environmentProfiles.loadFromRoot(
            "assets/data/celestial/environment"
        );

    // Dev fallback: запуск из build без копии assets/data.
    if (!m_environmentProfilesLoaded)
    {
        m_environmentProfilesLoaded =
            m_environmentProfiles.loadFromRoot(
                "src/assets/data/celestial/environment"
            );
    }

    if (m_environmentProfilesLoaded)
    {
        std::cout
            << "[SystemMapRenderer] celestial environment profiles loaded:"
            << " presets="
            << m_environmentProfiles.presets().size()
            << " bodyProfiles="
            << m_environmentProfiles.profiles().size()
            << " failed="
            << m_environmentProfiles.failedCount()
            << "\n";
    }
    else
    {
        std::cerr
            << "[SystemMapRenderer] celestial environment profiles not loaded; "
            << "atmosphere/clouds from JSON disabled.\n";
    }
}

void SystemMapRenderer::beginEnvironmentRenderSessionIfNeeded(
    Mode mode,
    int systemId,
    const std::string& bodyId
)
{
    const std::string bodyPart =
        lastGeneratedIdentityPathPart(
            bodyId
        );

    
        (void)mode;

        const std::string key =
            std::to_string(systemId) +
            ":" +
            bodyPart;



    if (m_environmentRenderSessionKey == key &&
        m_environmentMapOpenSeed != 0u)
    {
        return;
    }

    m_environmentRenderSessionKey =
        key;

    m_environmentMapOpenSeed =
        makeEnvironmentRuntimeSeed();

    // Важно:
    // JSON не перезагружаем.
    // Чистим только процедурные cloud textures, чтобы память не росла
    // при прыжках между телами.
    m_proceduralCloudLayer.clearCache();

    std::cout
        << "[SystemMapRenderer] environment render session: "
        << key
        << " seed="
        << m_environmentMapOpenSeed
        << "\n";
}







world::celestial::visual::CelestialEnvironmentProfile
SystemMapRenderer::resolvedEnvironmentProfileForBody(
    int systemId,
    const std::string& bodyId,
    const std::string& displayName,
    const std::string& environmentPresetId
) const
{
    if (!m_environmentProfilesLoaded)
        return {};

    std::string systemFolder =
        generatedSystemFolderForSystemId(
            systemId
        );

    if (systemFolder.empty())
    {
        systemFolder =
            "system_" +
            std::to_string(systemId);
    }

    std::vector<std::string> bodyCandidates;

    auto addCandidate =
        [&](const std::string& raw)
        {
            const std::string token =
                normalizeGeneratedIdentityToken(
                    raw
                );

            if (token.empty())
                return;

            if (std::find(
                    bodyCandidates.begin(),
                    bodyCandidates.end(),
                    token
                ) == bodyCandidates.end())
            {
                bodyCandidates.push_back(
                    token
                );
            }
        };

    // ------------------------------------------------------------
    // 1. Кандидат из generated asset.
    // ------------------------------------------------------------
    const auto* asset =
        generatedAssetForIdentity(
            systemId,
            bodyId,
            displayName
        );

    if (asset)
    {
        addCandidate(
            asset->bodyFolderName
        );

        addCandidate(
            asset->displayName
        );
    }

    // ------------------------------------------------------------
    // 2. Кандидаты из snapshot.
    // ------------------------------------------------------------
    addCandidate(
        lastGeneratedIdentityPathPart(
            bodyId
        )
    );

    addCandidate(
        bodyId
    );

    addCandidate(
        displayName
    );

    // ------------------------------------------------------------
    // 3. Reverse aliases.
    // Если snapshot говорит Terra/Gaia/Земля,
    // надо всё равно попробовать canonical earth.
    // ------------------------------------------------------------
    const auto& aliases =
        knownGeneratedSolAliases();

    const std::vector<std::string> current =
        bodyCandidates;

    for (const std::string& candidate : current)
    {
        for (const auto& pair : aliases)
        {
            const std::string canonical =
                normalizeGeneratedIdentityToken(
                    pair.first
                );

            if (candidate == canonical)
            {
                addCandidate(
                    pair.first
                );

                continue;
            }

            for (const std::string& alias : pair.second)
            {
                if (candidate ==
                    normalizeGeneratedIdentityToken(
                        alias
                    ))
                {
                    addCandidate(
                        pair.first
                    );
                }
            }
        }
    }

    for (const std::string& bodyCandidate : bodyCandidates)
    {
        const auto profile =
            m_environmentProfiles.resolve(
                environmentPresetId,
                systemFolder,
                bodyCandidate
            );

        if (profile.found)
            return profile;
    }

    static int missCounter = 0;

    if ((missCounter++ % 120) == 0)
    {
        std::cerr
            << "[EnvironmentProfile] missing profile"
            << " systemId=" << systemId
            << " systemFolder=" << systemFolder
            << " bodyId=" << bodyId
            << " displayName=" << displayName
            << " candidates=";

        for (const std::string& candidate : bodyCandidates)
        {
            std::cerr
                << candidate
                << " ";
        }

        std::cerr
            << "\n";
    }

    return {};
}








std::vector<
    render::celestial::ProceduralCloudStyle
>
SystemMapRenderer::cloudStylesForBody(
    int systemId,
    const std::string& bodyId,
    const std::string& displayName,
    const std::string& environmentPresetId,
    double planetRadiusMeters,
    int textureWidth,
    int textureHeight
) const
{
    std::vector<
        render::celestial::ProceduralCloudStyle
    > result;

    const auto profile =
        resolvedEnvironmentProfileForBody(
            systemId,
            bodyId,
            displayName,
            environmentPresetId
        );

    if (!profile.found ||
        !profile.clouds.enabled ||
        profile.clouds.pattern ==
            world::celestial::visual::
                EnvironmentCloudPattern::None ||
        profile.clouds.layers.empty())
    {
        return result;
    }

    const auto& clouds =
        profile.clouds;




    const auto& rendering =
        profile.rendering;

    const bool surfaceHidden =
        rendering.surfaceVisibility ==
            "hidden";

    const bool venusian =
        rendering.visualClass ==
            "venusian_cloud_deck";

    const bool gasGiant =
        rendering.visualClass ==
            "gas_giant";

    const bool iceGiant =
        rendering.visualClass ==
            "ice_giant";




    
    const std::string atmosphereKind =
        profile.atmosphere.kind;

    const bool venusLike =
        atmosphereKind.find("venus") != std::string::npos ||
        atmosphereKind.find("sulfuric") != std::string::npos;

    const bool gasGiantLike =
        clouds.pattern ==
            world::celestial::visual::EnvironmentCloudPattern::Banded;

    /*
        Оценка суммарного покрытия независимых слоёв:

            union = 1 - Π(1 - coverage_i)

        Затем корректируем покрытия слоёв так, чтобы их
        объединение примерно соответствовало globalCoverage.
    */
    double clearProbability = 1.0;

    for (const auto& layer : clouds.layers)
    {
        clearProbability *=
            1.0 -
            std::clamp(
                static_cast<double>(
                    layer.coverage
                ),
                0.0,
                0.98
            );
    }

    const double sourceUnionCoverage =
        1.0 -
        clearProbability;

    const double coverageCorrection =
        sourceUnionCoverage > 0.0001
            ? std::clamp(
                static_cast<double>(
                    clouds.globalCoverage
                ) /
                sourceUnionCoverage,
                0.35,
                1.65
            )
            : 1.0;

    const std::string identityKey =
        profile.systemFolderName +
        "/" +
        profile.bodyFolderName +
        "/" +
        profile.presetId;

    const std::uint32_t identitySeed =
        hashEnvironmentString32(
            identityKey
        );

    constexpr std::size_t maximumRenderedLayers = 8u;

    const std::size_t layerCount =
        std::min(
            clouds.layers.size(),
            maximumRenderedLayers
        );

    result.reserve(
        layerCount
    );

    for (std::size_t layerIndex = 0;
         layerIndex < layerCount;
         ++layerIndex)
    {
        const auto& layer =
            clouds.layers[layerIndex];

        if (layer.coverage <= 0.001f ||
            layer.opacity <= 0.001f ||
            layer.topHeightKm <= layer.baseHeightKm)
        {
            continue;
        }

        render::celestial::ProceduralCloudStyle style;

        style.enabled = true;

        style.pattern =
            toProceduralCloudPattern(
                clouds.pattern
            );

        style.layerId =
            layer.id;

        style.layerType =
            layer.type;


        style.renderRole =
            layer.renderRole;

        style.visualClass =
            rendering.visualClass;

        style.surfaceHidden =
            surfaceHidden;

        style.primaryOpaqueDeck =
            layer.id ==
                rendering.primaryLayerId ||
            layer.renderRole ==
                "primary_opaque_deck";

        style.opacityFloor =
            style.primaryOpaqueDeck
                ? rendering.primaryLayerOpacityFloor
                : 0.0f;

        if (venusian)
        {
            style.morphology =
                render::celestial::
                    ProceduralCloudMorphology::
                        VenusianStreaks;
        }
        else if (gasGiant)
        {
            style.morphology =
                render::celestial::
                    ProceduralCloudMorphology::
                        GasGiantBanded;
        }
        else if (iceGiant)
        {
            style.morphology =
                render::celestial::
                    ProceduralCloudMorphology::
                        IceGiantBanded;
        }
        else
        {
            style.morphology =
                render::celestial::
                    ProceduralCloudMorphology::
                        Terrestrial;
        }



        style.baseHeightKm =
            layer.baseHeightKm;

        style.topHeightKm =
            layer.topHeightKm;

        style.layerCoverage =
            layer.coverage;

        style.layerDensity =
            layer.density;

        style.particleScale =
            layer.particleScale;

        style.textureWidth =
            textureWidth;

        style.textureHeight =
            textureHeight;

        /*
            Каждый высотный слой должен иметь собственный seed,
            иначе все слои совпадут формой и будут выглядеть
            как одна и та же texture на разных радиусах.
        */
        const std::uint32_t layerSeed =
            hashEnvironmentString32(
                identityKey +
                "/" +
                layer.id +
                "/" +
                std::to_string(layerIndex)
            );

        if (clouds.generation.seedPolicy ==
                "random_on_session" ||
            clouds.generation.seedPolicy ==
                "random_on_map_open")
        {
            style.seed =
                clouds.generation.seedBase ^
                m_environmentMapOpenSeed ^
                identitySeed ^
                layerSeed;
        }
        else
        {
            style.seed =
                clouds.generation.seedBase ^
                identitySeed ^
                layerSeed;
        }

        /*
            Покрытие каждого слоя берём из layer.coverage,
            а globalCoverage используется только как коррекция
            общей вероятности покрытия всех слоёв.
        */
        style.globalCoverage =
            std::clamp(
                static_cast<float>(
                    static_cast<double>(
                        layer.coverage
                    ) *
                    coverageCorrection
                ),
                0.01f,
                0.92f
            );

        /*
            Плотное ядро должно закрывать поверхность.
            Прозрачность должна появляться в texture alpha
            на краях и в тонких частях, а не за счёт превращения
            всего слоя в серую плёнку.
        */
        style.opacity =
            std::clamp(
                layer.opacity *
                    layer.appearance.opacityScale,
                0.0f,
                1.0f
            );

        if (style.primaryOpaqueDeck)
        {
            style.opacity =
                std::max(
                    style.opacity,
                    style.opacityFloor
                );

            style.globalCoverage =
                1.0f;

            style.density =
                1.0f;
        }

        style.density =
            std::clamp(
                layer.density,
                0.05f,
                1.0f
            );

        style.cloudColor =
            layer.appearance.baseColor;

        style.shadowColor =
            layer.appearance.shadowColor;

        style.generation =
            clouds.generation;

        style.circulation =
            clouds.circulation;

        style.layers.clear();
        style.layers.push_back(
            layer
        );

        /*
            Разные физические типы облаков должны иметь
            разные характеристики формы.
        */
        const std::string normalizedType =
            normalizeCloudToken(
                layer.type
            );

        const bool isHighIce =
            normalizedType.find("ice") !=
                std::string::npos ||
            normalizedType.find("cirrus") !=
                std::string::npos;

        const bool isHaze =
            normalizedType.find("haze") !=
                std::string::npos ||
            normalizedType.find("aerosol") !=
                std::string::npos;

        if (isHighIce)
        {
            // Мелкие, тонкие, вытянутые, перистые структуры.
            style.generation.massScale *=
                1.55f;

            style.generation.detailScale *=
                1.65f;

            style.generation.edgeFragmentScale *=
                1.45f;

            style.generation.edgeFragmentStrength *=
                1.20f;

            style.generation.softEdgeWidth *=
                1.45f;

            style.generation.worleyErosionStrength *=
                0.72f;

            style.opacity *=
                0.72f;

            style.cloudColor =
                glm::mix(
                    style.cloudColor,
                    glm::vec3(
                        0.92f,
                        0.94f,
                        0.97f
                    ),
                    0.35f
                );
        }
        else if (isHaze)
        {
            // Почти сплошная мягкая аэрозольная оболочка.
            style.generation.massScale *=
                0.70f;

            style.generation.detailScale *=
                0.62f;

            style.generation.edgeFragmentStrength *=
                0.35f;

            style.generation.worleyErosionStrength *=
                0.30f;

            style.generation.softEdgeWidth *=
                1.80f;

            style.opacity *=
                0.78f;
        }
        else
        {
            // Низкие/средние водяные облака:
            // крупные плотные массы и рваные края.
            style.generation.massScale *=
                std::max(
                    0.65f,
                    layer.particleScale
                );

            style.generation.worleyErosionStrength *=
                1.10f;

            style.generation.edgeFragmentStrength *=
                1.08f;
        }




        if (venusLike)
        {
            style.pattern =
                render::celestial::ProceduralCloudPattern::DenseOvercast;

            style.generation.weatherScale *=
                0.42f;

            style.generation.massScale *=
                0.34f;

            style.generation.detailScale *=
                0.52f;

            style.generation.domainWarpStrength *=
                0.25f;

            style.generation.worleyErosionStrength *=
                0.18f;

            style.generation.edgeFragmentStrength *=
                0.12f;

            style.generation.detachedFragmentProbability =
                0.0f;

            style.generation.softEdgeWidth =
                std::max(
                    style.generation.softEdgeWidth,
                    0.24f
                );

            style.globalCoverage =
                std::clamp(
                    std::max(
                        style.globalCoverage,
                        0.76f
                    ),
                    0.01f,
                    0.96f
                );

            style.opacity *=
                0.62f;

            style.cloudColor =
                glm::mix(
                    style.cloudColor,
                    glm::vec3(
                        0.93f,
                        0.86f,
                        0.70f
                    ),
                    0.65f
                );
        }
        else if (gasGiantLike)
        {
            style.opacity *=
                0.84f;

            style.generation.domainWarpStrength *=
                0.55f;

            style.generation.edgeFragmentStrength *=
                0.45f;

            style.generation.worleyErosionStrength *=
                0.55f;

            style.generation.softEdgeWidth *=
                1.35f;
        }







        style.coverageThreshold =
            std::clamp(
                1.0f -
                    style.globalCoverage,
                0.05f,
                0.97f
            );

        style.edgeSharpness =
            std::clamp(
                1.0f -
                    style.generation.softEdgeWidth,
                0.0f,
                1.0f
            );

        style.detailStrength =
            style.generation.
                worleyErosionStrength;

        style.macroScale =
            style.generation.weatherScale;

        style.cellScale =
            style.generation.massScale;

        style.detailScale =
            style.generation.detailScale;

        /*
            Собственный ветер каждого слоя.
        */
        const double meanWindMps =
            (
                static_cast<double>(
                    layer.wind.minimumSpeedMps
                ) +
                static_cast<double>(
                    layer.wind.maximumSpeedMps
                )
            ) * 0.5;

        const double directionRadians =
            glm::radians(
                static_cast<double>(
                    layer.wind.predominantDirectionDeg
                )
            );

        const double longitudinalWindMps =
            meanWindMps *
            std::sin(
                directionRadians
            );

        const double cloudRadiusMeters =
            std::max(
                1.0,
                planetRadiusMeters +
                    static_cast<double>(
                        layer.baseHeightKm
                    ) *
                    1000.0
            );

        const double physicalUvSpeed =
            longitudinalWindMps /
            (
                glm::two_pi<double>() *
                cloudRadiusMeters
            );

        const double windScale =
            cloudWindTimeScale();

        style.windSpeedMps =
            static_cast<float>(
                meanWindMps
            );

        style.windDirectionDeg =
            layer.wind.predominantDirectionDeg;

        style.driftSpeed =
            static_cast<float>(
                physicalUvSpeed *
                windScale
            );

        std::cout
            << "[CloudLayerStyle]"
            << " body=" << bodyId
            << " layer=" << style.layerId
            << " type=" << style.layerType
            << " heightKm="
            << style.baseHeightKm
            << ".."
            << style.topHeightKm
            << " coverage="
            << style.globalCoverage
            << " opacity="
            << style.opacity
            << " windMps="
            << style.windSpeedMps
            << " directionDeg="
            << style.windDirectionDeg
            << " driftUvPerSecond="
            << style.driftSpeed
            << "\n";

        result.push_back(
            std::move(style)
        );
    }

    /*
        Рисуем от нижних слоёв к верхним.
    */
    std::sort(
        result.begin(),
        result.end(),
        [](
            const auto& left,
            const auto& right
        )
        {
            return
                left.baseHeightKm <
                right.baseHeightKm;
        }
    );

    return result;
}





SystemMapRenderer::HubPlanetAtmosphereStyle
SystemMapRenderer::atmosphereStyleForBody(
    int systemId,
    const std::string& bodyId,
    const std::string& displayName,
    const std::string& environmentPresetId
) const
{
    HubPlanetAtmosphereStyle style;

    const auto profile =
        resolvedEnvironmentProfileForBody(
            systemId,
            bodyId,
            displayName,
            environmentPresetId
        );

    if (!profile.found ||
        !profile.atmosphere.enabled)
    {
        style.enabled = false;
        return style;
    }

    const auto& atmosphere = profile.atmosphere;

    style.enabled = true;

    style.visualIntensity =
        atmosphere.visualIntensity;

    style.radiusScale =
        atmosphere.radiusScale;

    style.surfaceHaze =
        atmosphere.surfaceHaze;

    style.limbCore =
        atmosphere.limbCore;

    style.nearAtmosphere =
        atmosphere.nearAtmosphere;

    style.outerAtmosphere =
        atmosphere.outerAtmosphere;

    const std::string kind =
        atmosphere.kind;

    // Это пока цвет базовой кинематографической массы планеты
    // в Hub backdrop. Позже лучше вынести в отдельный surface/backdrop profile.
    if (kind.find("dust") != std::string::npos ||
        kind.find("dry") != std::string::npos ||
        kind.find("mars") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.105f,
                0.050f,
                0.030f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.180f,
                0.080f,
                0.045f,
                0.96f
            );
    }
    else if (kind.find("sulfuric") != std::string::npos ||
             kind.find("venus") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.140f,
                0.105f,
                0.060f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.220f,
                0.170f,
                0.095f,
                0.96f
            );
    }
    else if (kind.find("orange") != std::string::npos ||
             kind.find("titan") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.080f,
                0.055f,
                0.035f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.155f,
                0.100f,
                0.055f,
                0.96f
            );
    }
    else if (kind.find("methane_deep_blue") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.015f,
                0.050f,
                0.145f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.035f,
                0.110f,
                0.260f,
                0.96f
            );
    }
    else if (kind.find("methane_cyan") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.035f,
                0.105f,
                0.120f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.080f,
                0.210f,
                0.230f,
                0.96f
            );
    }
    else if (kind.find("hydrogen_helium") != std::string::npos)
    {
        style.oceanInner =
            glm::vec4(
                0.120f,
                0.090f,
                0.060f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.240f,
                0.180f,
                0.110f,
                0.96f
            );
    }
    else
    {
        style.oceanInner =
            glm::vec4(
                0.006f,
                0.035f,
                0.090f,
                0.96f
            );

        style.oceanOuter =
            glm::vec4(
                0.025f,
                0.095f,
                0.170f,
                0.96f
            );
    }

    return style;
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

    m_environmentVisualTimeSeconds = 0.0;
    m_environmentLastSourceTimeSeconds = 0.0;
    m_environmentLastWallClockSeconds = 0.0;
    m_environmentVisualTimeInitialized = false;
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

        // Стартовый cinematic-orbit вид:
        // станция/хаб на фоне горизонта планеты.
        m_hubCamera.yaw =
            0.60;

        m_hubCamera.pitch =
            0.18;

        m_hubCamera.zoom =
            1.0;

        m_hubCamera.pan =
            glm::dvec2(
                0.0,
                0.0
            );

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









void SystemMapRenderer::addSystemBodyMarker(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view,
    int segments
)
{
    if (radius <= 0.0f ||
        segments < 8)
    {
        return;
    }

    glm::vec3 right {
        view[0][0],
        view[1][0],
        view[2][0]
    };

    glm::vec3 up {
        view[0][1],
        view[1][1],
        view[2][1]
    };

    if (glm::length(right) <= 0.000001f ||
        glm::length(up) <= 0.000001f)
    {
        return;
    }

    right =
        glm::normalize(
            right
        );

    up =
        glm::normalize(
            up
        );

    const glm::vec4 ringColor(
        color.r,
        color.g,
        color.b,
        0.82f
    );

    const glm::vec4 crossColor(
        color.r,
        color.g,
        color.b,
        0.48f
    );

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            glm::two_pi<float>() *
            static_cast<float>(i) /
            static_cast<float>(segments);

        const float a1 =
            glm::two_pi<float>() *
            static_cast<float>(i + 1) /
            static_cast<float>(segments);

        const glm::vec3 p0 =
            center +
            (
                std::cos(a0) * right +
                std::sin(a0) * up
            ) * radius;

        const glm::vec3 p1 =
            center +
            (
                std::cos(a1) * right +
                std::sin(a1) * up
            ) * radius;

        addLine(
            p0,
            p1,
            ringColor
        );
    }

    const float crossSize =
        radius * 0.62f;

    addLine(
        center - right * crossSize,
        center + right * crossSize,
        crossColor
    );

    addLine(
        center - up * crossSize,
        center + up * crossSize,
        crossColor
    );
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

    m_proceduralCloudLayer.beginFrame();

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








GLuint SystemMapRenderer::globalAlbedoTextureForHubSnapshot(
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

    return globalAlbedoTextureForGeneratedAsset(
        *asset
    );
}










GLuint SystemMapRenderer::globalNormalTextureForGeneratedAsset(
    const world::celestial::visual::CelestialGeneratedAssetSet& asset
)
{
    if (asset.global.normalPath.empty())
        return 0;

    const std::string key =
        generatedAssetKey(
            asset
        );

    auto existing =
        m_globalNormalTextureByAssetKey.find(
            key
        );

    if (existing !=
        m_globalNormalTextureByAssetKey.end())
    {
        return existing->second;
    }

    const std::filesystem::path resolvedPath =
        resolveSystemMapAssetPath(
            asset.global.normalPath
        );

    const GLuint texture =
        TextureLoader::load2D(
            resolvedPath.generic_string(),
            false
        );

    m_globalNormalTextureByAssetKey[key] =
        texture;

    if (texture == 0)
    {
        std::cerr
            << "[SystemMapRenderer]"
            << " failed to load global normal texture for "
            << key
            << " path="
            << resolvedPath.generic_string()
            << "\n";
    }

    return texture;
}

GLuint SystemMapRenderer::globalNormalTextureForHubSnapshot(
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

    return globalNormalTextureForGeneratedAsset(
        *asset
    );
}












SystemMapRenderer::HubPlanetAtmosphereStyle
SystemMapRenderer::hubPlanetAtmosphereStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    return atmosphereStyleForBody(
        hub.systemId,
        hub.parentBodyId,
        hub.parentBodyId,
        hub.parentEnvironmentPresetId
    );
}




render::celestial::HubSphericalGridStyle
SystemMapRenderer::hubSphericalGridStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    render::celestial::HubSphericalGridStyle style;

    std::string bodyKey =
        normalizeGeneratedIdentityToken(
            lastGeneratedIdentityPathPart(
                hub.parentBodyId
            )
        );

    const auto* asset =
        generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (asset)
    {
        const std::string assetKey =
            normalizeGeneratedIdentityToken(
                asset->bodyFolderName
            );

        if (!assetKey.empty())
            bodyKey = assetKey;
    }

    // По умолчанию — холодная голубая сетка.
    style.radiusScale = 1.12;
    style.latitudeStepDeg = 10;
    style.longitudeStepDeg = 10;
    style.majorEvery = 3;
    style.samplesPerLine = 180;
    style.minorColor =
        glm::vec4(
            0.28f,
            0.66f,
            1.00f,
            0.055f
        );

    style.majorColor =
        glm::vec4(
            0.46f,
            0.82f,
            1.00f,
            0.105f
        );

    style.horizonFadeStart = 0.04f;
    style.horizonFadeEnd = 0.28f;

    if (bodyKey == "mars" ||
        bodyKey == "ares")
    {
        style.minorColor =
            glm::vec4(
                1.00f,
                0.56f,
                0.26f,
                0.07f
            );

        style.majorColor =
            glm::vec4(
                1.00f,
                0.72f,
                0.34f,
                0.13f
            );
    }
    else if (bodyKey == "venus")
    {
        style.minorColor =
            glm::vec4(
                1.00f,
                0.74f,
                0.34f,
                0.08f
            );

        style.majorColor =
            glm::vec4(
                1.00f,
                0.86f,
                0.48f,
                0.15f
            );
    }
    else if (bodyKey == "titan")
    {
        style.minorColor =
            glm::vec4(
                1.00f,
                0.62f,
                0.24f,
                0.07f
            );

        style.majorColor =
            glm::vec4(
                1.00f,
                0.76f,
                0.34f,
                0.13f
            );
    }

    return style;
}








std::vector<
    render::celestial::ProceduralCloudStyle
>
SystemMapRenderer::hubPlanetCloudStylesForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    auto styles =
        cloudStylesForBody(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId,
            hub.parentEnvironmentPresetId,
            hub.parentPlanetRadiusMeters,
            1536,
            768
        );

    /*
        Hub — режим просмотра огромной планеты.
        Здесь разрешаем усиленное preview-движение,
        но сохраняем относительные скорости слоёв.
    */
    for (auto& style : styles)
    {
        style.driftSpeed *=
            2.5f;
    }

    return styles;
}





std::vector<
    render::celestial::ProceduralCloudStyle
>
SystemMapRenderer::planetCloudStylesForPlanet(
    const world::celestial::PlanetMapSnapshot& planet
) const
{
    return cloudStylesForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId,
        planet.planetRadiusMeters,
        1024,
        512
    );
}





SystemMapRenderer::HubPlanetAtmosphereStyle
SystemMapRenderer::planetAtmosphereStyleForPlanet(
    const world::celestial::PlanetMapSnapshot& planet
) const
{
    return atmosphereStyleForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId
    );
}






render::celestial::rings::PlanetRingRenderContext
SystemMapRenderer::planetRingRenderContext(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    std::vector<
        world::celestial::SystemMapRing
    >& normalizedBands
) const
{
    using render::celestial::rings::
        PlanetRingRenderContext;

    PlanetRingRenderContext context;

    context.planetCenterPx =
        glm::dvec2(
            centerPx.x +
                activeDetailCamera().pan.x,
            centerPx.y +
                activeDetailCamera().pan.y
        );

    if (planet.planetRadiusMeters <= 1.0)
        return context;

    const double planetRadiusKm =
        planet.planetRadiusMeters /
        1000.0;

    normalizedBands.clear();

    normalizedBands.reserve(
        planet.rings.size()
    );

    /*
        Renderer ожидает радиусы в planet-radius units.
    */
    for (const auto& sourceBand :
         planet.rings)
    {
        if (sourceBand.outerRadiusKm <=
            sourceBand.innerRadiusKm)
        {
            continue;
        }

        auto band =
            sourceBand;

        band.innerRadiusKm /=
            planetRadiusKm;

        band.outerRadiusKm /=
            planetRadiusKm;

        normalizedBands.push_back(
            std::move(band)
        );
    }

    context.visual = planet.ringVisual;

    context.bands =
        &normalizedBands;

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

    /*
        Опциональный наклон плоскости колец
        вокруг prime axis.
    */
    const double inclination =
        glm::radians(
            planet.
                ringPlaneInclinationOffsetDeg
        );

    const glm::dvec3 tiltedEast =
        glm::normalize(
            east *
                std::cos(inclination) +
            north *
                std::sin(inclination)
        );

    const glm::dvec3 ringAxisXWorld =
        prime;

    const glm::dvec3 ringAxisYWorld =
        tiltedEast;

    const glm::dvec3 ringAxisXCamera =
        planetMapCameraSpaceRelative(
            ringAxisXWorld *
                planet.planetRadiusMeters
        );

    const glm::dvec3 ringAxisYCamera =
        planetMapCameraSpaceRelative(
            ringAxisYWorld *
                planet.planetRadiusMeters
        );

    const double finalScale =
        scale *
        activeDetailCamera().zoom;

    /*
        planetMapProject использует:
            screen.x += camera.x * finalScale
            screen.y -= camera.y * finalScale
    */
    context.ringAxisXPx =
        glm::dvec2(
            ringAxisXCamera.x *
                finalScale,
            -ringAxisXCamera.y *
                finalScale
        );

    context.ringAxisYPx =
        glm::dvec2(
            ringAxisYCamera.x *
                finalScale,
            -ringAxisYCamera.y *
                finalScale
        );

    /*
        Координаты shader-а выражены в planet radii,
        поэтому depth coefficients нормализуем обратно.
    */
    context.ringDepthCoefficients =
        glm::dvec2(
            ringAxisXCamera.z /
                planet.planetRadiusMeters,
            ringAxisYCamera.z /
                planet.planetRadiusMeters
        );

    context.planetRotationPhaseRad =
        planet.planetRotationPhaseRad;

    return context;
}






void SystemMapRenderer::drawPlanetEnvironmentLayers(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (!planet.valid ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const auto cloudStyles =
        planetCloudStylesForPlanet(
            planet
        );

    const HubPlanetAtmosphereStyle atmosphereStyle =
        planetAtmosphereStyleForPlanet(
            planet
        );

    /*
        Центр и радиус экранного диска Details.

        Эти вычисления должны совпадать с теми,
        которые используются старым atmosphere renderer.
    */
    const glm::dvec2 planetCenterPx(
        centerPx.x +
            activeDetailCamera().pan.x,
        centerPx.y +
            activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    /*
        Сначала рисуем облачные оболочки.
    */
    for (const auto& cloudStyle : cloudStyles)
    {
        if (!cloudStyle.enabled)
            continue;

        drawPlanetAnimatedCloudLayers(
            planet,
            scale,
            centerPx,
            cloudStyle
        );
    }

    /*
        Затем один GPU-pass рисует:

        - внутреннюю дымку;
        - атмосферный лимб;
        - внешнее свечение.

        Функция больше не является специфичной для Hub:
        она работает с любым экранным центром и радиусом.
    */
    if (atmosphereStyle.enabled)
    {
        drawHubMapPlanetAtmosphereStack(
            planetCenterPx,
            planetRadiusPx,
            atmosphereStyle
        );
    }
}










void SystemMapRenderer::drawPlanetAtmosphereInterior(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const HubPlanetAtmosphereStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const glm::dvec2 planetCenterPx(
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    glm::vec4 haze =
        style.surfaceHaze;

    haze.a *=
        std::max(
            0.0f,
            style.visualIntensity
        );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        haze,
        0.58,
        0.86,
        1.002,
        24,
        256
    );
}









void SystemMapRenderer::drawPlanetAtmosphereLimb(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const HubPlanetAtmosphereStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const glm::dvec2 planetCenterPx(
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const float intensity =
        std::max(
            0.0f,
            style.visualIntensity
        );

    glm::vec4 limbCore =
        style.limbCore;

    glm::vec4 nearAtmosphere =
        style.nearAtmosphere;

    glm::vec4 outerAtmosphere =
        style.outerAtmosphere;

    limbCore.a *= intensity;
    nearAtmosphere.a *= intensity;
    outerAtmosphere.a *= intensity;

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        limbCore,
        0.986,
        1.001,
        1.020,
        14,
        256
    );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        nearAtmosphere,
        0.998,
        1.018,
        static_cast<double>(style.radiusScale) + 0.025,
        20,
        256
    );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        outerAtmosphere,
        1.012,
        static_cast<double>(style.radiusScale) + 0.020,
        static_cast<double>(style.radiusScale) + 0.075,
        26,
        256
    );
}








void SystemMapRenderer::drawPlanetAnimatedCloudLayers(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const render::celestial::ProceduralCloudStyle& style
)
{
    const double renderTimeSeconds =
        environmentVisualTimeSeconds(
            planet.universeTimeSeconds
        );


    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const double meanHeightMeters =
        (
            static_cast<double>(
                style.baseHeightKm
            ) +
            static_cast<double>(
                style.topHeightKm
            )
        ) *
        500.0;

    const double physicalRadiusScale =
        1.0 +
        meanHeightMeters /
            planet.planetRadiusMeters;

    /*
        Минимальный визуальный разнос нужен, иначе различие
        в несколько километров на планете радиусом 6371 км
        практически исчезает и появляется z-overlap.
    */
    const double visualLayerOffset =
        0.0015 *
        static_cast<double>(
            1 +
            (
                &style -
                &style
            )
        );

    const double cloudRadiusScale =
        std::clamp(
            physicalRadiusScale +
                visualLayerOffset,
            1.0015,
            1.045
        );

    drawPlanetProceduralCloudGlobeLayer(
        planet,
        scale,
        centerPx,
        cloudRadiusScale,
        0.0,
        renderTimeSeconds,
        style
    );
}






void SystemMapRenderer::drawPlanetProceduralCloudGlobeLayer(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double cloudRadiusScale,
    double longitudeOffset,
    double timeSeconds,
    const render::celestial::ProceduralCloudStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const GLuint texture =
        m_proceduralCloudLayer.textureForStyle(
            style,
            timeSeconds
        );

    if (texture == 0)
        return;





    static GLuint lastLoggedDetailsTexture = 0;

    if (lastLoggedDetailsTexture != texture)
    {
        lastLoggedDetailsTexture = texture;

        std::cout
            << "[PlanetDetailsCloudRenderer]"
            << " texture=" << texture
            << " enabled=" << style.enabled
            << " opacity=" << style.opacity
            << " driftSpeed=" << style.driftSpeed
            << " radiusScale=" << cloudRadiusScale
            << "\n";
    }



    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                        std::max(
                            0.000001,
                            edge1 - edge0
                        ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    const double radiusMeters =
        planet.planetRadiusMeters *
        cloudRadiusScale;

    const double driftU =
        std::fmod(
            timeSeconds *
                static_cast<double>(style.driftSpeed),
            1.0
        );

    constexpr int latSegments = 56;
    constexpr int lonSegments = 112;

    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    GLint oldTextureBinding = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    GLint oldTextureEnvMode =
        GL_MODULATE;

    glGetTexEnviv(
        GL_TEXTURE_ENV,
        GL_TEXTURE_ENV_MODE,
        &oldTextureEnvMode
    );

    glUseProgram(0);

    glDisable(
        GL_DEPTH_TEST
    );

    glEnable(
        GL_TEXTURE_2D
    );

    glBindTexture(
        GL_TEXTURE_2D,
        texture
    );

    glTexEnvi(
        GL_TEXTURE_ENV,
        GL_TEXTURE_ENV_MODE,
        GL_MODULATE
    );

    glEnable(
        GL_BLEND
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    auto emit =
        [&](double lat, double lon)
        {
            const glm::dvec3 world =
                planetSurfacePointMeters(
                    planet,
                    lat,
                    lon,
                    cloudRadiusScale
                );

            const glm::dvec3 relative =
                world -
                planet.planetCenterMeters;

            const glm::dvec3 cameraSpace =
                planetMapCameraSpaceRelative(
                    relative
                );

            const double front =
                std::clamp(
                    cameraSpace.z /
                        std::max(
                            1.0,
                            radiusMeters
                        ),
                    0.0,
                    1.0
                );

            const double horizonFade =
                smoothStep(
                    0.035,
                    0.30,
                    front
                );


            const double normalizedLatitude =
                std::abs(
                    lat /
                    glm::half_pi<double>()
                );

            const double polarGeometryFade =
                1.0 -
                smoothStep(
                    0.82,
                    0.995,
                    normalizedLatitude
                );


            



            const glm::dvec2 screen =
                planetMapProject(
                    world,
                    planet,
                    scale,
                    centerPx
                );

            const double u =
                0.5 +
                lon / glm::two_pi<double>() +
                longitudeOffset +
                driftU;

            const double v =
                std::clamp(
                    0.5 -
                        lat / glm::pi<double>(),
                    0.0,
                    1.0
                );

            const float renderOpacity =
                std::clamp(
                    style.opacity,
                    0.0f,
                    0.92f
                );

            const float alpha =
                static_cast<float>(
                    renderOpacity *
                    horizonFade *   
                    polarGeometryFade
                );


            glColor4f(
                1.0f,
                1.0f,
                1.0f,
                alpha
            );

            glTexCoord2d(
                u,
                v
            );

            glVertex2d(
                screen.x,
                screen.y
            );
        };

    glBegin(
        GL_TRIANGLES
    );

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
                    lonMid,
                    cloudRadiusScale
                );

            const glm::dvec3 midRelative =
                midWorld -
                planet.planetCenterMeters;

            const glm::dvec3 midCamera =
                planetMapCameraSpaceRelative(
                    midRelative
                );

            // +Z — видимая полусфера.
            if (midCamera.z < 0.0)
                continue;

            emit(
                lat0,
                lon0
            );

            emit(
                lat0,
                lon1
            );

            emit(
                lat1,
                lon1
            );

            emit(
                lat0,
                lon0
            );

            emit(
                lat1,
                lon1
            );

            emit(
                lat1,
                lon0
            );
        }
    }

    glEnd();

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(oldTextureBinding)
    );

    glTexEnvi(
        GL_TEXTURE_ENV,
        GL_TEXTURE_ENV_MODE,
        oldTextureEnvMode
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}























GLuint SystemMapRenderer::globalCloudsTextureForGeneratedAsset(
    const world::celestial::visual::CelestialGeneratedAssetSet& asset
)
{
    std::string cloudsPath;

    if (!asset.global.cloudsPath.empty())
    {
        cloudsPath =
            asset.global.cloudsPath;
    }
    else if (!asset.base.cloudsPath.empty())
    {
        cloudsPath =
            asset.base.cloudsPath;
    }

    if (cloudsPath.empty())
        return 0;

    const std::string key =
        generatedAssetKey(asset);

    auto existing =
        m_globalCloudsTextureByAssetKey.find(key);

    if (existing != m_globalCloudsTextureByAssetKey.end())
        return existing->second;

    const std::filesystem::path resolvedPath =
        resolveSystemMapAssetPath(
            cloudsPath
        );

    const GLuint tex =
        TextureLoader::load2D(
            resolvedPath.generic_string(),
            false
        );

    m_globalCloudsTextureByAssetKey[key] =
        tex;

    if (tex == 0)
    {
        std::cerr
            << "[SystemMapRenderer] failed to load global clouds texture for "
            << key
            << " path="
            << resolvedPath.generic_string()
            << "\n";
    }

    return tex;
}

GLuint SystemMapRenderer::globalCloudsTextureForHubSnapshot(
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

    return globalCloudsTextureForGeneratedAsset(
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









SystemMapRenderer::SystemBodyVisualMetrics
SystemMapRenderer::computeSystemBodyVisualMetrics(
    const world::celestial::SystemMapBody& body,
    float physicalRadiusWorld,
    double worldUnitsPerPixel
) const
{
    using world::celestial::BodyType;

    SystemBodyVisualMetrics out;

    out.physicalRadiusWorld =
        std::max(
            0.0f,
            physicalRadiusWorld
        );

    if (worldUnitsPerPixel > 0.0 &&
        std::isfinite(worldUnitsPerPixel))
    {
        out.physicalRadiusPx =
            static_cast<float>(
                static_cast<double>(out.physicalRadiusWorld) /
                worldUnitsPerPixel
            );
    }

    out.drawPhysicalBody =
        out.physicalRadiusWorld > 0.0f &&
        out.physicalRadiusPx >= m_systemControls.minPhysicalBodyRadiusPx;

    float desiredMarkerRadiusPx =
        0.0f;

    if (body.type == BodyType::Star)
    {
        if (out.physicalRadiusPx < m_systemControls.starMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.starMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Planet)
    {
        if (out.physicalRadiusPx < m_systemControls.planetMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.planetMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Moon)
    {
        if (out.physicalRadiusPx < m_systemControls.tinyMoonProxyRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.tinyMoonProxyRadiusPx;
        }
    }

    if (desiredMarkerRadiusPx > 0.0f &&
        worldUnitsPerPixel > 0.0 &&
        std::isfinite(worldUnitsPerPixel))
    {
        out.drawMarker = true;

        out.markerRadiusPx =
            desiredMarkerRadiusPx;

        out.markerRadiusWorld =
            static_cast<float>(
                worldUnitsPerPixel *
                static_cast<double>(desiredMarkerRadiusPx)
            );
    }

    const float visibleRadiusPx =
        std::max(
            out.physicalRadiusPx,
            out.markerRadiusPx
        );

    out.pickRadiusPx =
        std::max(
            visibleRadiusPx,
            m_systemControls.pickMinBodyRadiusPx
        );

    return out;
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
                camera.yaw +=
                    dx *
                    controls.rotateSensitivity *
                    0.65;

                camera.pitch +=
                    dy *
                    controls.rotateSensitivity *
                    0.65;

                camera.yaw =
                    wrapAngleRadD(
                        camera.yaw
                    );

                // 0.12: низкий orbital horizon view.
                // 1.20: почти взгляд вниз.
                // Отрицательный pitch запрещён, иначе визуально камера
                // оказывается "изнутри планеты".
                camera.pitch =
                    std::clamp(
                        camera.pitch,
                        0.12,
                        1.20
                    );
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

        if (m_mode == Mode::Hub)
{
    camera.pitch =
        std::clamp(
            camera.pitch,
            -0.95,
            0.95
        );

    const double maxPanX =
        static_cast<double>(vp.width) *
        0.55;

    const double maxPanY =
        static_cast<double>(vp.height) *
        0.45;

    camera.pan.x =
        std::clamp(
            camera.pan.x,
            -maxPanX,
            maxPanX
        );

    camera.pan.y =
        std::clamp(
            camera.pan.y,
            -maxPanY,
            maxPanY
        );
}

        if (m_mode == Mode::Hub)
        {
            camera.pitch =
                std::clamp(
                    camera.pitch,
                    0.12,
                    1.20
                );
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

        
        auto radiusIt =
            drawRadiusById.find(
                b.id
            );

        const float physicalRadiusWorld =
            radiusIt != drawRadiusById.end()
                ? radiusIt->second
                : 0.0f;

        const SystemBodyVisualMetrics labelMetrics =
            computeSystemBodyVisualMetrics(
                b,
                physicalRadiusWorld,
                worldUnitsPerPixel
            );

        const bool selected =
            b.id == m_selectedBodyId;

        const float screenRadiusPx =
            std::max(
                labelMetrics.physicalRadiusPx,
                labelMetrics.markerRadiusPx
            );








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






double SystemMapRenderer::environmentVisualTimeSeconds(
    double sourceTimeSeconds
)
{
    using Clock =
        std::chrono::steady_clock;

    const double nowSeconds =
        std::chrono::duration<double>(
            Clock::now().time_since_epoch()
        ).count();

    if (!m_environmentVisualTimeInitialized)
    {
        m_environmentVisualTimeSeconds =
            sourceTimeSeconds;

        m_environmentLastSourceTimeSeconds =
            sourceTimeSeconds;

        m_environmentLastWallClockSeconds =
            nowSeconds;

        m_environmentVisualTimeInitialized =
            true;

        return sourceTimeSeconds;
    }

    const double wallDt =
        std::clamp(
            nowSeconds -
                m_environmentLastWallClockSeconds,
            0.0,
            0.05
        );

    m_environmentLastWallClockSeconds =
        nowSeconds;

    if (sourceTimeSeconds <
        m_environmentLastSourceTimeSeconds - 0.001)
    {
        m_environmentVisualTimeSeconds =
            sourceTimeSeconds;

        m_environmentLastSourceTimeSeconds =
            sourceTimeSeconds;

        return sourceTimeSeconds;
    }

    m_environmentLastSourceTimeSeconds =
        sourceTimeSeconds;

    // Плавно двигаем визуальное время локально.
    m_environmentVisualTimeSeconds +=
        wallDt;

    // Если сильно отстали — мягко подтягиваемся.
    const double maxLagSeconds =
        0.35;

    if (m_environmentVisualTimeSeconds <
        sourceTimeSeconds - maxLagSeconds)
    {
        m_environmentVisualTimeSeconds =
            sourceTimeSeconds - maxLagSeconds;
    }

    // Не даём времени убежать слишком далеко вперёд.
    const double maxLeadSeconds =
        0.10;

    if (m_environmentVisualTimeSeconds >
        sourceTimeSeconds + maxLeadSeconds)
    {
        m_environmentVisualTimeSeconds =
            sourceTimeSeconds + maxLeadSeconds;
    }

    return m_environmentVisualTimeSeconds;
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




    std::unordered_map<
        std::string,
        float
    > selectionRadiusById;






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


        
       






        
        



       const SystemBodyVisualMetrics bodyMetrics =
    computeSystemBodyVisualMetrics(
        b,
        r,
        systemWorldUnitsPerPixel
    );

const float selectionRadiusWorld =
    std::max(
        bodyMetrics.physicalRadiusWorld,
        static_cast<float>(
            systemWorldUnitsPerPixel *
            static_cast<double>(
                bodyMetrics.pickRadiusPx
            )
        )
    );

selectionRadiusById[b.id] =
    selectionRadiusWorld;

if (b.type == BodyType::Planet ||
    b.type == BodyType::Moon)
{
    BodyScreenPoint bp;

    bp.bodyId = b.id;
    bp.name = b.name;

    // Это уже не "физический радиус диска".
    // Это интерактивный радиус выбора.
    bp.screenRadiusPx =
        bodyMetrics.pickRadiusPx;

    bp.screen =
        projectToScreen(
            p,
            mvp,
            vp,
            bp.visible,
            bp.depth
        );

    m_lastSystemBodyScreenPoints.push_back(
        bp
    );
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

    addCircleXZ(
        center,
        beltR - 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    addCircleXZ(
        center,
        beltR,
        {0.65f, 0.68f, 0.72f, 0.24f},
        160
    );

    addCircleXZ(
        center,
        beltR + 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    continue;
}

if (bodyMetrics.drawPhysicalBody)
{
    GLuint bodyAlbedoTexture =
        0;

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
            bodyMetrics.physicalRadiusWorld > 0.16f;

        const int latSeg =
            largeBody ? 64 : 24;

        const int lonSeg =
            largeBody ? 128 : 48;

        addTexturedSystemBodySphere(
            b,
            bodyAlbedoTexture,
            p,
            bodyMetrics.physicalRadiusWorld,
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            latSeg,
            lonSeg
        );
    }
    else
    {
        // Fallback для звёзд и тел без generated-текстуры.
        // Радиус здесь физический, не proxy.
        addBillboardBall(
            p,
            bodyMetrics.physicalRadiusWorld,
            c,
            view,
            32
        );
    }
}

if (bodyMetrics.drawMarker)
{
    glm::vec4 markerColor =
        c;

    if (b.type == BodyType::Moon)
    {
        markerColor =
            glm::vec4(
                0.72f,
                0.78f,
                0.86f,
                0.90f
            );
    }
    else if (b.type == BodyType::Planet)
    {
        markerColor =
            glm::vec4(
                0.48f,
                0.76f,
                1.00f,
                0.90f
            );
    }
    else if (b.type == BodyType::Star)
    {
        markerColor =
            glm::vec4(
                1.0f,
                0.86f,
                0.36f,
                0.90f
            );
    }

    addSystemBodyMarker(
        p,
        bodyMetrics.markerRadiusWorld,
        markerColor,
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
            selectionRadiusById.find(m_selectedBodyId);

        if (posIt != posById.end() &&
                radiusIt != selectionRadiusById.end())
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
    ensureEnvironmentProfiles();

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


    drawMapStarfield(
        viewport,
        planet.systemPositionLy
    );



    beginEnvironmentRenderSessionIfNeeded(
        Mode::Planet,
        planet.systemId,
        planet.planetBodyId
    );

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



    std::vector<
        world::celestial::SystemMapRing
    > normalizedRingBands;

    const auto ringContext =
        planetRingRenderContext(
            planet,
            scale,
            centerPx,
            normalizedRingBands
        );

    



        const auto environmentProfile =
    resolvedEnvironmentProfileForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId
    );

const bool hidePhysicalSurface =
    environmentProfile.found &&
    (
        environmentProfile.rendering.surfaceVisibility ==
            "hidden" ||
        !environmentProfile.rendering.loadSurfaceTextures
    );






m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Back
);



const bool shapeModelDrawn =
    !hidePhysicalSurface &&
    drawPlanetShapeModelDetail(
        planet,
        scale,
        centerPx
    );



   

// ------------------------------------------------------------
// 1. Базовая поверхность планеты.
// Если shape model не нарисован, рисуем fallback disk + textured globe.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
    /*
        Даже при скрытой поверхности оставляем тёмный
        непрозрачный fallback disk под primary cloud deck.

        Он нужен только как страховка от щелей на краю.
    */
    drawPlanetFilledDisk(
        planet,
        scale,
        centerPx
    );

    if (!hidePhysicalSurface)
    {
        drawPlanetTexturedGlobe(
            planet,
            scale,
            centerPx
        );
    }
}

// ------------------------------------------------------------
// 2. Environment layer.
// Для Planet Details НЕ используем Hub screen-space clouds.
// Здесь должна работать отдельная spherical Details-цепочка:
// drawPlanetEnvironmentLayers()
//     -> drawPlanetAtmosphereInterior()
//     -> drawPlanetAnimatedCloudLayers()
//         -> drawPlanetProceduralCloudGlobeLayer()
//     -> drawPlanetAtmosphereLimb()
// ------------------------------------------------------------
drawPlanetEnvironmentLayers(
    planet,
    scale,
    centerPx
);



m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Front
);





// ------------------------------------------------------------
// 3. Сетка/ориентационный оверлей поверх поверхности,
// облаков и атмосферы.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
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










void SystemMapRenderer::drawHubMapScreenMarker(
    const glm::dvec2& screenPx,
    double radiusPx,
    const glm::vec4& color,
    bool drawCross,
    int segments
)
{
    if (radiusPx <= 0.5)
        return;

    segments =
        std::max(
            12,
            segments
        );

    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    drawPlanetMapCircle(
        screenPx,
        radiusPx,
        segments
    );

    if (!drawCross)
        return;

    const double s =
        radiusPx * 0.62;

    glBegin(GL_LINES);

    glVertex2d(
        screenPx.x - s,
        screenPx.y
    );

    glVertex2d(
        screenPx.x + s,
        screenPx.y
    );

    glVertex2d(
        screenPx.x,
        screenPx.y - s
    );

    glVertex2d(
        screenPx.x,
        screenPx.y + s
    );

    glEnd();
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




void SystemMapRenderer::drawHubMapPlanetHorizonBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& innerColor,
    const glm::vec4& outerColor,
    double innerRadiusFactor,
    double outerRadiusFactor,
    int segments
)
{
    if (planetRadiusPx <= 1.0)
        return;

    segments =
        std::max(
            48,
            segments
        );

    const double innerR =
        planetRadiusPx *
        innerRadiusFactor;

    const double outerR =
        planetRadiusPx *
        outerRadiusFactor;

    glBegin(GL_TRIANGLE_STRIP);

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

        glColor4f(
            innerColor.r,
            innerColor.g,
            innerColor.b,
            innerColor.a
        );

        glVertex2d(
            planetCenterPx.x + ca * innerR,
            planetCenterPx.y + sa * innerR
        );

        glColor4f(
            outerColor.r,
            outerColor.g,
            outerColor.b,
            outerColor.a
        );

        glVertex2d(
            planetCenterPx.x + ca * outerR,
            planetCenterPx.y + sa * outerR
        );
    }

    glEnd();
}










void SystemMapRenderer::drawHubMapPlanetSoftBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& peakColor,
    double startRadiusFactor,
    double peakRadiusFactor,
    double endRadiusFactor,
    int radialSteps,
    int segments
)
{
    if (planetRadiusPx <= 1.0)
        return;

    if (endRadiusFactor <= startRadiusFactor)
        return;

    if (peakColor.a <= 0.0001f)
        return;

    radialSteps =
        std::max(
            8,
            radialSteps
        );

    segments =
        std::max(
            96,
            segments
        );

    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    GLboolean depthMaskWasEnabled =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldTextureBinding = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(
        0
    );

    // ВАЖНО:
    // Atmosphere band — это чистая цветная геометрия.
    // Если оставить GL_TEXTURE_2D включённым, fixed pipeline будет
    // умножать цвет на текущую текстуру и текущие texture coords.
    // В результате band может стать полностью невидимым.
    glDisable(
        GL_TEXTURE_2D
    );

    glBindTexture(
        GL_TEXTURE_2D,
        0
    );

    glEnable(
        GL_BLEND
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_FALSE
    );

    const double startR =
        planetRadiusPx *
        startRadiusFactor;

    const double peakR =
        planetRadiusPx *
        peakRadiusFactor;

    const double endR =
        planetRadiusPx *
        endRadiusFactor;

    const double totalSpan =
        std::max(
            0.000001,
            endR - startR
        );

    const double peakT =
        std::clamp(
            (peakR - startR) / totalSpan,
            0.0,
            1.0
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    auto alphaAt =
        [&](double t) -> float
        {
            double a = 0.0;

            if (t <= peakT)
            {
                a =
                    smoothStep(
                        0.0,
                        std::max(
                            0.000001,
                            peakT
                        ),
                        t
                    );
            }
            else
            {
                a =
                    1.0 -
                    smoothStep(
                        peakT,
                        1.0,
                        t
                    );
            }

            return static_cast<float>(
                a * peakColor.a
            );
        };

    for (int ring = 0; ring < radialSteps; ++ring)
    {
        const double t0 =
            static_cast<double>(ring) /
            static_cast<double>(radialSteps);

        const double t1 =
            static_cast<double>(ring + 1) /
            static_cast<double>(radialSteps);

        const double r0 =
            startR +
            (endR - startR) * t0;

        const double r1 =
            startR +
            (endR - startR) * t1;

        const float a0 =
            alphaAt(
                t0
            );

        const float a1 =
            alphaAt(
                t1
            );

        glBegin(
            GL_TRIANGLE_STRIP
        );

        for (int i = 0; i <= segments; ++i)
        {
            const double ang =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double ca =
                std::cos(
                    ang
                );

            const double sa =
                std::sin(
                    ang
                );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a0
            );

            glVertex2d(
                planetCenterPx.x + ca * r0,
                planetCenterPx.y + sa * r0
            );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a1
            );

            glVertex2d(
                planetCenterPx.x + ca * r1,
                planetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }

    glDepthMask(
        depthMaskWasEnabled
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(oldTextureBinding)
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}






void SystemMapRenderer::drawHubMapPlanetAtmosphereStack(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const HubPlanetAtmosphereStyle& style
)
{
    if (!style.enabled ||
        planetRadiusPx <= 1.0)
    {
        return;
    }

    static GLuint atmosphereShader = 0;
    static GLuint fullscreenVao = 0;

    static GLint viewportOriginLocation = -1;
    static GLint viewportSizeLocation = -1;
    static GLint planetCenterLocation = -1;
    static GLint planetRadiusLocation = -1;

    static GLint radiusScaleLocation = -1;
    static GLint visualIntensityLocation = -1;

    static GLint surfaceHazeLocation = -1;
    static GLint limbCoreLocation = -1;
    static GLint nearAtmosphereLocation = -1;
    static GLint outerAtmosphereLocation = -1;

    if (atmosphereShader == 0)
    {
        atmosphereShader =
            ShaderLibrary::instance().get(
                "hub_planet_atmosphere"
            );

        if (atmosphereShader == 0)
        {
            static bool warned = false;

            if (!warned)
            {
                warned = true;

                std::cerr
                    << "[HubAtmosphere]"
                    << " shader not available"
                    << "\n";
            }

            return;
        }

        viewportOriginLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportOriginPx"
            );

        viewportSizeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportSize"
            );

        planetCenterLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetCenterPx"
            );

        planetRadiusLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetRadiusPx"
            );

        radiusScaleLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uRadiusScale"
            );

        visualIntensityLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uVisualIntensity"
            );

        surfaceHazeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uSurfaceHaze"
            );

        limbCoreLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uLimbCore"
            );

        nearAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uNearAtmosphere"
            );

        outerAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uOuterAtmosphere"
            );
    }

    if (fullscreenVao == 0)
    {
        glGenVertexArrays(
            1,
            &fullscreenVao
        );
    }

    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    GLint previousProgram = 0;
    GLint previousVao = 0;

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    const GLboolean scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );

    GLint previousBlendSourceRgb = 0;
    GLint previousBlendDestinationRgb = 0;
    GLint previousBlendSourceAlpha = 0;
    GLint previousBlendDestinationAlpha = 0;

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &previousBlendDestinationAlpha
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glDisable(
        GL_SCISSOR_TEST
    );

    glEnable(
        GL_BLEND
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glUseProgram(
        atmosphereShader
    );

    glUniform2f(
        viewportOriginLocation,
        static_cast<float>(viewport[0]),
        static_cast<float>(viewport[1])
    );

    glUniform2f(
        viewportSizeLocation,
        static_cast<float>(
            viewport[2]
        ),
        static_cast<float>(
            viewport[3]
        )
    );

    glUniform2f(
        planetCenterLocation,
        static_cast<float>(
            planetCenterPx.x
        ),
        static_cast<float>(
            planetCenterPx.y
        )
    );

    glUniform1f(
        planetRadiusLocation,
        static_cast<float>(
            planetRadiusPx
        )
    );

    glUniform1f(
        radiusScaleLocation,
        style.radiusScale
    );

    glUniform1f(
        visualIntensityLocation,
        style.visualIntensity
    );

    glUniform4f(
        surfaceHazeLocation,
        style.surfaceHaze.r,
        style.surfaceHaze.g,
        style.surfaceHaze.b,
        style.surfaceHaze.a
    );

    glUniform4f(
        limbCoreLocation,
        style.limbCore.r,
        style.limbCore.g,
        style.limbCore.b,
        style.limbCore.a
    );

    glUniform4f(
        nearAtmosphereLocation,
        style.nearAtmosphere.r,
        style.nearAtmosphere.g,
        style.nearAtmosphere.b,
        style.nearAtmosphere.a
    );

    glUniform4f(
        outerAtmosphereLocation,
        style.outerAtmosphere.r,
        style.outerAtmosphere.g,
        style.outerAtmosphere.b,
        style.outerAtmosphere.a
    );

    glBindVertexArray(
        fullscreenVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        3
    );

    glBindVertexArray(
        static_cast<GLuint>(
            previousVao
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            previousProgram
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            previousBlendDestinationAlpha
        )
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
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









void SystemMapRenderer::drawHubMapTexturedSphereDiskLayer(
    GLuint texture,
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& color,
    double uOffset,
    int gridX,
    int gridY
)
{
    if (texture == 0 ||
        radiusPx <= 1.0)
    {
        return;
    }

    gridX =
        std::max(
            96,
            gridX
        );

    gridY =
        std::max(
            64,
            gridY
        );

    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    const double viewW =
        static_cast<double>(
            std::max(
                viewport[2],
                1
            )
        );

    const double viewH =
        static_cast<double>(
            std::max(
                viewport[3],
                1
            )
        );

    GLboolean textureWasEnabled =
        glIsEnabled(GL_TEXTURE_2D);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    GLint oldTextureBinding =
        0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(
        GL_TEXTURE_2D,
        texture
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    auto wrap01 =
        [](double x) -> double
        {
            x =
                std::fmod(x, 1.0);

            if (x < 0.0)
                x += 1.0;

            return x;
        };

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(0.000001, edge1 - edge0),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    auto emit =
        [&](double sx, double sy)
        {
            const double nx =
                (sx - centerPx.x) /
                radiusPx;

            const double ny =
                -(sy - centerPx.y) /
                radiusPx;

            const double rr =
                nx * nx +
                ny * ny;

            if (rr > 1.0)
                return;

            // Видимая полусфера.
            const double nz =
                std::sqrt(
                    std::max(
                        0.0,
                        1.0 - rr
                    )
                );

            
                


            // ВАЖНО:
            // Это всё ещё сфера: screen point -> visible hemisphere -> UV.
            // Но для cinematic orbital view нужно брать больший angular field,
            // иначе один кусок суши раздувается на пол-экрана.
            constexpr double kLongitudeAngularScale =
                2.15;

            constexpr double kLatitudeAngularScale =
                1.08;

            const double lon =
                std::atan2(
                    nx * kLongitudeAngularScale,
                    std::max(
                        0.000001,
                        nz
                    )
                );

            const double lat =
                std::asin(
                    std::clamp(
                        ny * kLatitudeAngularScale,
                        -0.96,
                        0.96
                    )
                );

            double u =
                0.5 +
                lon / glm::two_pi<double>() +
                uOffset;

            double v =
                0.5 -
                lat / glm::pi<double>();

            u =
                wrap01(u);

            // Полюса всё ещё не пускаем в кадр слишком агрессивно,
            // но диапазон шире, чтобы не было размазанной локальной суши.
            v =
                std::clamp(
                    v,
                    0.12,
                    0.88
                );







            // К горизонту текстуру гасим.
            const double limb =
                std::sqrt(
                    std::max(
                        0.0,
                        1.0 - rr
                    )
                );

            const double textureFade =
                smoothStep(
                    0.18,
                    0.58,
                    limb
                );

            const float alpha =
                static_cast<float>(
                    color.a *
                    textureFade
                );

            glColor4f(
                color.r,
                color.g,
                color.b,
                alpha
            );

            glTexCoord2d(
                u,
                v
            );

            glVertex2d(
                sx,
                sy
            );
        };

    const double cellW =
        viewW /
        static_cast<double>(gridX);

    const double cellH =
        viewH /
        static_cast<double>(gridY);

    glBegin(GL_TRIANGLES);

    for (int iy = 0; iy < gridY; ++iy)
    {
        const double y0 =
            static_cast<double>(iy) *
            cellH;

        const double y1 =
            static_cast<double>(iy + 1) *
            cellH;

        for (int ix = 0; ix < gridX; ++ix)
        {
            const double x0 =
                static_cast<double>(ix) *
                cellW;

            const double x1 =
                static_cast<double>(ix + 1) *
                cellW;

            const double nx00 =
                (x0 - centerPx.x) /
                radiusPx;

            const double ny00 =
                -(y0 - centerPx.y) /
                radiusPx;

            const double nx10 =
                (x1 - centerPx.x) /
                radiusPx;

            const double ny10 =
                -(y0 - centerPx.y) /
                radiusPx;

            const double nx11 =
                (x1 - centerPx.x) /
                radiusPx;

            const double ny11 =
                -(y1 - centerPx.y) /
                radiusPx;

            const double nx01 =
                (x0 - centerPx.x) /
                radiusPx;

            const double ny01 =
                -(y1 - centerPx.y) /
                radiusPx;

            const bool q00 =
                nx00 * nx00 + ny00 * ny00 <= 1.0;

            const bool q10 =
                nx10 * nx10 + ny10 * ny10 <= 1.0;

            const bool q11 =
                nx11 * nx11 + ny11 * ny11 <= 1.0;

            const bool q01 =
                nx01 * nx01 + ny01 * ny01 <= 1.0;

            if (q00 && q10 && q11)
            {
                emit(x0, y0);
                emit(x1, y0);
                emit(x1, y1);
            }

            if (q00 && q11 && q01)
            {
                emit(x0, y0);
                emit(x1, y1);
                emit(x0, y1);
            }
        }
    }

    glEnd();

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(oldTextureBinding)
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
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


    beginEnvironmentRenderSessionIfNeeded(
        Mode::Hub,
        hub.systemId,
        hub.parentBodyId
    );



    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    const double viewW =
        static_cast<double>(
            std::max(
                viewport[2],
                1
            )
        );

    const double viewH =
        static_cast<double>(
            std::max(
                viewport[3],
                1
            )
        );

    const double maxDim =
        std::max(
            viewW,
            viewH
        );

    const double pitch =
        std::clamp(
            activeDetailCamera().pitch,
            0.12,
            1.20
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    const double lookDownT =
        smoothStep(
            0.12,
            1.20,
            pitch
        );

    // Оставляем текущую кинематографическую композицию:
    // при малом pitch виден горизонт,
    // при большом pitch уходим к взгляду вниз.
    const double horizonY =
        (1.0 - lookDownT) * (viewH * 0.74) +
        lookDownT * (-viewH * 0.38);

    const double visualRadiusPx =
        maxDim *
        (1.38 + 0.46 * lookDownT);

    const double horizonCenterY =
        horizonY + visualRadiusPx;

    const double nadirCenterY =
        viewH * 0.54;

    const glm::dvec2 visualPlanetCenterPx(
        viewW * 0.50 +
            activeDetailCamera().pan.x * 0.015,
        (1.0 - lookDownT) * horizonCenterY +
            lookDownT * nadirCenterY
    );

    m_lastHubPlanetVisualRadiusPx =
        visualRadiusPx;

    m_lastHubPlanetVisualCenterPx =
        visualPlanetCenterPx;


    const HubPlanetAtmosphereStyle atmosphereStyle =
    hubPlanetAtmosphereStyleForHub(
        hub
    );


    auto mixColor =
        [](const glm::vec4& a, const glm::vec4& b, float t) -> glm::vec4
        {
            return glm::vec4(
                a.r + (b.r - a.r) * t,
                a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t,
                a.a + (b.a - a.a) * t
            );
        };


   

    


    const GLuint albedoTexture =
        globalAlbedoTextureForHubSnapshot(
            hub
        );

    const GLuint normalTexture =
        globalNormalTextureForHubSnapshot(
            hub
        );

    const GLuint previewTexture =
        mapPreviewTextureForHubSnapshot(
            hub
        );

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    // -----------------------------------------------------------------
// 1. Базовое тело планеты.
// Гладкая океаническая масса без текстурной грязи.
//
// ВАЖНО:
// Внутри GL_TRIANGLE_STRIP на каждый угол должно быть ровно две вершины:
//   - внутренняя окружность r0
//   - внешняя окружность r1
//
// Если оставить третью вершину, появятся треугольные "зубья"
// по всей планете.
// -----------------------------------------------------------------
{
    constexpr int bands = 32;
    constexpr int segments = 256;

    for (int band = 0; band < bands; ++band)
    {
        const double t0 =
            static_cast<double>(band) /
            static_cast<double>(bands);

        const double t1 =
            static_cast<double>(band + 1) /
            static_cast<double>(bands);

        const double r0 =
            visualRadiusPx *
            t0;

        const double r1 =
            visualRadiusPx *
            t1;

        const float c0 =
            static_cast<float>(t0);

        const float c1 =
            static_cast<float>(t1);

        const glm::vec4 color0 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c0
            );

        const glm::vec4 color1 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c1
            );

        glBegin(GL_TRIANGLE_STRIP);

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

            // Вершина внутренней окружности.
            glColor4f(
                color0.r,
                color0.g,
                color0.b,
                color0.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r0,
                visualPlanetCenterPx.y + sa * r0
            );

            // Вершина внешней окружности.
            glColor4f(
                color1.r,
                color1.g,
                color1.b,
                color1.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r1,
                visualPlanetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }
}

    // -----------------------------------------------------------------
    // 2. Текстура поверхности.
    // ВРЕМЕННО ОТКЛЮЧЕНА.
    //
    // Причина:
    // equirectangular albedo сейчас натягивается на cinematic fake-hemisphere.
    // Для карты Хаба это даёт растянутую сушу и грязные пятна.
    // Пока оставляем чистую океаническую массу + haze + atmosphere.
    // -----------------------------------------------------------------
    const double planetLongitudeOffsetU =
        activeDetailCamera().yaw /
        glm::two_pi<double>() *
        0.20;



    const GLuint surfaceTexture =
        albedoTexture != 0
            ? albedoTexture
            : previewTexture;

    if (surfaceTexture != 0)
    {
        /*
            Пока это screen-space свет.

            Позже вместо фиксированного направления передадим
            реальное направление к звезде системы.
        */
        const glm::vec3 lightDirection =
            glm::normalize(
                glm::vec3(
                    -0.42f,
                    0.34f,
                    0.84f
                )
            );

        m_hubPlanetSurfaceRenderer.render(
            surfaceTexture,
            normalTexture,
            visualPlanetCenterPx,
            visualRadiusPx,
            planetLongitudeOffsetU,
            lightDirection
        );
    }
    

    const double renderTimeSeconds =
        environmentVisualTimeSeconds(
            hub.universeTimeSeconds
        );
    
    const auto cloudStyles =
        hubPlanetCloudStylesForHub(
            hub
        );

    for (std::size_t layerIndex = 0;
        layerIndex < cloudStyles.size();
        ++layerIndex)
    {
        const auto& cloudStyle =
            cloudStyles[layerIndex];

        if (!cloudStyle.enabled)
            continue;

        const double meanHeightMeters =
            (
                static_cast<double>(
                    cloudStyle.baseHeightKm
                ) +
                static_cast<double>(
                    cloudStyle.topHeightKm
                )
            ) *
            500.0;

        const double physicalRadiusScale =
            1.0 +
            meanHeightMeters /
                std::max(
                    1.0,
                    hub.parentPlanetRadiusMeters
                );

        /*
            На fake cinematic hemisphere физическая разница
            высот слишком мала, поэтому добавляем небольшой
            screen-space separation по индексу.
        */
        const double cloudRadiusScale =
            std::clamp(
                physicalRadiusScale +
                    0.0025 *
                    static_cast<double>(
                        layerIndex + 1
                    ),
                1.003,
                1.055
            );

        m_hubBackdropCloudRenderer.render(
            m_proceduralCloudLayer,
            visualPlanetCenterPx,
            visualRadiusPx,
            cloudRadiusScale,
            planetLongitudeOffsetU,
            renderTimeSeconds,
            cloudStyle
        );
    }





    drawHubMapPlanetAtmosphereStack(
        visualPlanetCenterPx,
        visualRadiusPx,
        atmosphereStyle
    );

    
    


    




    // -----------------------------------------------------------------
    // 6. Тактический оверлей оставляем.
    // -----------------------------------------------------------------
    const glm::dvec3 planetCenterLocal =
        hub.parentPlanetCenterLocalMeters;

    glColor4f(
        0.95f,
        0.82f,
        0.32f,
        0.12f
    );

    drawHubMapCircleLocalXY(
        planetCenterLocal,
        hub.hubOrbitRadiusMeters,
        scale,
        centerPx,
        256
    );

    const glm::dvec3 surfacePoint =
        planetCenterLocal +
        glm::dvec3(
            0.0,
            hub.parentPlanetRadiusMeters,
            0.0
        );

    glColor4f(
        0.70f,
        0.96f,
        1.0f,
        0.30f
    );

    drawPlanetMapCross(
        hubMapProject(
            surfacePoint,
            scale,
            centerPx
        ),
        5.0f
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
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

    GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    auto restoreGlState =
        [&]()
        {
            if (depthWasEnabled)
                glEnable(GL_DEPTH_TEST);
            else
                glDisable(GL_DEPTH_TEST);

            if (blendWasEnabled)
                glEnable(GL_BLEND);
            else
                glDisable(GL_BLEND);
        };

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

    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

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
    {
        restoreGlState();
        return;
    }


    drawMapStarfield(
        viewport,
        hub.systemPositionLy
    );


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
    const double finalScale =
        scale *
        activeDetailCamera().zoom;




   drawHubMapPlanetSurfaceHint(
        hub,
        scale,
        centerPx
    );

    // render::celestial::HubSphericalGridStyle sphericalGridStyle =
    // hubSphericalGridStyleForHub(
    //     hub
    // );



    // // Сетка Хаба должна быть не на поверхности планеты,
    // // а на визуальной высоте хаба.
    // //
    // // centerPx — это экранная позиция hub-local origin,
    // // то есть центр старой плоской сетки.
    // // Поэтому радиус сферической оболочки сетки должен проходить через centerPx.
    // const double hubGridShellRadiusPx =
    //     glm::length(
    //         centerPx -
    //         m_lastHubPlanetVisualCenterPx
    //     );

    // if (m_lastHubPlanetVisualRadiusPx > 1.0 &&
    //     hubGridShellRadiusPx > m_lastHubPlanetVisualRadiusPx)
    // {
    //     sphericalGridStyle.radiusScale =
    //         std::clamp(
    //             hubGridShellRadiusPx /
    //                 m_lastHubPlanetVisualRadiusPx,
    //             1.02,
    //             2.20
    //         );

    //     m_hubSphericalGridRenderer.render(
    //         m_lastHubPlanetVisualCenterPx,
    //         m_lastHubPlanetVisualRadiusPx,
    //         activeDetailCamera().yaw *
    //             0.35,
    //         sphericalGridStyle
    //     );
    // }



    // drawHubMapAdaptiveGrid(
    //     viewport,
    //     scale,
    //     centerPx,
    //     maxDist
    // );

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

    const glm::dvec2 modScreen =
        hubMapProject(
            mod.localPositionMeters,
            scale,
            centerPx
        );

    const double moduleRadiusMeters =
        glm::length(
            mod.sizeMeters
        ) * 0.5;

    const double moduleRadiusPx =
        moduleRadiusMeters *
        finalScale;

    {
        HubMapPickable pickable;

        pickable.localCenterMeters =
            mod.localPositionMeters;

        pickable.screenCenterPx =
            modScreen;

        pickable.screenRadiusPx =
            std::max(
                18.0,
                moduleRadiusPx
            );

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

    // Если модуль на текущем масштабе слишком мелкий,
    // добавляем screen-space маркер. Это не физический размер.
    if (moduleRadiusPx < 8.0)
    {
        const glm::vec4 markerColor =
            mod.prime
                ? glm::vec4(0.85f, 0.98f, 1.0f, 0.95f)
                : glm::vec4(0.48f, 0.76f, 1.0f, 0.82f);

        drawHubMapScreenMarker(
            modScreen,
            mod.prime ? 9.0 : 7.0,
            markerColor,
            mod.prime,
            32
        );
    }
}





// Игрок / корабли.
for (const auto& ship : hub.ships)
{
    if (!ship.valid)
        continue;

    const glm::dvec2 shipScreen =
        hubMapProject(
            ship.localPositionMeters,
            scale,
            centerPx
        );

    const glm::dvec3 shipVisualSize =
        visualSizeForHubShip(
            ship,
            scale
        );

    const double shipRadiusMeters =
        glm::length(
            shipVisualSize
        ) * 0.5;

    const double shipRadiusPx =
        shipRadiusMeters *
        finalScale;

    {
        HubMapPickable pickable;

        pickable.localCenterMeters =
            ship.localPositionMeters;

        pickable.screenCenterPx =
            shipScreen;

        pickable.screenRadiusPx =
            std::max(
                ship.player ? 22.0 : 18.0,
                shipRadiusPx
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

    // Если корабль на карте слишком маленький, wire-модель будет шумом.
    // Тогда рисуем fallback box с увеличенным visual size.
    const bool allowWireModel =
        shipRadiusPx >= 10.0;

    bool shipModelDrawn =
        false;

    if (allowWireModel)
    {
        shipModelDrawn =
            drawHubMapAssemblyWire(
                ship.typeId,
                ship.localPositionMeters,
                ship.localAxes,
                scale,
                centerPx
            );
    }

    if (!shipModelDrawn)
    {
        drawHubMapBox(
            ship.localPositionMeters,
            ship.localAxes,
            shipVisualSize,
            scale,
            centerPx
        );
    }

    const double shipAxisLen =
        std::max(
            ship.player ? 26.0 : 16.0,
            glm::length(shipVisualSize) * 0.65
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
        std::max(
            80.0,
            shipAxisLen * 2.0
        ),
        scale,
        centerPx
    );

    // Экранный маркер поверх корабля.
    // PLAYER виден всегда, остальные — когда мелкие.
    if (ship.player ||
        shipRadiusPx < 12.0)
    {
        const glm::vec4 markerColor =
            ship.player
                ? glm::vec4(1.0f, 0.84f, 0.25f, 0.98f)
                : glm::vec4(1.0f, 0.62f, 0.32f, 0.82f);

        drawHubMapScreenMarker(
            shipScreen,
            ship.player ? 13.0 : 8.0,
            markerColor,
            true,
            32
        );
    }
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

                
                if (p.x < -160.0 ||
                    p.y < -80.0 ||
                    p.x > static_cast<double>(viewport.width) + 160.0 ||
                    p.y > static_cast<double>(viewport.height) + 80.0)
                {
                    continue;
                }

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





                if (p.x < -160.0 ||
                    p.y < -80.0 ||
                    p.x > static_cast<double>(viewport.width) + 160.0 ||
                    p.y > static_cast<double>(viewport.height) + 80.0)
                {
                    continue;
                }









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












    restoreGlState();
}





