#pragma once

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "src/game/client/ClientWorldState.h"
#include "src/game/navigation/ClientNavigationWorkspace.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::presentation
{

struct GuidanceHudFramePresentation
{
    double timeAheadSeconds = 0.0;
    glm::dvec3 relativeCenterMeters {0.0};
    glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
    double widthMeters = 0.0;
    double heightMeters = 0.0;
    double recommendedSpeedMps = 0.0;
    // Manual tunnel frames fade with distance so nearby geometry guides the
    // pilot without turning the far corridor into a bright wire cage.
    float opacity = 1.0f;
    bool requiredVehiclePose = false;
};

struct GuidanceCorridorHudPresentation
{
    bool visible = false;
    std::string corridorId;
    game::navigation::GuidanceSource source =
        game::navigation::GuidanceSource::Unknown;
    game::navigation::GuidancePurpose purpose =
        game::navigation::GuidancePurpose::Transit;
    double confidence = 1.0;
    bool advisoryOnly = true;
    bool spatialManualTunnel = false;
    bool noSafePrimarySolution = false;
    std::vector<GuidanceHudFramePresentation> frames;
};

inline GuidanceCorridorHudPresentation buildGuidanceCorridorHudPresentation(
    const game::navigation::ClientNavigationWorkspace& navigation,
    const ClientShipState& player,
    double universeTimeSeconds,
    double lookAheadSeconds = 15.0,
    std::size_t maxFrames = 40
)
{
    GuidanceCorridorHudPresentation out;
    if (!navigation.modules().enabled(
            game::navigation::NavigationModuleId::HudGuidanceCorridor))
    {
        return out;
    }

    const int systemId = player.renderTransform.motion.systemId;
    const auto* corridor = navigation.guidance().activeSpatialManualTunnel(
        systemId,
        universeTimeSeconds,
        &navigation.modules()
    );
    if (!corridor)
    {
        corridor = navigation.guidance().active(
            systemId,
            universeTimeSeconds,
            &navigation.modules()
        );
    }
    if (!corridor)
        return out;

    const glm::dvec3 playerMeters =
        world::coordinates::fullMeters(player.renderTransform.worldPosition);

    std::vector<const game::navigation::GuidanceFrame*> candidates;
    candidates.reserve(corridor->frames.size());
    const bool spatialTunnel = corridor->spatialManualTunnel;
    const double maxTime = universeTimeSeconds + std::max(0.5, lookAheadSeconds);

    for (const auto& frame : corridor->frames)
    {
        const double timeAheadSeconds =
            frame.universeTimeSeconds - universeTimeSeconds;
        const glm::dvec3 relativeCenter = frame.centerMeters - playerMeters;
        const double frameDistanceMeters = glm::length(relativeCenter);
        const double frameScaleMeters = std::max(
            1.0,
            std::max(frame.widthMeters, frame.heightMeters)
        );

        // A spatial manual tunnel is regenerated from the actual ship pose,
        // so its gates intentionally share the current epoch.  Time filtering
        // is only meaningful for predictive corridors.  The first gate still
        // lives on the ship and is suppressed to avoid a giant cockpit blob.
        const double nearCullMeters = spatialTunnel
            ? 30.0
            : std::max(30.0, frameScaleMeters * 0.75);
        if (frameDistanceMeters < nearCullMeters)
            continue;
        if (!spatialTunnel &&
            (timeAheadSeconds < 0.75 || frame.universeTimeSeconds > maxTime))
        {
            continue;
        }
        candidates.push_back(&frame);
    }

    if (candidates.empty())
        return out;

    const std::size_t frameLimit = spatialTunnel
        ? std::max<std::size_t>(maxFrames, 96)
        : std::max<std::size_t>(1, maxFrames);
    const std::size_t stride = std::max<std::size_t>(
        1,
        (candidates.size() + frameLimit - 1) / frameLimit
    );

    out.frames.reserve(std::min(frameLimit, candidates.size()));
    for (std::size_t i = 0; i < candidates.size(); i += stride)
    {
        const auto& frame = *candidates[i];
        GuidanceHudFramePresentation item;
        item.timeAheadSeconds =
            frame.universeTimeSeconds - universeTimeSeconds;
        item.relativeCenterMeters = frame.centerMeters - playerMeters;
        item.orientation = frame.orientation;
        item.widthMeters = frame.widthMeters;
        item.heightMeters = frame.heightMeters;
        item.recommendedSpeedMps = frame.recommendedSpeedMps;
        item.requiredVehiclePose = frame.requiredVehiclePose;
        out.frames.push_back(std::move(item));
        if (out.frames.size() >= frameLimit)
            break;
    }

    // Preserve the final dock gate even when decimation skipped it.
    if (candidates.size() > 1)
    {
        const auto* last = candidates.back();
        const glm::dvec3 lastRelative = last->centerMeters - playerMeters;
        const bool alreadyLast = !out.frames.empty() &&
            glm::length(out.frames.back().relativeCenterMeters - lastRelative) <= 1.0e-6;
        if (!alreadyLast)
        {
            GuidanceHudFramePresentation item;
            item.timeAheadSeconds =
                last->universeTimeSeconds - universeTimeSeconds;
            item.relativeCenterMeters = lastRelative;
            item.orientation = last->orientation;
            item.widthMeters = last->widthMeters;
            item.heightMeters = last->heightMeters;
            item.recommendedSpeedMps = last->recommendedSpeedMps;
            item.requiredVehiclePose = last->requiredVehiclePose;
            if (out.frames.size() >= frameLimit && !out.frames.empty())
                out.frames.back() = std::move(item);
            else
                out.frames.push_back(std::move(item));
        }
    }

    if (spatialTunnel && !out.frames.empty())
    {
        const std::size_t lastIndex = out.frames.size() - 1;
        for (std::size_t i = 0; i < out.frames.size(); ++i)
        {
            const double u = lastIndex > 0
                ? static_cast<double>(i) / static_cast<double>(lastIndex)
                : 0.0;
            const double smooth = u * u * (3.0 - 2.0 * u);
            out.frames[i].opacity = static_cast<float>(
                0.34 * (1.0 - smooth) + 0.07 * smooth
            );
        }
    }

    out.visible = !out.frames.empty();
    out.corridorId = corridor->id;
    out.source = corridor->source;
    out.purpose = corridor->purpose;
    out.confidence = corridor->confidence;
    out.advisoryOnly = corridor->advisoryOnly;
    out.spatialManualTunnel = corridor->spatialManualTunnel;
    out.noSafePrimarySolution = corridor->noSafePrimarySolution;
    return out;
}

} // namespace game::presentation
