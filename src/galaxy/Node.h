#pragma once
#include "GalaxyIds.h"
#include "GalaxyEnums.h"
#include <string>
#include <vector>

struct Node
{
    NodeId id;

    std::string name;

    NodeType type;          // физическая форма
    NodeRole role;          // классифицированная роль
    std::string roleText;   // ОРИГИНАЛЬНЫЙ "тип" из nodes.txt

    StarSystemId systemId;  // может быть 0

    ActorId owner;          // формальный владелец
    SubActorId controller;  // фактический (если есть)

    long long population;   // В ШТУКАХ, -1 если нет данных
    float securityLevel;    // 0..1
    float techLevel;        // 0..1
    float economicValue;    // 0..1

    std::string description;

    std::string locationText;              // текстовая "локация", если есть
    std::vector<std::string> tags;
};
