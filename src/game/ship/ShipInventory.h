#pragma once
#include <vector>
#include "game/items/Item.h"
#include <algorithm>

struct ShipInventory
{
    std::vector<Item*> items;

    void add(Item* item)
    {
        items.push_back(item);
    }

    bool remove(Item* item)
    {
        auto it = std::find(items.begin(), items.end(), item);
        if (it == items.end())
            return false;

        items.erase(it);
        return true;
    }
};
