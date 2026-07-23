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
#include "src/game/navigation/NavigationAddressFormatter.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/render/bitmap/TextureLoader.h"
#include <nlohmann/json.hpp>

namespace
{
    constexpr double AU_KM = 149597870.7;
    constexpr float GALAXY_RENDER_UNITS_PER_LY = 2.2f;

    double hubPerfNowMs()
    {
        using Clock =
            std::chrono::steady_clock;

        return
            std::chrono::duration<
                double,
                std::milli
            >(
                Clock::now().time_since_epoch()
            ).count();
    }

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
















    void configureSphericalTextureSampling(
        GLuint texture
    )
    {
        if (texture == 0)
            return;

        GLint previousTexture = 0;

        glGetIntegerv(
            GL_TEXTURE_BINDING_2D,
            &previousTexture
        );

        glBindTexture(
            GL_TEXTURE_2D,
            texture
        );

        /*
            Equirectangular planetary textures are periodic
            along longitude (U), but not along latitude (V).

            GL_CLAMP_TO_EDGE on U creates a visible meridian
            from one pole to the other when longitude jumps
            from 1 back to 0.
        */
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_S,
            GL_REPEAT
        );

        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_WRAP_T,
            GL_CLAMP_TO_EDGE
        );

        glBindTexture(
            GL_TEXTURE_2D,
            static_cast<GLuint>(
                previousTexture
            )
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

    /*
        System использует ортографическую камеру от масштаба
        всей системы до куба порядка 100 km.

        Фиксированный диапазон глубины 0.001...120000 терял
        точность уже около S3. Глубина камеры теперь вычисляется
        относительно текущего half-height.
    */
    constexpr float SYSTEM_MAP_ORTHO_DEPTH_MULTIPLIER =
        8.0f;

    float systemMapOrthoDepthHalfRange(
        float cameraHalfHeight
    )
    {
        const float safeHalfHeight =
            std::clamp(
                cameraHalfHeight,
                SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
            );

        return std::max(
            safeHalfHeight *
                SYSTEM_MAP_ORTHO_DEPTH_MULTIPLIER,
            0.000001f
        );
    }

    float systemMapOrthoNearPlane(
        float cameraHalfHeight
    )
    {
        return std::max(
            systemMapOrthoDepthHalfRange(
                cameraHalfHeight
            ) * 0.0001f,
            0.000000001f
        );
    }

    float systemMapOrthoEyeDistance(
        float cameraHalfHeight
    )
    {
        return
            systemMapOrthoDepthHalfRange(
                cameraHalfHeight
            ) +
            systemMapOrthoNearPlane(
                cameraHalfHeight
            );
    }

    float systemMapOrthoFarPlane(
        float cameraHalfHeight
    )
    {
        return
            systemMapOrthoEyeDistance(
                cameraHalfHeight
            ) +
            systemMapOrthoDepthHalfRange(
                cameraHalfHeight
            );
    }

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




    float galaxyStarTypeVisualScale(
        const std::string& starType
    )
    {
        if (starType.empty())
            return 1.0f;

        const char spectralClass =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        starType.front()
                    )
                )
            );

        float scale = 1.0f;

        switch (spectralClass)
        {
            case 'O': scale = 1.65f; break;
            case 'B': scale = 1.45f; break;
            case 'A': scale = 1.25f; break;
            case 'F': scale = 1.12f; break;
            case 'G': scale = 1.00f; break;
            case 'K': scale = 0.90f; break;
            case 'M': scale = 0.78f; break;

            // Белые и коричневые карлики.
            case 'D': scale = 0.68f; break;
            case 'L': scale = 0.66f; break;
            case 'T': scale = 0.62f; break;

            default: scale = 1.0f; break;
        }

        /*
            Это не физический радиус, а визуальная поправка
            по классу светимости.
        */
        if (starType.find("III") != std::string::npos)
        {
            scale *= 1.50f;
        }
        else if (starType.find("IV") != std::string::npos)
        {
            scale *= 1.20f;
        }

        return std::clamp(
            scale,
            0.58f,
            1.85f
        );
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

    m_navigationCoordinateFormat =
        game::navigation::navigationCoordinateFormatFromString(
            m_galaxyNavigationGrid
                .config()
                .defaultCoordinateFormat
        );

    if (!m_navigationRegionCatalog.loaded())
    {
        const bool namesLoaded =
            m_navigationRegionCatalog.loadFromRuntimeOrSource(
                "assets/data/navigation/region_names.json",
                "src/assets/data/navigation/region_names.json"
            );

        if (!namesLoaded)
        {
            std::cerr
                << "[SystemMapRenderer] navigation region names not loaded\n";
        }
    }

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
    m_galaxyCamera.distance = m_galaxyVisuals.initialCameraDistance;
    m_galaxyCameraFlight.reset();
    m_systemCamera = SystemCamera{};
    m_systemCameraFlight = SystemCameraFlight{};

    m_galaxyNavigationFocusLy =
        glm::dvec3(0.0);

    m_galaxyNavigationFocusValid =
        false;

    m_systemHoverVisualCell.reset();
    m_systemHoverVisualAlpha = 0.0f;
    m_systemHoverOutgoingCell.reset();
    m_systemHoverOutgoingAlpha = 0.0f;
    m_systemHoverVisualLastTimeSeconds = 0.0;
    m_systemCubeClickTracker.reset();
    m_galaxyCubeClickTracker.reset();
    m_lastSystemCameraFitSystemId = -1;

    m_navigationLevelAnnouncement.text.clear();
    m_navigationLevelAnnouncement.startedAtSeconds = -1.0;

    m_planetCamera = DetailCamera{};
    m_hubCamera = DetailCamera{};
    m_selectedSystemId = -1;
    m_requestedSystemEntry.reset();
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

    m_galaxyNavigationGrid.reset();

    m_hasGalaxyMapEntryState = false;
    m_lastGalaxyMapEntrySystemId = -1;
    m_lastGalaxyMapEntryTerminalCell = {};

}


