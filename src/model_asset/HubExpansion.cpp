#include "src/model_asset/HubExpansion.h"

#include "src/world/time/ConstructionClock.h"
#include "src/game/navigation/HubPlanPads.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace elite::model_asset
{
namespace
{
using json = nlohmann::json;

ExpansionNode nodeOf(const json& r, std::size_t at)
{
    ExpansionNode n;
    n.id = r.at(0).get<std::string>();
    n.folder = r.at(1).get<std::string>();
    n.position = glm::vec3(r.at(at).get<float>(), r.at(at + 1).get<float>(), r.at(at + 2).get<float>());
    n.rotationDeg = glm::vec3(r.at(at + 3).get<float>(), r.at(at + 4).get<float>(), r.at(at + 5).get<float>());
    return n;
}

std::unique_ptr<HubExpansionPlan> loadPlan(const std::string& hubId)
{
    // DEV: ELITE_DEV_HUB_EXPANSION_DIR=<dir> reads the plans from there (a test plan: tools/models/hub_expansion.py
    // --test-build, e.g. a Sol city shown as being built for a live check in the start system)
    static const std::string dir = [] {
        const char* v = std::getenv("ELITE_DEV_HUB_EXPANSION_DIR");
        return std::string(v && *v ? v : "assets/data/navigation/hub_expansion");
    }();
    std::string path = dir + "/" + hubId + ".json";
    if (!std::filesystem::exists(path))
        path = "assets/data/navigation/hub_expansion/" + hubId + ".json";
    std::ifstream in(path);
    if (!in)
        return nullptr;
    try
    {
        json doc;
        in >> doc;
        if (doc.value("schema", std::string()) != "elite.hub_expansion.v1")
            return nullptr;
        auto plan = std::make_unique<HubExpansionPlan>();
        plan->hubId = hubId;
        plan->epochUniverseS = doc.value("epoch_universe_s", 853005600.0);
        plan->daySeconds = doc.value("day_s", 86400.0);
        plan->gravity = doc.value("gravity", false);
        std::map<std::string, int> kitIndex;
        for (const auto& [name, stages] : doc.at("kits").items())
        {
            kitIndex[name] = static_cast<int>(plan->kits.size());
            plan->kitNames.push_back(name);
            plan->kits.push_back(stages.get<std::vector<std::string>>());
        }
        const auto kitOf = [&](const json& v) {
            const auto it = kitIndex.find(v.get<std::string>());
            return it == kitIndex.end() ? 0 : it->second;
        };
        for (const auto& jp : doc.at("projects"))
        {
            ExpansionProject p;
            p.id = jp.value("id", std::string());
            p.kind = jp.value("kind", std::string());
            p.startDay = jp.value("start_day", 0);
            p.workDays = std::max(1, jp.value("work_days", 1));
            for (const auto& h : jp.value("halts", json::array()))
                p.halts.push_back({h.at(0).get<int>(), h.at(1).is_null() ? -1 : h.at(1).get<int>()});
            for (const auto& r : jp.value("replace", json::array()))
                p.replace.push_back({r.at(0).get<std::uint32_t>(), kitOf(r.at(1)), r.at(2).get<float>(), r.at(3).get<float>()});
            for (const auto& r : jp.value("add", json::array()))
            {
                ExpansionProject::Add a;
                a.node = nodeOf(r, 3);
                a.kit = kitOf(r.at(2));
                a.a = r.at(9).get<float>();
                a.b = r.at(10).get<float>();
                p.add.push_back(std::move(a));
            }
            for (const auto& r : jp.value("fixed", json::array()))
                p.fixed.push_back(nodeOf(r, 2));
            for (const auto& r : jp.value("props", json::array()))
                p.props.push_back(nodeOf(r, 2));
            for (const auto& r : jp.value("platforms", json::array()))
                p.platforms.emplace_back(r.at(0).get<float>(), r.at(1).get<float>(), r.at(2).get<float>(), r.at(3).get<float>());
            for (const auto& r : jp.value("front", json::array()))
                p.front.emplace_back(r.at(0).get<float>(), r.at(1).get<float>(), r.at(2).get<float>());
            plan->projects.push_back(std::move(p));
        }
        return plan;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[HubExpansion] " << path << ": " << e.what() << "\n";
        return nullptr;
    }
}

// a module package exists (a plan may name construction modules not packaged yet: those nodes keep their final
// module / are left out instead of failing the whole city)
bool packaged(const std::string& folder)
{
    static std::mutex mutex;
    static std::unordered_map<std::string, bool> known;
    std::lock_guard<std::mutex> lock(mutex);
    const auto [it, inserted] = known.try_emplace(folder, false);
    if (inserted)
        it->second = std::filesystem::exists("assets/compiled/hub_modules/" + folder + "/" + folder + ".elmodel");
    return it->second;
}

int stageIndex(std::size_t stages, float u)
{
    if (u >= 1.0f)
        return -1;
    const float n = static_cast<float>(stages);
    return std::clamp(static_cast<int>(std::floor(std::max(0.0f, u) * n)), 0, static_cast<int>(stages) - 1);
}

bool propsShown(ConstructionStatus s)
{
    return s == ConstructionStatus::Building || s == ConstructionStatus::Halted || s == ConstructionStatus::Abandoned;
}
} // namespace

namespace
{
std::mutex& planMutex()
{
    static std::mutex m;
    return m;
}
std::unordered_map<std::string, std::unique_ptr<HubExpansionPlan>>& planCache()
{
    static std::unordered_map<std::string, std::unique_ptr<HubExpansionPlan>> cache;
    return cache;
}
std::atomic<std::uint64_t> g_overrideGeneration {0};   // bumps on a runtime override: the stage keys are recomputed
} // namespace

const HubExpansionPlan* hubExpansionPlan(const std::string& hubId)
{
    std::lock_guard<std::mutex> lock(planMutex());
    auto& cache = planCache();
    const auto [it, inserted] = cache.try_emplace(hubId);
    if (inserted)
        it->second = loadPlan(hubId);
    return it->second.get();
}

bool setHubExpansionProjectOverride(const std::string& hubId, const std::string& projectId, int startDay, int workDays,
                                    const std::vector<ExpansionProject::Halt>& halts)
{
    (void)hubExpansionPlan(hubId);   // loaded
    std::lock_guard<std::mutex> lock(planMutex());
    auto& cache = planCache();
    const auto it = cache.find(hubId);
    if (it == cache.end() || !it->second)
        return false;
    for (auto& p : it->second->projects)
        if (p.id == projectId)
        {
            p.startDay = startDay;
            p.workDays = std::max(1, workDays);
            p.halts = halts;
            ++g_overrideGeneration;
            return true;
        }
    return false;
}

int hubExpansionDay(const HubExpansionPlan& plan, double universeSeconds)
{
    return static_cast<int>(std::floor((universeSeconds - plan.epochUniverseS) / plan.daySeconds));
}

float projectProgress(const ExpansionProject& p, int day, ConstructionStatus* status)
{
    ConstructionStatus s = ConstructionStatus::Building;
    if (day < p.startDay)
    {
        if (status) *status = ConstructionStatus::Planned;
        return 0.0f;
    }
    int worked = day - p.startDay;
    for (const auto& h : p.halts)
    {
        if (day <= h.from)
            continue;
        if (h.to < 0)
        {
            worked -= day - h.from;
            s = ConstructionStatus::Abandoned;
        }
        else
        {
            worked -= std::min(day, h.to) - h.from;
            if (day < h.to)
                s = ConstructionStatus::Halted;
        }
    }
    const float prog = std::clamp(static_cast<float>(worked) / static_cast<float>(p.workDays), 0.0f, 1.0f);
    if (prog >= 1.0f)
        s = ConstructionStatus::Done;
    if (status) *status = s;
    return prog;
}

const std::string& kitStageFolder(const HubExpansionPlan& plan, int kit, float u)
{
    static const std::string none;
    if (kit < 0 || static_cast<std::size_t>(kit) >= plan.kits.size() || plan.kits[static_cast<std::size_t>(kit)].empty())
        return none;
    const auto& stages = plan.kits[static_cast<std::size_t>(kit)];
    const int i = stageIndex(stages.size(), u);
    return i < 0 ? none : stages[static_cast<std::size_t>(i)];
}

void setHubExpansionUniverseTime(double universeSeconds)
{
    world::time::setConstructionUniverseTime(universeSeconds);
}

double hubExpansionUniverseTime()
{
    return world::time::constructionUniverseTime();   // the server's / client's universe time (ConstructionClock.h)
}

std::uint64_t hubExpansionStageKey(const std::string& hubId)
{
    const HubExpansionPlan* plan = hubExpansionPlan(hubId);
    if (!plan)
        return 0;
    const int day = hubExpansionDay(*plan, hubExpansionUniverseTime());
    static std::mutex mutex;
    static std::unordered_map<std::string, std::pair<int, std::uint64_t>> cache;
    static std::uint64_t seenGeneration = 0;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (seenGeneration != g_overrideGeneration.load())
        {
            cache.clear();   // a runtime override changed some project: recompute every key
            seenGeneration = g_overrideGeneration.load();
        }
        const auto it = cache.find(hubId);
        if (it != cache.end() && it->second.first == day)
            return it->second.second;
    }
    std::uint64_t key = 1469598103934665603ull;
    const auto mix = [&](std::uint64_t v) { key = (key ^ v) * 1099511628211ull; };
    for (const auto& p : plan->projects)
    {
        ConstructionStatus s;
        const float prog = projectProgress(p, day, &s);
        mix(static_cast<std::uint64_t>(s));
        for (const auto& r : p.replace)
            mix(static_cast<std::uint64_t>(stageIndex(4, r.b > r.a ? (prog - r.a) / (r.b - r.a) : 1.0f) + 2));
        for (const auto& a : p.add)
            mix(static_cast<std::uint64_t>(stageIndex(4, a.b > a.a ? (prog - a.a) / (a.b - a.a) : 1.0f) + 2));
    }
    std::lock_guard<std::mutex> lock(mutex);
    cache[hubId] = {day, key};
    return key;
}

