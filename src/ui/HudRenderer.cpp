#include <glad/gl.h>

#include "core/Log.h"

#include "ui/HudRenderer.h"

#include "render/HUD/TextRenderer.h"
#include "render/Font.h"

void HudRenderer::init(StateContext& context)
{

    m_context = &context;

    if (!m_font)
        m_font = new Font("assets/fonts/Roboto-Light.ttf", 18);
}


void HudRenderer::renderText(const std::vector<HudStaticItem>& items)
{

    LOG("[HudRenderer] renderText");
    LOG("  m_context = " << static_cast<void*>(m_context));

    assert(m_context && "HudRenderer: context not set");
    TextRenderer& tr = TextRenderer::instance();
    
    const Viewport& vp = m_context->viewport();
    int w = vp.width;
    int h = vp.height;
    
    for (const auto& it : items)
    {
        if (!m_font) return;
        
        LOG("[HudRenderer] about to draw text: " << it.text);
        tr.textDraw(
            *m_font,
            it.text,
            it.posNorm.x * w,
            it.posNorm.y * h,
            it.color
        );
    }
}




void HudRenderer::renderRects(const std::vector<HudLineRect>& rects)
{
    LOG("[HudRenderer] renderRects");
    LOG("  m_context = " << static_cast<void*>(m_context));

    assert(m_context && "HudRenderer: context not set");

    const Viewport& vp = m_context->viewport();
    int w = vp.width;
    int h = vp.height;

    glDisable(GL_DEPTH_TEST);

    for (const auto& r : rects)
    {
        float x = r.posNorm.x * w;
        float y = r.posNorm.y * h;
        float rw = r.sizeNorm.x * w;
        float rh = r.sizeNorm.y * h;

        glBegin(GL_LINE_LOOP);
            glColor3f(r.color.r, r.color.g, r.color.b);
            glVertex2f(x, y);
            glVertex2f(x + rw, y);
            glVertex2f(x + rw, y + rh);
            glVertex2f(x, y + rh);
        glEnd();
    }

    glEnable(GL_DEPTH_TEST);
}
