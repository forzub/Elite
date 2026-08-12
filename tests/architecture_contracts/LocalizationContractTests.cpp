#include <iostream>
#include <string>

#include "src/game/localization/LocalizationService.h"
#include "src/game/ui/GameUiHotkeyPolicy.h"

namespace
{
int failures = 0;

void expect(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    }
}
}

int main()
{
    game::localization::LocalizationService localization;
    expect(
        localization.load(
            "src/assets/webui/localization/ui_strings.json",
            "src/assets/data/localization/catalog_names.json"
        ),
        "client localization tables must load"
    );

    expect(localization.locale() == "en", "English must remain the initial locale");
    expect(localization.text("main.new_game", "BAD") == "NEW GAME",
           "English UI lookup changed");
    expect(localization.catalogName("systems", "0", "BAD") == "Sol",
           "English system catalog lookup changed");

    expect(localization.setLocale("ru"), "Russian locale must be selectable");
    expect(localization.text("main.new_game", "BAD") == "НОВАЯ ИГРА",
           "Russian UI lookup changed");
    expect(localization.catalogName("systems", "0", "BAD") == "Солнечная система",
           "Russian system catalog lookup changed");

    expect(localization.setLocale("zh-Hans"), "Chinese locale must be selectable");
    expect(localization.text("main.exit", "BAD") == "退出",
           "Chinese UI lookup changed");
    expect(localization.catalogName("bodies", "0:system_0.Sol.Земля", "BAD") == "地球",
           "Chinese body catalog lookup changed");
    expect(localization.catalogName("hubs", "earth_orbital_hub", "BAD") == "地球轨道枢纽",
           "Chinese hub catalog lookup changed");
    expect(localization.text("overlay.player", "BAD") == "玩家",
           "native map-overlay UI lookup changed");
    expect(localization.text("cockpit.mode.newtonian", "BAD") == "牛顿模式",
           "native cockpit-service UI lookup changed");

    expect(localization.setLocale("ja"), "Japanese locale must be selectable");
    expect(localization.catalogName("systems", "3", "BAD") == "Wolf 359",
           "missing catalog translation must fall back to English, not prior locale") ;

    expect(!localization.setLocale("xx-not-supported"),
           "unsupported global locale must be rejected");
    expect(localization.locale() == "ja",
           "rejected locale must not change current global language");

    // The F12 chord matrix is a protected player-facing contract.
    using game::ui::F12HotkeyAction;
    using game::ui::resolveF12HotkeyAction;
    expect(resolveF12HotkeyAction(false, false, true) == F12HotkeyAction::NavigateLocal,
           "plain F12 must remain Local/Hub navigation");
    expect(resolveF12HotkeyAction(true, false, true) == F12HotkeyAction::ToggleConstellations,
           "Ctrl+F12 must remain constellation visibility");
    expect(resolveF12HotkeyAction(false, true, true) == F12HotkeyAction::CycleSkyCulture,
           "Alt+F12 must cycle sky culture");
    expect(resolveF12HotkeyAction(true, true, true) == F12HotkeyAction::CycleUiLanguage,
           "Ctrl+Alt+F12 must cycle global UI language");
    expect(resolveF12HotkeyAction(true, true, false) == F12HotkeyAction::CycleUiLanguage,
           "global UI language chord must work outside SpaceState");
    expect(resolveF12HotkeyAction(false, true, false) == F12HotkeyAction::None,
           "sky-culture chord must not act outside SpaceState");

    if (failures != 0)
        return 1;

    std::cout << "[PASS] global localization + stable-ID catalog fallback + F12 chord policy\n";
    return 0;
}
