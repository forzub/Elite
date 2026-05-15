#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
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
        m_labelFont = new Font("assets/fonts/Roboto-Regular.ttf", 12);

    if (!m_distFont)
        m_distFont  = new Font("assets/fonts/Roboto-Regular.ttf", 11);

    // ----------------- загрузка шейдеров --------------------------
    m_waveProgram = compileShaderFromFiles(
        "assets/shaders/hud/wave.vert",
        "assets/shaders/hud/wave.frag"
    );

    if (!m_waveProgram)
    {
        std::cerr << "[WorldLabelRenderer] Wave shader failed\n";
    }

    if (m_waveProgram)
    {
        m_locWaveCenter =
            glGetUniformLocation(m_waveProgram, "uCenterPx");

        m_locWaveViewport =
            glGetUniformLocation(m_waveProgram, "uViewport");

        m_locWaveRadius =
            glGetUniformLocation(m_waveProgram, "uRadiusPx");

        m_locWaveThickness =
            glGetUniformLocation(m_waveProgram, "uThickness");

        m_locWaveColor =
            glGetUniformLocation(m_waveProgram, "uColor");
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



    // -------------------------------------------------
    // HUD primitive renderer: lines / arrows / wave rings
    // -------------------------------------------------
    m_primitiveProgram = compileShaderFromFiles(
        "assets/shaders/hud/hudPrimitive.vert",
        "assets/shaders/hud/hudPrimitive.frag"
    );

    if (!m_primitiveProgram)
    {
        std::cerr << "[WorldLabelRenderer] Primitive shader failed\n";
    }

    glGenVertexArrays(1, &m_primitiveVAO);
    glBindVertexArray(m_primitiveVAO);

    glGenBuffers(1, &m_primitiveVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_primitiveVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(HudPrimitiveVertex) * 4096,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(HudPrimitiveVertex),
        reinterpret_cast<void*>(0)
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(HudPrimitiveVertex),
        reinterpret_cast<void*>(sizeof(glm::vec2))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);    

}





glm::vec2 WorldLabelRenderer::pxToNdc(const glm::vec2& p) const
{
    assert(m_context && "WorldLabelRenderer: context not set");

    const Viewport& vp = m_context->viewport();

    const float w = static_cast<float>(vp.width);
    const float h = static_cast<float>(vp.height);

    return {
        (2.0f * p.x / w) - 1.0f,
        1.0f - (2.0f * p.y / h)
    };
}

void WorldLabelRenderer::beginPrimitiveBatch()
{
    m_primitiveVertices.clear();
}

void WorldLabelRenderer::flushPrimitiveBatch()
{
    if (m_primitiveVertices.empty())
        return;

    if (!m_primitiveProgram)
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_primitiveProgram);

    glBindVertexArray(m_primitiveVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_primitiveVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(
            m_primitiveVertices.size() * sizeof(HudPrimitiveVertex)
        ),
        m_primitiveVertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(m_primitiveVertices.size())
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    m_primitiveVertices.clear();
}

void WorldLabelRenderer::emitTrianglePx(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec2& c,
    const glm::vec4& color
)
{
    m_primitiveVertices.push_back({ pxToNdc(a), color });
    m_primitiveVertices.push_back({ pxToNdc(b), color });
    m_primitiveVertices.push_back({ pxToNdc(c), color });
}

void WorldLabelRenderer::emitLinePx(
    const glm::vec2& a,
    const glm::vec2& b,
    float thickness,
    const glm::vec4& color
)
{
    const glm::vec2 d = b - a;

    const float len2 = glm::dot(d, d);
    if (len2 < 0.0001f)
        return;

    const glm::vec2 dir = glm::normalize(d);
    const glm::vec2 n(-dir.y, dir.x);

    const float half = thickness * 0.5f;

    const glm::vec2 p0 = a + n * half;
    const glm::vec2 p1 = b + n * half;
    const glm::vec2 p2 = b - n * half;
    const glm::vec2 p3 = a - n * half;

    emitTrianglePx(p0, p1, p2, color);
    emitTrianglePx(p0, p2, p3, color);
}

