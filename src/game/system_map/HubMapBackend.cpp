#include "src/game/system_map/HubMapBackend.h"
#include "src/game/system_map/LocalMapPrimitiveRenderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include <GLFW/glfw3.h>

#include "render/HUD/TextRenderer.h"

namespace
{
    double hubPerfNowMs()
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double, std::milli>(
            Clock::now().time_since_epoch()
        ).count();
    }
}

namespace game::system_map
{
HubMapBackend::HubMapBackend(
    MapCelestialRenderResources& resources
) noexcept
    : m_resources(resources),
      m_geometryPass(*this, resources.hubVisuals()),
      m_planetPass(resources, *this)
{
}


const LocalMapCameraSnapshot&
HubMapBackend::activeCamera() const
{
    if (!m_activeCamera)
        throw std::logic_error("HubMapBackend camera is unavailable");

    return *m_activeCamera;
}


void HubMapBackend::ensureGpuQueries()
{
    if (m_gpuQueriesInitialized)
        return;

    for (auto& slot : m_gpuQueries)
    {
        glGenQueries(
            static_cast<GLsizei>(
                slot.size()
            ),
            slot.data()
        );
    }

    m_gpuQueriesInitialized = true;
}


void HubMapBackend::collectGpuQueries()
{
    if (!m_gpuQueriesInitialized)
        return;

    for (std::size_t slotIndex = 0;
         slotIndex < kGpuQuerySlotCount;
         ++slotIndex)
    {
        if (!m_gpuSlotPending[slotIndex])
            continue;

        const std::uint32_t issuedMask =
            m_gpuIssuedMasks[slotIndex];

        bool allAvailable = true;

        for (std::size_t stageIndex = 0;
             stageIndex < kGpuStageCount;
             ++stageIndex)
        {
            const std::uint32_t stageBit =
                1u <<
                static_cast<std::uint32_t>(
                    stageIndex
                );

            if ((issuedMask & stageBit) == 0u)
                continue;

            GLint available = GL_FALSE;

            glGetQueryObjectiv(
                m_gpuQueries[slotIndex][stageIndex],
                GL_QUERY_RESULT_AVAILABLE,
                &available
            );

            if (available != GL_TRUE)
            {
                allAvailable = false;
                break;
            }
        }

        if (!allAvailable)
            continue;

        std::array<
            double,
            kGpuStageCount
        > stageMilliseconds {};

        for (std::size_t stageIndex = 0;
             stageIndex < kGpuStageCount;
             ++stageIndex)
        {
            const std::uint32_t stageBit =
                1u <<
                static_cast<std::uint32_t>(
                    stageIndex
                );

            if ((issuedMask & stageBit) == 0u)
                continue;

            GLuint64 elapsedNanoseconds = 0;

            glGetQueryObjectui64v(
                m_gpuQueries[slotIndex][stageIndex],
                GL_QUERY_RESULT,
                &elapsedNanoseconds
            );

            stageMilliseconds[stageIndex] =
                static_cast<double>(
                    elapsedNanoseconds
                ) /
                1000000.0;
        }

        const std::uint64_t slotSerial =
            m_gpuSlotSerials[slotIndex];

        /*
            Если сразу готовы несколько старых кадров,
            сохраняем самый новый из них.
        */
        if (slotSerial >
            m_gpuLastCollectedSerial)
        {
            auto stageMs =
                [&](
                    GpuStage stage
                ) -> double
                {
                    return
                        stageMilliseconds[
                            static_cast<std::size_t>(
                                stage
                            )
                        ];
                };

            m_performanceStats.gpuBackgroundMs =
                stageMs(
                    GpuStage::Background
                );

            m_performanceStats.gpuFallbackBodyMs =
                stageMs(
                    GpuStage::FallbackBody
                );

            m_performanceStats.gpuSurfaceMs =
                stageMs(
                    GpuStage::Surface
                );

            m_performanceStats.gpuCloudsMs =
                stageMs(
                    GpuStage::Clouds
                );

            m_performanceStats.gpuAtmosphereMs =
                stageMs(
                    GpuStage::Atmosphere
                );

            m_performanceStats.gpuGeometryMs =
                stageMs(
                    GpuStage::Geometry
                );

            m_performanceStats.gpuLabelsMs =
                stageMs(
                    GpuStage::Labels
                );

            m_performanceStats.gpuTotalMs =
                m_performanceStats.gpuBackgroundMs +
                m_performanceStats.gpuFallbackBodyMs +
                m_performanceStats.gpuSurfaceMs +
                m_performanceStats.gpuCloudsMs +
                m_performanceStats.gpuAtmosphereMs +
                m_performanceStats.gpuGeometryMs +
                m_performanceStats.gpuLabelsMs;

            m_performanceStats.gpuValid =
                true;

            m_gpuLastCollectedSerial =
                slotSerial;
        }

        m_gpuSlotPending[slotIndex] = false;
        m_gpuIssuedMasks[slotIndex] = 0u;
    }
}


void HubMapBackend::beginGpuFrame()
{
    ensureGpuQueries();
    collectGpuQueries();

    ++m_gpuFrameSerial;

    m_gpuCurrentSlot =
        static_cast<std::size_t>(
            m_gpuFrameSerial %
            kGpuQuerySlotCount
        );

    /*
        Если GPU ещё не закончил старый кадр в этом slot,
        текущий кадр просто не профилируем.

        Главное — не блокировать render thread.
    */
    if (m_gpuSlotPending[
            m_gpuCurrentSlot
        ])
    {
        m_gpuFrameActive = false;
        m_gpuStageOpen = false;
        return;
    }

    m_gpuIssuedMasks[
        m_gpuCurrentSlot
    ] = 0u;

    m_gpuSlotSerials[
        m_gpuCurrentSlot
    ] = m_gpuFrameSerial;

    m_gpuFrameActive = true;
    m_gpuStageOpen = false;
}


void HubMapBackend::endGpuFrame()
{
    if (m_gpuStageOpen)
    {
        endGpuStage();
    }

    if (m_gpuFrameActive)
    {
        m_gpuSlotPending[
            m_gpuCurrentSlot
        ] =
            m_gpuIssuedMasks[
                m_gpuCurrentSlot
            ] != 0u;
    }

    m_gpuFrameActive = false;
    m_gpuStageOpen = false;
}


void HubMapBackend::beginGpuStage(
    GpuStage stage
)
{
    if (!m_gpuFrameActive)
        return;

    /*
        GL_TIME_ELAPSED queries нельзя вкладывать друг в друга.
    */
    if (m_gpuStageOpen)
    {
        endGpuStage();
    }

    const std::size_t stageIndex =
        static_cast<std::size_t>(
            stage
        );

    glBeginQuery(
        GL_TIME_ELAPSED,
        m_gpuQueries[
            m_gpuCurrentSlot
        ][
            stageIndex
        ]
    );

    m_gpuIssuedMasks[
        m_gpuCurrentSlot
    ] |=
        1u <<
        static_cast<std::uint32_t>(
            stageIndex
        );

    m_gpuStageOpen = true;
}


void HubMapBackend::endGpuStage()
{
    if (!m_gpuFrameActive ||
        !m_gpuStageOpen)
    {
        return;
    }

    glEndQuery(
        GL_TIME_ELAPSED
    );

    m_gpuStageOpen = false;
}








void HubMapBackend::renderHubMapPasses(
    const game::system_map::HubMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* previousCameraSnapshot = m_activeCamera;
    m_activeCamera = &presentation.camera;

    struct RestoreCameraSnapshot
    {
        const LocalMapCameraSnapshot*& slot;
        const LocalMapCameraSnapshot* previous;
        ~RestoreCameraSnapshot() { slot = previous; }
    } restoreCameraSnapshot {
        m_activeCamera,
        previousCameraSnapshot
    };

    const double cpuTotalStartMs =
        hubPerfNowMs();

    m_performanceStats.cpuTotalMs = 0.0;
    m_performanceStats.cpuBackgroundMs = 0.0;
    m_performanceStats.cpuPlanetBackdropMs = 0.0;
    m_performanceStats.cpuGeometryMs = 0.0;
    m_performanceStats.cpuLabelsMs = 0.0;

    beginGpuFrame();

    const double cpuBackgroundStartMs =
        hubPerfNowMs();

    beginGpuStage(
        GpuStage::Background
    );


    m_resources.ensureGeneratedCelestialAssets();

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
        m_resources.hubVisuals().backgroundColor.r,
        m_resources.hubVisuals().backgroundColor.g,
        m_resources.hubVisuals().backgroundColor.b,
        m_resources.hubVisuals().backgroundColor.a
    );

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();


