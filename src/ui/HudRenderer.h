#pragma once
#include <vector>

#include "core/StateContext.h"

#include "ui/HudTypes.h"

#include "render/Font.h"
#include "render/types/Viewport.h"


class HudRenderer
{
public:
    void init(StateContext& context); 
    void renderText(const std::vector<HudStaticItem>& items);
    void renderRects(const std::vector<HudLineRect>& rects);

private:
    Font* m_font = nullptr;
    StateContext* m_context = nullptr;   // ← ВАЖНО
};