void WorldLabelRenderer::emitWaveRingPx(
    const glm::vec2& center,
    float radius,
    float thickness,
    const glm::vec4& color
)
{
    if (radius < 1.0f)
        return;

    const int segments = 48;

    const float outerR = radius + thickness * 0.5f;
    const float innerR = std::max(0.0f, radius - thickness * 0.5f);

    constexpr float PI = 3.14159265358979323846f;

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            (static_cast<float>(i) / static_cast<float>(segments)) *
            PI * 2.0f;

        const float a1 =
            (static_cast<float>(i + 1) / static_cast<float>(segments)) *
            PI * 2.0f;

        const glm::vec2 dir0 = {
            std::cos(a0),
            std::sin(a0)
        };

        const glm::vec2 dir1 = {
            std::cos(a1),
            std::sin(a1)
        };

        const glm::vec2 o0 = center + dir0 * outerR;
        const glm::vec2 o1 = center + dir1 * outerR;
        const glm::vec2 i0 = center + dir0 * innerR;
        const glm::vec2 i1 = center + dir1 * innerR;

        emitTrianglePx(o0, o1, i1, color);
        emitTrianglePx(o0, i1, i0, color);
    }
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




void WorldLabelRenderer::renderHUDBatch(const std::vector<WorldLabel>& labels)
{
    if (labels.empty())
        return;

    std::vector<const WorldLabel*> textLabels;
    std::vector<const WorldLabel*> waveLabels;
    std::vector<const WorldLabel*> edgeLabels;

    textLabels.reserve(labels.size());
    waveLabels.reserve(labels.size());
    edgeLabels.reserve(labels.size());

    for (const WorldLabel& label : labels)
    {
        if (label.visual.visibility < 0.02f)
            continue;

        if (!label.onScreen)
        {
            edgeLabels.push_back(&label);
            continue;
        }

        const bool isText =
            label.data.semanticState == SignalSemanticState::Decoded ||
            label.data.displayClass == SignalDisplayClass::Global;

        const bool isWave =
            !isText &&
            (
                label.data.semanticState == SignalSemanticState::Noise ||
                label.data.displayClass == SignalDisplayClass::Other
            );

        if (isText)
            textLabels.push_back(&label);
        else if (isWave)
            waveLabels.push_back(&label);
    }

    beginPrimitiveBatch();
    TextRenderer::instance().beginFrame();

    renderWaveLabelsBatch(waveLabels);
    renderTextLabelsBatch(textLabels);
    renderEdgeLabelsBatch(edgeLabels);

    flushPrimitiveBatch();
    TextRenderer::instance().endFrame();
}





void WorldLabelRenderer::renderTextLabelsBatch(
    const std::vector<const WorldLabel*>& labels
)
{
    if (labels.empty())
        return;

    for (const WorldLabel* label : labels)
    {
        const float v = label->visual.visibility;
        if (v < 0.02f)
            continue;

        const glm::vec2 anchor = label->screenPos;
        const glm::vec2 elbow  = anchor + glm::vec2(12.0f, -12.0f);
        const glm::vec2 textPos = elbow + glm::vec2(40.0f, 0.0f);

        const glm::vec4 lineColor = {
            0.6f,
            0.8f,
            1.0f,
            v
        };

        emitLinePx(anchor, elbow, 1.25f, lineColor);
        emitLinePx(elbow, textPos, 1.25f, lineColor);

        TextRenderer::instance().textDrawBatched(
            *m_labelFont,
            label->data.displayName,
            textPos.x,
            textPos.y,
            glm::vec4(0.7f, 0.9f, 1.0f, v)
        );

        if ((label->data.semanticState == SignalSemanticState::Decoded ||
             label->data.displayClass == SignalDisplayClass::Global) &&
            label->data.distance >= 0.0f)
        {
            char buf[32];

            if (label->data.distance >= 1000.0f)
            {
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "%.1f km",
                    label->data.distance / 1000.0f
                );
            }
            else
            {
                std::snprintf(
                    buf,
                    sizeof(buf),
                    "%.0f m",
                    label->data.distance
                );
            }

            const float distYOffset =
                m_labelFont->lineHeight() + 2.0f;

            TextRenderer::instance().textDrawBatched(
                *m_distFont,
                buf,
                textPos.x,
                textPos.y + distYOffset,
                glm::vec4(0.6f, 0.8f, 1.0f, v)
            );
        }
    }
}





