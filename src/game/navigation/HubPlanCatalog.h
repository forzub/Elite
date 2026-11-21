#pragma once

// The hub PLAN of a planar city for the Hub map (author 2026-11-18: «в 3Д нам достаточно будет просто коробок и отметок
// для дока ... что касается самого хаба - это будет в 2Д карте. там можно выбирать кварталы - в игре появляется метка на
// HUD»). assets/data/navigation/hub_plans/<hub>.json (elite.hub_plan.v1, written by tools/models/hub_belt_world.py from
// the hub map): in the frame of the city module (frame_module_id, metres; the city plane is local X-Z, +Y up).
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
struct HubPlanBlock
{
    std::string id;        // "b<i>_<j>"
    int i = 0;
    int j = 0;
    std::string zone;      // downtown | residential | port | ... (tools/models/units/hub_plane.py ZONE_TITLE)
    std::string title;     // the zone's Russian title (the generator's)
    glm::dvec2 centre {0.0};
    double halfMeters = 300.0;
    double topHeightMeters = 0.0;     // the tallest module over the plane
    double underDepthMeters = 0.0;    // the deepest module under it
    int modules = 0;
    // the address inside the hub (author 2026-11-21: «каждому блоку присвоить адрес ... развозить по заявке»):
    // the column letter + the row number of the city grid, "C4"
    std::string address;
};

struct HubPlanModule
{
    int block = -1;        // index into blocks, -1: none
    bool under = false;
    bool pier = false;     // a rim pier (role "dock" in the layout)
    std::string module;
    glm::dvec4 rect {0.0}; // x0, z0, x1, z1
    double y0 = 0.0;
    double y1 = 0.0;
    // its address: the block + the building number, "C4-07" (the upper tier) / "C4-U03" (the lower tier, under the plane)
    std::string address;
    int number = 0;
    // a landing pad on its roof (author 2026-11-21: every city module without a dock of its own); the lower tier
    // has it on its underside, facing down
    bool pad = false;
    glm::dvec3 padPosition {0.0};   // city frame
    double padRadiusMeters = 0.0;
    // how it is built (author 2026-11-21: «на свободных участках, на крышах, на ножках, как вертолётные площадки на
    // платформах, ... как гараж на 1-2 места»): Roof - a tall wide building; Garage - a low wide one (1-2 places at
    // the plane, in its ground floor); Stilts - a narrow one: a pad on legs on a free plot of its block beside it
    enum class PadKind { None, Roof, Stilts, Garage };
    PadKind padKind = PadKind::None;
    int padPlaces = 1;
};

struct HubPlanStreet
{
    glm::dvec2 a {0.0};
    glm::dvec2 b {0.0};
    bool avenue = false;
    bool monorail = false;
};

struct HubPlanDock
{
    std::string moduleId;  // the dock module ("<hub>.dock")
    std::string anchorId;  // its docking anchor ("dock_gate_front")
    std::string title;
    glm::dvec3 position {0.0};
    glm::dvec2 out {0.0, -1.0};   // the slot's outward direction in the plane
};

struct HubPlanSpinning
{
    std::string id;
    std::string module;
    glm::dvec2 centre {0.0};
    double radius = 100.0;
};

// the roads / monorail as built (tools/models/hub_plan.py "roads"): what the city's traffic drives on
struct HubPlanRoads
{
    double topY = 0.0;                 // the deck top over the plane
    double underY = -44.0;             // the deck surface under the plane (facing down)
    std::vector<double> lanesMeters {3.0, 9.0};   // lane centres off the street's centre line (each way)
    double monorailY = 16.8;           // the rail top
    double monorailSideMeters = 18.0;  // to the street's +X (Z streets) / -Z (X streets)
};

