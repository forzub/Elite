#include "src/game/system_map/SystemMapRenderer.h"
#include "src/input/Input.h"

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
#include <limits>
#include <filesystem>
#include <cctype>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

#include "render/HUD/TextRenderer.h"
#include "src/game/navigation/NavigationAddressFormatter.h"
#include "src/render/ShaderLibrary.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"
#include "src/render/bitmap/TextureLoader.h"
#include <nlohmann/json.hpp>

namespace
{
    constexpr double AU_KM = 149597870.7;

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
        const world::celestial::DetailMapSnapshot& planet
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

    glm::dvec3 systemBodyPrimeAxisWorld(
        const glm::dvec3& north
    )
    {
        return planetPrimeAxisWorld(
            north
        );
    }

    glm::dvec3 systemBodyEastAxisWorld(
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        return planetEastAxisWorld(
            north,
            prime
        );
    }

    glm::dvec3 systemBodyRingAxisYWorld(
        const world::celestial::SystemMapBody& body,
        const glm::dvec3& north,
        const glm::dvec3& prime
    )
    {
        const glm::dvec3 east =
            systemBodyEastAxisWorld(
                north,
                prime
            );

        const double inclination =
            degToRadD(
                body.ringPlaneInclinationOffsetDeg
            );

        return safeNormalizeD(
            east * std::cos(inclination) +
            north * std::sin(inclination),
            east
        );
    }

    glm::dvec3 planetSurfacePointMeters(
        const world::celestial::DetailMapSnapshot& planet,
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





















double SystemMapRenderer::currentTimeSeconds() const
{
    return glfwGetTime();
}


void SystemMapRenderer::beginTextFrame(
    int viewportWidth,
    int viewportHeight
)
{
    TextRenderer::instance().beginFrameForViewport(
        viewportWidth,
        viewportHeight
    );
}


void SystemMapRenderer::drawTextPx(
    const std::string& text,
    float x,
    float y,
    int pixelHeight,
    const glm::vec4& color
)
{
    TextRenderer::instance().textDrawPx(
        text,
        x,
        y,
        pixelHeight,
        color
    );
}


void SystemMapRenderer::endTextFrame()
{
    TextRenderer::instance().endFrame();
}


SystemMapRenderer::SystemMapRenderer()
    : m_detailBackend(*this),
      m_hubBackend(*this)
{
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
            m_galaxyView.state().navigationGrid
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
        // System/Detail/Hub skies use the real astronomical catalog.
        // Runtime-only game-system proxies remain available for metadata,
        // but are not rendered as false background stars. Constellation
        // figures are a gameplay-sky aid and are not part of map rendering.
        m_mapStarfieldRenderer.setConstellationOverlayAvailable(false);
        m_mapStarfieldRenderer.setCatalogFilter(
            0.0f,
            true
        );

        m_mapStarfieldInitialized =
            m_mapStarfieldRenderer.initialize(
                "assets/data/galaxy_details"
            );

        if (!m_mapStarfieldInitialized)
        {
            std::cerr
                << "[SystemMapRenderer]"
                << " failed to initialize map starfield"
                << "\n";
        }
    }

    if (!m_galaxyBackdropStarfieldInitialized)
    {
        m_galaxyBackdropStarfieldRenderer.setConstellationOverlayAvailable(false);
        m_galaxyBackdropStarfieldRenderer.setCatalogFilter(
            m_galaxyView.visuals().starfieldMinimumDistanceLy,
            true
        );

        m_galaxyBackdropStarfieldInitialized =
            m_galaxyBackdropStarfieldRenderer.initialize(
                "assets/data/galaxy_details"
            );

        if (!m_galaxyBackdropStarfieldInitialized)
        {
            std::cerr
                << "[SystemMapRenderer]"
                << " failed to initialize galaxy backdrop starfield"
                << "\n";
        }
    }

    m_initialized = true;
}






void SystemMapRenderer::drawMapStarfield(
    const Viewport& viewport,
    const glm::dvec3& observerPositionLy
)
{
    const glm::mat4 view =
        activeLocalCameraSnapshot().starfieldViewMatrix();

    const bool hubMode =
        m_mode == Mode::Hub;

    const float fieldOfViewDeg =
        hubMode
            ? m_hubVisuals.starfieldFieldOfViewDeg
            : m_detailVisuals.starfieldFieldOfViewDeg;

    const float sizeScale =
        hubMode
            ? m_hubVisuals.starfieldSizeScale
            : m_detailVisuals.starfieldSizeScale;

    const float starBrightnessScale =
        hubMode
            ? m_hubVisuals.starfieldBrightnessScale
            : m_detailVisuals.starfieldBrightnessScale;

    const float milkyWayIntensityScale =
        hubMode
            ? m_hubVisuals.milkyWayIntensityScale
            : m_detailVisuals.milkyWayIntensityScale;

    const glm::vec3 milkyWayColorTint =
        hubMode
            ? m_hubVisuals.milkyWayColorTint
            : m_detailVisuals.milkyWayColorTint;

    drawMapStarfield(
        viewport,
        observerPositionLy,
        view,
        fieldOfViewDeg,
        sizeScale,
        false,
        starBrightnessScale,
        milkyWayIntensityScale,
        milkyWayColorTint
    );
}

void SystemMapRenderer::drawMapStarfield(
    const Viewport& viewport,
    const glm::dvec3& observerPositionLy,
    const glm::mat4& cameraView,
    float fieldOfViewDeg,
    float sizeScale,
    bool distantGalaxyBackdrop,
    float starBrightnessScale,
    float milkyWayIntensityScale,
    const glm::vec3& milkyWayColorTint
)
{
    GalaxyStarfieldRenderer& renderer =
        distantGalaxyBackdrop
            ? m_galaxyBackdropStarfieldRenderer
            : m_mapStarfieldRenderer;

    const bool initialized =
        distantGalaxyBackdrop
            ? m_galaxyBackdropStarfieldInitialized
            : m_mapStarfieldInitialized;

    if (!initialized ||
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
                std::clamp(
                    fieldOfViewDeg,
                    20.0f,
                    120.0f
                )
            ),
            aspect,
            0.1f,
            500.0f
        );

