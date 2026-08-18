#pragma once

#include <string>
#include <utility>

namespace ui::platform
{
enum class UiShellRoute
{
    MainMenuHome,
    MultiplayerAuthorization
};

class UiNavigationState
{
public:
    UiShellRoute route() const noexcept { return m_route; }
    const std::string& transientMessageCode() const noexcept
    {
        return m_transientMessageCode;
    }

    void showMainMenuHome()
    {
        m_route = UiShellRoute::MainMenuHome;
        m_transientMessageCode.clear();
    }

    void showMultiplayerAuthorization(std::string messageCode = {})
    {
        m_route = UiShellRoute::MultiplayerAuthorization;
        m_transientMessageCode = std::move(messageCode);
    }

    void clearTransientMessage()
    {
        m_transientMessageCode.clear();
    }

private:
    UiShellRoute m_route = UiShellRoute::MainMenuHome;
    std::string m_transientMessageCode;
};
}
