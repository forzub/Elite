#pragma once

#include <string>
#include <vector>

#include "src/game/catalog/CatalogTypes.h"
#include "src/game/catalog/codes/FlightArchitectureCodes.h"
#include "src/game/catalog/codes/OperationalClassCodes.h"
#include "src/game/catalog/power/KobraReactorPassports.h"
#include "src/game/catalog/propulsion/KobraMainDrivePassports.h"
#include "src/game/catalog/rcs/KobraRcsPassports.h"
#include "src/game/ship/presets/cobra/CobraNewtonianRcsNozzles.h"

// Cobrett ship models of Kobersberg-Braunauer Raumfahrzeugwerke GmbH.
// N1 and A3 are DIFFERENT serial products/generations of the Cobrett-I line,
// not one ship with a flight-law switch. They temporarily share the legacy
// Cobra hull geometry; their data identities are already distinct.
// Historical Cobra Y/X validation configurations have no ship passport.
namespace game::catalog::ships
{

inline constexpr std::string_view CobrettIN1Id = "kb.ship.cobrett-i.frnu.n1";
inline constexpr std::string_view CobrettIA3Id =
    "kb.ship.cobrett-i.frnu-inta.a3";

struct ShipPassport
{
    ProductPassport product;
    std::string_view fullName {};
    const CodeDescription* layout = nullptr;
    std::vector<const CodeDescription*> operationalClasses;
    const CodeDescription* flightGeneration = nullptr;
    std::string_view flightArchitecture {};
    int generationNumber = 0;
    // Hierarchical installation-code tree, root first. Codes are positions
    // inside THIS model; root codes are provisional.
    std::vector<InstallationPosition> assemblies;
    // Historical validation loadout currently realizing this model.
    std::string_view legacyLoadoutId {};
};

namespace detail
{
inline void appendRcsGroup(
    std::vector<InstallationPosition>& out,
    const std::string& groupCode)
{
    out.push_back({groupCode, "Translational/rotational RCS group", "", ""});
    const auto& nozzles = game::ship::presets::cobra::newtonianRcsNozzles();
    std::string node;
    int nodeIndex = 0;
    int nozzleIndex = 0;
    for (const auto& nozzle : nozzles)
    {
        if (std::string(nozzle.nodeId) != node)
        {
            node = std::string(nozzle.nodeId);
            ++nodeIndex;
            nozzleIndex = 0;
            out.push_back({groupCode + "A" + std::to_string(nodeIndex),
                "RCS mount node " + node, "", ""});
        }
        ++nozzleIndex;
        out.push_back({
            groupCode + "A" + std::to_string(nodeIndex) +
                "A" + std::to_string(nozzleIndex),
            "RCS nozzle " + std::string(nozzle.nozzleId),
            std::string(nozzle.nozzleId),
            std::string(rcs::FieldCoupledRcsNozzleId)});
    }
}
} // namespace detail

[[nodiscard]] inline const ShipPassport& cobrettIN1()
{
    static const ShipPassport passport = [] {
        ShipPassport p;
        p.product = {
            CobrettIN1Id,
            ProductCategory::Ship,
            "Kobersberg-Braunauer Raumfahrzeugwerke GmbH",
            "Cobrett-I FRN-U N1",
            "Cobrett-I FRN-U N1",
            "Cobrett",
            "Cobrett-I",
            "N1",
            "KB-CBI-FRNU-N1",
            "KB-SHP-CBI-N1-0001",
            "rev 0",
            PassportStatus::Provisional,
            "",
            0.0, 0.0, 0.0, 0.0,
            "Older Newtonian generation of the Cobrett-I line. Mass, hull "
            "and non-propulsion equipment currently use the legacy Cobra "
            "hull data.",
            {}
        };
        p.fullName =
            "Kobersberg-Braunauer Raumfahrzeugwerke GmbH Cobrett-I FRN-U N1";
        p.layout = &codes::LayoutI;
        p.operationalClasses = {&codes::FrontierUtility};
        p.flightGeneration = &codes::NewtonianGeneration1;
        p.flightArchitecture = "Newtonian";
        p.generationNumber = 1;
        p.legacyLoadoutId = "Newtonian-I";
        p.assemblies = {
            {"36", "Cobrett-I FRN-U N1 ship assembly", "", ""},
            {"36A1", "Main propulsion group (axial, centre line)", "", ""},
            {"36A1A1", "Propulsion reactor",
             "ship_newtonian_i_drive_center_reactor",
             std::string(power::ER5PropulsionReactorId)},
            {"36A1A2", "Main engine",
             "ship_newtonian_i_main_center",
             std::string(propulsion::KobraN1AxialMainDriveId)},
        };
        detail::appendRcsGroup(p.assemblies, "36A2");
        p.assemblies.push_back(
            {"36A3", "Ship service reactor", "ship_service_reactor", ""});
        p.assemblies.push_back(
            {"36A4", "Common cooling", "ship_common_cooling", ""});
        return p;
    }();
    return passport;
}

[[nodiscard]] inline const ShipPassport& cobrettIA3()
{
    static const ShipPassport passport = [] {
        ShipPassport p;
        p.product = {
            CobrettIA3Id,
            ProductCategory::Ship,
            "Kobersberg-Braunauer Raumfahrzeugwerke GmbH",
            "Cobrett-I FRN-U/INT-A A3",
            "Cobrett-I FRN-U/INT-A A3",
            "Cobrett",
            "Cobrett-I",
            "A3",
            "KB-CBI-FRNU-INTA-A3",
            "KB-SHP-CBI-A3-0001",
            "rev 0",
            PassportStatus::Provisional,
            "",
            0.0, 0.0, 0.0, 0.0,
            "Modern Assisted generation of the Cobrett-I line. Mass, hull "
            "and non-propulsion equipment currently use the legacy Cobra "
            "hull data.",
            {}
        };
        p.fullName =
            "Kobersberg-Braunauer Raumfahrzeugwerke GmbH "
            "Cobrett-I FRN-U/INT-A A3";
        p.layout = &codes::LayoutI;
        p.operationalClasses = {
            &codes::FrontierUtility, &codes::InterSystemArmedTransport};
        p.flightGeneration = &codes::AssistedGeneration3;
        p.flightArchitecture = "Assisted";
        p.generationNumber = 3;
        p.legacyLoadoutId = "Assisted-I";
        const std::string engine(propulsion::CR1VectorMainDriveId);
        const std::string reactor(power::ER5PropulsionReactorId);
        p.assemblies = {
            {"37", "Cobrett-I FRN-U/INT-A A3 ship assembly", "", ""},
            {"37A1", "Right propulsion group", "", ""},
            {"37A1A1", "Right propulsion reactor",
             "ship_drive_right_reactor", reactor},
            {"37A1A2", "Right aft main engine", "ship_engine_R", engine},
            {"37A1A3", "Right fore main engine", "ship_fore_engine_R", engine},
            {"37A2", "Left propulsion group", "", ""},
            {"37A2A1", "Left propulsion reactor",
             "ship_drive_left_reactor", reactor},
            {"37A2A2", "Left aft main engine", "ship_engine_L", engine},
            {"37A2A3", "Left fore main engine", "ship_fore_engine_L", engine},
            {"37A3", "Ship service reactor", "ship_service_reactor", ""},
            {"37A4", "Common cooling", "ship_common_cooling", ""},
        };
        return p;
    }();
    return passport;
}

} // namespace game::catalog::ships
