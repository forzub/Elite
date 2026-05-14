#pragma once

namespace world::navigation
{

struct NavigationAgentProfile
{
    float bodyRadius = 1.0f;

    float sensorRange = 100.0f;
    float lookAheadTime = 2.0f;

    float routeReplanInterval = 1.0f;
    float tacticalCheckInterval = 0.1f;

    float maxSpeed = 10.0f;
    float maxAcceleration = 5.0f;

    float caution = 1.0f;
    float aggression = 0.0f;
};

inline NavigationAgentProfile makeRepairDroneNavigationProfile()
{
    NavigationAgentProfile p;
    p.bodyRadius = 0.8f;
    p.sensorRange = 80.0f;
    p.lookAheadTime = 2.5f;
    p.routeReplanInterval = 1.5f;
    p.tacticalCheckInterval = 0.15f;
    p.maxSpeed = 8.0f;
    p.maxAcceleration = 4.0f;
    p.caution = 1.3f;
    p.aggression = 0.0f;
    return p;
}

inline NavigationAgentProfile makeHumanPilotNavigationProfile()
{
    NavigationAgentProfile p;
    p.bodyRadius = 4.0f;
    p.sensorRange = 300.0f;
    p.lookAheadTime = 1.5f;
    p.routeReplanInterval = 0.3f;
    p.tacticalCheckInterval = 0.05f;
    p.maxSpeed = 80.0f;
    p.maxAcceleration = 20.0f;
    p.caution = 1.0f;
    p.aggression = 0.5f;
    return p;
}

inline NavigationAgentProfile makeMissileNavigationProfile()
{
    NavigationAgentProfile p;
    p.bodyRadius = 0.5f;
    p.sensorRange = 220.0f;
    p.lookAheadTime = 1.0f;
    p.routeReplanInterval = 0.7f;
    p.tacticalCheckInterval = 0.05f;
    p.maxSpeed = 160.0f;
    p.maxAcceleration = 60.0f;
    p.caution = 0.5f;
    p.aggression = 1.0f;
    return p;
}

} // namespace world::navigation