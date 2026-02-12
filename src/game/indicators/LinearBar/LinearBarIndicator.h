#pragma once

#include <string>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "game/ship/cockpit/CockpitContours.h"
#include "game/ship/cockpit/ShipCockpitState.h"

class LinearBarIndicator
{
public:
    LinearBarIndicator(
        const std::string& id,
        glm::vec2 position01,
        glm::vec2 size01,
        glm::vec4 color
    );

    CockpitDrawCommand createDrawCommand() const;

    void update(float value01);
    void apply(ShipCockpitState& state);

private:
    std::string m_id;

    glm::vec2 m_pos01;
    glm::vec2 m_size01;
    glm::vec4 m_color;

    float m_value01 = 0.0f;
};
