#include "src/game/presentation/ClientHudPresentation.h"

#include <cmath>
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "src/ui/components/UIComponent.h"
#include "src/ui/components/UIText.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::presentation
{
namespace
{
bool setText(
    UIComponent& root,
    const char* id,
    const std::string& value
)
{
    auto* component = root.findById(id);
    if (!component)
        return false;

    auto* text = dynamic_cast<UIText*>(component);
    if (!text)
        return false;

    text->label = value;
    return true;
}
}

namespace
{
glm::dvec3 safeNormalized(
    const glm::dvec3& value,
    const glm::dvec3& fallback
)
{
    const double length = glm::length(value);
    if (length <= 1.0e-9)
        return fallback;
    return value / length;
}

std::string formatSpeedText(
    double speedMps,
    const FlightInstrumentTextProfile& profile
)
{
    std::ostringstream out;
    out << std::fixed
        << std::setprecision(std::max(0, profile.speedDecimals))
        << speedMps * profile.displayUnitsPerMps;

    if (!profile.speedUnitLabel.empty())
        out << ' ' << profile.speedUnitLabel;

    return out.str();
}
}

FlightVectorIndicatorPresentation buildFlightVectorIndicatorPresentation(
    const ClientShipState& ship,
    const FlightInstrumentTextProfile& textProfile
)
{
    FlightVectorIndicatorPresentation out;
    out.visible = true;

    const auto& transform = ship.renderTransform;
    const auto& motion = transform.motion;

    out.controlLaw = motion.localControlLaw;
    out.alignmentMode = motion.velocityAlignmentMode;
    out.speedMps = glm::length(motion.localVelocityMps);

    if (ship.descriptor)
        out.speedLimitMps = std::max(
            0.0,
            static_cast<double>(ship.descriptor->physics.maxCombatSpeed)
        );

    if (out.speedLimitMps > 0.0)
    {
        out.speedFraction01 = static_cast<float>(
            std::clamp(out.speedMps / out.speedLimitMps, 0.0, 1.0)
        );
    }

    out.speedText = formatSpeedText(out.speedMps, textProfile);
    out.fontPath = textProfile.fontPath;
    out.modeText =
        motion.localControlLaw == game::navigation::LocalFlightControlLaw::Assisted
            ? textProfile.assistedModeLabel
            : textProfile.newtonianModeLabel;

    switch (motion.velocityAlignmentMode)
    {
        case game::navigation::VelocityAlignmentMode::ForwardToVelocity:
            out.actionText = textProfile.alignForwardLabel;
            break;
        case game::navigation::VelocityAlignmentMode::BackwardToVelocity:
            out.actionText = textProfile.alignBackwardLabel;
            break;
        case game::navigation::VelocityAlignmentMode::BrakeToStop:
            out.actionText = textProfile.brakingLabel;
            break;
        default:
            break;
    }

    // Velocity is fixed as +Y in indicator space. Hull attitude is expressed
    // relative to it; no galactic/system coordinates enter the HUD renderer.
    if (out.speedMps <= 1.0e-6 || !motion.travelFrame.valid)
    {
        out.shipModelToIndicatorBasis = glm::mat3(1.0f);
        return out;
    }

    const glm::dvec3 velocityWorld =
        motion.travelFrame.localToWorldVector(motion.localVelocityMps);
    const glm::dvec3 indicatorY =
        safeNormalized(velocityWorld, glm::dvec3(0.0, 0.0, -1.0));

    glm::dvec3 referenceUp = motion.travelFrame.localToWorldVector(
        glm::dvec3(0.0, 1.0, 0.0)
    );

    if (glm::length(glm::cross(indicatorY, referenceUp)) <= 1.0e-6)
    {
        referenceUp = motion.travelFrame.localToWorldVector(
            glm::dvec3(1.0, 0.0, 0.0)
        );
    }

    const glm::dvec3 indicatorX = safeNormalized(
        glm::cross(indicatorY, referenceUp),
        glm::dvec3(1.0, 0.0, 0.0)
    );
    const glm::dvec3 indicatorZ = safeNormalized(
        glm::cross(indicatorX, indicatorY),
        glm::dvec3(0.0, 0.0, 1.0)
    );

    auto toIndicator = [&](const glm::vec3& worldAxis)
    {
        const glm::dvec3 axis(worldAxis);
        return glm::vec3(
            static_cast<float>(glm::dot(axis, indicatorX)),
            static_cast<float>(glm::dot(axis, indicatorY)),
            static_cast<float>(glm::dot(axis, indicatorZ))
        );
    };

    out.shipModelToIndicatorBasis = glm::mat3(
        toIndicator(transform.right()),
        toIndicator(transform.forward()),
        toIndicator(transform.up())
    );

    return out;
}

PlayerHudTelemetry buildPlayerHudTelemetry(
    const ClientShipState& ship
)
{
    PlayerHudTelemetry out;

    const auto& wp = ship.renderTransform.worldPosition;
    out.globalMeters =
        glm::dvec3(
            static_cast<double>(wp.cell.x),
            static_cast<double>(wp.cell.y),
            static_cast<double>(wp.cell.z)
        ) * world::coordinates::GalacticCellSizeM +
        wp.localMeters;

    // Pilot-local speed is defined relative to the ship travel frame. Legacy
    // float mirrors are not authoritative and may legitimately remain zero.
    out.speedMps =
        glm::length(ship.renderTransform.motion.localVelocityMps);

    char buffer[128];

    std::snprintf(
        buffer,
        sizeof(buffer),
        "CELL %lld %lld %lld",
        static_cast<long long>(wp.cell.x),
        static_cast<long long>(wp.cell.y),
        static_cast<long long>(wp.cell.z)
    );
    out.cellLabel = buffer;

    std::snprintf(buffer, sizeof(buffer), "X %.0f m", out.globalMeters.x);
    out.xLabel = buffer;

    std::snprintf(buffer, sizeof(buffer), "Y %.0f m", out.globalMeters.y);
    out.yLabel = buffer;

    std::snprintf(buffer, sizeof(buffer), "Z %.0f m", out.globalMeters.z);
    out.zLabel = buffer;

    std::snprintf(
        buffer,
        sizeof(buffer),
        "VREL %.1f m/s  %s",
        out.speedMps,
        game::navigation::localFlightControlLawName(
            ship.renderTransform.motion.localControlLaw
        )
    );
    out.speedLabel = buffer;

    return out;
}

bool applyPlayerHudTelemetry(
    UIComponent& root,
    const PlayerHudTelemetry& telemetry
)
{
    bool ok = true;
    ok = setText(root, "main_coord_cell", telemetry.cellLabel) && ok;
    ok = setText(root, "main_coord_x", telemetry.xLabel) && ok;
    ok = setText(root, "main_coord_y", telemetry.yLabel) && ok;
    ok = setText(root, "main_coord_z", telemetry.zLabel) && ok;
    ok = setText(root, "main_coord_v", telemetry.speedLabel) && ok;
    return ok;
}

}