    renderer.setObserverPositionLy(
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

    renderer.render(
        cameraView,
        projection,
        sizeScale,
        starBrightnessScale,
        milkyWayIntensityScale,
        milkyWayColorTint
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
    m_galaxyView.reset();
    m_systemView.reset();

    m_navigationLevelAnnouncement.text.clear();
    m_navigationLevelAnnouncement.startedAtSeconds = -1.0;

    m_detailView.reset();
    m_hubView.reset();
    m_detailPresentation =
        game::system_map::DetailMapPresentation{};
    m_hubPresentation =
        game::system_map::HubMapPresentation{};
    m_pendingScrollY = 0.0;
    m_systemPresentation =
        game::system_map::SystemMapPresentation{};
    m_systemSceneFrame =
        game::system_map::SystemMapSceneFrame{};
    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;


    m_systemView.state().lastScale = 1.0f;

    m_systemView.state().presentationSystemId = -1;
    m_systemView.state().presentationSourceTimeSeconds = 0.0;
    m_systemView.state().presentationWallTimeSeconds = 0.0;
    m_systemView.state().presentationTimeScale = 1.0;

    m_environmentVisualTimeSeconds = 0.0;
    m_environmentLastSourceTimeSeconds = 0.0;
    m_environmentLastWallClockSeconds = 0.0;
    m_environmentVisualTimeInitialized = false;

    m_navigationLevelZeroButtonHovered = false;
    m_navigationOverlayLeftWasDown = false;

}


void SystemMapRenderer::cycleNavigationCoordinateFormat()
{
    m_navigationCoordinateFormat =
        game::navigation::nextNavigationCoordinateFormat(
            m_navigationCoordinateFormat
        );
}




















void SystemMapRenderer::onGalaxyMapEntered(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation
)
{
    m_galaxyView.onEntered(
        galaxy,
        navigation
    );
}


void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    m_galaxyView.focusSystem(
        systemId,
        galaxy,
        m_mode == Mode::Galaxy,
        glfwGetTime()
    );
}


int SystemMapRenderer::selectedSystemId() const
{
    return m_galaxyView.state().selectedSystemId;
}

int SystemMapRenderer::focusedSystemId() const
{
    return m_galaxyView.state().focusedSystemId;
}























void SystemMapRenderer::setMode(Mode mode)
{
    if (m_mode == mode)
        return;

    const Mode previousMode = m_mode;

    if (previousMode == Mode::Detail)
    {
        auto& systemState = m_systemView.state();
        const auto& detailState = m_detailView.state();

        systemState.selectedHubId =
            detailState.selectedHubId;
        systemState.selectedHubParentBodyId =
            detailState.selectedHubParentBodyId;

        if (!systemState.selectedHubId.empty())
            systemState.selectedBodyId.clear();
    }

    m_mode = mode;

    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;

    /*
        Если пользователь открыл другую карту во время перелёта,
        сохраняем конечную позицию Galaxy-камеры.
    */
    if (m_mode != Mode::Galaxy)
    {
        m_galaxyView.cancelCameraFlight(
            true
        );
    }

    if (m_mode == Mode::Detail)
    {
        m_detailView.reset();
        m_detailView.selectHub(
            m_systemView.state().selectedHubId,
            m_systemView.state().selectedHubParentBodyId
        );
        m_detailPresentation =
            game::system_map::DetailMapPresentation{};
    }

    if (m_mode == Mode::Hub)
    {
        if (previousMode != Mode::Detail)
        {
            m_detailView.selectHub(
                m_systemView.state().selectedHubId,
                m_systemView.state().selectedHubParentBodyId
            );
        }

        m_hubView.beginScene();
        m_hubPresentation =
            game::system_map::HubMapPresentation{};
    }
}



SystemMapRenderer::Mode SystemMapRenderer::mode() const
{
    return m_mode;
}




const game::system_map::LocalMapCameraSnapshot&
SystemMapRenderer::activeLocalCameraSnapshot() const
{
    static const game::system_map::LocalMapCameraSnapshot
        fallback;

    if (!m_activeLocalCameraSnapshot)
        return fallback;

    return *m_activeLocalCameraSnapshot;
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
    const world::celestial::DetailMapSnapshot& planet,
    const world::celestial::HubMapSnapshot& hub,
    const world::celestial::PlayerNavigationState& nav
)
{


    if (!m_initialized)
        init();

    const double nowSeconds =
        glfwGetTime();

    m_galaxyView.updateCameraFlight(
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




    if (m_mode == Mode::Detail)
    {
        if (!m_detailFramePrepared || m_detailFrameDirty)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    viewport,
                    planet
                );

            m_detailFrameDirty = false;
        }

        m_detailSceneRenderer.render(
            m_detailPresentation,
            m_detailBackend,
            viewport,
            planet
        );

        m_detailFramePrepared = false;
        m_detailFrameDirty = true;
    }
    else if (m_mode == Mode::Hub)
    {
        if (!m_hubFramePrepared || m_hubFrameDirty)
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    viewport,
                    hub
                );

