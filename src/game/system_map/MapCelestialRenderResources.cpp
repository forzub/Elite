#include "src/game/system_map/MapCelestialRenderResources.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <nlohmann/json.hpp>

#include "src/render/bitmap/TextureLoader.h"

namespace
{
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
}

namespace game::system_map
{
void MapCelestialRenderResources::init(
    float galaxyBackdropMinimumDistanceLy
)
{
    ensureGeneratedCelestialAssets();
    ensureEnvironmentProfiles();

    if (!m_mapStarfieldInitialized)
    {
        m_mapStarfieldRenderer.setConstellationOverlayAvailable(false);
        m_mapStarfieldRenderer.setCatalogFilter(0.0f, true);

        m_mapStarfieldInitialized =
            m_mapStarfieldRenderer.initialize(
                "assets/data/galaxy_details"
            );

        if (!m_mapStarfieldInitialized)
        {
            std::cerr
                << "[MapCelestialRenderResources]"
                << " failed to initialize map starfield\n";
        }
    }

    if (!m_galaxyBackdropStarfieldInitialized)
    {
        m_galaxyBackdropStarfieldRenderer
            .setConstellationOverlayAvailable(false);
        m_galaxyBackdropStarfieldRenderer.setCatalogFilter(
            galaxyBackdropMinimumDistanceLy,
            true
        );

        m_galaxyBackdropStarfieldInitialized =
            m_galaxyBackdropStarfieldRenderer.initialize(
                "assets/data/galaxy_details"
            );

        if (!m_galaxyBackdropStarfieldInitialized)
        {
            std::cerr
                << "[MapCelestialRenderResources]"
                << " failed to initialize galaxy backdrop starfield\n";
        }
    }
}

void MapCelestialRenderResources::beginFrame()
{
    m_proceduralCloudLayer.beginFrame();
}

void MapCelestialRenderResources::resetPresentationTime()
{
    /*
        Celestial/cloud animation no longer owns a second clock. The caller
        supplies reconstructed universe time for every frame.
    */
}

void MapCelestialRenderResources::ensureGeneratedCelestialAssets()
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

void MapCelestialRenderResources::ensureEnvironmentProfiles()
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

void MapCelestialRenderResources::beginEnvironmentRenderSessionIfNeeded(
    MapMode mode,
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
MapCelestialRenderResources::resolvedEnvironmentProfileForBody(
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

std::vector<render::celestial::ProceduralCloudStyle>
MapCelestialRenderResources::cloudStylesForBody(
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

LocalMapAtmosphereStyle
MapCelestialRenderResources::atmosphereStyleForBody(
    int systemId,
    const std::string& bodyId,
    const std::string& displayName,
    const std::string& environmentPresetId
) const
{
    LocalMapAtmosphereStyle style;

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

const world::celestial::visual::CelestialGeneratedAssetSet*
MapCelestialRenderResources::generatedAssetForBody(
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
MapCelestialRenderResources::generatedAssetForIdentity(
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

GLuint MapCelestialRenderResources::mapPreviewTextureForGeneratedAsset(
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

GLuint MapCelestialRenderResources::globalAlbedoTextureForGeneratedAsset(
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

GLuint MapCelestialRenderResources::globalAlbedoTextureForHubSnapshot(
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

GLuint MapCelestialRenderResources::globalNormalTextureForGeneratedAsset(
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

GLuint MapCelestialRenderResources::globalAlbedoTextureForBody(
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

double MapCelestialRenderResources::visualEffectTimeSeconds(
    double sourceTimeSeconds
)
{
    /*
        One time source only. The client reconstructs canonical universe time
        from the latest server anchor; render resources never advance, rewind
        or correct that time independently.
    */
    return std::isfinite(sourceTimeSeconds)
        ? sourceTimeSeconds
        : 0.0;
}

void MapCelestialRenderResources::drawStarfield(
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

}