void SystemMapRenderer::cycleNavigationCoordinateFormat()
{
    m_navigationCoordinateFormat =
        game::navigation::nextNavigationCoordinateFormat(
            m_navigationCoordinateFormat
        );
}




















int SystemMapRenderer::selectedSystemId() const
{
    return m_selectedSystemId;
}

std::optional<SystemMapRenderer::SystemEntryRequest>
SystemMapRenderer::consumeRequestedSystemEntry()
{
    const auto requested =
        m_requestedSystemEntry;

    m_requestedSystemEntry.reset();

    return requested;
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

    /*
        Если пользователь открыл другую карту во время перелёта,
        сохраняем конечную позицию Galaxy-камеры.
    */
    if (m_mode != Mode::Galaxy)
    {
        cancelGalaxyCameraFlight(
            true
        );
    }



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

    configureSphericalTextureSampling(
        tex
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

    configureSphericalTextureSampling(
        tex
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

    configureSphericalTextureSampling(
        texture
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

    const double nowSeconds =
        glfwGetTime();

    updateGalaxyCameraFlight(
        nowSeconds
    );

    updateSystemCameraFlight(
        nowSeconds
    );

    m_mapTransition.update(
        nowSeconds
    );



    /*
        Начало нового кадра процедурных облаков.

        Это должно выполняться ровно один раз до любого
        вызова textureForStyle(), независимо от режима карты
        и наличия старой solid geometry.
    */
    m_proceduralCloudLayer.beginFrame();





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

    drawNavigationCoordinateOverlay(
        vp,
        galaxy,
        system,
        nav
    );

    /*
        Настоящий crossfade.

        Если переход только начался, framebuffer ещё содержит
        старое состояние. Сохраняем его в текстуру и только
        после этого выполняем смену камеры или режима.

        На следующих кадрах уже рисуется новое состояние,
        а старый снимок постепенно растворяется поверх него.
    */
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

    if (m_mapTransition.needsOutgoingCapture())
    {
        captureMapTransitionSnapshot(
            viewport
        );

        m_mapTransition.outgoingCaptured(
            glfwGetTime()
        );
    }
    else if (m_mapTransition.active())
    {
        drawMapTransitionSnapshot(
            viewport,
            m_mapTransition.outgoingAlpha()
        );
    }





    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}




#include "src/game/system_map/SystemMapRendererGalaxy.inl"
#include "src/game/system_map/SystemMapRendererSystem.inl"





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



    /*
        Во время crossfade нельзя вращать или перемещать
        старую либо новую сцену.

        AwaitingCapture тоже считается активной фазой.
    */
    if (m_mapTransition.blocksInput())
    {
        m_pendingScrollY = 0.0;
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
        handleDetailAndHubInput(
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

    if (!std::isfinite(sourceTimeSeconds))
    {
        sourceTimeSeconds =
            0.0;
    }

    /*
        Первый кадр синхронизируем с universe time,
        чтобы разные тела получали детерминированную
        исходную фазу.
    */
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

        return
            m_environmentVisualTimeSeconds;
    }

    /*
        Реальное время кадра.

        Верхнее ограничение защищает анимацию от большого
        скачка после breakpoint, сворачивания окна или лагов.
    */
    const double wallDeltaSeconds =
        std::clamp(
            nowSeconds -
                m_environmentLastWallClockSeconds,
            0.0,
            0.10
        );

    m_environmentLastWallClockSeconds =
        nowSeconds;

    const bool sourceWentBackward =
        sourceTimeSeconds <
        m_environmentLastSourceTimeSeconds -
            0.001;

    const bool sourceAdvanced =
        sourceTimeSeconds >
        m_environmentLastSourceTimeSeconds +
            0.000001;

    m_environmentLastSourceTimeSeconds =
        sourceTimeSeconds;

    /*
        Явный откат universe time:
        загрузка состояния, перемотка или сброс сервера.

        В этом случае локальное визуальное время тоже
        должно немедленно синхронизироваться.
    */
    if (sourceWentBackward)
    {
        m_environmentVisualTimeSeconds =
            sourceTimeSeconds;

        return
            m_environmentVisualTimeSeconds;
    }

    /*
        Главное изменение:

        визуальное время всегда движется по steady_clock,
        даже если snapshot кэширован и его universeTimeSeconds
        несколько секунд не обновляется.
    */
    m_environmentVisualTimeSeconds +=
        wallDeltaSeconds;

    /*
        Когда свежий snapshot всё-таки приходит, мягко
        корректируем небольшое расхождение.

        Большой скачок синхронизируем сразу — например,
        при ускоренном universe time или смене состояния.
    */
    if (sourceAdvanced)
    {
        const double timeError =
            sourceTimeSeconds -
            m_environmentVisualTimeSeconds;

        if (std::abs(timeError) > 2.0)
        {
            m_environmentVisualTimeSeconds =
                sourceTimeSeconds;
        }
        else
        {
            const double correctionBlend =
                1.0 -
                std::exp(
                    -6.0 *
                    wallDeltaSeconds
                );

            const double limitedError =
                std::clamp(
                    timeError,
                    -0.25,
                    0.25
                );

            m_environmentVisualTimeSeconds +=
                limitedError *
                correctionBlend;
        }
    }

    return
        m_environmentVisualTimeSeconds;
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

    const GLboolean depthWasEnabled =
        glIsEnabled(GL_DEPTH_TEST);

    const GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(m_bgShader);

    /*
        Проход 0 — обычный фон карты.
        Проход 1 используется только для transition snapshot.
    */
    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glBindVertexArray(m_bgVao);

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(0);
    glUseProgram(0);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




















void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}



#include "src/game/system_map/SystemMapRendererDetail.inl"

#include "src/game/system_map/SystemMapRendererHub.inl"


#include "src/game/system_map/SystemMapRendererCommon.inl"
