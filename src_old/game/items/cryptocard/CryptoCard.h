#pragma once

#include <string>
#include "src/galaxy/Actors.h"
#include "game/items/Item.h"

struct CryptoCard : public Item
{
    ActorCode           code;        // КОНКРЕТНЫЙ КЛЮЧ
    ActorId             actor;       // ЧЕЙ это код (для UI / лора / проверки)
    std::string         label;    // чисто UI / лор
    
    bool                installed = false;
    
    bool usesCargoSpace() const override { return false; }
    CryptoCard(ActorCode c, std::string l): code(c), label(std::move(l)) {}
};

