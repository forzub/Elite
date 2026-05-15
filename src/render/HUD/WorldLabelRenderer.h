#pragma once

#include <string>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "render/HUD/WorldLabel.h"


class Font;
struct StateContext;


class WorldLabelRenderer
{
public:
    void init(StateContext& context);

    void renderHUD(const WorldLabel& label);
    void renderHUDBatch(const std::vector<WorldLabel>& labels);

private: 

    Font*                                   m_labelFont = nullptr;
    Font*                                   m_distFont  = nullptr;
    StateContext*                           m_context = nullptr;

    GLuint                                  m_waveProgram = 0;
    GLuint                                  m_waveVAO = 0;
    GLuint                                  m_waveVBO = 0;

    GLint m_locWaveCenter = -1;
    GLint m_locWaveViewport = -1;
    GLint m_locWaveRadius = -1;
    GLint m_locWaveThickness = -1;  
    GLint m_locWaveColor = -1;

    int                                     m_screenW = 0;
    int                                     m_screenH = 0;

    void renderTextLabelsBatch(const std::vector<const WorldLabel*>& labels);
    void renderWaveLabelsBatch(const std::vector<const WorldLabel*>& labels);
    void renderEdgeLabelsBatch(const std::vector<const WorldLabel*>& labels);
    

    void renderEdgeArrow(const WorldLabel& label); 

    void renderWaves(
        const WorldLabelData&               data,
        const WorldLabelVisualState&        visual,
        const glm::vec2&                    center
    );

    void renderTextLabel(
        const WorldLabel&                   label,
        const glm::vec2&                    screenPos
    );

    struct HudPrimitiveVertex
    {
        glm::vec2 posNdc;
        glm::vec4 color;
    };

    GLuint m_primitiveProgram = 0;
    GLuint m_primitiveVAO = 0;
    GLuint m_primitiveVBO = 0;

    std::vector<HudPrimitiveVertex> m_primitiveVertices;

    glm::vec2 pxToNdc(const glm::vec2& p) const;

    void beginPrimitiveBatch();
    void flushPrimitiveBatch();

    void emitTrianglePx(
        const glm::vec2& a,
        const glm::vec2& b,
        const glm::vec2& c,
        const glm::vec4& color
    );

    void emitLinePx(
        const glm::vec2& a,
        const glm::vec2& b,
        float thickness,
        const glm::vec4& color
    );

    void emitWaveRingPx(
        const glm::vec2& center,
        float radius,
        float thickness,
        const glm::vec4& color
    );
 

};
