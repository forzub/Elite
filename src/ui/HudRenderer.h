#pragma once
#include <vector>

#include "ui/HudTypes.h"
#include "render/Font.h"


class HudRenderer
{
public:
    void init();   // ВАЖНО
    void renderText(const std::vector<HudStaticItem>& items);
    void renderRects(const std::vector<HudLineRect>& rects);
private:
    Font* m_font = nullptr;
};
