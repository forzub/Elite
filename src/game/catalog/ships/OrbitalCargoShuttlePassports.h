#pragma once

#include <memory>
#include <string>
#include <vector>

#include "src/game/catalog/CatalogTypes.h"
#include "src/game/catalog/codes/FlightArchitectureCodes.h"
#include "src/game/catalog/codes/OperationalClassCodes.h"
#include "src/game/catalog/ships/CobrettPassports.h"

// OCS-4 "Lastochka" orbital cargo shuttle of the Orbital Freight Union
// "Ural-Kourou": a state-backed Earth heavy-lift consortium that runs the
// scheduled surface <-> Earth Orbital Hub freight line. Not a frontier ship
// (that is Kobersberg-Braunauer's Cobrett line): a box hull around a 2600 m^3
// cargo bay, one axial main engine, simple RCS, short-range sensors.
//
// Status: Provisional. The shuttle currently flies only as dock-traffic
// (TrafficShell, src/game/traffic/DockServiceTraffic), so its equipment has no
// runtime presets yet; the technical figures below are the design values the
// traffic model uses and stay here until real presets exist.
//
//   hull 56 x 18 x 14 m (engine included), dry 380 t, cargo 450 t / 2600 m^3
//   main drive 3.3 MN axial (4 m/s^2 fully loaded, 8.7 m/s^2 dry)
//   propulsion reactor 2.4 GW electric class, service reactor 30 MW
//   radiators 2 x 1100 m^2 deployable panels (folded inside the hull lines)
//   RCS 16 nozzles in 4 clusters, argon working gas
//   docking class Medium (needs a 22 x 18 m swept aperture, 50 x 16 m pad)
namespace game::catalog::ships
{

inline constexpr std::string_view OrbitalCargoShuttleOcs4Id = "ofu.ship.ocs-4.lastochka";

namespace ofu
{
inline constexpr std::string_view Manufacturer = "Orbital Freight Union \"Ural-Kourou\"";

[[nodiscard]] inline const ProductPassport& product(
    std::string_view id, ProductCategory category, std::string_view designation, std::string_view display,
    std::string_view stock, std::string_view notes, std::string_view installationCode)
{
    // Distinct static per id (each call site passes a literal).
    static std::vector<std::unique_ptr<ProductPassport>> storage;
    for (const auto& p : storage)
        if (p->stableId == id)
            return *p;
    auto p = std::make_unique<ProductPassport>();
    p->stableId = id;
    p->category = category;
    p->manufacturer = Manufacturer;
    p->designation = designation;
    p->displayName = display;
    p->family = "OCS";
    p->model = designation;
    p->generation = "4";
    p->manufacturerPartNumber = designation;
    p->stockNumber = stock;
    p->revision = "rev 0";
    p->status = PassportStatus::Provisional;
    p->notes = notes;
    p->compatibility = {{CompatibilityKind::InstalledOn, OrbitalCargoShuttleOcs4Id, {installationCode}}};
    storage.push_back(std::move(p));
    return *storage.back();
}
} // namespace ofu

[[nodiscard]] inline const ProductPassport& ocs4MainDrive()
{
    return ofu::product("ofu.engine.m12-axial", ProductCategory::MainPropulsion, "OFU-M12", "OFU M12 axial freight drive",
        "OFU-ENG-M12-0001", "3.3 MN axial, no thrust vectoring beyond trim; burns are planned flip-and-brake.", "52A1A2");
}
[[nodiscard]] inline const ProductPassport& ocs4PropulsionReactor()
{
    return ofu::product("ofu.reactor.rt40", ProductCategory::Reactor, "OFU-RT40", "OFU RT-40 propulsion reactor",
        "OFU-RCT-RT40-0001", "2.4 GW class propulsion reactor of the cheap-energy generation.", "52A1A1");
}
[[nodiscard]] inline const ProductPassport& ocs4RcsCluster()
{
    return ofu::product("ofu.rcs.k4-cluster", ProductCategory::ManoeuvreThruster, "OFU-K4", "OFU K4 RCS cluster",
        "OFU-RCS-K4-0001", "4 nozzles per cluster, argon; 4 clusters on the hull.", "52A2");
}
[[nodiscard]] inline const ProductPassport& ocs4Cooling()
{
    return ofu::product("ofu.cooling.dp1100", ProductCategory::Cooling, "OFU-DP1100", "OFU DP-1100 radiator pair",
        "OFU-CLG-DP1100-0001", "2 x 1100 m^2 deployable panels, stowed along the hull for docking.", "52A4");
}
[[nodiscard]] inline const ProductPassport& ocs4SensorSuite()
{
    return ofu::product("ofu.sensor.nr6", ProductCategory::Sensor, "OFU-NR6", "OFU NR-6 nav radar + traffic transponder",
        "OFU-SNS-NR6-0001", "Short-range navigation radar, dock-control data link, traffic transponder.", "52A5");
}

[[nodiscard]] inline const ShipPassport& orbitalCargoShuttleOcs4()
{
    static const ShipPassport passport = [] {
        ShipPassport p;
        p.product = {
            OrbitalCargoShuttleOcs4Id,
            ProductCategory::Ship,
            ofu::Manufacturer,
            "OFU OCS-4",
            "OFU OCS-4 orbital cargo shuttle",
            "OCS",
            "OCS-4",
            "4",
            "OFU-OCS4",
            "OFU-SHP-OCS4-0001",
            "rev 0",
            PassportStatus::Provisional,
            "",
            380000.0, 56.0, 18.0, 14.0,
            "Scheduled Earth surface <-> Earth Orbital Hub freight shuttle. Cargo 450 t / 2600 m^3. "
            "Currently realised as dock traffic only (no player hardware model).",
            {}
        };
        p.fullName = "Orbital Freight Union \"Ural-Kourou\" OCS-4 orbital cargo shuttle";   // \"Lastochka\" is the Ural shop nickname, not official (canon ship_naming.json)
        p.layout = &codes::LayoutI;
        p.operationalClasses = {};
        p.flightGeneration = &codes::NewtonianGeneration1;
        p.flightArchitecture = "Newtonian";
        p.generationNumber = 1;
        p.assemblies = {
            {"52", "OCS-4 ship assembly", "", ""},
            {"52A1", "Main propulsion group (axial, centre line)", "", ""},
            {"52A1A1", "Propulsion reactor", "", std::string(ocs4PropulsionReactor().stableId)},
            {"52A1A2", "Main engine", "", std::string(ocs4MainDrive().stableId)},
            {"52A2", "RCS (4 clusters, argon)", "", std::string(ocs4RcsCluster().stableId)},
            {"52A3", "Ship service reactor (30 MW)", "", ""},
            {"52A4", "Radiators", "", std::string(ocs4Cooling().stableId)},
            {"52A5", "Navigation radar / dock-control link / transponder", "", std::string(ocs4SensorSuite().stableId)},
            {"52A6", "Cargo bay 2600 m^3, 450 t, pallet rails + crane", "", ""},
        };
        return p;
    }();
    return passport;
}

} // namespace game::catalog::ships
