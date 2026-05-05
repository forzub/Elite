#include "game/debug/VolumeViewerPayload.h"

#include <unordered_map>
#include <vector>
#include <string>

#include "game/SpaceState.h"
#include "src/debug/DebugSettings.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"
#include "src/game/geometry/ObjectAssembly.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"
#include "src/world/types/ObjectType.h"

using json = nlohmann::json;
using namespace game::ship::geometry;

namespace
{
    static ObjectType objectTypeFromString(const std::string& id)
    {
        if (id == "cobra_mk1")
            return ObjectType::CobraMk1;

        if (id == "station_01")
            return ObjectType::Station;

        return ObjectType::CobraMk1;
    }

    static std::string displayNameForType(ObjectType typeId)
    {
        switch (typeId)
        {
            case ObjectType::CobraMk1: return "Cobra Mk.I";
            case ObjectType::Station:  return "Station 01";
            default:                   return "Unknown";
        }
    }

    static json buildModulesJson(
        const IObjectDescriptor& desc,
        const ObjectAssembly& assembly
    )
    {
        json out = json::array();

        std::unordered_map<std::string, const AssemblyModule*> assemblyById;
        for (const auto& module : assembly.modules)
            assemblyById[module.id] = &module;

        for (const auto& modDesc : desc.moduleDescriptors())
        {
            json m;
            m["moduleId"] = modDesc.moduleId;
            m["parentModuleId"] = modDesc.parentModuleId;
            m["subsystemId"] = modDesc.subsystemId;
            m["meshPartIds"] = modDesc.meshPartIds;

            auto itAsm = assemblyById.find(modDesc.moduleId);
            if (itAsm != assemblyById.end() && itAsm->second)
            {
                const auto* asmMod = itAsm->second;

                m["moduleLocalPosition"] = {
                    asmMod->localPosition.x,
                    asmMod->localPosition.y,
                    asmMod->localPosition.z
                };

                m["moduleLocalRotationDeg"] = {
                    asmMod->localRotationDeg.x,
                    asmMod->localRotationDeg.y,
                    asmMod->localRotationDeg.z
                };
            }
            else
            {
                m["moduleLocalPosition"] = {0.0, 0.0, 0.0};
                m["moduleLocalRotationDeg"] = {0.0, 0.0, 0.0};
            }

            out.push_back(m);
        }

        return out;
    }

    static json buildMeshPartsJson(const ObjectAssembly& assembly)
    {
        json out = json::array();

        for (const auto& module : assembly.modules)
        {
            for (const auto& part : module.meshes)
            {
                json p;
                p["id"] = part.id;
                p["moduleId"] = module.id;

                p["moduleLocalPosition"] = {
                    module.localPosition.x,
                    module.localPosition.y,
                    module.localPosition.z
                };

                p["moduleLocalRotationDeg"] = {
                    module.localRotationDeg.x,
                    module.localRotationDeg.y,
                    module.localRotationDeg.z
                };

                p["localOffset"] = {
                    part.localOffset.x,
                    part.localOffset.y,
                    part.localOffset.z
                };

                json vertices = json::array();
                for (const auto& v : part.lod0Mesh.vertices)
                {
                    vertices.push_back({
                        v.position.x,
                        v.position.y,
                        v.position.z
                    });
                }

                json triangles = json::array();
                for (const auto& t : part.lod0Mesh.triangles)
                {
                    triangles.push_back({ t.v0, t.v1, t.v2 });
                }

                p["vertices"] = vertices;
                p["triangles"] = triangles;

                out.push_back(p);
            }
        }

        return out;
    }

    static json serializeHitVolume(const game::damage::HitVolume& v)
    {
        json item;

        item["center"] = {
            v.center.x,
            v.center.y,
            v.center.z
        };

        item["halfSize"] = {
            v.halfSize.x,
            v.halfSize.y,
            v.halfSize.z
        };

        item["orientation"] = {
            { v.orientation[0].x, v.orientation[0].y, v.orientation[0].z },
            { v.orientation[1].x, v.orientation[1].y, v.orientation[1].z },
            { v.orientation[2].x, v.orientation[2].y, v.orientation[2].z }
        };

        item["moduleId"] = v.moduleId;
        item["subsystemId"] = v.subsystemId;
        item["layerIndex"] = v.layerIndex;
        item["priority"] = v.priority;
        item["destroyed"] = v.destroyed;
        item["supportLinkVolume"] = v.supportLinkVolume;
        item["supportLinkId"] = v.supportLinkId;
        item["supportModuleId"] = v.supportModuleId;

        return item;
    }