// a ship of the city's air traffic (tools/models/hub_plan.py air_fleet: the hub's political bloc)
struct HubPlanAirShip
{
    std::string model;          // "ships/ocs-4-lastochka" | "hub_modules/hubt_shuttle" (under assets/compiled)
    std::string name;
    double lengthMeters = 30.0;
    double weight = 1.0;
    std::string passport;       // the catalog ship (empty: a presentation-only hull, e.g. the city shuttle)
};

class HubPlan
{
public:
    bool load(const std::string& path);

    const std::string& hubId() const { return m_hubId; }
    const std::string& frameModuleId() const { return m_frameModuleId; }
    const glm::dvec4& bounds() const { return m_bounds; }   // x0, z0, x1, z1
    double cellMeters() const { return m_cellMeters; }
    int populationEstimate() const { return m_population; }
    const std::vector<HubPlanBlock>& blocks() const { return m_blocks; }
    const std::vector<HubPlanModule>& modules() const { return m_modules; }
    const std::vector<HubPlanStreet>& streets() const { return m_streets; }
    const std::vector<HubPlanDock>& docks() const { return m_docks; }
    const std::vector<HubPlanSpinning>& spinning() const { return m_spinning; }
    const HubPlanRoads& roads() const { return m_roads; }
    const std::vector<HubPlanAirShip>& airFleet() const { return m_airFleet; }
    const std::string& bloc() const { return m_bloc; }
    const HubPlanBlock* block(const std::string& id) const;
    // the block under a point of the plane (city frame x, z), or nullptr
    const HubPlanBlock* blockAt(double x, double z) const;
    const std::string& failure() const { return m_failure; }

private:
    std::string m_hubId;
    std::string m_frameModuleId;
    glm::dvec4 m_bounds {0.0};
    double m_cellMeters = 600.0;
    int m_population = 0;
    std::vector<HubPlanBlock> m_blocks;
    std::vector<HubPlanModule> m_modules;
    std::vector<HubPlanStreet> m_streets;
    std::vector<HubPlanDock> m_docks;
    std::vector<HubPlanSpinning> m_spinning;
    HubPlanRoads m_roads;
    std::vector<HubPlanAirShip> m_airFleet;
    std::string m_bloc;
    std::string m_failure;
};

class HubPlanCatalog final
{
public:
    static const HubPlan* forHub(const std::string& hubId);
    static const HubPlan* forFrameModule(const std::string& moduleId);
};

// the localization key of a block's zone title ("hub.zone.<zone>", flight.json) and its Russian fallback
std::string hubPlanZoneKey(const std::string& zone);

} // namespace game::navigation

namespace game::navigation
{
// The hub composition (economy module types, the map panel's "Состав") <-> the city plan's block zones (author
// 2026-11-21: «клик по элементу хаба на схеме выделяет элемент в списке, клик по элементу списка - выделяет рамочкой
// часть хаба»). Types with no city zone (the station, the customs post, the medical bay ...) map to none.
inline const std::vector<std::string>& hubCompositionZones(const std::string& economyType)
{
    static const std::vector<std::string> none;
    static const std::vector<std::pair<std::string, std::vector<std::string>>> table = {
        {"habitat", {"residential", "hab_rot"}},
        {"hotel", {"hotel"}},
        {"shuttle_dock", {"dock"}},
        {"liner_platform", {"rig"}},
        {"cargo_terminal", {"port", "logistics"}},
        {"warehouse", {"storage"}},
        {"refuel_dock", {"refuel"}},
        {"service_dock", {"repair"}},
        {"market_exchange", {"downtown"}},
    };
    for (const auto& [type, zones] : table)
        if (type == economyType)
            return zones;
    return none;
}

inline std::string hubCompositionTypeForZone(const std::string& zone)
{
    for (const char* type : {"habitat", "hotel", "shuttle_dock", "liner_platform", "cargo_terminal", "warehouse", "refuel_dock",
                             "service_dock", "market_exchange"})
        for (const auto& z : hubCompositionZones(type))
            if (z == zone)
                return type;
    return {};
}
} // namespace game::navigation