void WorldLabelRenderer::renderWaveLabelsBatch(
    const std::vector<const WorldLabel*>& labels
)
{
    if (labels.empty())
        return;

    for (const WorldLabel* label : labels)
    {
        const WorldLabelData& labelData = label->data;
        const WorldLabelVisualState& visual = label->visual;
        const glm::vec2& center = label->screenPos;

        for (int i = 0; i < WorldLabelVisualState::MaxWaves; ++i)
        {
            const SignalWave& w = visual.waves[i];

            if (!w.alive)
                continue;

            const float radiusPx = w.radius;

            if (radiusPx < 1.0f)
                continue;

            float alpha =
                1.0f -
                (w.radius / VisualTuning::Waves::maxWaveRadius);

            alpha = std::max(
                alpha * alpha,
                VisualTuning::Waves::minWaveAlpha
            );

            emitWaveRingPx(
                center,
                radiusPx,
                VisualTuning::Waves::lineThickness,
                glm::vec4(
                    VisualTuning::Waves::color.r,
                    VisualTuning::Waves::color.g,
                    VisualTuning::Waves::color.b,
                    alpha * visual.visibility
                )
            );
        }

        char bufRSSI[32];
        char bufSNR[32];

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

        const float textX = center.x + 6.0f;
        const float textY =
            center.y + VisualTuning::Waves::maxWaveRadius + 14.0f;

        const float v = visual.visibility;

        TextRenderer::instance().textDrawBatched(
            *m_distFont,
            bufRSSI,
            textX,
            textY,
            glm::vec4(
                VisualTuning::Waves::color.r,
                VisualTuning::Waves::color.g,
                VisualTuning::Waves::color.b,
                v
            )
        );

        TextRenderer::instance().textDrawBatched(
            *m_distFont,
            bufSNR,
            textX,
            textY + 12.0f,
            glm::vec4(
                VisualTuning::Waves::color.r,
                VisualTuning::Waves::color.g,
                VisualTuning::Waves::color.b,
                v
            )
        );
    }
}



void WorldLabelRenderer::renderEdgeLabelsBatch(
    const std::vector<const WorldLabel*>& labels
)
{
    if (labels.empty())
        return;

    for (const WorldLabel* label : labels)
    {
        const float v = label->visual.visibility;
        if (v < 0.02f)
            continue;

        if (glm::length(label->edgeDir) < 1e-4f)
            continue;

        const glm::vec2 normal = glm::normalize(label->edgeDir);
        const glm::vec2 tangent(-normal.y, normal.x);

        const glm::vec2 anchor = label->screenPos;

        constexpr float ARROW_LENGTH =
            VisualTuning::wl_edge::ARROW_LENGTH;

        constexpr float ARROW_WIDTH =
            VisualTuning::wl_edge::ARROW_WIDTH;

        const glm::vec2 tip =
            anchor + normal * ARROW_LENGTH;

        const glm::vec2 base =
            anchor - normal * VisualTuning::wl_edge::DISTANCE;

        const glm::vec2 left =
            base + tangent * (ARROW_WIDTH * 0.5f);

        const glm::vec2 right =
            base - tangent * (ARROW_WIDTH * 0.5f);

        emitTrianglePx(
            tip,
            left,
            right,
            glm::vec4(0.7f, 0.9f, 1.0f, v)
        );

        const std::string name =
            (label->data.displayName.empty() ||
             label->data.displayName == "undefined")
            ? "UNDEF"
            : label->data.displayName;

        const float textWidth =
            m_labelFont->measureText(name);

        const bool textOnLeftSide =
            normal.x > 0.3f;

        const bool textOnRightSide =
            normal.x < -0.3f;

        glm::vec2 textPos =
            anchor - normal * 18.0f + tangent * 6.0f;

        if (textOnLeftSide)
        {
            textPos.x -= textWidth;
        }
        else if (!textOnRightSide)
        {
            textPos.x -= textWidth * 0.5f;
        }

        TextRenderer::instance().textDrawBatched(
            *m_labelFont,
            name,
            textPos.x,
            textPos.y,
            glm::vec4(0.7f, 0.9f, 1.0f, v)
        );
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

    // const GLint locCenter   = glGetUniformLocation(m_waveProgram, "uCenterPx");
    // const GLint locViewport = glGetUniformLocation(m_waveProgram, "uViewport");
    // const GLint locRadius   = glGetUniformLocation(m_waveProgram, "uRadiusPx");
    // const GLint locThick    = glGetUniformLocation(m_waveProgram, "uThickness");
    // const GLint locColor    = glGetUniformLocation(m_waveProgram, "uColor");

    // -------------------------------------------------
    // Set uniforms that do NOT change per-wave
    // -------------------------------------------------
    glUniform2f(m_locWaveCenter, center.x, center.y);
    glUniform2f(m_locWaveViewport, vw, vh);
    glUniform1f(m_locWaveThickness, VisualTuning::Waves::lineThickness);

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

        glUniform1f(m_locWaveRadius, radiusPx);
        glUniform4f(
            m_locWaveColor,
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

