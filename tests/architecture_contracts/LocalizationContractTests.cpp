#include <filesystem>
#include <fstream>
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
        localization.loadDirectory("src/assets/localization"),
        "recursive client localization tree must load"
    );
    expect(localization.skippedFileCount() == 0,
           "committed localization tree must contain no skipped files");
    expect(localization.loadedFileCount() >= 80,
           "categorized localization tree unexpectedly lost files");

    expect(localization.locale() == "en", "English must remain the initial locale");
    expect(localization.localeDirection() == "ltr",
           "English locale direction must be LTR");
    expect(localization.localeScript() == "Latn",
           "English locale script metadata must be Latin");
    expect(localization.text("main.new_local_game", "BAD") == "NEW LOCAL GAME",
           "English UI lookup changed");
    expect(localization.catalogName("systems", "0", "BAD") == "Sol",
           "English system catalog lookup changed");

    expect(localization.setLocale("ru"), "Russian locale must be selectable");
    expect(localization.text("main.new_local_game", "BAD") == "НОВАЯ ЛОКАЛЬНАЯ ИГРА",
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
    expect(localization.localeDirection() == "ltr" && localization.localeScript() == "Jpan",
           "Japanese locale metadata must reach the native localization service");
    expect(localization.catalogName("systems", "3", "BAD") == "Wolf 359",
           "missing catalog translation must fall back to English");
    expect(!localization.setLocale("xx-not-supported"),
           "unsupported global locale must be rejected");

    const std::string webBundle = localization.webUiBundleJson();
    expect(webBundle.find("main.new_local_game") != std::string::npos &&
           webBundle.find("locale_order") != std::string::npos &&
           webBundle.find("locale_metadata") != std::string::npos &&
           webBundle.find("\"direction\":\"rtl\"") != std::string::npos,
           "WebUI runtime bundle lost shared localization/direction metadata");

    // Runtime resilience contract: one broken JSON file must not poison valid
    // siblings in the same recursively scanned localization root.
    namespace fs = std::filesystem;
    const fs::path tempRoot = fs::temp_directory_path() / "elite_localization_contract";
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot / "ui", ec);
    {
        std::ofstream out(tempRoot / "languages.json");
        out << R"({"schema_version":1,"kind":"languages","default_locale":"en","locale_order":["en"],"languages":{"en":{"en":"English"}},"locale_metadata":{"en":{"native_name":"English","english_name":"English","direction":"ltr","script":"Latn"}}})";
    }
    {
        std::ofstream out(tempRoot / "ui" / "good.json");
        out << R"({"schema_version":1,"kind":"ui_strings","strings":{"test.good":{"en":"GOOD"}}})";
    }
    {
        std::ofstream out(tempRoot / "ui" / "broken.json");
        out << R"({"schema_version":1,"kind":"ui_strings","strings":)";
    }

    game::localization::LocalizationService resilient;
    expect(resilient.loadDirectory(tempRoot.string()),
           "broken sibling JSON must not prevent valid localization load");
    expect(resilient.skippedFileCount() == 1,
           "broken sibling JSON must be counted as skipped");
    expect(resilient.text("test.good", "BAD") == "GOOD",
           "valid sibling translation disappeared after broken JSON");
    fs::remove_all(tempRoot, ec);

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

    if (failures != 0)
        return 1;

    std::cout << "[PASS] recursive localization + resilient file isolation + F12 chord policy\n";
    return 0;
}
