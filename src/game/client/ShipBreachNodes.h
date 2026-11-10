#pragma once

// Step 3 S4 (author 2026-11-09: interiors drawn only for a breached compartment). The breachable nodes of a
// ship's compartment asset (tools/ships/ship_asset_source.py: cover plates plate_cover_<k>, reactor pads
// pad_eq_er5_l / _r) are DERIVED on the client from the replicated module states of the legacy assembly - the
// same way ClientShipState::hiddenPartIds is - so no static node id ever goes on the wire
// (check_replication_static_definitions). A legacy module that is not locally visible (destroyed / detached /
// hanging, or under such a parent) breaches the asset nodes it covers.
//
// Cobrett-I layout (sketch v3): plates in 5 columns x 3 rows, k = column * 3 + row; columns x -6.5..6.5 in
// 2.6 m steps (0, 1 left; 2 centre; 3, 4 right), rows fore -5.2..-2.7 / mid -2.7..3.4 / aft 3.4..9.6 (k % 3 = 0/1/2).
// Legacy Cobra top panels: L / C / R x F (fore + mid) / B (aft).

#include <string>
#include <vector>

#include "src/world/types/ObjectType.h"

namespace game::client
{

[[nodiscard]] inline std::vector<std::string> breachNodesForLegacyModule(ObjectType type, const std::string& moduleId)
{
    if (type != ObjectType::CobraMk1)
        return {};
    if (moduleId == "ship_panel_top_LF") return {"plate_cover_0", "plate_cover_1", "plate_cover_3", "plate_cover_4"};
    if (moduleId == "ship_panel_top_LB") return {"plate_cover_2", "plate_cover_5"};
    if (moduleId == "ship_panel_top_CF") return {"plate_cover_6", "plate_cover_7"};
    if (moduleId == "ship_panel_top_CB") return {"plate_cover_8"};
    if (moduleId == "ship_panel_top_RF") return {"plate_cover_9", "plate_cover_10", "plate_cover_12", "plate_cover_13"};
    if (moduleId == "ship_panel_top_RB") return {"plate_cover_11", "plate_cover_14"};
    // propulsion reactors (Assisted-I: ship_drive_left/right_reactor; other models: <prefix><slot>_reactor)
    const bool reactor = moduleId.size() > 8 && moduleId.compare(moduleId.size() - 8, 8, "_reactor") == 0 &&
                         moduleId.find("service") == std::string::npos;
    if (reactor && moduleId.find("left") != std::string::npos) return {"pad_eq_er5_l"};
    if (reactor && moduleId.find("right") != std::string::npos) return {"pad_eq_er5_r"};
    return {};
}

} // namespace game::client
