#include "src/game/navigation/HubPlanCatalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
glm::dvec2 vec2(const nlohmann::json& j)
{
    return j.is_array() && j.size() >= 2 ? glm::dvec2(j[0].get<double>(), j[1].get<double>()) : glm::dvec2(0.0);
}
} // namespace

bool HubPlan::load(const std::string& path)
{
    *this = HubPlan();
    std::ifstream in(path);
    if (!in)
    {
        m_failure = "no plan file " + path;
        return false;
    }
    const nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_discarded() || j.value("schema", std::string()) != "elite.hub_plan.v1")
    {
        m_failure = "not an elite.hub_plan.v1 document: " + path;
        return false;
    }
    m_hubId = j.value("hub_id", std::string());
    m_frameModuleId = j.value("frame_module_id", std::string());
    m_cellMeters = j.value("cell_m", 600.0);
    m_population = j.value("population_estimate", 0);
    if (j.contains("bounds") && j["bounds"].size() == 4)
        m_bounds = glm::dvec4(j["bounds"][0].get<double>(), j["bounds"][1].get<double>(), j["bounds"][2].get<double>(),
                              j["bounds"][3].get<double>());
    for (const auto& b : j.value("blocks", nlohmann::json::array()))
    {
        HubPlanBlock block;
        block.id = b.value("id", std::string());
        block.i = b.value("i", 0);
        block.j = b.value("j", 0);
        block.zone = b.value("zone", std::string());
        block.title = b.value("title", block.zone);
        block.centre = vec2(b.value("centre", nlohmann::json::array()));
        block.halfMeters = b.value("half_m", 300.0);
        block.topHeightMeters = b.value("top_h", 0.0);
        block.underDepthMeters = b.value("under_h", 0.0);
        block.modules = b.value("modules", 0);
        m_blocks.push_back(std::move(block));
    }
    for (const auto& m : j.value("modules", nlohmann::json::array()))
    {
        HubPlanModule module;
        const std::string blockId = m.value("block", nlohmann::json()).is_string() ? m["block"].get<std::string>() : std::string();
        for (std::size_t k = 0; k < m_blocks.size(); ++k)
            if (m_blocks[k].id == blockId)
                module.block = static_cast<int>(k);
        module.under = m.value("layer", std::string()) == "under";
        module.pier = m.value("role", std::string()) == "dock";
        module.module = m.value("module", std::string());
        const auto r = m.value("rect", nlohmann::json::array());
        if (r.size() == 4)
            module.rect = glm::dvec4(r[0].get<double>(), r[1].get<double>(), r[2].get<double>(), r[3].get<double>());
        const auto y = m.value("y", nlohmann::json::array());
        if (y.size() == 2)
        {
            module.y0 = y[0].get<double>();
            module.y1 = y[1].get<double>();
        }
        m_modules.push_back(std::move(module));
    }
    // addresses: the block = the grid column letter + the row number ("C4"); a building = its number in the block, the
    // lower tier with a U ("C4-07", "C4-U03"). Pads: every building that is not a dock of its own (a pier, a dock / pad
    // module) and is wide enough for a small ship gets one on its roof (the lower tier: on its underside).
    for (auto& block : m_blocks)
        block.address = std::string(1, static_cast<char>('A' + std::clamp(block.i, 0, 25))) + std::to_string(block.j + 1);
    std::vector<int> upper(m_blocks.size(), 0), lower(m_blocks.size(), 0);
    for (auto& module : m_modules)
    {
        if (module.block < 0 || module.pier)
            continue;
        const auto k = static_cast<std::size_t>(module.block);
        module.number = module.under ? ++lower[k] : ++upper[k];
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s-%s%02d", m_blocks[k].address.c_str(), module.under ? "U" : "", module.number);
        module.address = buf;
        const bool ownDock = module.module.find("dock") != std::string::npos || module.module.find("pad") != std::string::npos ||
                             module.module.find("pier") != std::string::npos;
        if (ownDock)
            continue;
        const double half = 0.5 * std::min(module.rect.z - module.rect.x, module.rect.w - module.rect.y);
        const double height = module.under ? -module.y0 : module.y1;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        if (half >= 15.0 && height > 60.0)
        {
            // a tall wide building: the pad on its roof (the lower tier: its underside)
            module.padKind = HubPlanModule::PadKind::Roof;
            module.padRadiusMeters = std::min(40.0, 0.8 * half);
            module.padPosition = glm::dvec3(centre.x, module.under ? module.y0 : module.y1, centre.y);
        }
        else if (half >= 15.0)
        {
            // a low wide one: a garage for 1-2 small ships beside it, its apron on a free plot of the block at the plane
            module.padKind = HubPlanModule::PadKind::Garage;   // placed on a free plot below, at the plane
            module.padPlaces = half >= 30.0 ? 2 : 1;
        }
        else
            module.padKind = HubPlanModule::PadKind::Stilts;   // placed on a free plot below (needs every rect)
        module.pad = true;
    }
    // the pads on legs: the nearest free plot of the block (no building within the pad + 12 m) on a 10 m grid,
    // the deck 25 m over the plane (under it for the lower tier); none free: no pad
    for (auto& module : m_modules)
    {
        const bool garage = module.padKind == HubPlanModule::PadKind::Garage;
        if ((module.padKind != HubPlanModule::PadKind::Stilts && !garage) || module.block < 0)
            continue;
        const auto& block = m_blocks[static_cast<std::size_t>(module.block)];
        const double radius = 18.0, clear = radius + 12.0;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        std::vector<const HubPlanModule*> near;   // this block, this tier: the buildings and the pads on legs placed so far
        for (const auto& other : m_modules)
            if (other.block == module.block && other.under == module.under)
                near.push_back(&other);
        double best = 1.0e30;
        glm::dvec2 at(0.0);
        for (double x = block.centre.x - block.halfMeters + clear; x <= block.centre.x + block.halfMeters - clear; x += 10.0)
            for (double z = block.centre.y - block.halfMeters + clear; z <= block.centre.y + block.halfMeters - clear; z += 10.0)
            {
                bool free = true;
                for (const HubPlanModule* o : near)
                    if (const auto& other = *o; x > other.rect.x - clear && x < other.rect.z + clear && z > other.rect.y - clear && z < other.rect.w + clear)
                    {
                        free = false;
                        break;
                    }
                for (const HubPlanModule* o : near)   // and no other pad on legs already there
                    if (const auto& other = *o; free && o != &module && (other.padKind == HubPlanModule::PadKind::Stilts || other.padKind == HubPlanModule::PadKind::Garage) &&
                        other.padRadiusMeters > 0.0 && glm::length(glm::dvec2(other.padPosition.x, other.padPosition.z) - glm::dvec2(x, z)) <
                            2.0 * clear)
                        free = false;
                const double d = glm::length(glm::dvec2(x, z) - centre);
                if (free && d < best)
                {
                    best = d;
                    at = glm::dvec2(x, z);
                }
            }
        if (best > 1.0e29 || best > 250.0)
        {
            module.pad = false;
            module.padKind = HubPlanModule::PadKind::None;
            continue;
        }
        module.padRadiusMeters = radius;
        const double deck = garage ? 6.0 : 25.0;   // the garage apron at the plane; the pad on legs up high
        module.padPosition = glm::dvec3(at.x, module.under ? -deck : deck, at.y);
    }
    for (const auto& s : j.value("streets", nlohmann::json::array()))
        m_streets.push_back({vec2(s.value("a", nlohmann::json::array())), vec2(s.value("b", nlohmann::json::array())),
                             s.value("avenue", false), s.value("monorail", false)});
    for (const auto& d : j.value("docks", nlohmann::json::array()))
    {
        HubPlanDock dock;
        dock.moduleId = d.value("module_id", std::string());
        dock.anchorId = d.value("anchor_id", std::string());
        dock.title = d.value("title", dock.moduleId);
        dock.position = glm::dvec3(d.value("x", 0.0), d.value("y", 0.0), d.value("z", 0.0));
        dock.out = vec2(d.value("out", nlohmann::json::array({0.0, -1.0})));
        m_docks.push_back(std::move(dock));
    }
    for (const auto& s : j.value("spinning", nlohmann::json::array()))
        m_spinning.push_back({s.value("id", std::string()), s.value("module", std::string()),
                              glm::dvec2(s.value("x", 0.0), s.value("z", 0.0)), s.value("r", 100.0)});
    if (j.contains("roads") && j["roads"].is_object())
    {
        const auto& r = j["roads"];
        m_roads.topY = r.value("top_y", m_roads.topY);
        m_roads.underY = r.value("under_y", m_roads.underY);
        m_roads.monorailY = r.value("monorail_y", m_roads.monorailY);
        m_roads.monorailSideMeters = r.value("monorail_side_m", m_roads.monorailSideMeters);
        if (r.contains("lanes_m") && r["lanes_m"].is_array() && !r["lanes_m"].empty())
            m_roads.lanesMeters = r["lanes_m"].get<std::vector<double>>();
    }
    if (j.contains("air_fleet") && j["air_fleet"].is_object())
    {
        const auto& f = j["air_fleet"];
        m_bloc = f.value("bloc", std::string());
        for (const auto& s : f.value("ships", nlohmann::json::array()))
            m_airFleet.push_back({s.value("model", std::string()), s.value("name", std::string()), s.value("length_m", 30.0),
                                  s.value("weight", 1.0), s.value("passport", std::string())});
    }
    if (m_frameModuleId.empty() || m_blocks.empty())
    {
        m_failure = "empty plan " + path;
        return false;
    }
    return true;
}

