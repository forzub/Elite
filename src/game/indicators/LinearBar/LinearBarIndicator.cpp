#include "LinearBarIndicator.h"
#include <glm/common.hpp>

LinearBarIndicator::LinearBarIndicator(
    const std::string& id,
    glm::vec2 position01,
    glm::vec2 size01,
    glm::vec4 color)
    : m_id(id),
      m_pos01(position01),
      m_size01(size01),
      m_color(color)
{
}

CockpitDrawCommand LinearBarIndicator::createDrawCommand() const
{
    CockpitPolygon poly;

    float x0 = m_pos01.x;
    float y0 = m_pos01.y;
    float x1 = x0 + m_size01.x;
    float y1 = y0 + m_size01.y;

    poly.contour01 = {
        {x0, y0},
        {x1, y0},
        {x1, y1},
        {x0, y1}
    };

    poly.fillType = CockpitFillType::Solid;
    poly.colorA   = m_color;

    CockpitDrawCommand cmd;
    cmd.type = CockpitDrawType::Fill;
    cmd.id   = m_id;
    cmd.polygon = poly;

    return cmd;
}

void LinearBarIndicator::update(float value01)
{
    m_value01 = glm::clamp(value01, 0.0f, 1.0f);
}

void LinearBarIndicator::apply(ShipCockpitState& state)
{
    auto& ov = state.overrides[m_id];

    ov.visible = true;
    ov.overrideFill = true;
    ov.fill01 = m_value01;
}
