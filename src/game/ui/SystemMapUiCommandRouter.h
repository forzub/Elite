#pragma once

#include <functional>
#include <optional>
#include <string_view>

namespace game::ui
{
enum class SystemMapUiCommandType
{
    OpenSelectedSystem,
    SelectSystem,
    Galaxy,
    CurrentSystem,
    Hub,
    LoadedDetail,
    SelectedDetail,
    Close
};

struct SystemMapUiCommand
{
    SystemMapUiCommandType type = SystemMapUiCommandType::Galaxy;
    int systemId = -1;
};

class ISystemMapUiTarget
{
public:
    virtual ~ISystemMapUiTarget() = default;

    virtual void selectSystemMapSystem(int systemId) = 0;
    virtual void setSystemMapGalaxyMode() = 0;
    virtual void setSystemMapCurrentSystemMode() = 0;
    virtual void setSystemMapHubMode() = 0;
    virtual void setSystemMapLoadedDetailMode() = 0;
    virtual void setSystemMapDetailMode() = 0;
};

std::optional<SystemMapUiCommand> parseSystemMapUiCommand(
    std::string_view command
);

// This is the production command-to-action seam used by Application and by
// headless acceptance tests. Browser transport and visual styling stay outside
// this function; the functional meaning of each command lives here.
void dispatchSystemMapUiCommand(
    const SystemMapUiCommand& command,
    ISystemMapUiTarget* target,
    const std::function<void()>& closeSystemMap
);
}