    if (!hub.valid)
    {
        endGpuStage();
        endGpuFrame();

        m_performanceStats.cpuBackgroundMs =
            hubPerfNowMs() -
            cpuBackgroundStartMs;

        restoreGlState();

        m_performanceStats.cpuTotalMs =
            hubPerfNowMs() -
            cpuTotalStartMs;

        return;
    }


    if (m_resources.hubVisuals().drawStarfield)
    {
        const auto& visuals =
            m_resources.hubVisuals();

        m_resources.drawStarfield(
            viewport,
            hub.systemPositionLy,
            presentation.camera.starfieldViewMatrix(),
            visuals.starfieldFieldOfViewDeg,
            visuals.starfieldSizeScale,
            false,
            visuals.starfieldBrightnessScale,
            visuals.milkyWayIntensityScale,
            visuals.milkyWayColorTint
        );
    }

    endGpuStage();

    m_performanceStats.cpuBackgroundMs =
        hubPerfNowMs() -
        cpuBackgroundStartMs;


    const glm::dvec2& centerPx =
        presentation.centerPx;
    const double scale =
        presentation.scale;
    const double finalScale =
        scale * activeCamera().state.zoom;



   const double cpuPlanetBackdropStartMs =
        hubPerfNowMs();

    m_planetPass.drawHubMapPlanetSurfaceHint(
        hub,
        scale,
        centerPx
    );

m_performanceStats.cpuPlanetBackdropMs =
    hubPerfNowMs() -
    cpuPlanetBackdropStartMs;

