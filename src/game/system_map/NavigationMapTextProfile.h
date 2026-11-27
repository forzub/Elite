#pragma once

#include <string>

namespace game::system_map
{
struct NavigationMapTextProfile;

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
    std::string routeOfficial = "FLY: OFFICIAL ROUTE";
    std::string routeUnofficial = "FLY: UNOFFICIAL ROUTE";
    std::string routeRefuelOn = "REFUEL HERE: YES";
    std::string routeRefuelOff = "REFUEL HERE: NO";
    // the point's program (ROUTE_PROGRAMS_DESIGN.md): the button cycles the programs its target admits
    std::string routeCalculate = "CALCULATE (NO DEPARTURE)";
    std::string routePause = "PAUSE (MANUAL MODE)";
    std::string routeResume = "RESUME ROUTE";
    std::string routeProgramPrefix = "PROGRAM: ";
    std::string programAuto = "AUTO";
    std::string programPassThrough = "FLY-BY";
    std::string programDock = "DOCKING";
    std::string programOrbit = "ORBIT INSERTION";
    std::string programOrbitSurvey = "ORBITAL SURVEY";
    std::string programLand = "LANDING (AUTO)";
    std::string programLandAtSettlement = "PAD LANDING";
    std::string programRendezvous = "RENDEZVOUS";
    std::string programStop = "STOP";
    std::string routeRepairOn = "REPAIR HERE: YES";
    std::string routeRepairOff = "REPAIR HERE: NO";
    std::string routePauseOn = "PAUSE HERE (MANUAL MODE): YES";
    std::string routePauseOff = "PAUSE HERE (MANUAL MODE): NO";
    std::string routeTakeOffOn = "TAKE OFF AFTER LANDING: YES";
    std::string routeTakeOffOff = "TAKE OFF AFTER LANDING: NO";
    std::string cancelFinish;
    std::string setIntermediate;
    std::string cancelIntermediate;
    std::string spaceTarget;
    // Context menu commands.
    std::string contextInfo = "INFO";
    std::string contextDetails = "SHOW DETAILS";
    std::string contextSurroundings = "VIEW SURROUNDINGS";
    std::string starSystem = "Star system";
    std::string finishTarget;
    std::string intermediateTarget;
    std::string navigationPoint;

    std::string dockingPorts;
    std::string hubBlockBuildings;
    std::string hubAddress;
    std::string hubPads;
    std::string hubBuilding;
    std::string hubPad;
    std::string hubPadRoof;
    std::string hubPadGarage;
    std::string hubPadStilts;
    std::string hubPadNone;
    std::string dockOpening;
    std::string shipEnvelope;
    std::string dockFit;
    std::string dockStatus;
    std::string dockAccess;
    std::string dockOperational;
    std::string dockClearance;
    std::string dockMaxEntrySpeed;
    std::string calculateRoute;
    std::string showDockingRoute;
    std::string startDocking;
    std::string landPad;

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
