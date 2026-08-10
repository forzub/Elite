#include "src/game/ui/SystemMapUiCommandRouter.h"

#include <charconv>
#include <string_view>

namespace game::ui
{
namespace
{
std::optional<int> parseIdAfterPrefix(
    std::string_view command,
    std::string_view prefix
)
{
    if (command.substr(0, prefix.size()) != prefix)
        return std::nullopt;

    const std::string_view text = command.substr(prefix.size());
    if (text.empty())
        return std::nullopt;

    int id = -1;
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, id);

    if (result.ec != std::errc{} || result.ptr != end || id < 0)
        return std::nullopt;

    return id;
}
}

std::optional<SystemMapUiCommand> parseSystemMapUiCommand(
    std::string_view command
)
{
    constexpr std::string_view OpenSelectedPrefix =
        "system_map_open_selected:";
    constexpr std::string_view SelectPrefix =
        "system_map_select:";

    if (command.substr(0, OpenSelectedPrefix.size()) == OpenSelectedPrefix)
    {
        const auto id = parseIdAfterPrefix(command, OpenSelectedPrefix);
        if (!id)
            return std::nullopt;
        return SystemMapUiCommand{
            SystemMapUiCommandType::OpenSelectedSystem,
            *id
        };
    }

    if (command.substr(0, SelectPrefix.size()) == SelectPrefix)
    {
        const auto id = parseIdAfterPrefix(command, SelectPrefix);
        if (!id)
            return std::nullopt;
        return SystemMapUiCommand{
            SystemMapUiCommandType::SelectSystem,
            *id
        };
    }

    if (command == "system_map_galaxy")
        return SystemMapUiCommand{SystemMapUiCommandType::Galaxy};
    if (command == "system_map_current_system")
        return SystemMapUiCommand{SystemMapUiCommandType::CurrentSystem};
    if (command == "system_map_hub")
        return SystemMapUiCommand{SystemMapUiCommandType::Hub};
    if (command == "system_map_detail")
        return SystemMapUiCommand{SystemMapUiCommandType::LoadedDetail};
    if (command == "system_map_planet")
        return SystemMapUiCommand{SystemMapUiCommandType::SelectedDetail};
    if (command == "close_system_map")
        return SystemMapUiCommand{SystemMapUiCommandType::Close};

    return std::nullopt;
}
}

namespace game::ui
{
void dispatchSystemMapUiCommand(
    const SystemMapUiCommand& command,
    ISystemMapUiTarget* target,
    const std::function<void()>& closeSystemMap
)
{
    switch (command.type)
    {
        case SystemMapUiCommandType::OpenSelectedSystem:
            if (target)
            {
                target->selectSystemMapSystem(command.systemId);
                target->setSystemMapCurrentSystemMode();
            }
            return;

        case SystemMapUiCommandType::SelectSystem:
            if (target)
                target->selectSystemMapSystem(command.systemId);
            return;

        case SystemMapUiCommandType::Galaxy:
            if (target)
                target->setSystemMapGalaxyMode();
            return;

        case SystemMapUiCommandType::CurrentSystem:
            if (target)
                target->setSystemMapCurrentSystemMode();
            return;

        case SystemMapUiCommandType::Hub:
            if (target)
                target->setSystemMapHubMode();
            return;

        case SystemMapUiCommandType::LoadedDetail:
            if (target)
                target->setSystemMapLoadedDetailMode();
            return;

        case SystemMapUiCommandType::SelectedDetail:
            if (target)
                target->setSystemMapDetailMode();
            return;

        case SystemMapUiCommandType::Close:
            if (closeSystemMap)
                closeSystemMap();
            return;
    }
}
}
