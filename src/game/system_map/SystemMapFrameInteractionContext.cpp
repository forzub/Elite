#include "src/game/system_map/SystemMapFrameInteractionContext.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "src/game/system_map/SystemMapFrameData.h"
#include "src/game/system_map/SystemMapView.h"

namespace game::system_map
{
SystemMapFrameInteractionContext::SystemMapFrameInteractionContext(
    const SystemMapFrameData& frame,
    const SystemMapControlSettings& controls
)
    : m_frame(frame),
      m_controls(controls)
{
}

int SystemMapFrameInteractionContext::pickHub(
    double x,
    double y
) const
{
    int bestIndex = -1;
    float bestDistance = std::numeric_limits<float>::max();
    const glm::vec2 mouse(
        static_cast<float>(x),
        static_cast<float>(y)
    );

    for (int index = 0;
         index < static_cast<int>(m_frame.hubScreenPoints.size());
         ++index)
    {
        const auto& point = m_frame.hubScreenPoints[index];

        if (!point.visible ||
            !std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y))
        {
            continue;
        }

        const float distance =
            glm::length(point.screen - mouse);

        if (distance <= point.screenRadiusPx &&
            distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = index;
        }
    }

    return bestIndex;
}

int SystemMapFrameInteractionContext::pickBody(
    double x,
    double y
) const
{
    int bestIndex = -1;
    float bestScore = std::numeric_limits<float>::max();
    const glm::vec2 mouse(
        static_cast<float>(x),
        static_cast<float>(y)
    );

    for (int index = 0;
         index < static_cast<int>(m_frame.bodyScreenPoints.size());
         ++index)
    {
        const auto& point = m_frame.bodyScreenPoints[index];

        if (!std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y) ||
            !std::isfinite(point.screenRadiusPx))
        {
            continue;
        }

        const bool depthOk =
            point.depth >= -1.0f && point.depth <= 1.0f;

        if (!point.visible && !depthOk)
            continue;

        const float centerDistance =
            glm::length(point.screen - mouse);
        const float realBodyRadiusPx =
            std::max(0.0f, point.screenRadiusPx);
        const float haloBodyRadiusPx =
            std::clamp(
                realBodyRadiusPx,
                0.0f,
                m_controls.pickMaxBodyRadiusPx
            );
        const float distanceToRealDisk =
            std::max(
                0.0f,
                centerDistance - realBodyRadiusPx
            );
        const float pickHaloPx =
            std::clamp(
                haloBodyRadiusPx *
                    m_controls.pickHaloRadiusFactor +
                    m_controls.pickHaloBasePx,
                m_controls.pickHaloBasePx,
                m_controls.pickHaloMaxPx
            );

        if (distanceToRealDisk > pickHaloPx)
            continue;

        const bool insideDisk =
            centerDistance <= realBodyRadiusPx;

        const float score =
            insideDisk
                ? centerDistance * 0.001f
                : 1000000.0f +
                    distanceToRealDisk *
                        m_controls.pickScoreDiskWeight +
                    centerDistance;

        if (score < bestScore)
        {
            bestScore = score;
            bestIndex = index;
        }
    }

    return bestIndex;
}

int SystemMapFrameInteractionContext::pickCameraBody(
    double x,
    double y
) const
{
    int bestIndex = -1;
    float bestCenterDistance =
        std::numeric_limits<float>::max();
    double bestCameraDepth =
        std::numeric_limits<double>::max();
    const glm::vec2 mouse(
        static_cast<float>(x),
        static_cast<float>(y)
    );

    for (int index = 0;
         index < static_cast<int>(
             m_frame.orbitPivotScreenPoints.size()
         );
         ++index)
    {
        const auto& point =
            m_frame.orbitPivotScreenPoints[index];

        if (!std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y) ||
            !std::isfinite(point.cameraDepthWorld) ||
            point.cameraDepthWorld <= 0.0)
        {
            continue;
        }

        const bool depthOk =
            std::isfinite(point.depth) &&
            point.depth >= -1.0f &&
            point.depth <= 1.0f;

        if (!depthOk)
            continue;

        const float centerDistance =
            glm::length(point.screen - mouse);

        if (centerDistance >
            m_controls.cameraBodyAnchorMaxDistancePx)
        {
            continue;
        }

        const bool nearer =
            centerDistance < bestCenterDistance - 0.01f;
        const bool sameDistance =
            std::abs(centerDistance - bestCenterDistance) <= 0.01f;
        const bool inFront =
            sameDistance &&
            point.cameraDepthWorld < bestCameraDepth;

        if (bestIndex < 0 || nearer || inFront)
        {
            bestIndex = index;
            bestCenterDistance = centerDistance;
            bestCameraDepth = point.cameraDepthWorld;
        }
    }

    return bestIndex;
}

std::optional<std::string>
SystemMapFrameInteractionContext::pickSystemBodyId(
    double x,
    double y
) const
{
    const int index = pickBody(x, y);

    if (index < 0 ||
        index >= static_cast<int>(m_frame.bodyScreenPoints.size()))
    {
        return std::nullopt;
    }

    return m_frame.bodyScreenPoints[index].bodyId;
}

std::optional<SystemMapHubSelection>
SystemMapFrameInteractionContext::pickSystemHubSelection(
    double x,
    double y
) const
{
    const int index = pickHub(x, y);

    if (index < 0 ||
        index >= static_cast<int>(m_frame.hubScreenPoints.size()))
    {
        return std::nullopt;
    }

    const auto& point = m_frame.hubScreenPoints[index];
    SystemMapHubSelection result;
    result.hubId = point.hubId;
    result.parentBodyId = point.parentBodyId;
    return result;
}

std::optional<SystemMapCameraBodyTarget>
SystemMapFrameInteractionContext::pickSystemCameraBodyTarget(
    double x,
    double y,
    const Viewport&
) const
{
    const int index = pickCameraBody(x, y);

    if (index < 0 ||
        index >= static_cast<int>(
            m_frame.orbitPivotScreenPoints.size()
        ))
    {
        return std::nullopt;
    }

    const auto& point =
        m_frame.orbitPivotScreenPoints[index];
    const auto positionIt =
        m_frame.bodyAbsolutePositionById.find(point.bodyId);

    if (positionIt == m_frame.bodyAbsolutePositionById.end())
        return std::nullopt;

    SystemMapCameraBodyTarget result;
    result.bodyId = point.bodyId;
    result.absolutePosition = positionIt->second;

    const auto radiusIt =
        m_frame.bodyPhysicalRadiusWorldById.find(point.bodyId);

    if (radiusIt != m_frame.bodyPhysicalRadiusWorldById.end())
    {
        result.physicalRadiusWorld =
            std::max(
                0.0,
                static_cast<double>(radiusIt->second)
            );
    }

    return result;
}

std::optional<glm::dvec3>
SystemMapFrameInteractionContext::systemBodyAbsolutePosition(
    const std::string& bodyId
) const
{
    const auto found =
        m_frame.bodyAbsolutePositionById.find(bodyId);

    if (found == m_frame.bodyAbsolutePositionById.end())
        return std::nullopt;

    return found->second;
}

std::optional<glm::dvec3>
SystemMapFrameInteractionContext::systemObjectAbsolutePosition(
    const std::string& objectId
) const
{
    const auto found =
        m_frame.objectAbsolutePositionById.find(objectId);

    if (found == m_frame.objectAbsolutePositionById.end())
        return std::nullopt;

    return found->second;
}
}
