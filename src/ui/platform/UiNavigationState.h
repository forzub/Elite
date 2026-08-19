#pragma once

#include <string>
#include <utility>

namespace ui::platform
{
enum class UiShellRoute
{
    MainMenuHome,
    MultiplayerAuthorization,
    SignInPassword,
    Registration,
    Recovery,
    Account
};

class UiNavigationState
{
public:
    UiShellRoute route() const noexcept { return m_route; }
    const std::string& transientMessageCode() const noexcept
    {
        return m_transientMessageCode;
    }
    const std::string& endpointDraft() const noexcept { return m_endpointDraft; }
    const std::string& accountHandleDraft() const noexcept { return m_accountHandleDraft; }

    void setConnectionDraft(std::string endpoint, std::string accountHandle)
    {
        m_endpointDraft = std::move(endpoint);
        m_accountHandleDraft = std::move(accountHandle);
    }

    void showMainMenuHome()
    {
        setRoute(UiShellRoute::MainMenuHome);
    }

    void showMultiplayerAuthorization(std::string messageCode = {})
    {
        setRoute(UiShellRoute::MultiplayerAuthorization, std::move(messageCode));
    }

    void showSignInPassword(std::string messageCode = {})
    {
        setRoute(UiShellRoute::SignInPassword, std::move(messageCode));
    }

    void showRegistration(std::string messageCode = {})
    {
        setRoute(UiShellRoute::Registration, std::move(messageCode));
    }

    void showRecovery(std::string messageCode = {})
    {
        setRoute(UiShellRoute::Recovery, std::move(messageCode));
    }

    void showAccount(std::string messageCode = {})
    {
        setRoute(UiShellRoute::Account, std::move(messageCode));
    }

    void clearTransientMessage()
    {
        m_transientMessageCode.clear();
    }

private:
    void setRoute(UiShellRoute route, std::string messageCode = {})
    {
        m_route = route;
        m_transientMessageCode = std::move(messageCode);
    }

private:
    UiShellRoute m_route = UiShellRoute::MainMenuHome;
    std::string m_transientMessageCode;
    std::string m_endpointDraft;
    std::string m_accountHandleDraft;
};
}
