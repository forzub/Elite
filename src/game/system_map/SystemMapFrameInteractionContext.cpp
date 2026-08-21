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
    double bestPhysicalSizeMeters = -1.0;
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

        if (distance > point.screenRadiusPx)
            continue;

        const double size =
            std::max(0.0, point.physicalSizeMeters);
        const bool larger =
            size > bestPhysicalSizeMeters + 1.0e-6;
        const bool sameSize =
            std::abs(size - bestPhysicalSizeMeters) <= 1.0e-6;
        const bool nearer =
            sameSize && distance < bestDistance - 0.01f;

        if (bestIndex < 0 || larger || nearer)
        {
            bestPhysicalSizeMeters = size;
            bestDistance = distance;
            bestIndex = index;
        }
    }

    if (bestIndex < 0)
        return -1;

    // A body whose visible disk/marker is directly under the pointer is part
    // of the same semantic click cluster. The physically larger object wins,
    // so a dense pile of tiny ship/Hub glyphs cannot hide a planet or moon.
    const double bodySize =
        largestDirectBodyPhysicalSizeMetersAt(x, y);
    if (bodySize > bestPhysicalSizeMeters + 1.0e-6)
        return -1;

    return bestIndex;
}

int SystemMapFrameInteractionContext::pickBody(
    double x,
    double y
) const
{
    int bestIndex = -1;
    int bestHitTier = 2;
    double bestPhysicalSizeMeters = -1.0;
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
        const int hitTier = insideDisk ? 0 : 1;
        const double physicalSizeMeters =
            std::max(0.0, point.physicalSizeMeters);

        const float score =
            insideDisk
                ? centerDistance
                : distanceToRealDisk *
                    m_controls.pickScoreDiskWeight +
                    centerDistance;

        const bool betterTier = hitTier < bestHitTier;
        const bool sameTier = hitTier == bestHitTier;
        const bool larger =
            sameTier &&
            physicalSizeMeters >
                bestPhysicalSizeMeters + 1.0e-6;
        const bool sameSize =
            sameTier &&
            std::abs(
                physicalSizeMeters -
                bestPhysicalSizeMeters
            ) <= 1.0e-6;
        const bool betterScore =
            sameSize && score < bestScore;

        if (bestIndex < 0 || betterTier || larger || betterScore)
        {
            bestIndex = index;
            bestHitTier = hitTier;
            bestPhysicalSizeMeters = physicalSizeMeters;
            bestScore = score;
        }
    }

    return bestIndex;
}

double
SystemMapFrameInteractionContext::largestDirectBodyPhysicalSizeMetersAt(
    double x,
    double y
) const
{
    double best = 0.0;
    const glm::vec2 mouse(
        static_cast<float>(x),
        static_cast<float>(y)
    );

    for (const auto& point : m_frame.bodyScreenPoints)
    {
        if (!point.visible ||
            !std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y) ||
            !std::isfinite(point.screenRadiusPx))
        {
            continue;
        }

        const float distance =
            glm::length(point.screen - mouse);
        if (distance > std::max(0.0f, point.screenRadiusPx))
            continue;

        best = std::max(
            best,
            std::max(0.0, point.physicalSizeMeters)
        );
    }

    return best;
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