const HubPlanBlock* HubPlan::block(const std::string& id) const
{
    for (const auto& b : m_blocks)
        if (b.id == id)
            return &b;
    return nullptr;
}

const HubPlanBlock* HubPlan::blockAt(double x, double z) const
{
    for (const auto& b : m_blocks)
        if (std::abs(x - b.centre.x) <= b.halfMeters && std::abs(z - b.centre.y) <= b.halfMeters)
            return &b;
    return nullptr;
}

namespace
{
struct CatalogData
{
    std::vector<std::unique_ptr<HubPlan>> plans;
    bool scanned = false;
};

CatalogData& catalog()
{
    // thread-safe first use (a function-local static's initializer): the server thread (hub security, promoted air
    // traffic) and the client main thread (the renderer) may ask first at the same time
    static CatalogData data = [] {
        CatalogData data;
        data.scanned = true;
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator("assets/data/navigation/hub_plans", ec))
        {
            if (entry.path().extension() != ".json")
                continue;
            auto plan = std::make_unique<HubPlan>();
            if (plan->load(entry.path().string()))
                data.plans.push_back(std::move(plan));
        }
        return data;
    }();
    return data;
}
} // namespace

const HubPlan* HubPlanCatalog::forHub(const std::string& hubId)
{
    for (const auto& plan : catalog().plans)
        if (plan->hubId() == hubId)
            return plan.get();
    return nullptr;
}

const HubPlan* HubPlanCatalog::forFrameModule(const std::string& moduleId)
{
    for (const auto& plan : catalog().plans)
        if (plan->frameModuleId() == moduleId)
            return plan.get();
    return nullptr;
}

std::string hubPlanZoneKey(const std::string& zone)
{
    return "hub.zone." + zone;
}

} // namespace game::navigation
