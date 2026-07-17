#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include <vector>

#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"

namespace render::celestial
{
    enum class ProceduralCloudPattern
    {
        ScatteredCumulus,
        BrokenFields,
        DenseOvercast,
        Banded
    };


    enum class ProceduralCloudMorphology
    {
        Terrestrial = 0,

        /*
            Юпитер и Сатурн:
            широтные пояса, вытянутые структуры,
            неодинаковая окраска слоёв.
        */
        GasGiantBanded = 1,

        /*
            Венера:
            полностью закрытый диск и длинные
            мягкие сернокислотные streak-структуры.
        */
        VenusianStreaks = 2,

        /*
            Уран и Нептун:
            мягкая полосатая база и редкие
            яркие высотные облака.
        */
        IceGiantBanded = 3
    };







    struct ProceduralCloudStyle
    {
        ProceduralCloudPattern pattern =
            ProceduralCloudPattern::BrokenFields;

        // Runtime seed. В JSON итоговый seed не сохраняется.
        std::uint32_t seed = 1337u;


        // Конкретный физический слой из Environment JSON.
        std::string layerId;
        std::string layerType;

        std::string renderRole;
        std::string visualClass;

        ProceduralCloudMorphology morphology =
            ProceduralCloudMorphology::Terrestrial;

        bool primaryOpaqueDeck = false;
        bool surfaceHidden = false;

        float opacityFloor = 0.0f;

        float baseHeightKm = 0.0f;
        float topHeightKm = 0.0f;

        float layerCoverage = 0.0f;
        float layerDensity = 0.0f;
        float particleScale = 1.0f;

        float windSpeedMps = 0.0f;
        float windDirectionDeg = 0.0f;



        int textureWidth = 1024;
        int textureHeight = 512;

        // Чем больше threshold, тем меньше облаков.
        float coverageThreshold = 0.56f;

        // Плотность результата генератора.
        float density = 0.82f;

        // Общая прозрачность слоя во время отрисовки.
        // В саму texture alpha повторно не запекается.
        float opacity = 0.34f;

        // Ширина и контраст рваной границы.
        float edgeSharpness = 0.18f;

        // Сила мелкой эрозии края.
        float detailStrength = 0.34f;

        // Масштаб огромных погодных регионов.
        float macroScale = 2.2f;

        // Масштаб облачных банков и скоплений.
        float cellScale = 12.0f;

        // Масштаб мелкой рваной кромки.
        float detailScale = 38.0f;

        // Скорость смещения текстуры по долготе.
        float driftSpeed = 0.000008f;

        glm::vec3 cloudColor {
            0.96f,
            0.985f,
            1.0f
        };

        glm::vec3 shadowColor {
            0.56f,
            0.66f,
            0.76f
        };


        float globalCoverage = 0.58f;

        world::celestial::visual::
            EnvironmentCloudGenerationProfile generation;

        world::celestial::visual::
            EnvironmentCloudCirculationProfile circulation;

        std::vector<
            world::celestial::visual::
                EnvironmentCloudLayerProfile
        > layers;

        bool enabled = true;
    };


class ProceduralCloudLayer
{
public:
    GLuint textureForStyle(
        const ProceduralCloudStyle& style,
        double timeSeconds
    );

    void beginFrame();

    void clearCache();

private:
    static constexpr int MaxClimateZones =
        8;



        struct GpuTextureEntry
    {
        GLuint previousTexture = 0;
        GLuint nextTexture = 0;
        GLuint futureTexture = 0;
        GLuint displayTexture = 0;

        int width = 0;
        int height = 0;

        /*
            Плавное локальное время визуальной анимации.
        */
        double animationTimeSeconds = 0.0;
        double lastSourceTimeSeconds = 0.0;

        /*
            Один и тот же style может быть запрошен несколько
            раз внутри одного render-frame.

            Анимационное время должно продвигаться только один
            раз за кадр, независимо от количества draw calls.
        */
        std::uint64_t lastAdvancedFrameSerial = 0;

        /*
            Времена двух состояний, между которыми выполняется blend.
        */
        double previousStateTimeSeconds = 0.0;
        double nextStateTimeSeconds = 0.0;

        /*
            Детерминированное смещение момента подготовки future state.
            Оно разносит тяжёлые GPU-pass разных слоёв по времени.
        */
        double updatePhaseSeconds = 0.0;

        double pendingFutureStateTimeSeconds = 0.0;
        bool futureGenerationPending = false;

        bool futureStatePrepared = false;
        bool initialized = false;
    };



    void ensureGpuResources();

    GLuint createGpuTexture(
        int width,
        int height
    );

    void generateGpuTexture(
        GLuint destinationTexture,
        int width,
        int height,
        const ProceduralCloudStyle& style,
        double stateTimeSeconds
    );

    void blendGpuTextures(
        GpuTextureEntry& entry,
        float transition
    );

    static std::string keyForStyle(
        const ProceduralCloudStyle& style
    );

private:
    std::unordered_map<
        std::string,
        GpuTextureEntry
    > m_textureByKey;

    GLuint m_generateFramebuffer = 0;
    GLuint m_generateVao = 0;
    GLuint m_generateShader = 0;

    GLuint m_blendShader = 0;

    GLint m_previousTextureLocation = -1;
    GLint m_nextTextureLocation = -1;
    GLint m_transitionLocation = -1;

    GLint m_seedLocation = -1;
    GLint m_patternLocation = -1;
    GLint m_morphologyLocation = -1;
    GLint m_primaryOpaqueDeckLocation = -1;
    GLint m_opacityFloorLocation = -1;
    GLint m_evolutionTimeLocation = -1;

    GLint m_globalCoverageLocation = -1;
    GLint m_layerDensityLocation = -1;

    GLint m_weatherScaleLocation = -1;
    GLint m_massScaleLocation = -1;
    GLint m_erosionScaleLocation = -1;
    GLint m_detailScaleLocation = -1;

    GLint m_domainWarpStrengthLocation = -1;
    GLint m_billowStrengthLocation = -1;
    GLint m_worleyErosionStrengthLocation = -1;

    GLint m_edgeFragmentStrengthLocation = -1;
    GLint m_edgeFragmentScaleLocation = -1;
    GLint m_detachedFragmentProbabilityLocation = -1;
    GLint m_softEdgeWidthLocation = -1;

    GLint m_cloudColorLocation = -1;
    GLint m_shadowColorLocation = -1;

    GLint m_zoneCountLocation = -1;
    GLint m_zoneALocation = -1;
    GLint m_zoneBLocation = -1;

    std::uint64_t m_frameSerial = 0;
    bool m_generationPerformedThisFrame = false;

    bool m_gpuWarningPrinted = false;
};



}