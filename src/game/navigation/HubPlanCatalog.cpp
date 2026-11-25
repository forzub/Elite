#include "src/game/navigation/HubPlanCatalog.h"
#include "src/game/navigation/HubPlanPads.h"
#include <cstdlib>

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
    assignHubPlanAddressesAndPads(m_blocks, m_modules);   // the addresses and the pads (HubPlanPads.h)
    // DEV: ELITE_DEV_PAD_CENSUS=<hub id> - the pads of that hub by kind, side and deck height (the pad concept review,
    // author 2026-11-22: «ты их абсолютно все разместил над крышей»)
    if (const char* census = std::getenv("ELITE_DEV_PAD_CENSUS"); census && m_hubId == census)
    {
        int n[5][2] = {};
        double hmin[5] = {1e9, 1e9, 1e9, 1e9, 1e9}, hmax[5] = {-1e9, -1e9, -1e9, -1e9, -1e9};
        for (const auto& m : m_modules)
            if (m.pad)
            {
                const int k = static_cast<int>(m.padKind);
                if (k < 0 || k > 4)
                    continue;
                ++n[k][m.under ? 1 : 0];
                if (m.padKind == HubPlanModule::PadKind::Hangar)
                    std::fprintf(stderr, "[PadCensus] %s repair hangar at %s\n", m_hubId.c_str(), m.address.c_str());
                hmin[k] = std::min(hmin[k], std::abs(m.padPosition.y));
                hmax[k] = std::max(hmax[k], std::abs(m.padPosition.y));
            }
        // the buildings with several places (their own + the lower side's under the footprint): several card buttons
        int multi = 0;
        for (const auto& m : m_modules)
        {
            if (m.under || m.pier || m.address.empty())
                continue;
            int places = m.pad ? 1 : 0;
            for (const auto& u : m_modules)
                places += u.under && u.pad && u.padPosition.x >= m.rect.x && u.padPosition.x <= m.rect.z &&
                          u.padPosition.z >= m.rect.y && u.padPosition.z <= m.rect.w ? 1 : 0;
            if (places > 1 && ++multi <= 6)
                std::fprintf(stderr, "[PadCensus] %s %s: %d places (upper + under)\n", m_hubId.c_str(), m.address.c_str(), places);
        }
        std::fprintf(stderr, "[PadCensus] %s buildings with several places: %d\n", m_hubId.c_str(), multi);
        static const char* names[] = {"none", "roof", "stilts", "garage", "hangar"};
        for (int k = 1; k < 5; ++k)
            std::fprintf(stderr, "[PadCensus] %s %s: %d upper, %d under, deck %.0f..%.0f m\n", m_hubId.c_str(), names[k], n[k][0],
                         n[k][1], hmin[k], hmax[k]);
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
    if (j.contains("ring") && j["ring"].is_object())
    {
        const auto& r = j["ring"];
        m_ring.radius = r.value("radius_m", 0.0);
        m_ring.length = r.value("length_m", 0.0);
        m_ring.x0 = r.value("x0_m", 0.0);
        m_ring.periodSeconds = r.value("period_s", 0.0);   // the ring city's spin (the plan's rim_g at the deck)
        m_ring.valid = m_ring.radius > 1.0 && m_ring.length > 1.0;
    }
    if (j.contains("monorail") && j["monorail"].is_object())
    {
        const auto& m = j["monorail"];
        for (const auto& p : m.value("loop", nlohmann::json::array()))
            if (p.is_array() && p.size() >= 2)
                m_roads.monorailLoop.push_back({p[0].get<double>(), p[1].get<double>()});
        for (const auto& p : m.value("stations", nlohmann::json::array()))
            if (p.is_array() && p.size() >= 2)
                m_roads.monorailStations.push_back({p[0].get<double>(), p[1].get<double>()});
        m_roads.monorailLoopY = m.value("rail_y", m_roads.monorailLoopY);
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
