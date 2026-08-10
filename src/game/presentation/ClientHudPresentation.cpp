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

    out.speedMps =
        glm::length(glm::dvec3(ship.renderTransform.localVelocity));

    if (std::abs(static_cast<double>(
            ship.renderTransform.forwardVelocity)) > out.speedMps)
    {
        out.speedMps = std::abs(static_cast<double>(
            ship.renderTransform.forwardVelocity));
    }

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

    std::snprintf(buffer, sizeof(buffer), "V %.1f m/s", out.speedMps);
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
