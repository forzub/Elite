#pragma once

// The magnitola's own non-interactive parts (CockpitRadioWidget.h): the station display and the signal indicator -
// separate children because each has its own layout and presentation; drawn in CockpitRadioWidgetDraw.cpp.

#include <string>

#include "src/render/cockpit/CockpitRadioWidget.h"

namespace render::cockpit
{

class RadioStationDisplay : public UIComponent
{
public:
    std::string text;
    bool lit = false;
    const RadioWidgetStyle* style = nullptr;
    void draw(ui::UIDrawContext& ctx) override;
};

class RadioSignalIndicator : public UIComponent
{
public:
    int signal = -1;   // 0..100
    const RadioWidgetStyle* style = nullptr;
    void draw(ui::UIDrawContext& ctx) override;
};

} // namespace render::cockpit