// applies the plan of `hubId` at the current game day to a frozen layout document (HubCityAssembly.cpp): replaced
// node modules, appended expansion nodes (after the layout's nodes: the layout's node indices stay what the server's
// damage state knows). Returns the number of changed / added nodes.
int applyHubExpansion(const std::string& hubId, json& doc)
{
    const HubExpansionPlan* plan = hubExpansionPlan(hubId);
    if (!plan)
        return 0;
    const int day = hubExpansionDay(*plan, hubExpansionUniverseTime());
    auto& modules = doc.at("modules");
    auto& nodes = doc.at("nodes");
    auto& groups = doc.at("groups");
    std::unordered_map<std::string, std::size_t> moduleOf;
    for (std::size_t i = 0; i < modules.size(); ++i)
        moduleOf[modules[i].at("folder").get<std::string>()] = i;
    const auto moduleIndex = [&](const std::string& folder) {
        const auto [it, inserted] = moduleOf.try_emplace(folder, modules.size());
        if (inserted)   // bounds 0: the assembly takes the module's mesh bounds for its far proxy
            modules.push_back({{"folder", folder}, {"lo", {0, 0, 0}}, {"hi", {0, 0, 0}}, {"proxy_material", "eq_hub_hull"}});
        return it->second;
    };
    const std::size_t layoutNodes = nodes.size();
    std::size_t group = groups.size();
    bool groupAdded = false;
    const auto append = [&](const ExpansionNode& n, const std::string& folder) {
        if (!packaged(folder))
            return 0;
        if (!groupAdded)
        {
            groups.push_back("construction");
            groupAdded = true;
        }
        nodes.push_back({n.id, moduleIndex(folder), group, n.position.x, n.position.y, n.position.z, n.rotationDeg.x,
                         n.rotationDeg.y, n.rotationDeg.z});
        return 1;
    };
    int changed = 0;
    for (const auto& p : plan->projects)
    {
        ConstructionStatus s;
        const float prog = projectProgress(p, day, &s);
        for (const auto& r : p.replace)
        {
            if (r.node >= layoutNodes)
                continue;
            const float u = s == ConstructionStatus::Planned ? -1.0f : r.b > r.a ? (prog - r.a) / (r.b - r.a) : 1.0f;
            const std::string& folder = kitStageFolder(*plan, r.kit, u);
            if (folder.empty() || !packaged(folder))
                continue;
            nodes[r.node][1] = moduleIndex(folder);
            ++changed;
        }
        if (s == ConstructionStatus::Planned)
            continue;
        for (const auto& f : p.fixed)
            changed += append(f, f.folder);
        for (const auto& a : p.add)
        {
            const float u = a.b > a.a ? (prog - a.a) / (a.b - a.a) : 1.0f;
            const std::string& stage = kitStageFolder(*plan, a.kit, u);
            changed += append(a.node, stage.empty() ? a.node.folder : stage);   // a stage not packaged yet: left out
        }
        if (propsShown(s))
            for (const auto& pr : p.props)
                changed += append(pr, pr.folder);
    }
    return changed;
}

} // namespace elite::model_asset

