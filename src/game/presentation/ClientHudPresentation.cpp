#include "src/game/presentation/ClientHudPresentation.h"

#include <cmath>
#include <cstdio>

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