    /*
        The spherical Hub grid remains disabled exactly as before. Its
        renderer and cached planet geometry now belong to HubMapPlanetPass;
        HubMapBackend only coordinates enabled passes.
    */


    const double cpuGeometryStartMs =
        hubPerfNowMs();

    beginGpuStage(
        GpuStage::Geometry
    );


    m_geometryPass.beginFrame(
        viewport.width,
        viewport.height,

        /*
            В старом hubMapProject screen origin был:
                center + camera.pan
        */
        glm::dvec2(
            centerPx.x +
                activeCamera().state.pan.x,

            centerPx.y +
                activeCamera().state.pan.y
        ),

        /*
            Старый finalScale:
                scale * camera.zoom
        */
        finalScale,

        activeCamera().state.yaw,
        activeCamera().state.pitch
    );



    // Оси хаба.
    m_geometryPass.drawHubMapAxes(
        glm::dvec3(0.0),
        hub.hubAxes,
        900.0,
        scale,
        centerPx
    );

    // Центр хаба / текущая орбитальная точка.
    const glm::dvec2 hubOriginScreen =
        activeCamera().project(glm::dvec3(0.0));

    const glm::vec4 hubOriginColor(
        1.0f,
        0.86f,
        0.35f,
        0.95f
    );

    if (m_geometryPass.active())
    {
        m_geometryPass.submitScreenCross(
            hubOriginScreen,
            6.0,
            hubOriginColor
        );
    }
    else
    {
        glColor4f(
            hubOriginColor.r,
            hubOriginColor.g,
            hubOriginColor.b,
            hubOriginColor.a
        );

        drawLocalMapCross(
            hubOriginScreen,
            6.0f
        );
    }




    // Модули станции.
    for (const auto& mod : hub.scene.objects)
    {
        if (!mod.valid ||
            mod.objectClass !=
                world::celestial::DetailObjectClass::Hub)
        {
            continue;
        }

        const glm::dvec2 modScreen =
            activeCamera().project(mod.positionMeters);

        const double moduleRadiusMeters =
            glm::length(
                mod.sizeMeters
            ) * 0.5;

        const double moduleRadiusPx =
            moduleRadiusMeters *
            finalScale;

        const glm::vec4 moduleWireColor =
            mod.prime ||
            mod.kind == "station"
                ? m_resources.hubVisuals().primeModuleWireColor
                : m_resources.hubVisuals().regularModuleWireColor;

        const bool modelDrawn =
            m_geometryPass.drawHubMapAssemblyWire(
                mod.typeId,
                mod.positionMeters,
                mod.axes,
                moduleWireColor
            );

        if (!modelDrawn)
        {
            m_geometryPass.drawHubMapBox(
                mod.positionMeters,
                mod.axes,
                mod.sizeMeters,
                moduleWireColor,
                scale,
                centerPx
            );
        }

        const double moduleAxisLen =
            std::max(
                350.0,
                glm::length(mod.sizeMeters) * 0.08
            );

        m_geometryPass.drawHubMapAxes(
            mod.positionMeters,
            mod.axes,
            moduleAxisLen,
            scale,
            centerPx
        );

        // Если модуль на текущем масштабе слишком мелкий,
        // добавляем screen-space маркер. Это не физический размер.
        if (moduleRadiusPx < m_resources.hubVisuals().moduleMarkerThresholdPx)
        {
            const glm::vec4 markerColor =
                mod.prime
                    ? m_resources.hubVisuals().primeModuleMarkerColor
                    : m_resources.hubVisuals().regularModuleMarkerColor;

            m_geometryPass.drawHubMapScreenMarker(
                modScreen,
                mod.prime
                    ? m_resources.hubVisuals().primeModuleMarkerRadiusPx
                    : m_resources.hubVisuals().regularModuleMarkerRadiusPx,
                markerColor,
                mod.prime,
                m_resources.hubVisuals().moduleMarkerSegments
            );
        }
    }





