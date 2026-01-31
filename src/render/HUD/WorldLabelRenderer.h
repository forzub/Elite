#pragma once
#include <string>
#include <glad/gl.h>   // ← ОБЯЗАТЕЛЬНО для GLuint
#include <glm/glm.hpp>

#include "render/HUD/WorldLabel.h"


class Font;
struct StateContext;


class WorldLabelRenderer
{
public:
    void init(StateContext& context);

    void renderHUD(const WorldLabel& label);

private: 

    Font* m_labelFont = nullptr;
    Font* m_distFont  = nullptr;
    StateContext* m_context = nullptr;

    GLuint m_waveProgram = 0;
    GLuint m_waveVAO = 0;
    GLuint m_waveVBO = 0;

    int m_screenW = 0;
    int m_screenH = 0;
    

    void renderEdgeArrow(const WorldLabel& label); 

    void renderWaves(
        const WorldLabelData&           data,
        const WorldLabelVisualState&    visual,
        const glm::vec2& center
    );

    void renderTextLabel(
        const WorldLabel& label,
        const glm::vec2& screenPos
    );


 

};
