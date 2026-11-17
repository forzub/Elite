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
    OrbitalCargoShuttle,

    // Hub district prototype kit (tools/models/units/hub_district.py, game/station/descriptors/HubKitDescriptor.h).
    HubKitTruss,
    HubKitHabA,
    HubKitHabB,
    HubKitHabC,
    HubKitTower,
    HubKitMushroom,
    HubKitFactory,
    HubKitTanks,
    HubKitRock,
    HubDistrictProto,  // the whole district as one object (its package merges the kit modules)
    HubMegaProto,      // the megastructure prototype as one object (tools/models/units/hub_mega.py)
    // belt settlement (tools/models/units/hub_belt.py): the generated city of ONE hub (its package is
    // hub_modules/hub_<hub id> - one type for every settlement) + the spinning bodies, separate objects each
    HubBeltCity,
    HubBeltRing,
    HubBeltDrum,
    HubBeltHotel
};