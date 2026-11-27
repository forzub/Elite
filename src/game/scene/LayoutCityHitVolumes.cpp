#include "src/game/simulation/CityPadPresence.h"
#include <unordered_map>
#include "src/game/scene/LayoutCityHitVolumes.h"

#include "src/model_asset/ModelAssetBinary.h"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/damage/HitComponent.h"
#include "src/model_asset/HubCityAssembly.h"
#include "src/model_asset/HubExpansion.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/world/modules/HitVolumeSnapshotBuilder.h"

namespace game::scene
{
void collisionToHitVolumes(ObjectType type, const elite::model_asset::ModelAsset& asset,
                           std::vector<game::damage::HitVolume>& out)
{
    namespace ma = elite::model_asset;
    const glm::mat4 basis = ObjectDescriptorRegistry::get(type).meshToLogicalBasis4();
    for (const auto& c : asset.collisionVolumes)
    {
        if (!c.enabled ||
            (!c.activeStates.empty() &&
             std::find(c.activeStates.begin(), c.activeStates.end(), "intact") == c.activeStates.end()))
            continue;
        glm::vec3 half;
        switch (c.shape)
        {
            case ma::CollisionShape::Box: half = c.halfSize; break;
            case ma::CollisionShape::Sphere: half = glm::vec3(c.radius); break;
            case ma::CollisionShape::Capsule: half = glm::vec3(c.radius, c.halfHeight + c.radius, c.radius); break;
            case ma::CollisionShape::Cylinder:
            {
                const float r = std::max(c.radius, c.radiusTop);
                half = glm::vec3(r, c.halfHeight, r);
                break;
            }
            default: continue;
        }
        glm::mat4 m = glm::translate(glm::mat4(1.0f), c.localPosition);
        m = glm::rotate(m, glm::radians(c.localRotationDeg.x), glm::vec3(1, 0, 0));
        m = glm::rotate(m, glm::radians(c.localRotationDeg.y), glm::vec3(0, 1, 0));
        m = glm::rotate(m, glm::radians(c.localRotationDeg.z), glm::vec3(0, 0, 1));
        m = basis * m;
        game::damage::HitVolume v;
        v.zone = game::damage::HitZoneType::Hull;
        v.center = glm::vec3(m[3]);
        glm::mat3 r(m);
        for (int k = 0; k < 3; ++k)
        {
            const float len = glm::length(r[k]);
            half[k] *= len;
            if (len > 1.0e-6f)
                r[k] /= len;
        }
        v.orientation = r;
        v.halfSize = half;
        v.m_label = c.id;
        v.moduleId = c.moduleId;
        v.assemblyNode = c.parentNodeIndex;   // the building (city damage: game::simulation::CityDamage.h)
        // the node's module group (a block, a street run) - the route planner merges a group far from the route
        if (c.parentNodeIndex >= 0 && static_cast<std::size_t>(c.parentNodeIndex) < asset.nodes.size())
            v.subsystemId = asset.nodes[static_cast<std::size_t>(c.parentNodeIndex)].moduleId;
        out.push_back(std::move(v));
    }
}

std::string hubKitPackagePath(ObjectType type)
{
    static const std::unordered_map<ObjectType, std::string> folders {
        {ObjectType::HubKitTruss, "hubk_truss"},
        {ObjectType::HubKitHabA, "hubk_hab_a"},
        {ObjectType::HubKitHabB, "hubk_hab_b"},
        {ObjectType::HubKitHabC, "hubk_hab_c"},
        {ObjectType::HubKitTower, "hubk_tower"},
        {ObjectType::HubKitMushroom, "hubk_mushroom"},
        {ObjectType::HubKitFactory, "hubk_factory"},
        {ObjectType::HubKitTanks, "hubk_tanks"},
        {ObjectType::HubKitRock, "hubk_rock"},
        {ObjectType::HubDistrictProto, "hub_proto_district"},
        {ObjectType::HubMegaProto, "hub_proto_mega"},
        {ObjectType::HubBeltRing, "hubb_ring_core"},
        {ObjectType::HubBeltDrum, "hubb_hab_drum"},
        {ObjectType::HubBeltHotel, "hubb_hotel_hex"},
    };
    const auto it = folders.find(type);
    return it == folders.end() ? std::string()
                               : "assets/compiled/hub_modules/" + it->second + "/" + it->second + ".elmodel";
}

bool layoutCityHitVolumesOnClient(ObjectType type, const std::string& hubId)
{
    // ELITE_REPLICATE_CITY_HIT_VOLUMES=1: the old way (the server sends them)
    static const bool replicate = [] {
        const char* v = std::getenv("ELITE_REPLICATE_CITY_HIT_VOLUMES");
        return v && v[0] == '1';
    }();
    if (replicate)
        return false;
    namespace ma = elite::model_asset;
    if (const std::string kit = hubKitPackagePath(type); !kit.empty())
    {
        static std::mutex kitMutex;
        static std::map<std::string, bool> kitKnown;
        std::lock_guard<std::mutex> lock(kitMutex);
        const auto [it, inserted] = kitKnown.try_emplace(kit, false);
        if (inserted)
            it->second = std::filesystem::exists(kit);
        return it->second;
    }
    if (type != ObjectType::HubBeltCity || hubId.empty())
        return false;
    // cities with construction stages too: the client assembles the stages by its own clock, as its renderer already
    // does (271 of the 294 layout cities have an expansion plan - excluding them left the bootstrap at 63 MB)
    static std::mutex mutex;
    static std::map<std::string, bool> known;
    std::lock_guard<std::mutex> lock(mutex);
    const auto [it, inserted] = known.try_emplace(hubId, false);
    if (inserted)
        it->second = std::filesystem::exists(ma::hubCityLayoutPath(hubId));
    return it->second;
}

namespace
{
// the background assembly workers (audit 2 LOW-MEDIUM: they were detached - one still running at the process exit read
// statics being destroyed): joinable, joined by joinClientGeometryWorkers() at the application's shutdown
std::mutex& workerMutex()
{
    static std::mutex m;
    return m;
}
std::vector<std::thread>& workers()
{
    static std::vector<std::thread> w;
    return w;
}

struct CityEntry
{
    std::atomic<bool> ready {false};
    std::uint64_t stageKey = 0;
    std::vector<game::simulation::DebugHitVolumeSnapshot> volumes;
    std::unordered_map<std::string, std::int32_t> padNodes;   // CityPadPresence.h: the pads this package has
    bool failed = false;   // the assembly failed (no package): NOT "an empty city"
};
std::mutex& cityCacheMutex()
{
    static std::mutex m;
    return m;
}
std::map<std::string, std::shared_ptr<CityEntry>>& cityCache()
{
    static std::map<std::string, std::shared_ptr<CityEntry>> c;
    return c;
}
} // namespace

const std::vector<game::simulation::DebugHitVolumeSnapshot>* clientLayoutCityHitVolumes(ObjectType type,
                                                                                       const std::string& hubId)
{
    std::mutex& mutex = cityCacheMutex();
    // per hub: the current entry and the one being built for a new construction stage (the old one stays in use
    // until the new one is ready)
    auto& cache = cityCache();
    static std::map<std::string, std::shared_ptr<CityEntry>> building;
    const std::string kit = hubKitPackagePath(type);
    const std::string cacheKey = kit.empty() ? hubId : kit;
    const std::uint64_t key = kit.empty() ? elite::model_asset::hubExpansionStageKey(hubId) : 0;
    std::shared_ptr<CityEntry> entry;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto& e = cache[cacheKey];
        auto& b = building[cacheKey];
        if (b && b->ready)
        {
            e = b;
            b.reset();
        }
        const bool stale = e && e->ready && e->stageKey != key;
        if ((!e || stale) && !b)
        {
            auto fresh = std::make_shared<CityEntry>();
            fresh->stageKey = key;
            if (!e)
                e = fresh;
            else
                b = fresh;
            std::shared_ptr<CityEntry> job = fresh;
            const std::string id = hubId;
            std::cerr << "[CityHitVolumes] " << cacheKey << ": assembling on the client (stage key " << key << ")" << std::endl;
            std::thread worker([job, id, type, kit] {
                namespace ma = elite::model_asset;
                ma::ModelAsset city;
                game::damage::HitComponent hit;
                std::string error;
                if (!kit.empty() ? ma::ModelAssetBinary::load(kit, city, &error)
                                 : ma::assembleHubCityCollision(id, "assets/compiled/hub_modules", city))
                    collisionToHitVolumes(type, city, hit.volumes);
                else
                {
                    job->failed = true;
                    // audit SRV-5: a failed assembly was "ready" with 0 volumes and no word - said now; the planner
                    // treats an object without volumes as no proven geometry (it does not fly through it)
                    std::cerr << "[CityHitVolumes] " << (kit.empty() ? id : kit) << ": assembly FAILED"
                              << (error.empty() ? std::string() : " (" + error + ")") << " - no obstacle geometry" << std::endl;
                }
                job->volumes = world::modules::HitVolumeSnapshotBuilder::build(hit);
                job->padNodes = game::simulation::cityPadNodes(hit.volumes);   // from the labels, before they are dropped
                std::cerr << "[CityHitVolumes] " << (kit.empty() ? id : kit) << ": " << job->volumes.size()
                          << " hit volumes assembled on the client" << std::endl;
                job->ready = true;
            });
            {
                std::lock_guard<std::mutex> workerLock(workerMutex());
                workers().push_back(std::move(worker));
            }
        }
        entry = e;
    }
    return entry->ready ? &entry->volumes : nullptr;
}
ClientCityPackageState clientLayoutCityPackageState(ObjectType type, const std::string& hubId)
{
    if (!clientLayoutCityHitVolumes(type, hubId))   // builds / refreshes the entry
        return ClientCityPackageState::Assembling;
    const std::string kit = hubKitPackagePath(type);
    std::lock_guard<std::mutex> lock(cityCacheMutex());
    const auto it = cityCache().find(kit.empty() ? hubId : kit);
    if (it == cityCache().end() || !it->second || !it->second->ready)
        return ClientCityPackageState::Assembling;
    return it->second->failed ? ClientCityPackageState::Failed : ClientCityPackageState::Ready;
}

const std::unordered_map<std::string, std::int32_t>* clientLayoutCityPadNodes(ObjectType type, const std::string& hubId)
{
    if (!clientLayoutCityHitVolumes(type, hubId))   // builds / refreshes the entry
        return nullptr;
    const std::string kit = hubKitPackagePath(type);
    std::lock_guard<std::mutex> lock(cityCacheMutex());
    const auto it = cityCache().find(kit.empty() ? hubId : kit);
    return it != cityCache().end() && it->second && it->second->ready ? &it->second->padNodes : nullptr;
}

void joinClientGeometryWorkers()
{
    std::vector<std::thread> running;
    {
        std::lock_guard<std::mutex> lock(workerMutex());
        running.swap(workers());
    }
    for (auto& t : running)
        if (t.joinable())
            t.join();
}
} // namespace game::scene