namespace elite::model_asset
{
// The hub city's landing pads (HubPlanPads.h, author 2026-11-21): one pad module per building that has a pad, placed
// from the hub's plan right after the layout's own nodes (before the construction plan's: their count changes with the
// game day, the pads' node indices must not - the server's damage state is per node index). Not packaged: left out.
int applyHubPads(const std::string& hubId, json& doc)
{
    std::ifstream in("assets/data/navigation/hub_plans/" + hubId + ".json");
    if (!in)
        return 0;
    json plan;
    try
    {
        in >> plan;
    }
    catch (const std::exception&)
    {
        return 0;
    }
    std::vector<game::navigation::HubPlanBlock> blocks;
    std::vector<game::navigation::HubPlanModule> planModules;
    for (const auto& b : plan.value("blocks", json::array()))
    {
        game::navigation::HubPlanBlock block;
        block.id = b.value("id", std::string());
        block.i = b.value("i", 0);
        block.j = b.value("j", 0);
        block.zone = b.value("zone", std::string());
        const auto c = b.value("centre", json::array());
        if (c.size() == 2)
            block.centre = glm::dvec2(c[0].get<double>(), c[1].get<double>());
        block.halfMeters = b.value("half_m", 300.0);
        blocks.push_back(std::move(block));
    }
    for (const auto& m : plan.value("modules", json::array()))
    {
        game::navigation::HubPlanModule module;
        const std::string blockId = m.value("block", json()).is_string() ? m["block"].get<std::string>() : std::string();
        for (std::size_t k = 0; k < blocks.size(); ++k)
            if (blocks[k].id == blockId)
                module.block = static_cast<int>(k);
        module.under = m.value("layer", std::string()) == "under";
        module.pier = m.value("role", std::string()) == "dock";
        module.module = m.value("module", std::string());
        const auto r = m.value("rect", json::array());
        if (r.size() == 4)
            module.rect = glm::dvec4(r[0].get<double>(), r[1].get<double>(), r[2].get<double>(), r[3].get<double>());
        const auto y = m.value("y", json::array());
        if (y.size() == 2)
        {
            module.y0 = y[0].get<double>();
            module.y1 = y[1].get<double>();
        }
        planModules.push_back(std::move(module));
    }
    game::navigation::assignHubPlanAddressesAndPads(blocks, planModules);

    auto& modules = doc.at("modules");
    auto& nodes = doc.at("nodes");
    auto& groups = doc.at("groups");
    std::unordered_map<std::string, std::size_t> moduleOf, groupOf;
    for (std::size_t i = 0; i < modules.size(); ++i)
        moduleOf[modules[i].at("folder").get<std::string>()] = i;
    for (std::size_t i = 0; i < groups.size(); ++i)
        groupOf[groups[i].get<std::string>()] = i;
    int added = 0;
    for (const auto& m : planModules)
    {
        const std::string folder = game::navigation::hubPadModuleFolder(m);
        if (folder.empty() || m.block < 0 || !packaged(folder))
            continue;
        const auto [mit, newModule] = moduleOf.try_emplace(folder, modules.size());
        if (newModule)   // bounds 0: the assembly takes the module's mesh bounds for its far proxy
            modules.push_back({{"folder", folder}, {"lo", {0, 0, 0}}, {"hi", {0, 0, 0}}, {"proxy_material", "eq_hub_hull"}});
        // the block's own group (its far proxy) - "c<i>_<j>_<zone>" as the layout names them; else a "pads" group
        const auto& b = blocks[static_cast<std::size_t>(m.block)];
        auto git = groupOf.find("c" + std::to_string(b.i) + "_" + std::to_string(b.j) + "_" + b.zone);
        if (git == groupOf.end())
        {
            git = groupOf.try_emplace("pads", groups.size()).first;
            if (git->second == groups.size())
                groups.push_back("pads");
        }
        nodes.push_back({"pad_" + m.address, mit->second, git->second, m.padOrigin.x, m.padOrigin.y, m.padOrigin.z,
                         m.under ? 180.0 : 0.0, m.padYawDeg, 0.0});
        ++added;
    }
    return added;
}
} // namespace elite::model_asset
