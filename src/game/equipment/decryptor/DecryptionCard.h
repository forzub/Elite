#pragma once
#include <string>

struct GalaxyIds;


struct DecryptionCard
{
    ItemId     id;
    ActorId    actor;      // чей это код (для UI / лора)
    ActorCode  code;       // сам шифр

    std::string name;
    std::string description;
};
