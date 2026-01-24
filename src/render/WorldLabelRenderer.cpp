#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "render/WorldLabelRenderer.h"
#include "render/TextRenderer.h"
#include "render/Font.h"
#include "render/ScreenUtils.h"

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

void WorldLabelRenderer::init()
{
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

    // ---- distance string ----
    bool showDistance = (lbl.distance >= 0.0f);

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

    glm::vec2 labelTopPos = screenPos + glm::vec2(40.0f, -30.0f);

    float labelWidth  = m_labelFont->measureText(lbl.label);
    float labelAscent = m_labelFont->ascent();
    float labelHeight = m_labelFont->lineHeight();

    float labelBaselineY = labelTopPos.y + labelAscent;
    float underlineY = labelBaselineY + 2.0f;

    if (showDistance)
    {
        float distWidth = m_distFont->measureText(distBuf);
        float distBaselineY = underlineY + m_distFont->ascent() + 4.0f;
        float distX = labelTopPos.x + (labelWidth - distWidth) * 0.5f;

        tr.textDraw(
            *m_distFont,
            distBuf,
            distX,
            distBaselineY,
            {0.6f, 0.8f, 1.0f}
        );
    }

    glm::vec2 boxAnchor = {
        labelTopPos.x,
        labelTopPos.y + labelHeight * 0.5f
    };

    glm::vec2 elbow = boxAnchor + glm::vec2(-10.0f, 0.0f);

    drawLine(screenPos, elbow, {0.6f, 0.8f, 1.0f});
    drawLine(elbow, boxAnchor, {0.6f, 0.8f, 1.0f});

    tr.textDraw(
        *m_labelFont,
        lbl.label,
        labelTopPos.x + 6.0f,
        labelBaselineY,
        {0.7f, 0.9f, 1.0f}
    );

    drawLine(
        { labelTopPos.x, underlineY },
        { labelTopPos.x + labelWidth, underlineY },
        { 0.7f, 0.9f, 1.0f }
    );

    float distWidth = m_distFont->measureText(distBuf);
    float distBaselineY = underlineY + m_distFont->ascent() + 4.0f;
    float distX = labelTopPos.x + (labelWidth - distWidth) * 0.5f;

    tr.textDraw(
        *m_distFont,
        distBuf,
        distX,
        distBaselineY,
        {0.6f, 0.8f, 1.0f}
    );

    
}




void WorldLabelRenderer::renderDirectionOnly(
    const WorldLabel& lbl,
    const glm::vec2& center
)
{
    const int   waveCount   = 3;
    const float maxRadius   = 60.0f;
    const float baseSpeed   = 25.0f; // px/sec
    const float minAlpha    = 0.05f;

    // чем ниже stability — тем медленнее и "шире" волны
    float speed = baseSpeed * (0.3f + lbl.stability);
    float radiusScale = 0.6f + lbl.stability * 0.8f;

    float time = static_cast<float>(glfwGetTime());
   
    // фаза волны (0..1)
    // float textPhase = std::fmod(time, 3.0f) / 3.0f;
    float textPhase = std::fmod(time - 0.5f, 3.0f) / 3.0f;

    // альфа текста: появляется → держится → исчезает
    float textAlpha = 1.0f - std::abs(textPhase * 2.0f - 1.0f);

    // мягче
    textAlpha *= textAlpha;

    // зависимость от уверенности сигнала
    textAlpha *= (0.3f + lbl.stability * 0.7f);

    glDisable(GL_DEPTH_TEST);

    for (int i = 0; i < waveCount; ++i)
    {
        // каждая волна сдвинута во времени
        float t = std::fmod(time - i * 0.6f, 3.0f);
        if (t < 0.0f)
            continue;

        float r = t * speed * radiusScale;
        if (r > maxRadius)
            continue;

        // альфа убывает по мере роста
        float alpha = 1.0f - (r / maxRadius);
        alpha *= alpha; // мягче
        alpha = std::max(alpha, minAlpha);

        glColor4f(0.6f, 0.8f, 1.0f, alpha);
        drawCircle(center, r);
    }

    // ---- имя сигнала ----
    float textY = center.y - maxRadius - 16.0f;

    TextRenderer::instance().textDraw(
        *m_labelFont,
        lbl.label,
        center.x + 6.0f,
        textY,
        {
            0.7f * textAlpha,
            0.9f * textAlpha,
            1.0f * textAlpha
        }
    );

    glEnable(GL_DEPTH_TEST);
}



void WorldLabelRenderer::renderPreciseLabel(
    const WorldLabel& lbl,
    const glm::vec2& screenPos
)
{
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

    drawLine(screenPos, elbow, {0.6f, 0.8f, 1.0f});
    drawLine(elbow, boxAnchor, {0.6f, 0.8f, 1.0f});

    // -------------------------------------------------
    // label text
    // -------------------------------------------------
    tr.textDraw(
        *m_labelFont,
        lbl.label,
        labelTopPos.x + 6.0f,
        labelBaselineY,
        {0.7f, 0.9f, 1.0f}
    );

    // underline
    drawLine(
        { labelTopPos.x, underlineY },
        { labelTopPos.x + labelWidth, underlineY },
        { 0.7f, 0.9f, 1.0f }
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
        {0.6f, 0.8f, 1.0f}
    );
}

