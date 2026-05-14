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

    // --- абсолютная позиция ---
    float px = parentX + position.x * parentW;
    float py = parentY + position.y * parentH;

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

    // --- дети ---
    renderChildren(vp, px, py, parentW, parentH);
}
