#include "src/render/celestial/ProceduralCloudLayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <sstream>

#include <glm/gtc/type_ptr.hpp>

#include "src/render/ShaderLibrary.h"

namespace render::celestial
{

namespace
{

int clampedTextureWidth(
    int requestedWidth
)
{
    return
        std::clamp(
            requestedWidth,
            256,
            2048
        );
}

int clampedTextureHeight(
    int requestedHeight
)
{
    return
        std::clamp(
            requestedHeight,
            128,
            1024
        );
}







double cloudLayerUpdatePhase(
    const ProceduralCloudStyle& style,
    double intervalSeconds
)
{
    /*
        Детерминированный фазовый сдвиг по seed и layerId.

        Разные слои получают разные точки обновления,
        но результат остаётся повторяемым.
    */
    std::uint32_t hash =
        style.seed ^
        0x9e3779b9u;

    for (const unsigned char character :
         style.layerId)
    {
        hash ^=
            static_cast<std::uint32_t>(
                character
            ) +
            0x9e3779b9u +
            (hash << 6u) +
            (hash >> 2u);
    }

    const double normalized =
        static_cast<double>(
            hash & 0xffffu
        ) /
        65535.0;

    return
        normalized *
        intervalSeconds *
        0.72;
}










} // namespace

void ProceduralCloudLayer::ensureGpuResources()
{
    if (m_generateShader == 0)
    {
        m_generateShader =
            ShaderLibrary::instance().get(
                "celestial_cloud_texture_generate"
            );

        if (m_generateShader == 0)
        {
            if (!m_gpuWarningPrinted)
            {
                m_gpuWarningPrinted = true;

                std::cerr
                    << "[ProceduralCloudLayer]"
                    << " GPU cloud generation shader not available"
                    << "\n";
            }

            return;
        }


        if (m_blendShader == 0)
        {
            m_blendShader =
                ShaderLibrary::instance().get(
                    "celestial_cloud_texture_blend"
                );

            if (m_blendShader != 0)
            {
                m_previousTextureLocation =
                    glGetUniformLocation(
                        m_blendShader,
                        "uPreviousTexture"
                    );

                m_nextTextureLocation =
                    glGetUniformLocation(
                        m_blendShader,
                        "uNextTexture"
                    );

                m_transitionLocation =
                    glGetUniformLocation(
                        m_blendShader,
                        "uTransition"
                    );
            }
        }





        m_seedLocation =
            glGetUniformLocation(
                m_generateShader,
                "uSeed"
            );

        m_patternLocation =
            glGetUniformLocation(
                m_generateShader,
                "uPattern"
            );


        m_morphologyLocation =
            glGetUniformLocation(
                m_generateShader,
                "uMorphology"
            );

        m_primaryOpaqueDeckLocation =
            glGetUniformLocation(
                m_generateShader,
                "uPrimaryOpaqueDeck"
            );

        m_opacityFloorLocation =
            glGetUniformLocation(
                m_generateShader,
                "uOpacityFloor"
            );
            

        m_evolutionTimeLocation =
            glGetUniformLocation(
                m_generateShader,
                "uEvolutionTime"
            );

        m_globalCoverageLocation =
            glGetUniformLocation(
                m_generateShader,
                "uGlobalCoverage"
            );

        m_layerDensityLocation =
            glGetUniformLocation(
                m_generateShader,
                "uLayerDensity"
            );

        m_weatherScaleLocation =
            glGetUniformLocation(
                m_generateShader,
                "uWeatherScale"
            );

        m_massScaleLocation =
            glGetUniformLocation(
                m_generateShader,
                "uMassScale"
            );

        m_erosionScaleLocation =
            glGetUniformLocation(
                m_generateShader,
                "uErosionScale"
            );

        m_detailScaleLocation =
            glGetUniformLocation(
                m_generateShader,
                "uDetailScale"
            );

        m_domainWarpStrengthLocation =
            glGetUniformLocation(
                m_generateShader,
                "uDomainWarpStrength"
            );

        m_billowStrengthLocation =
            glGetUniformLocation(
                m_generateShader,
                "uBillowStrength"
            );

        m_worleyErosionStrengthLocation =
            glGetUniformLocation(
                m_generateShader,
                "uWorleyErosionStrength"
            );

        m_edgeFragmentStrengthLocation =
            glGetUniformLocation(
                m_generateShader,
                "uEdgeFragmentStrength"
            );

        m_edgeFragmentScaleLocation =
            glGetUniformLocation(
                m_generateShader,
                "uEdgeFragmentScale"
            );

        m_detachedFragmentProbabilityLocation =
            glGetUniformLocation(
                m_generateShader,
                "uDetachedFragmentProbability"
            );

        m_softEdgeWidthLocation =
            glGetUniformLocation(
                m_generateShader,
                "uSoftEdgeWidth"
            );

        m_cloudColorLocation =
            glGetUniformLocation(
                m_generateShader,
                "uCloudColor"
            );

        m_shadowColorLocation =
            glGetUniformLocation(
                m_generateShader,
                "uShadowColor"
            );

        m_zoneCountLocation =
            glGetUniformLocation(
                m_generateShader,
                "uZoneCount"
            );

        m_zoneALocation =
            glGetUniformLocation(
                m_generateShader,
                "uZoneA[0]"
            );

        m_zoneBLocation =
            glGetUniformLocation(
                m_generateShader,
                "uZoneB[0]"
            );
    }

    if (m_generateFramebuffer == 0)
    {
        glGenFramebuffers(
            1,
            &m_generateFramebuffer
        );
    }

    if (m_generateVao == 0)
    {
        glGenVertexArrays(
            1,
            &m_generateVao
        );
    }
}

GLuint ProceduralCloudLayer::createGpuTexture(
    int width,
    int height
)
{
    GLuint texture = 0;

    glGenTextures(
        1,
        &texture
    );

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

    /*
        CPU pixels не передаются.

        nullptr означает:
        только выделить texture storage на GPU.
    */
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        width,
        height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glBindTexture(
        GL_TEXTURE_2D,
        0
    );

    return texture;
}

std::string ProceduralCloudLayer::keyForStyle(
    const ProceduralCloudStyle& style
)
{
    std::ostringstream stream;

    /*
        В ключ НЕ входит время.

        Время меняет содержимое существующей GPU texture,
        а не создаёт новую texture каждый кадр.
    */
    stream
        << "gpu_cloud_v1"
        << "|layer=" << style.layerId
        << "|type=" << style.layerType
        << "|seed=" << style.seed
        << "|pattern="
        << static_cast<int>(style.pattern)

        << "|size="
        << clampedTextureWidth(style.textureWidth)
        << "x"
        << clampedTextureHeight(style.textureHeight)

        << "|coverage="
        << style.globalCoverage

        << "|density="
        << style.density

        << "|weatherScale="
        << style.generation.weatherScale

        << "|massScale="
        << style.generation.massScale

        << "|erosionScale="
        << style.generation.erosionScale

        << "|detailScale="
        << style.generation.detailScale

        << "|warp="
        << style.generation.domainWarpStrength

        << "|billow="
        << style.generation.billowStrength

        << "|worley="
        << style.generation.worleyErosionStrength

        << "|fragmentStrength="
        << style.generation.edgeFragmentStrength

        << "|fragmentScale="
        << style.generation.edgeFragmentScale

        << "|fragmentProbability="
        << style.generation.detachedFragmentProbability

        << "|softEdge="
        << style.generation.softEdgeWidth

        << "|zones="
        << style.circulation.zones.size();

    return stream.str();
}

void ProceduralCloudLayer::generateGpuTexture(
    GLuint destinationTexture,
    int width,
    int height,
    const ProceduralCloudStyle& style,
    double stateTimeSeconds
)
{
    ensureGpuResources();

    if (m_generateShader == 0 ||
        m_generateFramebuffer == 0 ||
        m_generateVao == 0 ||
        destinationTexture == 0)
    {
        return;
    }

    /*
        Глобальное движение по долготе остаётся плавным
        в самом сферическом renderer-е.

        Здесь обновляется только форма облака.

        Чем выше driftSpeed, тем быстрее развивается форма.
    */
    const double minimumEvolutionSpeed =
        0.0015;

    const double evolutionSpeed =
        std::max(
            minimumEvolutionSpeed,
            std::abs(
                static_cast<double>(
                    style.driftSpeed
                )
            ) *
            24.0
        );

    const float evolutionTime =
        static_cast<float>(
            std::fmod(
                stateTimeSeconds *
                    evolutionSpeed,
                4096.0
            )
        );

    GLint previousFramebuffer = 0;
    GLint previousProgram = 0;
    GLint previousVao = 0;

    GLint previousViewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_FRAMEBUFFER_BINDING,
        &previousFramebuffer
    );

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_VIEWPORT,
        previousViewport
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

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        m_generateFramebuffer
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        destinationTexture,
        0
    );

    const GLenum framebufferStatus =
        glCheckFramebufferStatus(
            GL_FRAMEBUFFER
        );

    if (framebufferStatus !=
        GL_FRAMEBUFFER_COMPLETE)
    {
        static bool warned = false;

        if (!warned)
        {
            warned = true;

            std::cerr
                << "[ProceduralCloudLayer]"
                << " framebuffer incomplete: "
                << framebufferStatus
                << "\n";
        }

        glBindFramebuffer(
            GL_FRAMEBUFFER,
            static_cast<GLuint>(
                previousFramebuffer
            )
        );

        return;
    }

    glViewport(
        0,
        0,
        width,
        height
    );

    glDisable(
        GL_BLEND
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

    glUseProgram(
        m_generateShader
    );

    glUniform1f(
        m_seedLocation,
        static_cast<float>(
            style.seed %
            1000003u
        )
    );

    glUniform1i(
        m_patternLocation,
        static_cast<int>(
            style.pattern
        )
    );



    glUniform1i(
        m_morphologyLocation,
        static_cast<int>(
            style.morphology
        )
    );

    glUniform1i(
        m_primaryOpaqueDeckLocation,
        style.primaryOpaqueDeck
            ? 1
            : 0
    );

    glUniform1f(
        m_opacityFloorLocation,
        std::clamp(
            style.opacityFloor,
            0.0f,
            1.0f
        )
    );





    glUniform1f(
        m_evolutionTimeLocation,
        evolutionTime
    );

    glUniform1f(
        m_globalCoverageLocation,
        std::clamp(
            style.globalCoverage,
            0.01f,
            0.98f
        )
    );

    glUniform1f(
        m_layerDensityLocation,
        std::clamp(
            style.density,
            0.05f,
            1.25f
        )
    );

    glUniform1f(
        m_weatherScaleLocation,
        style.generation.weatherScale
    );

    glUniform1f(
        m_massScaleLocation,
        style.generation.massScale
    );

    glUniform1f(
        m_erosionScaleLocation,
        style.generation.erosionScale
    );

    glUniform1f(
        m_detailScaleLocation,
        style.generation.detailScale
    );

    glUniform1f(
        m_domainWarpStrengthLocation,
        style.generation.domainWarpStrength
    );

    glUniform1f(
        m_billowStrengthLocation,
        style.generation.billowStrength
    );

    glUniform1f(
        m_worleyErosionStrengthLocation,
        style.generation.worleyErosionStrength
    );

    glUniform1f(
        m_edgeFragmentStrengthLocation,
        style.generation.edgeFragmentStrength
    );

    glUniform1f(
        m_edgeFragmentScaleLocation,
        style.generation.edgeFragmentScale
    );

    glUniform1f(
        m_detachedFragmentProbabilityLocation,
        style.generation.detachedFragmentProbability
    );

    glUniform1f(
        m_softEdgeWidthLocation,
        style.generation.softEdgeWidth
    );

    glUniform3fv(
        m_cloudColorLocation,
        1,
        glm::value_ptr(
            style.cloudColor
        )
    );

    glUniform3fv(
        m_shadowColorLocation,
        1,
        glm::value_ptr(
            style.shadowColor
        )
    );

    std::array<
        glm::vec4,
        MaxClimateZones
    > zoneA {};

    std::array<
        glm::vec4,
        MaxClimateZones
    > zoneB {};

    const int zoneCount =
        std::min(
            static_cast<int>(
                style.circulation.zones.size()
            ),
            MaxClimateZones
        );

    for (int zoneIndex = 0;
         zoneIndex < zoneCount;
         ++zoneIndex)
    {
        const auto& zone =
            style.circulation.zones[
                static_cast<std::size_t>(
                    zoneIndex
                )
            ];

        zoneA[
            static_cast<std::size_t>(
                zoneIndex
            )
        ] =
            glm::vec4(
                zone.latitudeCenterDeg,
                zone.halfWidthDeg,
                zone.coverageMultiplier,
                zone.densityMultiplier
            );

        zoneB[
            static_cast<std::size_t>(
                zoneIndex
            )
        ] =
            glm::vec4(
                zone.fragmentation,
                zone.zonalStretch,
                zone.driftSpeedMultiplier,
                0.0f
            );
    }

    glUniform1i(
        m_zoneCountLocation,
        zoneCount
    );

    if (m_zoneALocation >= 0)
    {
        glUniform4fv(
            m_zoneALocation,
            MaxClimateZones,
            glm::value_ptr(
                zoneA[0]
            )
        );
    }

    if (m_zoneBLocation >= 0)
    {
        glUniform4fv(
            m_zoneBLocation,
            MaxClimateZones,
            glm::value_ptr(
                zoneB[0]
            )
        );
    }

    glBindVertexArray(
        m_generateVao
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

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        static_cast<GLuint>(
            previousFramebuffer
        )
    );

    glViewport(
        previousViewport[0],
        previousViewport[1],
        previousViewport[2],
        previousViewport[3]
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





void ProceduralCloudLayer::blendGpuTextures(
    GpuTextureEntry& entry,
    float transition
)
{
    ensureGpuResources();

    if (m_blendShader == 0 ||
        m_generateFramebuffer == 0 ||
        m_generateVao == 0 ||
        entry.previousTexture == 0 ||
        entry.nextTexture == 0 ||
        entry.displayTexture == 0)
    {
        return;
    }

    GLint previousFramebuffer = 0;
    GLint previousProgram = 0;
    GLint previousVao = 0;
    GLint previousActiveTexture = 0;

    GLint previousTexture0 = 0;
    GLint previousTexture1 = 0;

    GLint previousViewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_FRAMEBUFFER_BINDING,
        &previousFramebuffer
    );

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previousActiveTexture
    );

    glGetIntegerv(
        GL_VIEWPORT,
        previousViewport
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture0
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture1
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

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        m_generateFramebuffer
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        entry.displayTexture,
        0
    );

    if (glCheckFramebufferStatus(
            GL_FRAMEBUFFER
        ) != GL_FRAMEBUFFER_COMPLETE)
    {
        glBindFramebuffer(
            GL_FRAMEBUFFER,
            static_cast<GLuint>(
                previousFramebuffer
            )
        );

        return;
    }

    glViewport(
        0,
        0,
        entry.width,
        entry.height
    );

    glDisable(
        GL_BLEND
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

    glUseProgram(
        m_blendShader
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        entry.previousTexture
    );

    glUniform1i(
        m_previousTextureLocation,
        0
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glBindTexture(
        GL_TEXTURE_2D,
        entry.nextTexture
    );

    glUniform1i(
        m_nextTextureLocation,
        1
    );

    glUniform1f(
        m_transitionLocation,
        std::clamp(
            transition,
            0.0f,
            1.0f
        )
    );

    glBindVertexArray(
        m_generateVao
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

    glActiveTexture(
        GL_TEXTURE0
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture0
        )
    );

    glActiveTexture(
        GL_TEXTURE1
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture1
        )
    );

    glActiveTexture(
        static_cast<GLenum>(
            previousActiveTexture
        )
    );

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        static_cast<GLuint>(
            previousFramebuffer
        )
    );

    glViewport(
        previousViewport[0],
        previousViewport[1],
        previousViewport[2],
        previousViewport[3]
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






void ProceduralCloudLayer::beginFrame()
{
    ++m_frameSerial;
    m_generationPerformedThisFrame = false;
}








GLuint ProceduralCloudLayer::textureForStyle(
    const ProceduralCloudStyle& style,
    double timeSeconds
)
{
    if (!style.enabled)
        return 0;

    ensureGpuResources();

    if (m_generateShader == 0 ||
        m_blendShader == 0)
    {
        return 0;
    }

    const int width =
        clampedTextureWidth(
            style.textureWidth
        );

    const int height =
        clampedTextureHeight(
            style.textureHeight
        );

    const std::string key =
        keyForStyle(
            style
        );

    auto iterator =
        m_textureByKey.find(
            key
        );

    if (iterator ==
        m_textureByKey.end())
    {
        GpuTextureEntry entry;

        entry.width =
            width;

        entry.height =
            height;


            


        entry.previousTexture =
            createGpuTexture(
                width,
                height
            );

        entry.nextTexture =
            createGpuTexture(
                width,
                height
            );

        entry.futureTexture =
            createGpuTexture(
                width,
                height
            );

        entry.displayTexture =
            createGpuTexture(
                width,
                height
            );




        iterator =
            m_textureByKey.emplace(
                key,
                entry
            ).first;

            std::cout
                << "[ProceduralCloudLayer]"
                << " GPU texture set allocated"
                << " layer=" << style.layerId
                << " size=" << width
                << "x" << height
                << " previous="
                << iterator->second.previousTexture
                << " next="
                << iterator->second.nextTexture
                << " future="
                << iterator->second.futureTexture
                << " display="
                << iterator->second.displayTexture
                << "\n";
    }

    GpuTextureEntry& entry =
        iterator->second;


        
/*
    Расстояние между двумя ключевыми procedural-состояниями.

    Само изображение меняется каждый кадр через blend.
*/
constexpr double stateIntervalSeconds =
    2.40;

/*
    Не позволяем внешнему серверному времени заставить
    визуальный renderer одномоментно догонять большой скачок.
*/
constexpr double maximumAnimationDeltaSeconds =
    0.10;

if (!entry.initialized)
{
    entry.lastSourceTimeSeconds =
        timeSeconds;

    entry.updatePhaseSeconds =
        cloudLayerUpdatePhase(
            style,
            stateIntervalSeconds
        );

    /*
        ВАЖНО:

        previous и next всегда образуют интервал [0; 2.4].

        Фазовый сдвиг задаётся animationTimeSeconds,
        а не смещением previousStateTimeSeconds.
        Поэтому слой сразу начинает интерполироваться,
        а не замирает перед началом своего интервала.
    */
    entry.previousStateTimeSeconds =
        0.0;

    entry.nextStateTimeSeconds =
        stateIntervalSeconds;

    entry.animationTimeSeconds =
        std::fmod(
            entry.updatePhaseSeconds,
            stateIntervalSeconds
        );

    generateGpuTexture(
        entry.previousTexture,
        entry.width,
        entry.height,
        style,
        entry.previousStateTimeSeconds
    );

    generateGpuTexture(
        entry.nextTexture,
        entry.width,
        entry.height,
        style,
        entry.nextStateTimeSeconds
    );

    /*
        Future пока не создаём, чтобы не делать три тяжёлых
        generation-pass сразу при первом открытии карты.
    */
    entry.futureStatePrepared = false;
    entry.initialized = true;
}

/*
    Получаем delta от внешнего времени.
*/
double sourceDelta =
    timeSeconds -
    entry.lastSourceTimeSeconds;

entry.lastSourceTimeSeconds =
    timeSeconds;

if (!std::isfinite(sourceDelta) ||
    sourceDelta < 0.0)
{
    sourceDelta = 0.0;
}

/*
    Если snapshot использует одно и то же время несколько кадров,
    визуальная анимация всё равно должна идти.

    Это временный fallback. Позже лучше передавать сюда
    настоящее render-frame delta.
*/
constexpr double nominalFrameDeltaSeconds =
    1.0 / 60.0;

if (sourceDelta <= 0.000001)
{
    sourceDelta =
        nominalFrameDeltaSeconds;
}

sourceDelta =
    std::clamp(
        sourceDelta,
        0.0,
        maximumAnimationDeltaSeconds
    );

entry.animationTimeSeconds +=
    sourceDelta;

const double intervalLength =
    std::max(
        0.000001,
        entry.nextStateTimeSeconds -
            entry.previousStateTimeSeconds
    );

float transition =
    static_cast<float>(
        std::clamp(
            (
                entry.animationTimeSeconds -
                    entry.previousStateTimeSeconds
            ) /
            intervalLength,
            0.0,
            1.0
        )
    );

/*
    Когда слой достаточно продвинулся в текущем интервале,
    помечаем future generation как pending.

    ВАЖНО:
    здесь мы не обязаны генерировать texture немедленно.
*/
constexpr float futurePreparationBase =
    0.58f;

const float phaseNormalized =
    static_cast<float>(
        entry.updatePhaseSeconds /
        stateIntervalSeconds
    );

const float preparationThreshold =
    std::clamp(
        futurePreparationBase +
            phaseNormalized * 0.20f,
        0.55f,
        0.82f
    );

if (!entry.futureStatePrepared &&
    !entry.futureGenerationPending &&
    transition >= preparationThreshold)
{
    entry.pendingFutureStateTimeSeconds =
        entry.nextStateTimeSeconds +
        stateIntervalSeconds;

    entry.futureGenerationPending = true;
}

/*
    Бюджет:
    не больше одной тяжёлой cloud-generation за кадр.

    Это главный фикс против подёргиваний.
*/
if (entry.futureGenerationPending &&
    !m_generationPerformedThisFrame)
{
    generateGpuTexture(
        entry.futureTexture,
        entry.width,
        entry.height,
        style,
        entry.pendingFutureStateTimeSeconds
    );

    entry.futureStatePrepared = true;
    entry.futureGenerationPending = false;
    m_generationPerformedThisFrame = true;
}

/*
    На самой границе интервала мы НЕ генерируем texture.

    Если future ещё не готова,
    просто удерживаем анимацию почти на 1.0,
    пока будущий state не будет подготовлен
    в одном из следующих кадров.
*/
if (entry.animationTimeSeconds >=
    entry.nextStateTimeSeconds)
{
    if (!entry.futureStatePrepared)
    {
        entry.animationTimeSeconds =
            entry.nextStateTimeSeconds -
            0.000001;

        transition = 0.9999f;
    }
    else
    {
        const GLuint reusableTexture =
            entry.previousTexture;

        entry.previousTexture =
            entry.nextTexture;

        entry.nextTexture =
            entry.futureTexture;

        entry.futureTexture =
            reusableTexture;

        entry.previousStateTimeSeconds =
            entry.nextStateTimeSeconds;

        entry.nextStateTimeSeconds =
            entry.previousStateTimeSeconds +
            stateIntervalSeconds;

        entry.futureStatePrepared = false;
        entry.futureGenerationPending = false;
        entry.pendingFutureStateTimeSeconds = 0.0;

        entry.animationTimeSeconds =
            std::clamp(
                entry.animationTimeSeconds,
                entry.previousStateTimeSeconds,
                entry.nextStateTimeSeconds -
                    0.000001
            );

        transition =
            static_cast<float>(
                std::clamp(
                    (
                        entry.animationTimeSeconds -
                            entry.previousStateTimeSeconds
                    ) /
                    stateIntervalSeconds,
                    0.0,
                    1.0
                )
            );
    }
}

blendGpuTextures(
    entry,
    transition
);

return
    entry.displayTexture;




}





void ProceduralCloudLayer::clearCache()
{
    for (auto& item :
         m_textureByKey)
    {
        auto& entry =
            item.second;

        if (entry.previousTexture != 0)
        {
            glDeleteTextures(
                1,
                &entry.previousTexture
            );
        }

        if (entry.nextTexture != 0)
        {
            glDeleteTextures(
                1,
                &entry.nextTexture
            );
        }

        if (entry.futureTexture != 0)
        {
            glDeleteTextures(
                1,
                &entry.futureTexture
            );
        }

        if (entry.displayTexture != 0)
        {
            glDeleteTextures(
                1,
                &entry.displayTexture
            );
        }
    }

    m_textureByKey.clear();
}




} // namespace render::celestial