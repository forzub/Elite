#pragma once

#include <cstdint>

enum class ObjectType : uint16_t
{
    None,
    CobraMk1,
    Station,
    Asteroid,
    Planet,

    RepairDroneDebug,

    GuidanceDockCube,
    GuidanceDockCylinder,

    // Earth Orbital Hub rotating hex dock + its fixed service block.
    HubHexDock,
    HubDockServiceBlock,

    // OCS-4 orbital cargo shuttle (dock traffic).
    OrbitalCargoShuttle
};