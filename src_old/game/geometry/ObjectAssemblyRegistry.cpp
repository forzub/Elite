#include "ObjectAssemblyRegistry.h"

#include <stdexcept>

using namespace game::ship::geometry;

std::unordered_map<uint16_t, ObjectAssemblyDesc>
ObjectAssemblyRegistry::s_registry;

void ObjectAssemblyRegistry::init()
{
    s_registry.clear();
    constexpr float DEFAULT_ASSEMBLY_LOD_SWITCH_DISTANCE = 2500.0f;

    auto mesh = [](
        const std::string& id,
        const std::string& lod0,
        const std::string& lod1,
        const glm::vec3& localOffset = glm::vec3(0.0f)
    ) -> AssemblyMeshPartDesc
    {
        AssemblyMeshPartDesc d;
        d.meshId = id;
        d.lod0Path = lod0;
        d.lod1Path = lod1;
        d.enabled = true;
        d.localOffset = localOffset;
        return d;
    };

    auto sameMesh = [&mesh](
        const std::string& id,
        const std::string& path,
        const glm::vec3& localOffset = glm::vec3(0.0f)
    ) -> AssemblyMeshPartDesc
    {
        return mesh(id, path, path, localOffset);
    };

        auto module = [](
        const std::string& id,
        const std::vector<AssemblyMeshPartDesc>& meshes,
        bool rotates = false,
        const glm::vec3& axis = glm::vec3(0.0f, 0.0f, 1.0f),
        float speed = 0.0f,
        const glm::vec3& localPosition = glm::vec3(0.0f),
        const glm::vec3& localRotationDeg = glm::vec3(0.0f),
        const glm::vec3& pivot = glm::vec3(0.0f),
        const std::string& subsystemId = "",
        const std::string& parentModuleId = ""
    ) -> AssemblyModuleDesc
    {
        AssemblyModuleDesc d;
        d.moduleId = id;
        d.parentModuleId = parentModuleId;
        d.subsystemId = subsystemId;
        d.enabled = true;
        d.localPosition = localPosition;
        d.localRotationDeg = localRotationDeg;
        d.pivot = pivot;
        d.rotates = rotates;
        d.rotationAxis = axis;
        d.rotationSpeed = speed;
        d.meshes = meshes;
        return d;
    };

    // =========================================================
    // COBRA MK1
    // Assembly теперь повторяет логические moduleId из ShipDescriptor.
    // Это устраняет разрыв между descriptor hierarchy и geometry hierarchy.
    // =========================================================
    {
        ObjectAssemblyDesc cobramk1;
        cobramk1.typeId = ObjectType::CobraMk1;
        cobramk1.lodSwitchDistance = DEFAULT_ASSEMBLY_LOD_SWITCH_DISTANCE;

        cobramk1.modules = {
            // =====================================================
            // FRAME / STRUCTURE
            // =====================================================

            module(
                "ship_frame_CB",
                {
                    sameMesh("ship_frame_CB", "assets/models/ships/cobrettymk1/LOD0/ship_frame_CB.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "structural_frame",
                ""
            ),

            module(
                "ship_frame_CF",
                {
                    sameMesh("ship_frame_CF", "assets/models/ships/cobrettymk1/LOD0/ship_frame_CF.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "structural_frame",
                "ship_frame_CB"
            ),

            module(
                "ship_frame_FL",
                {
                    sameMesh("ship_frame_FL", "assets/models/ships/cobrettymk1/LOD0/ship_frame_FL.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "structural_frame",
                "ship_frame_CF"
            ),

            module(
                "ship_frame_FR",
                {
                    sameMesh("ship_frame_FR", "assets/models/ships/cobrettymk1/LOD0/ship_frame_FR.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "structural_frame",
                "ship_frame_CF"
            ),

            module(
                "ship_frame_reactor",
                {
                    sameMesh("ship_frame_reactor", "assets/models/ships/cobrettymk1/LOD0/ship_frame_reactor.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "propulsion_block",
                "ship_frame_CB"
            ),

            // =====================================================
            // FRONT AVIONICS / COCKPIT
            // =====================================================

            module(
                "ship_cockpit",
                {
                    sameMesh("ship_cockpit", "assets/models/ships/cobrettymk1/LOD0/ship_cockpit.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "avionics_front",
                "ship_frame_CF"
            ),

            module(
                "ship_radar",
                {
                    sameMesh("ship_radar", "assets/models/ships/cobrettymk1/LOD0/ship_radar.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "sensor_radar",
                "ship_frame_CF"
            ),

            // =====================================================
            // FLIGHT CONTROL
            // =====================================================

            module(
                "ship_control_unit_L",
                {
                    sameMesh("ship_control_unit_L", "assets/models/ships/cobrettymk1/LOD0/ship_control_unit_L.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "flight_control",
                "ship_frame_FL"
            ),

            module(
                "ship_control_unit_R",
                {
                    sameMesh("ship_control_unit_R", "assets/models/ships/cobrettymk1/LOD0/ship_control_unit_R.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "flight_control",
                "ship_frame_FR"
            ),

            // =====================================================
            // WEAPONS
            // =====================================================

            module(
                "ship_laser_L",
                {
                    sameMesh("ship_laser_L", "assets/models/ships/cobrettymk1/LOD0/ship_laser_L.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "weapon_mounts",
                "ship_frame_FL"
            ),

            module(
                "ship_laser_R",
                {
                    sameMesh("ship_laser_R", "assets/models/ships/cobrettymk1/LOD0/ship_laser_R.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "weapon_mounts",
                "ship_frame_FR"
            ),

            // =====================================================
            // OUTER SKIN PANELS
            // =====================================================

            module(
                "ship_panel_bottom_LB",
                {
                    sameMesh("ship_panel_bottom_LB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_bottom_LB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_bottom_LF",
                {
                    sameMesh("ship_panel_bottom_LF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_bottom_LF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FL"
            ),

            module(
                "ship_panel_bottom_RB",
                {
                    sameMesh("ship_panel_bottom_RB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_bottom_RB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_bottom_RF",
                {
                    sameMesh("ship_panel_bottom_RF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_bottom_RF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FR"
            ),

            module(
                "ship_panel_bottom_C",
                {
                    sameMesh("ship_panel_bottom_C", "assets/models/ships/cobrettymk1/LOD0/ship_panel_bottom_C.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CF"
            ),

            module(
                "ship_panel_side_LB",
                {
                    sameMesh("ship_panel_side_LB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_side_LB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_side_LF",
                {
                    sameMesh("ship_panel_side_LF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_side_LF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FL"
            ),

            module(
                "ship_panel_side_RB",
                {
                    sameMesh("ship_panel_side_RB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_side_RB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_side_RF",
                {
                    sameMesh("ship_panel_side_RF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_side_RF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FR"
            ),

            module(
                "ship_panel_back",
                {
                    sameMesh("ship_panel_back", "assets/models/ships/cobrettymk1/LOD0/ship_panel_back.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_top_CB",
                {
                    sameMesh("ship_panel_top_CB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_CB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_top_CF",
                {
                    sameMesh("ship_panel_top_CF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_CF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CF"
            ),

            module(
                "ship_panel_top_LB",
                {
                    sameMesh("ship_panel_top_LB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_LB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_top_LF",
                {
                    sameMesh("ship_panel_top_LF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_LF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FL"
            ),

            module(
                "ship_panel_top_RB",
                {
                    sameMesh("ship_panel_top_RB", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_RB.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_CB"
            ),

            module(
                "ship_panel_top_RF",
                {
                    sameMesh("ship_panel_top_RF", "assets/models/ships/cobrettymk1/LOD0/ship_panel_top_RF.obj")
                },
                false, glm::vec3(0.0f, 0.0f, 1.0f), 0.0f,
                glm::vec3(0.0f), glm::vec3(0.0f), glm::vec3(0.0f),
                "outer_skin", "ship_frame_FR"
            ),

            // =====================================================
            // PROPULSION
            // =====================================================

            module(
                "ship_engine_L",
                {
                    sameMesh("ship_engine_L", "assets/models/ships/cobrettymk1/LOD0/ship_engine_L.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "propulsion_block",
                "ship_frame_reactor"
            ),

            module(
                "ship_engine_R",
                {
                    sameMesh("ship_engine_R", "assets/models/ships/cobrettymk1/LOD0/ship_engine_R.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "propulsion_block",
                "ship_frame_reactor"
            ),

            module(
                "ship_tank_L",
                {
                    sameMesh("ship_tank_L", "assets/models/ships/cobrettymk1/LOD0/ship_tank_L.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "propulsion_block",
                "ship_frame_reactor"
            ),

            module(
                "ship_tank_R",
                {
                    sameMesh("ship_tank_R", "assets/models/ships/cobrettymk1/LOD0/ship_tank_R.obj")
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "propulsion_block",
                "ship_frame_reactor"
            )
        };

        s_registry[static_cast<uint16_t>(cobramk1.typeId)] = std::move(cobramk1);
    }

    // =========================================================
    // STATION
    // Теперь habitat ring — это ОДИН логический модуль,
    // состоящий из нескольких мешей.
    // =========================================================
    {
        ObjectAssemblyDesc station;
        station.typeId = ObjectType::Station;
        // Для больших объектов можно задать свою дистанцию отдельно.
        station.lodSwitchDistance = 10000.0f;

        station.modules = {
            module(
                "station_core",
                {
                    
                    mesh(
                        "station_core_rear",
                        "assets/models/stations/LOD0/station_Core_Rear.obj",
                        "assets/models/stations/LOD1/lod1_station_Core_Rear.obj"
                    )
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),    // localPosition
                glm::vec3(0.0f),    // localRotationDeg
                glm::vec3(0.0f),    // pivot
                "station_core"
            ),

            module(
                "station_habitat_ring",
                {
                    mesh(
                        "station_core_front",
                        "assets/models/stations/LOD0/station_Core_Front.obj",
                        "assets/models/stations/LOD1/lod1_station_Core_Front.obj"
                    ),
                    mesh(
                        "station_habitat_s1",
                        "assets/models/stations/LOD0/station_Habitat_Module_S1.obj",
                        "assets/models/stations/LOD1/lod1_station_Habitat_Module_S1.obj"
                    ),
                    mesh(
                        "station_habitat_s2",
                        "assets/models/stations/LOD0/station_Habitat_Module_S2.obj",
                        "assets/models/stations/LOD1/lod1_station_Habitat_Module_S2.obj"
                    ),
                    mesh(
                        "station_habitat_s3",
                        "assets/models/stations/LOD0/station_Habitat_Module_S3.obj",
                        "assets/models/stations/LOD1/lod1_station_Habitat_Module_S3.obj"
                    )
                },
                true,
                glm::vec3(1.0f, 0.0f, 0.0f),
                0.05f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "habitat_ring"
            ),

            module(
                "station_solar_panels",
                {
                    mesh(
                        "station_solar_panels",
                        "assets/models/stations/LOD0/station_Solar_Panels.obj",
                        "assets/models/stations/LOD1/lod1_station_Solar_Panels.obj"
                    )
                },
                false,
                glm::vec3(0.0f, 0.0f, 1.0f),
                0.0f,
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f),
                "solar_array"
            )
        };

        s_registry[static_cast<uint16_t>(station.typeId)] = std::move(station);
    }
}

bool ObjectAssemblyRegistry::has(ObjectType typeId)
{
    return s_registry.find(static_cast<uint16_t>(typeId)) != s_registry.end();
}

const ObjectAssemblyDesc& ObjectAssemblyRegistry::get(ObjectType typeId)
{
    auto it = s_registry.find(static_cast<uint16_t>(typeId));
    if (it == s_registry.end())
        throw std::runtime_error("[ObjectAssemblyRegistry] assembly not found");

    return it->second;
}