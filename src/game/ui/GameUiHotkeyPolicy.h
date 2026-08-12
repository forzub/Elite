#pragma once

namespace game::ui
{
enum class F12HotkeyAction
{
    None,
    NavigateLocal,
    ToggleConstellations,
    CycleSkyCulture,
    CycleUiLanguage
};

// F12 chords are a player-facing contract. Keep modifier precedence explicit:
// global UI language > sky culture > overlay visibility > plain map navigation.
// The language chord is deliberately available outside SpaceState so menus and
// loading screens use the same global client locale.
constexpr F12HotkeyAction resolveF12HotkeyAction(
    bool ctrlDown,
    bool altDown,
    bool spaceStateAvailable
)
{
    if (ctrlDown && altDown)
        return F12HotkeyAction::CycleUiLanguage;
    if (altDown)
        return spaceStateAvailable
            ? F12HotkeyAction::CycleSkyCulture
            : F12HotkeyAction::None;
    if (ctrlDown)
        return spaceStateAvailable
            ? F12HotkeyAction::ToggleConstellations
            : F12HotkeyAction::None;
    return spaceStateAvailable
        ? F12HotkeyAction::NavigateLocal
        : F12HotkeyAction::None;
}
}
