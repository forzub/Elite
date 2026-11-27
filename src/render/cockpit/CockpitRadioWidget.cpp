// The magnitola as a composite UI component (CockpitRadioWidget.h): the children, their layout from the panel rect, the
// actions. No drawing (CockpitRadioWidgetDraw.cpp), no input reading (ui::UIInputRouter), no audio calls (onAction).
#include "src/render/cockpit/CockpitRadioWidget.h"
#include "src/game/localization/LocalizationService.h"

#include <algorithm>
#include <memory>

#include "src/render/cockpit/CockpitRadioWidgetParts.h"

namespace render::cockpit
{

CockpitRadioWidget::CockpitRadioWidget()
{
    id = "radio";
    const auto button = [&](const char* name, RadioAction action) {
        auto b = std::make_unique<ui::UIButton>();
        b->id = name;
        b->style = &m_buttonStyle;
        b->onClick = [this, action] { emit(action); };
        return add(std::move(b));
    };
    auto caption = std::make_unique<ui::UILabel>();
    caption->id = "radio.caption";
    caption->text = game::localization::tr("cockpit.radio.caption", "STEREO");
    m_caption = add(std::move(caption));
    m_power = button("radio.power", RadioAction::Power);
    m_source = button("radio.source", RadioAction::Source);
    m_prev = button("radio.previous", RadioAction::Previous);
    m_prev->label = "<";
    auto display = std::make_unique<RadioStationDisplay>();
    display->id = "radio.display";
    display->style = &m_style;
    m_display = add(std::move(display));
    m_next = button("radio.next", RadioAction::Next);
    m_next->label = ">";
    m_remove = button("radio.remove", RadioAction::RemoveTrack);
    m_remove->label = "x";
    auto header = std::make_unique<ui::UILabel>();
    header->id = "radio.header";
    m_header = add(std::move(header));
    auto signal = std::make_unique<RadioSignalIndicator>();
    signal->id = "radio.signal";
    signal->style = &m_style;
    m_signal = add(std::move(signal));
    m_volDown = button("radio.volume_down", RadioAction::VolumeDown);
    m_volDown->label = "-";
    auto slider = std::make_unique<ui::UISlider>();
    slider->id = "radio.volume";
    slider->style = &m_buttonStyle;
    slider->onChange = [this](float v) { emit(RadioAction::SetVolume, v); };
    m_volume = add(std::move(slider));
    m_volUp = button("radio.volume_up", RadioAction::VolumeUp);
    m_volUp->label = "+";
}

void CockpitRadioWidget::setPresentation(const RadioPresentation& p)
{
    m_presentation = p;
    m_caption->text = game::localization::tr("cockpit.radio.caption", "STEREO");   // the UI language may have changed
    m_power->label = p.on ? game::localization::tr("cockpit.radio.button.on", "ON") : game::localization::tr("cockpit.radio.button.off", "OFF");
    m_power->lit = p.on;
    m_source->label = p.radioSource ? game::localization::tr("cockpit.radio.button.radio", "RADIO")
                                    : game::localization::tr("cockpit.radio.button.list", "LIST");
    for (auto* b : {m_source, m_prev, m_next, m_volDown, m_volUp})
        b->lit = p.on;
    m_remove->lit = true;
    m_remove->visible = p.playlistMode;
    auto* display = static_cast<RadioStationDisplay*>(m_display);
    display->text = p.on ? p.title + (p.connecting ? " ..." : "") : "---";
    display->lit = p.on;
    auto* signal = static_cast<RadioSignalIndicator*>(m_signal);
    signal->signal = p.signal;
    m_signal->visible = p.on && p.signal >= 0;
    m_header->text = p.header;
    m_header->visible = p.on;
    m_volume->value01 = static_cast<float>(p.volume) / 100.0f;
    m_volume->lit = p.on;
}

ui::UIRect CockpitRadioWidget::panelAboveEngineGroup(const EngineGroupAnchor& frame, const RadioWidgetStyle& st)
{
    if (!frame.valid)
        return {};
    const float s = frame.scale;
    const float w = std::max(st.minWidth * s, frame.right - frame.left);
    const float h = 3.0f * st.rowHeight * s + 2.0f * st.pad * s;
    return ui::UIRect {frame.left, frame.top - st.gapAbove * s - h, w, h};
}

void CockpitRadioWidget::layoutPanel(const ui::UIRect& panel, float scale)
{
    m_rect = panel;
    m_scale = scale;
    const auto& st = m_style;
    const float s = scale;
    const float x = panel.x, w = panel.width;
    const float pad = st.pad * s, gap = st.gap * s, rowH = st.rowHeight * s;
    const float btn = rowH - st.buttonInset * s;
    m_buttonStyle.textPx = static_cast<int>(st.smallText);
    m_buttonStyle.accent = m_caption->colour;
    // row 1: the caption · the source · the power
    const float r1 = panel.y + pad;
    m_power->setResolvedRect({x + w - pad - st.powerWidth * s, r1, st.powerWidth * s, btn});
    m_source->setResolvedRect({m_power->resolvedRect().x - gap - st.sourceWidth * s, r1, st.sourceWidth * s, btn});
    m_caption->setResolvedRect({x + pad, r1, m_source->resolvedRect().x - gap - (x + pad), btn});
    m_caption->textPx = static_cast<int>(st.smallText);
    // row 2: < the display >
    const float r2 = r1 + rowH;
    m_prev->setResolvedRect({x + pad, r2, btn, btn});
    m_next->setResolvedRect({x + w - pad - btn, r2, btn, btn});
    const float dx = x + pad + btn + 6.0f * s;
    m_display->setResolvedRect({dx, r2, m_next->resolvedRect().x - 6.0f * s - dx, btn});
    // row 3: [x] the header · the signal | - [volume] +
    const float r3 = r2 + rowH;
    m_volUp->setResolvedRect({x + w - pad - btn, r3, btn, btn});
    m_volDown->setResolvedRect({x + w - pad - 2.0f * btn - st.volumeBarWidth * s - 8.0f * s, r3, btn, btn});
    const float vx = m_volDown->resolvedRect().right() + 4.0f * s;
    m_volume->setResolvedRect({vx, r3, m_volUp->resolvedRect().x - 4.0f * s - vx, btn});   // the whole row band: grab area
    const float bw = 6.0f * s, bg = 3.0f * s;
    const float sigW = 5.0f * (bw + bg);
    m_signal->setResolvedRect({m_volDown->resolvedRect().x - gap - sigW, r3, sigW, btn});
    m_remove->setResolvedRect({x + pad, r3, btn, btn});
    const float headerLeft = m_remove->visible ? m_remove->resolvedRect().right() + 6.0f * s : x + pad;
    const float headerRight = (m_signal->visible ? m_signal->resolvedRect().x : m_volDown->resolvedRect().x) - gap;
    m_header->setResolvedRect({headerLeft, r3, std::max(0.0f, headerRight - headerLeft), btn});
    m_header->textPx = static_cast<int>(st.smallText);
}

} // namespace render::cockpit
