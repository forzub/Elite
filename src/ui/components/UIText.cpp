#include "UIText.h"
#include "render/HUD/TextRenderer.h"

void UIText::render(
    const Viewport& vp,
    float parentX,
    float parentY,
    float parentW,
    float parentH
)
{
    if (!visible)
        return;

    // --- the position: the ONE layout of every component (position / size / anchor in the parent's rect; it used to take
    // only position here and ignore the anchor and the size) ---
    float px = 0.0f, py = 0.0f, pw = 0.0f, ph = 0.0f;
    computeLayout(parentX, parentY, parentW, parentH, px, py, pw, ph);
    m_rect = ui::UIRect {px, py, pw, ph};

    // --- вычисляем пиксельный размер ---
    float pixelSizeF;

    if (sizeRelativeToScreen)
        pixelSizeF = fontSizeRel * vp.height;
    else
        pixelSizeF = fontSizeRel * parentH;

    int pixelSize = static_cast<int>(pixelSizeF);

    // --- если размер изменился — пересоздаём шрифт ---
    if (pixelSize != cachedPixelSize)
    {
        font = std::make_unique<Font>(fontPath,  pixelSize);

        cachedPixelSize = pixelSize;
    }

    if (!font)
        return;

    // --- рисуем ---
    TextRenderer::instance().textDraw(
        *font,
        label,
        px,
        py,
        color
    );

    // --- the children: inside this text's own rect ---
    renderChildren(vp, px, py, pw, ph);
}
