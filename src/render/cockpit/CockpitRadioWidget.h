#pragma once

// THE MAGNITOLA WIDGET (author 2026-11-23: «сделай виджит магнитолы. кнопка лево-право, дисплей для титула станции,
// кнопка переключения между радио / свой лист, громкость. расположение - над виджетами двигательной группы»).
//
// The reference COMPOSITE component of the UI pass (author's brief 2026-11-24): the panel (its background and frame are
// its own drawing) holds children - the caption, the power and source buttons, < the station display >, the remove,
// the signal indicator, volume - [slider] +. Each child has ONE resolved rect (ui::UIRect): it is drawn there and it is
// clicked there - the user clicks the very subcomponent drawn at that place, not a hand-computed box.
//
// The widget is NOT the radio: it shows a read-only RadioPresentation and emits RadioActions; the caller (SpaceState)
// applies them to audio::CockpitRadio. It never reads GLFW (ui::UIInputRouter routes the pointer to its children).

#include <functional>
#include <string>

#include "src/ui/components/UIComponent.h"
#include "src/ui/components/UIWidgets.h"

namespace render::cockpit
{

// where the panel sits: the engine group's published frame (TRANSITIONAL - the engine group is still a legacy renderer;
// SpaceState converts its GroupFrame into this; a CockpitBottomLeftCluster container will own both)
struct EngineGroupAnchor
{
    bool valid = false;
    float left = 0.0f, right = 0.0f, top = 0.0f, scale = 1.0f;
};

struct RadioPresentation
{
    bool on = false;
    bool radioSource = true;     // false: the own playlist
    std::string title;           // the station / the track
    bool connecting = false;
    std::string header;          // the zone / the counter line
    int volume = 0;              // 0..100
    int signal = -1;             // 0..100, -1 none
    bool playlistMode = false;   // the "x" (remove the current track) is shown
};

enum class RadioAction
{
    Power,
    Source,
    Previous,
    Next,
    VolumeDown,
    VolumeUp,
    SetVolume,     // value01
    RemoveTrack
};

// the style: sizes in pixels at scale 1 (x the HUD scale), the colours
struct RadioWidgetStyle
{
    float minWidth = 260.0f;
    float gapAbove = 9.0f;      // between the panel and the engine group under it
    float gap = 9.0f;
    float rowHeight = 30.0f;
    float pad = 8.0f;
    float buttonInset = 6.0f;   // a row's button = row height - this
    float powerWidth = 58.0f;
    float sourceWidth = 96.0f;
    float volumeBarWidth = 70.0f;
    float smallText = 15.0f;
    float bigText = 19.0f;
    glm::vec4 lcd {0.35f, 0.95f, 0.80f, 1.0f};
    glm::vec4 lcdFill {0.01f, 0.06f, 0.05f, 0.95f};
};

class CockpitRadioWidget : public UIComponent
{
public:
    CockpitRadioWidget();

    void setPresentation(const RadioPresentation& p);
    const RadioPresentation& presentation() const { return m_presentation; }
    std::function<void(RadioAction, float)> onAction;

    // the panel's place: right above the engine group, as wide as it (TRANSITIONAL adapter - the engine group is still a
    // legacy renderer that publishes its frame; a CockpitBottomLeftCluster container will own both)
    static ui::UIRect panelAboveEngineGroup(const EngineGroupAnchor& frame, const RadioWidgetStyle& style);
    // lays the children out inside `panel` (the one geometry they are drawn and hit with)
    void layoutPanel(const ui::UIRect& panel, float scale);

    void draw(ui::UIDrawContext& ctx) override;

    // the children (tests, the debug overlay)
    UIComponent* child(const std::string& id) { return findById(id); }
    const RadioWidgetStyle& style() const { return m_style; }

private:
    void emit(RadioAction a, float v = 0.0f)
    {
        if (onAction)
            onAction(a, v);
    }
    RadioPresentation m_presentation;
    RadioWidgetStyle m_style;
    ui::UIWidgetStyle m_buttonStyle;
    float m_scale = 1.0f;
    ui::UILabel* m_caption = nullptr;
    ui::UIButton* m_power = nullptr;
    ui::UIButton* m_source = nullptr;
    ui::UIButton* m_prev = nullptr;
    ui::UIButton* m_next = nullptr;
    ui::UIButton* m_remove = nullptr;
    ui::UIButton* m_volDown = nullptr;
    ui::UIButton* m_volUp = nullptr;
    ui::UISlider* m_volume = nullptr;
    UIComponent* m_display = nullptr;
    UIComponent* m_signal = nullptr;
    ui::UILabel* m_header = nullptr;
};

} // namespace render::cockpit
