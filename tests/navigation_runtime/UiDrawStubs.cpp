// The UI tests check layout / hit-testing / routing only: the drawing (it needs the GL text renderer) is stubbed here.
#include "src/render/cockpit/CockpitRadioWidgetParts.h"
#include "src/ui/components/UIWidgets.h"

namespace ui
{
void UILabel::draw(UIDrawContext&) {}
void UIButton::draw(UIDrawContext&) {}
void UISlider::draw(UIDrawContext&) {}
bool debugBoundsEnabled() { return false; }
void drawDebugBounds(UIDrawContext&, const UIComponent&, const UIComponent*, const UIComponent*) {}
} // namespace ui

namespace render::cockpit
{
void CockpitRadioWidget::draw(ui::UIDrawContext&) {}
void RadioStationDisplay::draw(ui::UIDrawContext&) {}
void RadioSignalIndicator::draw(ui::UIDrawContext&) {}
} // namespace render::cockpit