            m_hubFrameDirty = false;
        }

        m_hubSceneRenderer.render(
            m_hubPresentation,
            m_hubBackend,
            viewport,
            hub
        );

        m_hubFramePrepared = false;
        m_hubFrameDirty = true;
    }
    else if (m_mode == Mode::Galaxy)
    {
        m_galaxyRenderer.render(
            m_galaxyView,
            *this,
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
    depth = 2.0f;

    if (clip.w <= 0.00001f)
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












std::optional<game::system_map::MapIntent>
SystemMapRenderer::handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& detail,
    const world::celestial::HubMapSnapshot& hub
)
{
    if (m_mode != Mode::Galaxy &&
        m_mode != Mode::System &&
        m_mode != Mode::Detail &&
        m_mode != Mode::Hub)
    {
        return std::nullopt;
    }

    const double inputNowSeconds =
        glfwGetTime();

    if (m_mode == Mode::System)
    {
        m_systemView.updateCameraFlight(
            inputNowSeconds
        );
    }

    /*
        Scroll input belongs to the application-wide Input service.
        SystemMapRenderer must not replace GLFW callbacks or keep raw
        pointers in a global table. The renderer only consumes the
        frame-local wheel delta while the map owns input focus.
    */
    m_pendingScrollY +=
        Input::instance().consumeScrollY();


    /*
        Во время crossfade нельзя вращать или перемещать
        старую либо новую сцену.

        AwaitingCapture тоже считается активной фазой.
    */
    if (m_mapTransition.blocksInput())
    {
        if (m_mode == Mode::System)
        {
            m_systemView.constrainCameraToNavigationBoundary(
                vp
            );
        }

        m_pendingScrollY = 0.0;
        m_navigationLevelZeroButtonHovered = false;
        m_navigationOverlayLeftWasDown = false;
        return std::nullopt;
    }






    GLFWwindow* window =
        glfwGetCurrentContext();

    if (!window)
        return std::nullopt;

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

    const bool showLevelZeroButton =
        m_mode == Mode::Galaxy ||
        m_mode == Mode::System;

    m_navigationLevelZeroButtonHovered =
        showLevelZeroButton &&
        inside &&
        render::navigation::
            NavigationCoordinateOverlay::
                levelZeroButtonBounds(
                    vp
                )
                .contains(
                    localMx,
                    localMy
                );

    const bool levelZeroPressed =
        m_navigationLevelZeroButtonHovered &&
        leftDown &&
        !m_navigationOverlayLeftWasDown;

    m_navigationOverlayLeftWasDown =
        leftDown;

    if (m_navigationLevelZeroButtonHovered)
    {
        m_pendingScrollY = 0.0;

        if (levelZeroPressed)
        {
            resetNavigationViewToLevelZero(
                vp
            );
        }

        /*
            The button is part of the map viewport, so explicitly prevent
            the scene behind it from receiving the same mouse gesture.
        */
        m_galaxyView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );

        m_systemView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );

        return std::nullopt;
    }

    if (m_mode == Mode::System)
    {
        m_systemPresentation =
            m_systemPresentationBuilder.build(
                m_systemView,
                vp,
                system,
                inputNowSeconds,
                false
            );

        m_systemSceneFrame =
            m_systemSceneFrameBuilder.build(
                m_systemView,
                *this,
                vp,
                system,
                m_systemPresentation
            );

        m_systemFramePrepared = true;
        m_systemSceneFrameDirty = false;

        const auto cameraBefore =
            m_systemView.state().camera;

        game::system_map::SystemMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.nowSeconds = inputNowSeconds;

        const game::system_map::SystemMapFrameInteractionContext
            interactionContext(
                m_systemSceneFrame.interaction,
                m_systemView.controls()
            );

        const auto result =
            m_systemInteraction.handleInput(
                m_systemView,
                interactionContext,
                frame,
                m_pendingScrollY
            );

        if (m_systemView.state().navigationGrid.enabled() &&
            m_systemPresentation.systemScale > 0.0f)
        {
            m_systemView.updateNavigationHoverPresentation(
                vp,
                inputNowSeconds
            );
        }

        const auto& cameraAfter =
            m_systemView.state().camera;

        m_systemSceneFrameDirty =
            glm::length(
                cameraBefore.target - cameraAfter.target
            ) > 0.0 ||
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.distance != cameraAfter.distance;

        if (result.systemLevelChanged.has_value())
        {
            announceNavigationLevel(
                'S',
                result.systemLevelChanged.value()
            );
        }

        return std::nullopt;
    }

    if (m_mode == Mode::Detail ||
        m_mode == Mode::Hub)
    {
        if (m_mode == Mode::Detail)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    vp,
                    detail
                );
            m_detailFramePrepared = true;
            m_detailFrameDirty = false;
        }
        else
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    vp,
                    hub
                );
            m_hubFramePrepared = true;
            m_hubFrameDirty = false;
        }

        const auto cameraBefore =
            m_mode == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

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

        const auto& cameraAfter =
            m_mode == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        const bool projectionChanged =
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.zoom != cameraAfter.zoom ||
            glm::length(
                cameraBefore.pan - cameraAfter.pan
            ) > 0.0;

        if (m_mode == Mode::Detail)
            m_detailFrameDirty = projectionChanged;
        else
            m_hubFrameDirty = projectionChanged;

        return std::nullopt;
    }

    if (m_mode == Mode::Galaxy)
    {
        game::system_map::GalaxyMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.transitionActive = m_mapTransition.active();
        frame.nowSeconds = glfwGetTime();

        const auto result =
            m_galaxyInteraction.handleInput(
                m_galaxyView,
                galaxy,
                frame,
                m_pendingScrollY
            );

        if (result.requestWindowFocus)
            glfwFocusWindow(window);

        if (result.galaxyLevelChanged.has_value())
        {
            announceNavigationLevel(
                'G',
                result.galaxyLevelChanged.value()
            );
        }

        if (result.mapIntent.has_value())
        {
            const auto type =
                result.mapIntent->type;

            if (type ==
                    game::system_map::MapIntentType::
                        EnterKnownSystem ||
                type ==
                    game::system_map::MapIntentType::
                        EnterEmptySector)
            {
                announceNavigationLevel(
                    'S',
                    m_systemView.state().navigationGrid
                        .definition()
                        .minimumLevel
                );
            }
        }

        return result.mapIntent;
    }

    return std::nullopt;

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




















