#pragma once
#include "GalaxyIds.h"
#include "GalaxyEnums.h"
#include <string>
#include <vector>

struct Actor
{
    ActorId id;

    // Названия
    std::string name;                    // официальное
    std::string selfName;                // самоназвание
    std::vector<std::string> altNames;   // старые, сленг, вражеские

    // Классификация
    ActorKind kind;
    ActorTier tier;

    // Идеология и культура
    std::string ideology;
    std::string culture;

    // Лор
    std::string nameOrigin;
    std::string description;
    ActorCode code;        // КОД ШИФРОВАНИЯ АКТОРА

    // Материальные и игровые маркеры
    std::vector<std::string> primaryShips;

    // Структура
    std::vector<SubActorId> subActors;

    // Метаданные (НИКОГДА не интерпретируются кодом жёстко)
    std::vector<std::string> tags;
};
