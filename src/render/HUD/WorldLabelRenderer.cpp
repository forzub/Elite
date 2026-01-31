#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "core/StateContext.h"

#include "render/HUD/WorldLabelRenderer.h"
#include "render/HUD/TextRenderer.h"
#include "render/HUD/WorldLabel.h"
#include "render/ScreenUtils.h"
#include "render/ShaderUtils.h"
#include "render/Font.h"

#include "render/VisualTuning.h"




// ВАЖНО: объявлены где-то у тебя
bool projectToScreen(
    const glm::vec3& worldPos,
    const glm::mat4& view,
    const glm::mat4& proj,
    int screenW,
    int screenH,
    glm::vec2& outScreen
);

void drawLine(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec3& color
);



// ===========================================================================
// INIT
// ===========================================================================
void WorldLabelRenderer::init(StateContext& context)
{
    m_context = &context;
    if (!m_labelFont)
        m_labelFont = new Font("assets/fonts/Roboto-Light.ttf", 12);

    if (!m_distFont)
        m_distFont = new Font("assets/fonts/Roboto-Light.ttf", 11);

    // ----------------- загрузка шейдеров --------------------------
    m_waveProgram = compileShaderFromFiles(
        "assets/shaders/hud/wave.vert",
        "assets/shaders/hud/wave.frag"
    );

    if (!m_waveProgram)
    {
        std::cerr << "[WorldLabelRenderer] Wave shader failed\n";
    }


        float quadVerts[] = {
        -1.2f, -1.2f,
        1.2f, -1.2f,
        1.2f,  1.2f,
        -1.2f,  1.2f
    };

    glGenVertexArrays(1, &m_waveVAO);
    glBindVertexArray(m_waveVAO);

    glGenBuffers(1, &m_waveVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_waveVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);

    // ВАЖНО: VAO должен быть привязан ПРЯМО СЕЙЧАС
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,                  // location = 0
        2,                  // vec2
        GL_FLOAT,
        GL_FALSE,
        2 * sizeof(float),
        (void*)0
    );

    // Сбрасываем
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    

}







//                                ###
//                                 ##
//  ######    ####    #####        ##    ####    ######
//   ##  ##  ##  ##   ##  ##    #####   ##  ##    ##  ##
//   ##      ######   ##  ##   ##  ##   ######    ##
//   ##      ##       ##  ##   ##  ##   ##        ##
//  ####      #####   ##  ##    ######   #####   ####

// ===========================================================================
// renderHUD
// ===========================================================================
void WorldLabelRenderer::renderHUD(const WorldLabel& label)
{
   

    if (!label.onScreen)
    {
        renderEdgeArrow(label);
        return;
    }


    if ((label.data.semanticState == SignalSemanticState::Noise ||
        label.data.displayClass == SignalDisplayClass::Other) &&
         (label.data.displayClass != SignalDisplayClass::Global)){

            renderWaves(label.data,label.visual, label.screenPos);
            return;
        }


    if (label.data.semanticState == SignalSemanticState::Decoded ||
        label.data.displayClass == SignalDisplayClass::Global){

            renderTextLabel(label, label.screenPos);
            return;
        }
}




//              ###
//               ##
//   ####        ##    ### ##   ####              ####    ######   ######    ####    ##   ##
//  ##  ##    #####   ##  ##   ##  ##                ##    ##  ##   ##  ##  ##  ##   ## # ##
//  ######   ##  ##   ##  ##   ######             #####    ##       ##      ##  ##   #######
//  ##       ##  ##    #####   ##                ##  ##    ##       ##      ##  ##   #######
//   #####    ######      ##    #####             #####   ####     ####      ####     ## ##
//                    #####