void SystemMapRenderer::drawMapAtmosphereVeil(
    float centerAlpha,
    float edgeAlpha,
    float aquaStrength
)
{
    if (!m_bgShader || !m_bgVao)
        return;

    /*
        Теперь это не "alpha тёмной пелены", а сила затемнения
        уже нарисованного starfield.

        0.0 = не затемнять
        1.0 = полностью убить яркость
    */
    centerAlpha =
        std::clamp(
            centerAlpha,
            0.0f,
            0.95f
        );

    edgeAlpha =
        std::clamp(
            edgeAlpha,
            centerAlpha,
            0.95f
        );

    aquaStrength =
        std::clamp(
            aquaStrength,
            0.0f,
            1.0f
        );

    if (edgeAlpha <= 0.0f)
        return;

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean previousDepthWriteMask =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &previousDepthWriteMask
    );

    GLint previousBlendEquationRgb =
        GL_FUNC_ADD;

    GLint previousBlendEquationAlpha =
        GL_FUNC_ADD;

    GLint previousBlendSourceRgb =
        GL_ONE;

    GLint previousBlendDestinationRgb =
        GL_ZERO;

    GLint previousBlendSourceAlpha =
        GL_ONE;

    GLint previousBlendDestinationAlpha =
        GL_ZERO;

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

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

    glDepthMask(
        GL_FALSE
    );

    glEnable(
        GL_BLEND
    );

    /*
        Самая важная часть.

        Мы не добавляем поверх тёмный цвет.
        Мы умножаем уже нарисованный starfield
        на вычисленный RGB-множитель из шейдера.
    */
    glBlendEquationSeparate(
        GL_FUNC_ADD,
        GL_FUNC_ADD
    );

    glBlendFuncSeparate(
        GL_ZERO,
        GL_SRC_COLOR,
        GL_ZERO,
        GL_ONE
    );

    glUseProgram(
        m_bgShader
    );

    const GLint passLoc =
        glGetUniformLocation(
            m_bgShader,
            "uPass"
        );

    const GLint centerAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilCenterAlpha"
        );

    const GLint edgeAlphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilEdgeAlpha"
        );

    const GLint aquaStrengthLoc =
        glGetUniformLocation(
            m_bgShader,
            "uMapVeilAquaStrength"
        );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            2
        );
    }

    if (centerAlphaLoc >= 0)
    {
        glUniform1f(
            centerAlphaLoc,
            centerAlpha
        );
    }

    if (edgeAlphaLoc >= 0)
    {
        glUniform1f(
            edgeAlphaLoc,
            edgeAlpha
        );
    }

    if (aquaStrengthLoc >= 0)
    {
        glUniform1f(
            aquaStrengthLoc,
            aquaStrength
        );
    }

    glBindVertexArray(
        m_bgVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        6
    );

    glBindVertexArray(
        0
    );

    if (passLoc >= 0)
    {
        glUniform1i(
            passLoc,
            0
        );
    }

    glUseProgram(
        0
    );

    glBlendEquationSeparate(
        static_cast<GLenum>(
            previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            previousBlendEquationAlpha
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

    glDepthMask(
        previousDepthWriteMask
    );

    if (depthWasEnabled)
    {
        glEnable(
            GL_DEPTH_TEST
        );
    }
    else
    {
        glDisable(
            GL_DEPTH_TEST
        );
    }

    if (blendWasEnabled)
    {
        glEnable(
            GL_BLEND
        );
    }
    else
    {
        glDisable(
            GL_BLEND
        );
    }
}














void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}




#include "src/game/system_map/SystemMapRendererDetail.inl"

#include "src/game/system_map/SystemMapRendererHub.inl"


#include "src/game/system_map/SystemMapRendererCommon.inl"