    static void buildPreviewVolumes(
        ObjectType typeId,
        const IObjectDescriptor& desc,
        const ObjectAssembly& assembly,
        json& primaryVolumesOut,
        json& supportVolumesOut,
        json& seamCandidatesOut
    )
    {
        primaryVolumesOut = json::array();
        supportVolumesOut = json::array();
        seamCandidatesOut = json::array();


        world::modules::ObjectModuleRuntime moduleRuntime;
        moduleRuntime.init(desc.moduleDescriptors());

        world::modules::ObjectStructuralLinkRuntime structuralLinkRuntime;
        structuralLinkRuntime.init(desc.moduleDescriptors());

        moduleRuntime.reevaluateStructuralStates(&structuralLinkRuntime);

        world::modules::ObjectAssemblyRuntime assemblyRuntime;
        assemblyRuntime.init(assembly);



        game::damage::HitComponent hitComponent;

        auto& dbg = debug::get().render;
        const bool oldCapture = dbg.captureSeamDebug;
        const auto oldSeams = dbg.seamDebugProxies;

        dbg.captureSeamDebug = true;
        dbg.seamDebugProxies.clear();

        world::modules::ObjectRuntimeHitBuilder::rebuild(
            hitComponent,
            typeId,
            desc,
            moduleRuntime,
            structuralLinkRuntime,
            assemblyRuntime
        );

        for (const auto& v : hitComponent.volumes)
        {
            if (v.supportLinkVolume)
                supportVolumesOut.push_back(serializeHitVolume(v));
            else
                primaryVolumesOut.push_back(serializeHitVolume(v));
        }

        for (const auto& s : dbg.seamDebugProxies)
        {
            json item;
            item["center"] = {
                s.centerWorld.x,
                s.centerWorld.y,
                s.centerWorld.z
            };

            item["halfSize"] = {
                s.halfSize.x,
                s.halfSize.y,
                s.halfSize.z
            };

            item["orientation"] = {
                { s.orientationWorld[0].x, s.orientationWorld[0].y, s.orientationWorld[0].z },
                { s.orientationWorld[1].x, s.orientationWorld[1].y, s.orientationWorld[1].z },
                { s.orientationWorld[2].x, s.orientationWorld[2].y, s.orientationWorld[2].z }
            };

            item["score"] = s.score;
            item["neighborModuleId"] = s.neighborModuleId;

            seamCandidatesOut.push_back(item);
        }

        dbg.seamDebugProxies = oldSeams;
        dbg.captureSeamDebug = oldCapture;
    }
}

json buildVolumeViewerPreviewForType(
    const SpaceState& state,
    const std::string& shipTypeId
)
{
    (void)state;

    const ObjectType typeId = objectTypeFromString(shipTypeId);

    const IObjectDescriptor& desc = ObjectDescriptorRegistry::get(typeId);
    const ObjectAssembly& assembly = AssemblyMeshLibrary::get(typeId);

    json ship;
    ship["shipId"] = shipTypeId;
    ship["displayName"] = displayNameForType(typeId);
    ship["typeId"] = shipTypeId;

    ship["visualBasisRotationDeg"] = {
        desc.visualBasisRotationDeg().x,
        desc.visualBasisRotationDeg().y,
        desc.visualBasisRotationDeg().z
    };

    ship["modules"] = buildModulesJson(desc, assembly);
    ship["meshParts"] = buildMeshPartsJson(assembly);

    buildPreviewVolumes(
        typeId,
        desc,
        assembly,
        ship["primaryVolumes"],
        ship["supportVolumes"],
        ship["seamCandidates"]
    );

    return ship;
}