#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

#include "core/StateContext.h"

#include "render/WorldLabelRenderer.h"
#include "render/TextRenderer.h"
#include "render/ScreenUtils.h"
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


void WorldLabelRenderer::init(StateContext& context)
{
    m_context = &context;
    if (!m_labelFont)
        m_labelFont = new Font("assets/fonts/Roboto-Light.ttf", 12);

    if (!m_distFont)
        m_distFont = new Font("assets/fonts/Roboto-Light.ttf", 11);
}









void WorldLabelRenderer::render(
    const WorldLabel& lbl,
    const glm::mat4& view,
    const glm::mat4& proj,
    int screenW,
    int screenH
)
{
    glm::vec2 screenPos;

    bool visible = projectToScreen(
        lbl.worldPos,
        view,
        proj,
        screenW,
        screenH,
        screenPos
    );

    if (!visible)
        return;

    if (lbl.hasDistance)
    {
        renderPreciseLabel(lbl, screenPos);
        return;
    }
    else
    {
        renderDirectionOnly(lbl, screenPos);
        return;
    }

    
}









// ----------------------debug---------------------- 

void WorldLabelRenderer::renderDirectionOnly(
    const WorldLabel& lbl,
    const glm::vec2& center
)
{
    
    
    // const float maxRadius = 60.0f;
    // const float minAlpha  = 0.05f;

    const float maxRadius = VisualTuning::WaveMaxRadius;
    const float minAlpha  = VisualTuning::MinWaveAlpha;

    glDisable(GL_DEPTH_TEST);

    for (int i = 0; i < lbl.waveCount; ++i)
    {
        const SignalWave& w = lbl.waves[i];

        if (!w.alive)
            continue;

        float r = w.radius;
// ----------------------debug----------------------
        // if (r <= 0.0f || r > maxRadius)
        if (r > maxRadius)
            continue;
// ----------------------debug----------------------
        float alpha = 1.0f - (r / maxRadius);
        alpha *= alpha;
        alpha = std::max(alpha, minAlpha);
// ----------------------debug---------------------- 
        // glColor4f(0.6f, 0.8f, 1.0f, alpha);

        const float colors[3][3] = {
            {0.6f, 0.8f, 1.0f},
            {0.4f, 0.7f, 1.0f},
            {0.3f, 0.6f, 1.0f}
        };

        glColor4f(
            colors[i][0],
            colors[i][1],
            colors[i][2],
            alpha
        );
// ----------------------debug----------------------         
        drawCircle(center, r);
    }

    // ---- имя сигнала (fade по visibility) ----
    float a = lbl.visibility;
    if (a > 0.02f)
    {
        float textY = center.y - maxRadius - 16.0f;

        TextRenderer::instance().textDraw(
            *m_labelFont,
            lbl.label,
            center.x + 6.0f,
            textY,
            {
                0.7f * a,
                0.9f * a,
                1.0f * a
            }
        );
    }

    glEnable(GL_DEPTH_TEST);
}











void WorldLabelRenderer::renderPreciseLabel(
    const WorldLabel& lbl,
    const glm::vec2& screenPos
)
{
    if (lbl.visibility < 0.02f)
        return; // текст и линии полностью погасли

    float a = lbl.visibility;

    // ---- distance string ----
    char distBuf[32];
    if (lbl.distance < 0.0f)
    {
        std::snprintf(distBuf, sizeof(distBuf), "--");
    }
    else if (lbl.distance >= 1000.0f)
    {
        std::snprintf(
            distBuf,
            sizeof(distBuf),
            "%.1f km",
            lbl.distance / 1000.0f
        );
    }
    else
    {
        std::snprintf(
            distBuf,
            sizeof(distBuf),
            "%.0f m",
            lbl.distance
        );
    }

    auto& tr = TextRenderer::instance();

    // -------------------------------------------------
    // label placement
    // -------------------------------------------------
    glm::vec2 labelTopPos = screenPos + glm::vec2(40.0f, -30.0f);

    float labelWidth  = m_labelFont->measureText(lbl.label);
    float labelAscent = m_labelFont->ascent();
    float labelHeight = m_labelFont->lineHeight();

    float labelBaselineY = labelTopPos.y + labelAscent;
    float underlineY     = labelBaselineY + 2.0f;

    // -------------------------------------------------
    // anchor + connector lines
    // -------------------------------------------------
    glm::vec2 boxAnchor = {
        labelTopPos.x,
        labelTopPos.y + labelHeight * 0.5f
    };

    glm::vec2 elbow = boxAnchor + glm::vec2(-10.0f, 0.0f);

    drawLine(screenPos, elbow, {0.6f * a, 0.8f * a, 1.0f * a});
    drawLine(elbow, boxAnchor, {0.6f * a, 0.8f * a, 1.0f * a});

    // -------------------------------------------------
    // label text
    // -------------------------------------------------
    tr.textDraw(
        *m_labelFont,
        lbl.label,
        labelTopPos.x + 6.0f,
        labelBaselineY,
        {0.7f * a, 0.9f * a, 1.0f * a}
    );

    // underline
    drawLine(
        { labelTopPos.x, underlineY },
        { labelTopPos.x + labelWidth, underlineY },
        {0.7f * a, 0.9f * a, 1.0f * a}
    );

    // -------------------------------------------------
    // distance text
    // -------------------------------------------------
    float distWidth     = m_distFont->measureText(distBuf);
    float distBaselineY = underlineY + m_distFont->ascent() + 4.0f;
    float distX         = labelTopPos.x + (labelWidth - distWidth) * 0.5f;

    tr.textDraw(
        *m_distFont,
        distBuf,
        distX,
        distBaselineY,
        {0.7f * a, 0.9f * a, 1.0f * a}
    );
}

