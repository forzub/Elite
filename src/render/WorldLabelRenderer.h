#pragma once
#include <string>
#include <glm/glm.hpp>

#include "WorldLabel.h"


class Font;
struct StateContext;


class WorldLabelRenderer
{
public:
    void init(StateContext& context);

    void render(
        const WorldLabel& lbl,
        const glm::mat4& view,
        const glm::mat4& projection,
        int screenW,
        int screenH
    );

private:
    Font* m_labelFont = nullptr;
    Font* m_distFont  = nullptr;
    StateContext* m_context = nullptr;
    

    void renderDirectionOnly(
        const WorldLabel& lbl,
        const glm::vec2& center
    );

    void renderPreciseLabel(
        const WorldLabel& lbl,
        const glm::vec2& screenPos
    );

};
