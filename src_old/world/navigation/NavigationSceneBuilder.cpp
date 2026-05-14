#include "NavigationSceneBuilder.h"

#include <glm/gtx/norm.hpp>

namespace world::navigation
{

NavigationSceneBuilder::NavigationSceneBuilder(
    const NavigationContactQuery& query
)
    : m_query(query)
{
}

void NavigationSceneBuilder::clear()
{
    m_scene.clear();
}

void NavigationSceneBuilder::addContact(
    const NavigationContact& contact
)
{
    if (!shouldAcceptContact(contact))
        return;

    m_scene.contacts.push_back(contact);
}

void NavigationSceneBuilder::addFeature(
    const NavigationFeature& feature
)
{
    if (!shouldAcceptFeature(feature))
        return;

    m_scene.features.push_back(feature);
}

const NavigationScene& NavigationSceneBuilder::scene() const
{
    return m_scene;
}

bool NavigationSceneBuilder::shouldAcceptContact(
    const NavigationContact& contact
) const
{
    if (contact.entityId != 0 &&
        contact.entityId == m_query.ignoreEntityId)
    {
        return false;
    }

    const float queryRadius =
        m_query.radius + contact.radius;

    const float dist2 =
        glm::length2(contact.center - m_query.center);

    return dist2 <= queryRadius * queryRadius;
}

bool NavigationSceneBuilder::shouldAcceptFeature(
    const NavigationFeature& feature
) const
{
    if (!feature.enabled)
        return false;

    if (feature.ownerEntityId != 0 &&
        feature.ownerEntityId == m_query.ignoreEntityId)
    {
        return false;
    }

    const float featureRadius =
        feature.radius + feature.length * 0.5f;

    const float queryRadius =
        m_query.radius + featureRadius;

    const float dist2 =
        glm::length2(feature.center - m_query.center);

    return dist2 <= queryRadius * queryRadius;
}

} // namespace world::navigation