void WorldLabelRenderer::renderEdgeArrow(const WorldLabel& label)
{
    const float v = label.visual.visibility;
    if (v < 0.02f)
        return;

    // -------------------------------------------------
    // Нам нужна direction, КОТОРУЮ ТЫ УЖЕ СЧИТАЛ
    // ВАЖНО: она должна быть сохранена в label
    // -------------------------------------------------

    // 👉 если ты ещё не сохранил:
    // добавь в WorldLabel:
    // glm::vec2 edgeDir;

    const glm::vec2 normal = glm::normalize(label.edgeDir);
    const glm::vec2 tangent(-normal.y, normal.x);

    const glm::vec2 anchor = label.screenPos;

    // -------------------------------------------------
    // Arrow geometry  - маркер - отображение
    // -------------------------------------------------
    constexpr float ARROW_LENGTH = VisualTuning::wl_edge::ARROW_LENGTH;         // 9.0f;
    constexpr float ARROW_WIDTH  = VisualTuning::wl_edge::ARROW_WIDTH;          // 8.0f;

    const glm::vec2 tip  = anchor + normal * ARROW_LENGTH;
    const glm::vec2 base = anchor - normal * VisualTuning::wl_edge::DISTANCE;   // 6.0f;

    const glm::vec2 left  = base + tangent * (ARROW_WIDTH * 0.5f);
    const glm::vec2 right = base - tangent * (ARROW_WIDTH * 0.5f);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(
        0.7f,
        0.9f,
        1.0f,
        v
    );

    glBegin(GL_TRIANGLES);
        glVertex2f(tip.x,   tip.y);
        glVertex2f(left.x,  left.y);
        glVertex2f(right.x, right.y);
    glEnd();

    // -------------------------------------------------
    // Text (против направления)
    // -------------------------------------------------

    const char* name =
        (label.data.displayName.empty() ||
         label.data.displayName == "undefined")
        ? "UNDEF"
        : label.data.displayName.c_str();

    float textWidth = m_labelFont->measureText(name);
    float textHeight = m_labelFont->lineHeight();

    // normal.x > 0 → цель справа → стрелка справа → текст рисуем влево
    bool textOnLeftSide = (normal.x > 0.3f);
    bool textOnRightSide = (normal.x < -0.3f);

    glm::vec2 textPos =
        anchor - normal * 18.0f + tangent * 6.0f;

    if (textOnLeftSide)
    {
        // стрелка справа → текст влево → сдвигаем на ширину
        textPos.x -= textWidth;
    }
    else if (!textOnRightSide)
    {
        // верх / низ → центрируем
        textPos.x -= textWidth * 0.5f;
    }

    

    TextRenderer::instance().textDraw(
        *m_labelFont,
        name,
        textPos.x,
        textPos.y,
        glm::vec4(0.7f, 0.9f, 1.0f, v)
    );
}







//  ##   ##   ####    ##  ##    ####
//  ## # ##      ##   ##  ##   ##  ##
//  #######   #####   ##  ##   ######
//  #######  ##  ##    ####    ##
//   ## ##    #####     ##      #####


void WorldLabelRenderer::renderWaves(
    const WorldLabelData&           labelData,
    const WorldLabelVisualState&    visual,
    const glm::vec2& center
)
{
    
        
    // -------------------------------------------------
    // HUD render state (фиксированный)
    // -------------------------------------------------
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // -------------------------------------------------
    // Activate wave shader
    // -------------------------------------------------
    glUseProgram(m_waveProgram);
    glBindVertexArray(m_waveVAO);

    // -------------------------------------------------
    // Viewport (ЕДИНЫЙ источник истины)
    // -------------------------------------------------
    assert(m_context && "HudRenderer: context not set");

    const Viewport& vp = m_context->viewport();
    const float vw = (float)vp.width;
    const float vh = (float)vp.height;

    // const float vw = (float)TextRenderer::instance().viewportWidth();
    // const float vh = (float)TextRenderer::instance().viewportHeight();

    const GLint locCenter   = glGetUniformLocation(m_waveProgram, "uCenterPx");
    const GLint locViewport = glGetUniformLocation(m_waveProgram, "uViewport");
    const GLint locRadius   = glGetUniformLocation(m_waveProgram, "uRadiusPx");
    const GLint locThick    = glGetUniformLocation(m_waveProgram, "uThickness");
    const GLint locColor    = glGetUniformLocation(m_waveProgram, "uColor");

    // -------------------------------------------------
    // Set uniforms that do NOT change per-wave
    // -------------------------------------------------
    glUniform2f(locCenter, center.x, center.y);
    glUniform2f(locViewport, vw, vh);
    glUniform1f(locThick, VisualTuning::Waves::lineThickness);

    // -------------------------------------------------
    // Draw waves
    // -------------------------------------------------
    for (int i = 0; i < WorldLabelVisualState::MaxWaves; ++i)
    {
        const SignalWave& w = visual.waves[i];
        if (!w.alive)
            continue;

        // логический радиус → пиксели
        const float radiusPx = w.radius;
            

        if (radiusPx < 1.0f)
            continue;

        // альфа затухания
        float alpha =
            1.0f - (w.radius / VisualTuning::Waves::maxWaveRadius);
        alpha = std::max(alpha * alpha, VisualTuning::Waves::minWaveAlpha);

        glUniform1f(locRadius, radiusPx);
        glUniform4f(
            locColor,
            VisualTuning::Waves::color.r,
            VisualTuning::Waves::color.g,
            VisualTuning::Waves::color.b,
            alpha
        );

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }


    // -------------------------------------------------
    // RSSI / SNR text (HUD, screen-space)
    // -------------------------------------------------
    {
        const Viewport& vp = m_context->viewport();

        char bufRSSI[32];
        char bufSNR[32];

        // RSSI — как есть (нормализован или dB — твой выбор)
        std::snprintf(
            bufRSSI,
            sizeof(bufRSSI),
            "RSSI %.2f",
            labelData.receivedPower
        );

        std::snprintf(
            bufSNR,
            sizeof(bufSNR),
            "SNR  %.2f",
            labelData.signalToNoiseRatio
        );

        // позиция текста: ниже максимального радиуса волн
        const float textX = center.x + 6.0f;
        const float textY =
            center.y + VisualTuning::Waves::maxWaveRadius + 14.0f;

        const float v = visual.visibility;
        // const float v = 1.0f;
        

        auto& tr = TextRenderer::instance();
        

        tr.textDraw(
            *m_distFont,
            bufRSSI,
            textX,
            textY,
            {
                VisualTuning::Waves::color.r,
                VisualTuning::Waves::color.g,
                VisualTuning::Waves::color.b,
                v
            }
            
        );

        tr.textDraw(
            *m_distFont,
            bufSNR,
            textX,
            textY + 12.0f,
            {
                VisualTuning::Waves::color.r ,
                VisualTuning::Waves::color.g ,
                VisualTuning::Waves::color.b ,
                v
            }
        );
       
    }           

    // -------------------------------------------------
    // Cleanup
    // -------------------------------------------------
    glBindVertexArray(0);
    glUseProgram(0);
}




