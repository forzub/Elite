#include "src/game/system_map/HubMapGeometryPass.h"
#include "src/game/system_map/SystemMapRenderer.h"

#include <algorithm>
#include <cmath>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/world/modules/ObjectAssemblyTransformUtils.h"

namespace game::system_map
{
void HubMapGeometryPass::drawHubMapBox(
    const glm::dvec3& center,
    const world::celestial::LocalSceneAxes& axes,
    const glm::dvec3& size,
    const glm::vec4& color,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec3 hx =
        axes.x * (size.x * 0.5);

    const glm::dvec3 hy =
        axes.y * (size.y * 0.5);

    const glm::dvec3 hz =
        axes.z * (size.z * 0.5);

    glm::dvec3 points[8];

    points[0] = center - hx - hy - hz;
    points[1] = center + hx - hy - hz;
    points[2] = center + hx + hy - hz;
    points[3] = center - hx + hy - hz;

    points[4] = center - hx - hy + hz;
    points[5] = center + hx - hy + hz;
    points[6] = center + hx + hy + hz;
    points[7] = center - hx + hy + hz;

    constexpr int edges[12][2] =
    {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    if (m_gpuGeometryRenderer.active())
    {
        for (const auto& edge : edges)
        {
            m_gpuGeometryRenderer.submitHubLine(
                points[edge[0]],
                points[edge[1]],
                color
            );
        }

        return;
    }

    /*
        Старый путь остаётся fallback-ом,
        если новые shaders не загрузились.
    */
    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    for (const auto& edge : edges)
    {
        m_host.drawPlanetMapLine(
            m_host.activeLocalCameraSnapshot().project(points[edge[0]]),
            m_host.activeLocalCameraSnapshot().project(points[edge[1]])
        );
    }
}



void HubMapGeometryPass::drawHubMapAxes(
    const glm::dvec3& center,
    const world::celestial::LocalSceneAxes& axes,
    double axisLenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::vec4 xColor(
        1.0f,
        0.2f,
        0.2f,
        0.95f
    );

    const glm::vec4 yColor(
        0.2f,
        1.0f,
        0.2f,
        0.95f
    );

    const glm::vec4 zColor(
        0.25f,
        0.55f,
        1.0f,
        0.95f
    );

    if (m_gpuGeometryRenderer.active())
    {
        m_gpuGeometryRenderer.submitHubLine(
            center,
            center + axes.x * axisLenMeters,
            xColor
        );

        m_gpuGeometryRenderer.submitHubLine(
            center,
            center + axes.y * axisLenMeters,
            yColor
        );

        m_gpuGeometryRenderer.submitHubLine(
            center,
            center + axes.z * axisLenMeters,
            zColor
        );

        return;
    }

    const glm::dvec2 origin =
        m_host.activeLocalCameraSnapshot().project(center);

    glColor4f(xColor.r, xColor.g, xColor.b, xColor.a);

    m_host.drawPlanetMapLine(
        origin,
        m_host.activeLocalCameraSnapshot().project(center + axes.x * axisLenMeters)
    );

    glColor4f(yColor.r, yColor.g, yColor.b, yColor.a);

    m_host.drawPlanetMapLine(
        origin,
        m_host.activeLocalCameraSnapshot().project(center + axes.y * axisLenMeters)
    );

    glColor4f(zColor.r, zColor.g, zColor.b, zColor.a);

    m_host.drawPlanetMapLine(
        origin,
        m_host.activeLocalCameraSnapshot().project(center + axes.z * axisLenMeters)
    );
}



void HubMapGeometryPass::drawHubMapVelocityArrow(
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

    const glm::dvec3 direction =
        velocity / speed;

    const glm::vec4 color(
        1.0f,
        0.9f,
        0.25f,
        0.95f
    );

    if (m_gpuGeometryRenderer.active())
    {
        m_gpuGeometryRenderer.submitHubLine(
            center,
            center + direction * lenMeters,
            color
        );

        return;
    }

    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    m_host.drawPlanetMapLine(
        m_host.activeLocalCameraSnapshot().project(center),
        m_host.activeLocalCameraSnapshot().project(center + direction * lenMeters)
    );
}



void HubMapGeometryPass::drawHubMapScreenMarker(
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



    if (m_gpuGeometryRenderer.active())
    {
        m_gpuGeometryRenderer.submitScreenCircle(
            screenPx,
            radiusPx,
            color,
            segments
        );

        if (drawCross)
        {
            m_gpuGeometryRenderer.submitScreenCross(
                screenPx,
                radiusPx * 0.62,
                color
            );
        }

        return;
    }








    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    m_host.drawPlanetMapCircle(
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



bool HubMapGeometryPass::drawHubMapAssemblyWire(
    ObjectType typeId,
    const glm::dvec3& objectCenter,
    const world::celestial::LocalSceneAxes& objectAxes,
    const glm::vec4& color
)
{
    using game::ship::geometry::AssemblyMeshLibrary;

    if (!m_gpuGeometryRenderer.active() ||
        typeId == ObjectType::None)
    {
        return false;
    }

    if (!AssemblyMeshLibrary::has(typeId))
        return false;

    const auto& assembly =
        AssemblyMeshLibrary::get(typeId);

    /*
        Локальная система объекта → hub-local meters.

        Столбцы матрицы:
            0 = object X
            1 = object Y
            2 = object Z
            3 = object center
    */
    glm::mat4 objectToHub(1.0f);

    objectToHub[0] =
        glm::vec4(
            glm::vec3(objectAxes.x),
            0.0f
        );

    objectToHub[1] =
        glm::vec4(
            glm::vec3(objectAxes.y),
            0.0f
        );

    objectToHub[2] =
        glm::vec4(
            glm::vec3(objectAxes.z),
            0.0f
        );

    objectToHub[3] =
        glm::vec4(
            glm::vec3(objectCenter),
            1.0f
        );

    /*
        Сохраняем прежнюю логику:
        при наличии whole-object proxy модульную
        сборку не рисуем.
    */
    if (assembly.hasWholeShipProxy &&
        assembly.wholeShipProxyGpu.getEdgeVertexCount() > 0)
    {
        m_gpuGeometryRenderer.submitMesh(
            assembly.wholeShipProxyGpu,
            objectToHub,
            color
        );

        return true;
    }

    bool submittedAnything = false;

    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            const auto& meshGpu =
                part.lod1Gpu.getEdgeVertexCount() > 0
                    ? part.lod1Gpu
                    : part.lod0Gpu;

            if (meshGpu.getEdgeVertexCount() == 0)
                continue;

            const glm::mat4 moduleToObject =
                world::modules::
                    buildAssemblyModuleStaticHierarchicalLocalModel(
                        assembly,
                        module.id
                    );

            const glm::mat4 partToModule =
                glm::translate(
                    glm::mat4(1.0f),
                    part.localOffset
                );

            m_gpuGeometryRenderer.submitMesh(
                meshGpu,
                objectToHub *
                    moduleToObject *
                    partToModule,
                color
            );

            submittedAnything = true;
        }
    }

    return submittedAnything;
}



void HubMapGeometryPass::drawHubMapAdaptiveGrid(
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

    while ((gridStep * scale * m_host.activeLocalCameraSnapshot().state.zoom) < 28.0)
        gridStep *= 2.0;

    while ((gridStep * scale * m_host.activeLocalCameraSnapshot().state.zoom) > 90.0 &&
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
        m_host.m_hubVisuals.localGridColor.r,
        m_host.m_hubVisuals.localGridColor.g,
        m_host.m_hubVisuals.localGridColor.b,
        m_host.m_hubVisuals.localGridColor.a
    );

    for (int i = -gridN; i <= gridN; ++i)
    {
        const double v =
            static_cast<double>(i) *
            gridStep;

        m_host.drawPlanetMapLine(
            m_host.activeLocalCameraSnapshot().project(glm::dvec3(-gridN * gridStep, 0.0, v)),
            m_host.activeLocalCameraSnapshot().project(glm::dvec3( gridN * gridStep, 0.0, v))
        );

        m_host.drawPlanetMapLine(
            m_host.activeLocalCameraSnapshot().project(glm::dvec3(v, 0.0, -gridN * gridStep)),
            m_host.activeLocalCameraSnapshot().project(glm::dvec3(v, 0.0,  gridN * gridStep))
        );
    }

    // Главные оси плоскости хаба.
    glColor4f(
        m_host.m_hubVisuals.localGridAxisColor.r,
        m_host.m_hubVisuals.localGridAxisColor.g,
        m_host.m_hubVisuals.localGridAxisColor.b,
        m_host.m_hubVisuals.localGridAxisColor.a
    );

    m_host.drawPlanetMapLine(
        m_host.activeLocalCameraSnapshot().project(glm::dvec3(-gridN * gridStep, 0.0, 0.0)),
        m_host.activeLocalCameraSnapshot().project(glm::dvec3( gridN * gridStep, 0.0, 0.0))
    );

    m_host.drawPlanetMapLine(
        m_host.activeLocalCameraSnapshot().project(glm::dvec3(0.0, 0.0, -gridN * gridStep)),
        m_host.activeLocalCameraSnapshot().project(glm::dvec3(0.0, 0.0,  gridN * gridStep))
    );
}



glm::dvec3 HubMapGeometryPass::visualSizeForHubShip(
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
        m_host.activeLocalCameraSnapshot().state.zoom;

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

}
