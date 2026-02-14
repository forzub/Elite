#pragma once

#include "UIComponent.h"
#include "render/Font.h"
#include <glm/glm.hpp>
#include <memory>
#include <string>

class UIText : public UIComponent
{
public:
    std::string label;
    std::unique_ptr<Font> font;
    glm::vec4 color {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec2 textOffset {0.0f, 0.0f};      // смещение внутри компонента
    
    std::string fontPath = "assets/fonts/Roboto-Light.ttf";
    float fontSizeRel = 0.03f;              // 3% от высоты экрана
    bool sizeRelativeToScreen = true;
    float pixelSize = 32.0f;                // размер шрифта

private:
    int cachedPixelSize = -1;      // ← вот он

protected:
    void render(
        const Viewport& vp,
        float parentX,
        float parentY,
        float parentW,
        float parentH
    ) override;    
};
