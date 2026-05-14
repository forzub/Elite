#pragma once

#include <vector>

#include "src/world/navigation/NavigationContact.h"
#include "src/world/navigation/NavigationFeature.h"

namespace world::navigation
{

struct NavigationScene
{
    std::vector<NavigationContact> contacts;
    std::vector<NavigationFeature> features;

    void clear()
    {
        contacts.clear();
        features.clear();
    }
};

} // namespace world::navigation