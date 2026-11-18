#pragma once

// The hub PLAN of a planar city for the Hub map (author 2026-11-18: «в 3Д нам достаточно будет просто коробок и отметок
// для дока ... что касается самого хаба - это будет в 2Д карте. там можно выбирать кварталы - в игре появляется метка на
// HUD»). assets/data/navigation/hub_plans/<hub>.json (elite.hub_plan.v1, written by tools/models/hub_belt_world.py from
// the hub map): in the frame of the city module (frame_module_id, metres; the city plane is local X-Z, +Y up).
#include <string>
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
