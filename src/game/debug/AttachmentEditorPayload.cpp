#include "game/debug/AttachmentEditorPayload.h"

#include "game/SpaceState.h"
#include "game/ship/ShipDescriptor.h"
#include "game/ship/ShipDescriptorRegistry.h"
#include "game/ship/ShipAttachmentPoint.h"
#include "game/geometry/AssemblyMeshLibrary.h"
#include "game/geometry/ObjectAssembly.h"
#include "src/world/types/ObjectType.h"
#include <unordered_map>

#include "src/debug/DebugSettings.h"
#include "src/world/descriptors/IObjectDescriptor.h"
#include "src/world/modules/ObjectAssemblyRuntime.h"
#include "src/world/modules/ObjectModuleRuntime.h"
#include "src/world/modules/ObjectStructuralLinkRuntime.h"
#include "src/world/modules/ObjectRuntimeHitBuilder.h"
#include "src/game/damage/HitComponent.h"

using json = nlohmann::json;
using namespace game::ship::geometry;

namespace
{
    static std::string attachmentKindToString(ShipAttachmentKind kind)
    {
        switch (kind)
        {
            case ShipAttachmentKind::CameraCockpit: return "CameraCockpit";
            case ShipAttachmentKind::CameraRear: return "CameraRear";
            case ShipAttachmentKind::CameraDrone: return "CameraDrone";
            case ShipAttachmentKind::WeaponMuzzle: return "WeaponMuzzle";
            case ShipAttachmentKind::MissileRack: return "MissileRack";
            case ShipAttachmentKind::ContainerMount: return "ContainerMount";
            case ShipAttachmentKind::DroneDock: return "DroneDock";
            default: return "Unknown";
        }
    }

    static ObjectType objectTypeFromString(const std::string& shipTypeId)
    {
        if (shipTypeId == "cobra_mk1")
            return ObjectType::CobraMk1;

        // fallback
        return ObjectType::CobraMk1;
    }


        static json buildModulesJson(
        const ShipDescriptor& desc,
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
}



static json serializeHitVolumeForAttachmentEditor(const game::damage::HitVolume& v)
{
    json item;

    item["id"] = v.supportLinkVolume ? v.supportLinkId : v.m_label;
    item["label"] = v.m_label;
    item["moduleId"] = v.moduleId;
    item["subsystemId"] = v.subsystemId;
    item["layerIndex"] = v.layerIndex;
    item["priority"] = v.priority;
    item["destroyed"] = v.destroyed;

    item["supportLinkVolume"] = v.supportLinkVolume;
    item["supportLinkId"] = v.supportLinkId;
    item["supportModuleId"] = v.supportModuleId;

    item["center"] = { v.center.x, v.center.y, v.center.z };
    item["halfSize"] = { v.halfSize.x, v.halfSize.y, v.halfSize.z };

    item["orientation"] = {
        { v.orientation[0].x, v.orientation[0].y, v.orientation[0].z },
        { v.orientation[1].x, v.orientation[1].y, v.orientation[1].z },
        { v.orientation[2].x, v.orientation[2].y, v.orientation[2].z }
    };

    return item;
}

static void buildHitVolumesForAttachmentEditor(
    ObjectType typeId,
    const IObjectDescriptor& desc,
    const ObjectAssembly& assembly,
    json& primaryVolumesOut,
    json& supportVolumesOut
)
{
    primaryVolumesOut = json::array();
    supportVolumesOut = json::array();

    world::modules::ObjectModuleRuntime moduleRuntime;
    moduleRuntime.init(desc.moduleDescriptors());

    world::modules::ObjectStructuralLinkRuntime structuralLinkRuntime;
    structuralLinkRuntime.init(desc.moduleDescriptors());

    moduleRuntime.reevaluateStructuralStates(&structuralLinkRuntime);

    world::modules::ObjectAssemblyRuntime assemblyRuntime;
    assemblyRuntime.init(assembly);

    game::damage::HitComponent hitComponent;

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
            supportVolumesOut.push_back(serializeHitVolumeForAttachmentEditor(v));
        else
            primaryVolumesOut.push_back(serializeHitVolumeForAttachmentEditor(v));
    }
}









json buildAttachmentEditorPreviewForType(
    const SpaceState& state,
    const std::string& shipTypeId
)
{
    const ObjectType typeId = objectTypeFromString(shipTypeId);

    const ShipDescriptor& desc = ShipDescriptorRegistry::get(typeId);
    const ObjectAssembly& assembly = AssemblyMeshLibrary::get(typeId);

    json ship;
    ship["shipId"] = shipTypeId;
    ship["displayName"] = "Cobra Mk.I";
    ship["typeId"] = shipTypeId;
    ship["visualBasisRotationDeg"] = {
        desc.visualBasisRotationDeg().x,
        desc.visualBasisRotationDeg().y,
        desc.visualBasisRotationDeg().z
    };
    ship["modules"] = buildModulesJson(desc, assembly);
    ship["meshParts"] = buildMeshPartsJson(assembly);
    ship["attachments"] = json::array();

    for (const auto& att : desc.attachments)
    {
        json a;
        a["id"] = att.id;
        a["kind"] = attachmentKindToString(att.kind);
        a["parentModuleId"] = att.parentModuleId;

        auto itOverride = state.attachmentEditorOverrides().find(att.id);
        const bool hasOverride = (itOverride != state.attachmentEditorOverrides().end());

        const glm::vec3 pos = hasOverride ? itOverride->second.localPosition : att.localPosition;
        const glm::vec3 rot = hasOverride ? itOverride->second.localRotationDeg : att.localRotationDeg;
        const bool enabled = hasOverride ? itOverride->second.enabled : att.enabled;

        a["localPosition"] = { pos.x, pos.y, pos.z };
        a["localRotationDeg"] = { rot.x, rot.y, rot.z };
        a["enabled"] = enabled;

        ship["attachments"].push_back(a);
    }

    buildHitVolumesForAttachmentEditor(
        typeId,
        desc,
        assembly,
        ship["primaryVolumes"],
        ship["supportVolumes"]
    );

    return ship;
}