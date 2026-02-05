#pragma once

#include <string>
#include "src/galaxy/Actors.h"
#include "game/items/Item.h"

struct CryptoCard : public Item
{
    ActorCode           code;        // КОНКРЕТНЫЙ КЛЮЧ
    std::string         label;    // чисто UI / лор
    
    bool                installed = false;

    CryptoCard(ActorCode c, std::string l)
        : code(c), label(std::move(l)) {}
};

