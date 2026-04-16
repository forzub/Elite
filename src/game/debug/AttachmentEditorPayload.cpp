#include "game/debug/AttachmentEditorPayload.h"

#include "game/SpaceState.h"
#include "game/ship/ShipDescriptor.h"
#include "game/ship/ShipDescriptorRegistry.h"
#include "game/ship/ShipAttachmentPoint.h"
#include "game/geometry/AssemblyMeshLibrary.h"
#include "game/geometry/ObjectAssembly.h"
#include "src/world/types/ObjectType.h"

using json = nlohmann::json;
using namespace game::ship::geometry;

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

nlohmann::json buildAttachmentEditorPayload(const SpaceState& state)
{
    json payload;
    payload["objects"] = json::array();

    const ObjectType typeId = ObjectType::CobraMk1;

    const ShipDescriptor& desc = ShipDescriptorRegistry::get(typeId);
    const ObjectAssembly& assembly = AssemblyMeshLibrary::get(typeId);

    json ship;
    ship["shipId"] = "player_ship";
    ship["displayName"] = "Cobra Mk.I";
    ship["typeId"] = static_cast<int>(typeId);
    ship["visualBasisRotationDeg"] = {
        desc.visualBasisRotationDeg().x,
        desc.visualBasisRotationDeg().y,
        desc.visualBasisRotationDeg().z
    };
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

    payload["objects"].push_back(ship);
    return payload;
}