    // Игрок / корабли.
    for (const auto& ship : hub.scene.objects)
    {
        if (!ship.valid ||
            ship.objectClass !=
                world::celestial::DetailObjectClass::Ship)
        {
            continue;
        }

        const glm::dvec2 shipScreen =
            activeCamera().project(ship.positionMeters);

        const glm::dvec3 shipVisualSize =
            m_geometryPass.visualSizeForHubShip(
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

       const glm::vec4 shipWireColor =
        ship.player
            ? m_resources.hubVisuals().playerShipWireColor
            : m_resources.hubVisuals().regularShipWireColor;

        // Если корабль на карте слишком маленький, wire-модель будет шумом.
        // Тогда рисуем fallback box с увеличенным visual size.
        const bool allowWireModel =
            shipRadiusPx >= 10.0;

        bool shipModelDrawn =
            false;

        if (allowWireModel)
        {
            shipModelDrawn =
                m_geometryPass.drawHubMapAssemblyWire(
                    ship.typeId,
                    ship.positionMeters,
                    ship.axes,
                    shipWireColor
                );
        }

        if (!shipModelDrawn)
        {
            m_geometryPass.drawHubMapBox(
                ship.positionMeters,
                ship.axes,
                shipVisualSize,
                shipWireColor,
                scale,
                centerPx
            );
        }

        const double shipAxisLen =
            std::max(
                ship.player ? 26.0 : 16.0,
                glm::length(shipVisualSize) * 0.65
            );

        m_geometryPass.drawHubMapAxes(
            ship.positionMeters,
            ship.axes,
            shipAxisLen,
            scale,
            centerPx
        );

        m_geometryPass.drawHubMapVelocityArrow(
            ship.positionMeters,
            ship.velocityMps,
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
            shipRadiusPx < m_resources.hubVisuals().shipMarkerThresholdPx)
        {
            const glm::vec4 markerColor =
                ship.player
                    ? m_resources.hubVisuals().playerShipMarkerColor
                    : m_resources.hubVisuals().regularShipMarkerColor;

            m_geometryPass.drawHubMapScreenMarker(
                shipScreen,
                ship.player
                    ? m_resources.hubVisuals().playerShipMarkerRadiusPx
                    : m_resources.hubVisuals().regularShipMarkerRadiusPx,
                markerColor,
                true,
                m_resources.hubVisuals().shipMarkerSegments
            );
        }
    }


        m_geometryPass.flush();
        endGpuStage();

        m_performanceStats.cpuGeometryMs =
            hubPerfNowMs() -
            cpuGeometryStartMs;

        const double cpuLabelsStartMs =
            hubPerfNowMs();

        beginGpuStage(
            GpuStage::Labels
        );


        {
            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                viewport.width,
                viewport.height
            );

            for (const auto& mod : hub.scene.objects)
            {
                if (!mod.valid ||
                    mod.objectClass !=
                        world::celestial::DetailObjectClass::Hub)
                {
                    continue;
                }

                const glm::dvec2 p =
                    activeCamera().project(mod.positionMeters);


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
                    m_resources.hubVisuals().primaryLabelPx,
                    m_resources.hubVisuals().moduleLabelColor
                );

                if (!mod.kind.empty())
                {
                    text.textDrawPx(
                        mod.kind,
                        static_cast<float>(p.x + 10.0),
                        static_cast<float>(p.y + 8.0),
                        m_resources.hubVisuals().secondaryLabelPx,
                        m_resources.hubVisuals().moduleSubtitleColor
                    );
                }
            }

            for (const auto& ship : hub.scene.objects)
            {
                if (!ship.valid ||
                    ship.objectClass !=
                        world::celestial::DetailObjectClass::Ship)
                {
                    continue;
                }

                const glm::dvec2 p =
                    activeCamera().project(ship.positionMeters);





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
                    m_resources.hubVisuals().primaryLabelPx,
                    m_resources.hubVisuals().shipLabelColor
                );
            }

            text.endFrame();
        }



        endGpuStage();

        m_performanceStats.cpuLabelsMs =
            hubPerfNowMs() -
            cpuLabelsStartMs;








    endGpuFrame();

    restoreGlState();

    m_performanceStats.cpuTotalMs =
        hubPerfNowMs() -
        cpuTotalStartMs;

}




}
