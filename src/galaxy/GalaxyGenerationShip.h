#pragma once
#include "GalaxyIds.h"
#include "GalaxyEnums.h"

#include <string>
#include <vector>
#include <optional>

struct GenerationShip
{
    GenerationShipId id;

    std::string name;            // GENESIS-1
    std::string className;       // Ark / Seeder / Nomad
    float lengthMeters;          // ОБЯЗАТЕЛЬНО
    float massTons;              // -1 если неизвестно

    int launchYear;
    std::optional<int> arrivalYear;

    ActorId originActor;         // кто отправил
    NodeId  originNode;          // откуда

    std::vector<StarSystemId> plannedRoute; // канонический маршрут
    std::optional<StarSystemId> targetSystem;
    std::string targetDescription;

    int initialPopulation;

    GenerationShipStatus status3024;

    std::string programDescription; // программа / замысел
    std::string fateDescription;    // судьба

    // --- текущее положение ---
    std::optional<StarSystemId> currentSystem;
    std::optional<RouteId> currentRoute;
    float routeProgress; // 0..1, если в пути, иначе -1

    bool emitsSignal;
    bool questRelevant;

    std::vector<std::string> tags;
};
