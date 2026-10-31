#pragma once

#include "src/game/ship/ShipDescriptor.h"

// World-side descriptor of the OCS-4 "Lastochka" orbital cargo shuttle
// (passport: src/game/catalog/ships/OrbitalCargoShuttlePassports.h). It
// currently exists as dock traffic (TrafficShell, kinematic Newtonian-looking
// flight from src/game/traffic/DockServiceTraffic); it has no player hardware
// loadout, so it is never materialised as a full ShipCore ship.
namespace game::ship::descriptors
{

[[nodiscard]] inline const ShipDescriptor& orbitalCargoShuttleDescriptor()
{
    static const ShipDescriptor descriptor = [] {
        ShipDescriptor d;
        d.typeId = ObjectType::OrbitalCargoShuttle;
        d.identity = {"OCS-4 Lastochka", "Orbital cargo shuttle"};
        d.physicalControlModel = game::ship::plant::PhysicalControlModelId::NewtonianI;
        d.localFlightControlLaw = game::ship::plant::nativeFlightLaw(d.physicalControlModel);
        d.physics.massKg = 380000.0;
        d.storage.cargoMass = 450000;
        d.storage.cargoVolume = 2600;
        d.hud.name = "OCS-4";
        d.meshSizeMeters = glm::vec3(18.0f, 14.0f, 56.0f);
        d.logicalDimensionsValue.length = 56.0f;
        d.logicalDimensionsValue.width = 18.0f;
        d.logicalDimensionsValue.height = 14.0f;
        d.logicalDimensionsValue.scaleReference = ScaleReference::Length;
        d.logicalDimensionsValue.enabled = true;
        d.reactor = {30.0, 36.0, 900.0, 300.0, 1400.0, 900.0, 60.0, 0.5, 0.9, 0.6};
        d.cooling = {2200.0, 2, 0.9, 2400.0, 30.0, 36.0, "radiator.ofu.dp1100"};
        d.boundingEllipsoid.radius = glm::vec3(9.0f, 7.0f, 28.0f);
        return d;
    }();
    return descriptor;
}

} // namespace game::ship::descriptors
