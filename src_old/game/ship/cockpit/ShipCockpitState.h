#pragma once
#include <unordered_map>
#include <string>
#include <glm/vec4.hpp>

struct CockpitVisualOverride
{
    bool visible = true;

    bool overrideColor = false;
    glm::vec4 color = {1,1,1,1};

    bool overrideAlpha = false;
    float alpha = 1.0f;

    // для стрелки
    bool overrideRotation = false;
    float rotationDeg = 0.0f;

    // на будущее
    bool overrideFill = false;
    float fill01 = 1.0f;

};


struct ShipCockpitState
{
    float forwardVelocity = 0.0f;
    float targetSpeed     = 0.0f;
    bool  cruiseActive    = false;
    
    std::unordered_map<std::string, CockpitVisualOverride> overrides;
};
