#pragma once

// Cobra Newtonian power frame, ship-body axes: +X right, +Y up, -Z nose.
// 8 independent mount nodes x 3 FIXED outward-facing nozzle motors.
// Values are normalized geometry until hull-scale physical mount dimensions
// are authored; do not treat them as metres or certification of authority.
// Every nozzle ID is an installed, separately damageable ObjectModuleRuntime
// module. Native Newtonian flight executes this exact fixed-direction bank;
// the eight aggregate capability nodes are NOT separate omnidirectional motors.
#include <array>
#include <string_view>
#include <glm/glm.hpp>

namespace game::ship::presets::cobra
{
struct RcsNozzleMount
{
    std::string_view nozzleId;
    std::string_view nodeId;
    glm::dvec3 positionBody;
    glm::dvec3 forceDirectionBody;
};

inline const std::array<RcsNozzleMount,24>& newtonianRcsNozzles() noexcept
{
    static const std::array<RcsNozzleMount,24> nozzles {{
        {"rcs_fore_upper_left_x", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_fore_upper_left_y", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {0.0, 1.0, 0.0}},
        {"rcs_fore_upper_left_z", "rcs_fore_upper_left", {-1.0, 1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_upper_right_x", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {1.0, 0.0, 0.0}},
        {"rcs_fore_upper_right_y", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {0.0, 1.0, 0.0}},
        {"rcs_fore_upper_right_z", "rcs_fore_upper_right", {1.0, 1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_lower_left_x", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_fore_lower_left_y", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}},
        {"rcs_fore_lower_left_z", "rcs_fore_lower_left", {-1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_fore_lower_right_x", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {1.0, 0.0, 0.0}},
        {"rcs_fore_lower_right_y", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {0.0, -1.0, 0.0}},
        {"rcs_fore_lower_right_z", "rcs_fore_lower_right", {1.0, -1.0, -1.0}, {0.0, 0.0, -1.0}},
        {"rcs_aft_upper_left_x", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_aft_upper_left_y", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {0.0, 1.0, 0.0}},
        {"rcs_aft_upper_left_z", "rcs_aft_upper_left", {-1.0, 1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_upper_right_x", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {1.0, 0.0, 0.0}},
        {"rcs_aft_upper_right_y", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {0.0, 1.0, 0.0}},
        {"rcs_aft_upper_right_z", "rcs_aft_upper_right", {1.0, 1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_lower_left_x", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {-1.0, 0.0, 0.0}},
        {"rcs_aft_lower_left_y", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {0.0, -1.0, 0.0}},
        {"rcs_aft_lower_left_z", "rcs_aft_lower_left", {-1.0, -1.0, 1.0}, {0.0, 0.0, 1.0}},
        {"rcs_aft_lower_right_x", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {1.0, 0.0, 0.0}},
        {"rcs_aft_lower_right_y", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {0.0, -1.0, 0.0}},
        {"rcs_aft_lower_right_z", "rcs_aft_lower_right", {1.0, -1.0, 1.0}, {0.0, 0.0, 1.0}},
    }};
    return nozzles;
}
} // namespace game::ship::presets::cobra
