#pragma once

#include "src/world/navigation/NavigationScene.h"

namespace world::navigation
{

class NavigationSceneBuilder
{
public:
    explicit NavigationSceneBuilder(const NavigationContactQuery& query);

    void clear();

    void addContact(const NavigationContact& contact);
    void addFeature(const NavigationFeature& feature);

    const NavigationScene& scene() const;

private:
    bool shouldAcceptContact(const NavigationContact& contact) const;
    bool shouldAcceptFeature(const NavigationFeature& feature) const;

private:
    NavigationContactQuery m_query;
    NavigationScene m_scene;
};

} // namespace world::navigation