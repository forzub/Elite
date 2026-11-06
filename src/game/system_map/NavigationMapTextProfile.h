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
    std::string routeOfficial = "ЛЕТЕТЬ: ОФИЦИАЛЬНЫЙ МАРШРУТ";
    std::string routeUnofficial = "ЛЕТЕТЬ: НЕОФИЦИАЛЬНЫЙ МАРШРУТ";
    std::string routeRefuelOn = "ЗАПРАВКА ЗДЕСЬ: ДА";
    std::string routeRefuelOff = "ЗАПРАВКА ЗДЕСЬ: НЕТ";
    // the point's program (ROUTE_PROGRAMS_DESIGN.md): the button cycles the programs its target admits
    std::string routeProgramPrefix = "ПРОГРАММА: ";
    std::string programAuto = "АВТО";
    std::string programPassThrough = "ПРОЛЁТ";
    std::string programDock = "СТЫКОВКА";
    std::string programOrbit = "ВЫХОД НА ОРБИТУ";
    std::string programOrbitSurvey = "РАЗВЕДКА С ОРБИТЫ";
    std::string programLand = "ПОСАДКА (АВТОМАТ)";
    std::string programLandAtSettlement = "ПОСАДКА НА ПЛОЩАДКУ";
    std::string programRendezvous = "ВСТРЕЧА";
    std::string programStop = "ОСТАНОВКА";
    std::string routeRepairOn = "РЕМОНТ ЗДЕСЬ: ДА";
    std::string routeRepairOff = "РЕМОНТ ЗДЕСЬ: НЕТ";
    std::string routePauseOn = "ПАУЗА ЗДЕСЬ (РУЧНОЙ РЕЖИМ): ДА";
    std::string routePauseOff = "ПАУЗА ЗДЕСЬ (РУЧНОЙ РЕЖИМ): НЕТ";
    std::string routeTakeOffOn = "ПОСЛЕ ПОСАДКИ ВЗЛЁТ: ДА";
    std::string routeTakeOffOff = "ПОСЛЕ ПОСАДКИ ВЗЛЁТ: НЕТ";
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
