#pragma once

#include <string>

namespace game::system_map
{

// Localized presentation vocabulary for native map overlays. The owning
// application resolves every value from LocalizationService; renderers never
// carry per-language branches or editable user-facing strings.
struct NavigationMapTextProfile
{
    std::string type;
    std::string name;
    std::string localSpeed;
    std::string globalSpeed;
    std::string azimuth;
    std::string elevation;
    std::string owner;
    std::string radius;
    std::string address;

    std::string setWaypoint;
    std::string setRendezvous;
    std::string cancelWaypoint;
    std::string setFinish;
    std::string cancelFinish;
    std::string setIntermediate;
    std::string cancelIntermediate;
    std::string spaceTarget;
    std::string finishTarget;
    std::string intermediateTarget;
    std::string navigationPoint;

    std::string dockingPorts;
    std::string dockOpening;
    std::string shipEnvelope;
    std::string dockFit;
    std::string dockStatus;
    std::string dockAccess;
    std::string dockOperational;
    std::string dockClearance;
    std::string dockMaxEntrySpeed;
    std::string calculateRoute;

    std::string statusAvailable;
    std::string statusUnavailable;
    std::string statusFree;
    std::string statusOccupied;
    std::string statusReserved;
    std::string statusAllowed;
    std::string statusDenied;
    std::string statusClearanceRequired;
    std::string statusOnline;
    std::string statusOffline;
    std::string statusDamaged;
    std::string statusUnknown;

    std::string routeTitle;
    std::string showOnHud;
    std::string start;
    std::string waypoint;
    std::string finish;
    std::string dragWaypoints;
    std::string deleteRoute;
    std::string deleteWaypoint;
    std::string yes;
    std::string no;

    std::string arrivalSafeZone;
    std::string arrivalFollow;
    std::string arrivalFormation;
    std::string arrivalParade;
};

} // namespace game::system_map
