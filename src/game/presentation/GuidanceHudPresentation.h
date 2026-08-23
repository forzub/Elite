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
    bool noSafePrimarySolution = false;
    std::vector<GuidanceHudFramePresentation> frames;
};

inline GuidanceCorridorHudPresentation buildGuidanceCorridorHudPresentation(
    const game::navigation::ClientNavigationWorkspace& navigation,
    const ClientShipState& player,
    double universeTimeSeconds,
    double lookAheadSeconds = 15.0,
    std::size_t maxFrames = 18
)
{
    GuidanceCorridorHudPresentation out;
    if (!navigation.modules().enabled(
            game::navigation::NavigationModuleId::HudGuidanceCorridor))
    {
        return out;
    }

    const int systemId = player.renderTransform.motion.systemId;
    const auto* corridor = navigation.guidance().active(
        systemId,
        universeTimeSeconds,
        &navigation.modules()
    );
    if (!corridor)
        return out;

    const glm::dvec3 playerMeters =
        world::coordinates::fullMeters(player.renderTransform.worldPosition);

    std::vector<const game::navigation::GuidanceFrame*> candidates;
    candidates.reserve(corridor->frames.size());
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

        // The predictor contains a t=0 frame at the ship. Projecting frames
        // almost on top of the camera creates a giant square/central blob and
        // conveys no useful guidance. Start the HUD corridor visibly ahead.
        if (timeAheadSeconds < 0.75 ||
            frameDistanceMeters < std::max(30.0, frameScaleMeters * 0.75) ||
            frame.universeTimeSeconds > maxTime)
        {
            continue;
        }
        candidates.push_back(&frame);
    }

    if (candidates.empty())
        return out;

    const std::size_t stride = std::max<std::size_t>(
        1,
        (candidates.size() + std::max<std::size_t>(1, maxFrames) - 1) /
            std::max<std::size_t>(1, maxFrames)
    );

    out.frames.reserve(std::min(maxFrames, candidates.size()));
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
        if (out.frames.size() >= maxFrames)
            break;
    }

    // Preserve the final visible frame even when decimation skipped it.
    if (out.frames.size() < maxFrames && candidates.size() > 1)
    {
        const auto* last = candidates.back();
        const double lastTime = last->universeTimeSeconds - universeTimeSeconds;
        if (out.frames.empty() ||
            std::abs(out.frames.back().timeAheadSeconds - lastTime) > 1.0e-6)
        {
            GuidanceHudFramePresentation item;
            item.timeAheadSeconds = lastTime;
            item.relativeCenterMeters = last->centerMeters - playerMeters;
            item.orientation = last->orientation;
            item.widthMeters = last->widthMeters;
            item.heightMeters = last->heightMeters;
            item.recommendedSpeedMps = last->recommendedSpeedMps;
            item.requiredVehiclePose = last->requiredVehiclePose;
            out.frames.push_back(std::move(item));
        }
    }

    out.visible = !out.frames.empty();
    out.corridorId = corridor->id;
    out.source = corridor->source;
    out.purpose = corridor->purpose;
    out.confidence = corridor->confidence;
    out.advisoryOnly = corridor->advisoryOnly;
    out.noSafePrimarySolution = corridor->noSafePrimarySolution;
    return out;
}

} // namespace game::presentation
