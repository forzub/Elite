// The UI component pass (author's brief 2026-11-24): the magnitola laid out at several resolutions / scales - a click
// at the CENTRE of every drawn child gives exactly that child's action; a slider drag keeps the pointer (capture) even
// outside its rect; the window -> UI mapping of a letterboxed viewport lands on the same child. No GL, no GLFW.
#include <cstdio>
#include <string>
#include <vector>

#include "src/render/cockpit/CockpitRadioWidget.h"
#include "src/ui/components/UIInputRouter.h"

using render::cockpit::RadioAction;

static int failures = 0, checks = 0;
static void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

struct Recorder
{
    std::vector<std::pair<RadioAction, float>> actions;
};

static ui::PointerSample at(const ui::UIPoint& p, bool down)
{
    ui::PointerSample s;
    s.valid = true;
    s.position = p;
    s.leftDown = down;
    return s;
}

static void clickCentre(ui::UIInputRouter& router, render::cockpit::CockpitRadioWidget& w, const char* childId)
{
    const auto c = w.child(childId)->resolvedRect().centre();
    router.route(w, at(c, true));
    router.route(w, at(c, false));
}

static void runAt(float uiW, float uiH, float scale, bool playlist)
{
    render::cockpit::CockpitRadioWidget w;
    Recorder rec;
    w.onAction = [&](RadioAction a, float v) { rec.actions.push_back({a, v}); };
    render::cockpit::RadioPresentation p;
    p.on = true;
    p.title = "Station";
    p.header = "zone";
    p.volume = 40;
    p.signal = 70;
    p.playlistMode = playlist;
    w.setPresentation(p);
    // the engine group in the lower left of the UI screen
    render::cockpit::EngineGroupAnchor group {true, 0.02f * uiW, 0.02f * uiW + 420.0f * scale, uiH - 160.0f * scale, scale};
    const auto panel = render::cockpit::CockpitRadioWidget::panelAboveEngineGroup(group, w.style());
    w.layoutPanel(panel, scale);
    const std::string tag = std::to_string(static_cast<int>(uiW)) + "x" + std::to_string(static_cast<int>(uiH)) + " s" +
                            std::to_string(scale) + (playlist ? " playlist" : "");
    check(panel.valid() && panel.bottom() <= group.top, tag + ": the panel sits above the engine group");

    ui::UIInputRouter router;
    const std::vector<std::pair<const char*, RadioAction>> buttons = {
        {"radio.power", RadioAction::Power},           {"radio.source", RadioAction::Source},
        {"radio.previous", RadioAction::Previous},     {"radio.next", RadioAction::Next},
        {"radio.volume_down", RadioAction::VolumeDown}, {"radio.volume_up", RadioAction::VolumeUp}};
    for (const auto& [id, action] : buttons)
    {
        rec.actions.clear();
        clickCentre(router, w, id);
        check(rec.actions.size() == 1 && rec.actions[0].first == action, tag + ": a click at the centre of " + id);
        // every child's rect lies inside the panel
        const auto r = w.child(id)->resolvedRect();
        check(r.x >= panel.x && r.right() <= panel.right() && r.y >= panel.y && r.bottom() <= panel.bottom(),
              tag + ": " + id + " inside the panel");
    }
    // the remove: only in the playlist mode
    rec.actions.clear();
    clickCentre(router, w, "radio.remove");
    if (playlist)
        check(rec.actions.size() == 1 && rec.actions[0].first == RadioAction::RemoveTrack, tag + ": remove clicked");
    else
        check(rec.actions.empty(), tag + ": the hidden remove takes no click");
    // the display / the caption: not interactive - no action
    rec.actions.clear();
    clickCentre(router, w, "radio.display");
    check(rec.actions.empty(), tag + ": the display is not a button");

    // the slider: press at 25 %, drag OUTSIDE its rect to the right, release - the capture keeps it to 100 %
    const auto vr = w.child("radio.volume")->resolvedRect();
    rec.actions.clear();
    router.route(w, at({vr.x + 0.25f * vr.width, vr.y + 0.5f * vr.height}, true));
    check(router.captured() == w.child("radio.volume"), tag + ": the slider captures the pointer");
    check(!rec.actions.empty() && rec.actions.back().first == RadioAction::SetVolume &&
              std::abs(rec.actions.back().second - 0.25f) < 0.02f, tag + ": the press sets 25 %");
    router.route(w, at({vr.right() + 300.0f, vr.y - 200.0f}, true));   // far outside
    check(std::abs(rec.actions.back().second - 1.0f) < 1e-4f, tag + ": dragged outside: 100 %");
    router.route(w, at({vr.right() + 300.0f, vr.y - 200.0f}, false));
    check(router.captured() == nullptr, tag + ": the release frees the pointer");
    // after the release a move over the power is not a click
    rec.actions.clear();
    router.route(w, at(w.child("radio.power")->resolvedRect().centre(), false));
    check(rec.actions.empty(), tag + ": hovering is not a click");
}

int main()
{
    runAt(1920.0f, 1080.0f, 1.0f, false);
    runAt(1920.0f, 1080.0f, 1.0f, true);
    runAt(1280.0f, 720.0f, 0.75f, true);
    runAt(3840.0f, 2160.0f, 2.0f, false);

    // the platform mapping: a 2560x1440 window holding a letterboxed 16:9 viewport? -> a 21:9 window 3440x1440: the game
    // viewport 2560x1440 at x 440; the UI screen 1920x1080. A window pixel at the CENTRE of the power button maps back
    // onto it.
    {
        render::cockpit::CockpitRadioWidget w;
        Recorder rec;
        w.onAction = [&](RadioAction a, float v) { rec.actions.push_back({a, v}); };
        render::cockpit::RadioPresentation p;
        p.on = true;
        w.setPresentation(p);
        render::cockpit::EngineGroupAnchor group {true, 40.0f, 460.0f, 900.0f, 1.0f};
        w.layoutPanel(render::cockpit::CockpitRadioWidget::panelAboveEngineGroup(group, w.style()), 1.0f);
        const auto c = w.child("radio.power")->resolvedRect().centre();
        // UI (1920x1080) -> viewport (2560x1440, x 440, y 0) -> window (3440x1440, framebuffer = window)
        const double wx = 440.0 + c.x * 2560.0 / 1920.0, wy = c.y * 1440.0 / 1080.0;
        const auto back = ui::UIInputRouter::windowToUi(wx, wy, 3440, 1440, 3440, 1440, 440, 0, 2560, 1440, 1920.0f, 1080.0f);
        check(std::abs(back.x - c.x) < 0.01f && std::abs(back.y - c.y) < 0.01f, "letterbox: window -> UI is the inverse");
        ui::UIInputRouter router;
        router.route(w, at(back, true));
        check(rec.actions.size() == 1 && rec.actions[0].first == RadioAction::Power, "letterbox: the power is clicked");
        // a HiDPI window (framebuffer 2x the window) with a bottom letterbox (viewport y 100 in GL)
        const double hx = (0.0 + c.x * 1920.0 / 1920.0) / 2.0, hy = (1180.0 - 100.0 - 1080.0 + c.y) / 2.0;
        const auto hi = ui::UIInputRouter::windowToUi(hx, hy, 960, 590, 1920, 1180, 0, 100, 1920, 1080, 1920.0f, 1080.0f);
        check(std::abs(hi.x - c.x) < 0.01f && std::abs(hi.y - c.y) < 0.01f, "HiDPI + bottom letterbox: window -> UI");
    }
    std::printf("ui_radio_widget: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