//    ##                         ##               ###              ###                ###
//    ##                         ##                ##               ##                 ##
//   #####    ####    ##  ##    #####              ##      ####     ##       ####      ##
//    ##     ##  ##    ####      ##                ##         ##    #####   ##  ##     ##
//    ##     ######     ##       ##                ##      #####    ##  ##  ######     ##
//    ## ##  ##        ####      ## ##             ##     ##  ##    ##  ##  ##         ##
//     ###    #####   ##  ##      ###             ####     #####   ######    #####    ####


void WorldLabelRenderer::renderTextLabel(
    const WorldLabel& label,
    const glm::vec2& screenPos
)
{
    const float v = label.visual.visibility;
    if (v < 0.02f)
        return;


    assert(m_context && "HudRenderer: context not set");
    const Viewport& vp = m_context->viewport();

    // -------------------------------------------------
    // Цвет (единый для линий и текста)
    // -------------------------------------------------
    const glm::vec3 lineColor = {
        0.6f * v,
        0.8f * v,
        1.0f * v
    };

    // -------------------------------------------------
    // Геометрия сноски
    // -------------------------------------------------
    const glm::vec2 anchor = screenPos;
    const glm::vec2 elbow  = anchor + glm::vec2(12.0f, -12.0f);
    const glm::vec2 textPos = elbow + glm::vec2(40.0f, 0.0f);

    // -------------------------------------------------
    // Линии-сноски
    // -------------------------------------------------
    drawLine(anchor, elbow, lineColor);
    drawLine(elbow, textPos, lineColor);

    // -------------------------------------------------
    // Имя объекта
    // -------------------------------------------------
    TextRenderer::instance().textDraw(
        *m_labelFont,
        label.data.displayName,
        textPos.x,
        textPos.y,
        glm::vec4(0.7f, 0.9f, 1.0f, v)
    );

    // -------------------------------------------------
    // Дистанция (только для Decoded)
    // -------------------------------------------------
    if ((label.data.semanticState == SignalSemanticState::Decoded || 
        label.data.displayClass == SignalDisplayClass::Global) &&
        label.data.distance >= 0.0f)
    {
        char buf[32];
        if (label.data.distance >= 1000.0f)
            std::snprintf(buf, sizeof(buf), "%.1f km", label.data.distance / 1000.0f);
        else
            std::snprintf(buf, sizeof(buf), "%.0f m", label.data.distance);

        const float distYOffset =
            m_labelFont->lineHeight() + 2.0f;

        TextRenderer::instance().textDraw(
            *m_distFont,
            buf,
            textPos.x,
            textPos.y + distYOffset,
            glm::vec4(0.6f, 0.8f, 1.0f, v)
        );
    }
}







namespace
{
    // ------------------------------------------------------------
    // Clamp NDC direction to screen edge
    // ------------------------------------------------------------
    glm::vec2 clampToScreenEdge(const glm::vec2& ndc)
    {
        float m = std::max(std::abs(ndc.x), std::abs(ndc.y));
        return ndc / m;
    }
}

