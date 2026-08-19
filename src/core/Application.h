#pragma once

#include "StateContext.h"
#include "StateStack.h"
#include "window/Window.h"
#include "render/Renderer.h"
#include "render/RenderContext.h"
#include "ui/html/HtmlUiManager.h"
#include "game/localization/LocalizationService.h"
#include "game/network/SessionMessage.h"
#include "src/ui/platform/ClientPreferencesStore.h"
#include "src/ui/platform/UiNavigationState.h"
#include <string>
#include <memory>
#include <cstdint>
#include <functional>

#ifdef _WIN32
#include "ui/browser/GameWebView.h"
#endif

class SpaceState;

namespace game::session
{
class IGameSession;
}

enum class GameUiMode
{
    None,
    MainMenu,
    Loading,
    SystemMap,
    SessionMenu
};

enum class GameSessionLaunchKind
{
    None,
    LocalNewGame,
    RemoteMultiplayer
};

enum class GameUiNavigationAction
{
    OpenOrSwitch,
    Close
};

class GameUiController
{
public:
    GameUiMode mode() const
    {
        return m_mode;
    }



    GameUiMode loadedMode() const
    {
        return m_loadedMode;
    }

    bool isLoaded(GameUiMode mode) const
    {
        return m_loadedMode == mode;
    }

    void markLoaded(GameUiMode mode)
    {
        m_loadedMode = mode;
    }

    void clearLoaded()
    {
        m_loadedMode = GameUiMode::None;
        m_preparedMode = GameUiMode::None;
    }

    bool isPrepared(GameUiMode mode) const
    {
        return m_preparedMode == mode;
    }

    void markPrepared(GameUiMode mode)
    {
        m_preparedMode = mode;
    }

    void clearPrepared()
    {
        m_preparedMode = GameUiMode::None;
    }

    bool isOpen() const
    {
        return m_mode != GameUiMode::None;
    }

    bool isMode(GameUiMode mode) const
    {
        return m_mode == mode;
    }

    void forceMode(GameUiMode mode)
    {
        m_mode = mode;
    }

    bool open(GameUiMode mode)
    {
        if (mode == GameUiMode::None)
            return close();

        if (m_mode == mode)
            return false;

        m_mode = mode;
        return true;
    }

    bool close()
    {
        if (m_mode == GameUiMode::None)
            return false;

        m_mode = GameUiMode::None;
        return true;
    }

    bool toggle(GameUiMode mode)
    {
        if (m_mode == mode)
            return close();

        return open(mode);
    }

    GameUiNavigationAction navigationAction(
        bool requestedLevelIsCurrentlyVisible
    ) const
    {
        return
            isMode(GameUiMode::SystemMap) &&
            requestedLevelIsCurrentlyVisible
                ? GameUiNavigationAction::Close
                : GameUiNavigationAction::OpenOrSwitch;
    }

    bool consumeF9Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f9Latch = false;
            return false;
        }

        if (m_f9Latch)
            return false;

        m_f9Latch = true;
        return true;
    }

    bool consumeF11Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f11Latch = false;
            return false;
        }

        if (m_f11Latch)
            return false;

        m_f11Latch = true;
        return true;
    }

    bool consumeF10Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f10Latch = false;
            return false;
        }

        if (m_f10Latch)
            return false;

        m_f10Latch = true;
        return true;
    }

    bool consumeF12Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f12Latch = false;
            return false;
        }

        if (m_f12Latch)
            return false;

        m_f12Latch = true;
        return true;
    }

private:
    GameUiMode m_mode = GameUiMode::None;
    GameUiMode m_loadedMode = GameUiMode::None;
    GameUiMode m_preparedMode = GameUiMode::None;

    bool m_f9Latch = false;
    bool m_f10Latch = false;
    bool m_f11Latch = false;
    bool m_f12Latch = false;
};



class Application
{
public:
    Application();
    ~Application();

    void run();
    Viewport viewport() const;

    HtmlUiManager& htmlUi() { return m_htmlUi; }
    const HtmlUiManager& htmlUi() const { return m_htmlUi; }

    game::localization::LocalizationService& localization() { return m_localization; }
    const game::localization::LocalizationService& localization() const { return m_localization; }
    void cycleUiLanguage();

    void updatePendingSessionStart();
    void openGameUi(GameUiMode mode);
    void closeGameUi();
    void requestSystemMapClose();
    void invalidatePreparedSystemMapUi();
    void prepareSystemMapUiForEntry(SpaceState& space);
    void prewarmSystemMapPanel();
    void presentPreparedGameUi(GameUiMode mode);

    GameUiMode gameUiMode() const;
    bool isGameUiOpen() const;

    void evalGameUiScript(const std::string& script);

    void configureClientIdentity(
        std::string profileName,
        const game::network::SessionHello& hello
    );
    void configureClientIdentityProfileHint(std::string profileName);
    void configureRemoteServer(std::string host, std::uint16_t port);
    bool hasConfiguredRemoteServer() const;
    void startLocalGameSession();
    void startRemoteGameSession();
    void stopGameSession();
    game::session::IGameSession& gameSession();
    const game::session::IGameSession& gameSession() const;

private:
    void init();
    void mainLoop();
    void shutdown();
    void navigateGameUi(GameUiMode mode);
    void requestSessionStart(GameSessionLaunchKind kind);
    void startSessionNow(GameSessionLaunchKind kind);
    void showMultiplayerConnectionForm(
        const std::string& errorCode = std::string()
    );
    void showRegistrationForm(
        const std::string& errorCode = std::string()
    );
    void showPasswordSignInForm(
        const std::string& errorCode = std::string()
    );
    void showMainMenu();
    void applyMainMenuView();
    void showSessionMenu();
    void resumeSessionFromMenu();
    void applySessionMenuView();
    void returnSessionToMainMenu();
    void cancelPendingSessionStart();
    void setUiLanguage(const std::string& locale);
    void beginServiceUiTransition(std::function<void()> completion);
    void completeServiceUiTransition(std::uint64_t serial);
    void updateServiceUiTransition();
    bool serviceUiTransitionPending() const;
    void setLoadingUiProgress(
        double progress,
        std::string stageKey,
        std::string englishFallback
    );
    bool prepareRemoteIdentity(
        const std::string& profileName,
        game::network::AuthenticationIntent intent,
        std::string& outError
    );

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void handleResize(int width, int height);

    bool m_running;
    Renderer m_renderer;
    Window* m_window;
    StateContext m_context;
    std::unique_ptr<game::session::IGameSession> m_gameSession;
    std::string m_remoteServerHost;
    std::uint16_t m_remoteServerPort = 0;
    std::uint16_t m_gameUiHttpPort = 0;
    std::string m_clientIdentityProfileName;
    game::network::SessionHello m_clientIdentityHello {};
    std::string m_localPlayerDisplayName;
    std::string m_authenticatedRemoteAccountHandle;
    std::string m_authenticatedRemoteEndpoint;
    ui::platform::UiNavigationState m_uiNavigationState;
    ui::platform::ClientPreferences m_clientPreferences;
    StateStack   m_states;
    RenderContext renderContext;
    HtmlUiManager m_htmlUi;
    game::localization::LocalizationService m_localization;

    enum class SessionStartStage
    {
        Idle,
        WaitingForLoadingScreen,
        SynchronizingSession,
        BuildingSpaceState
    };

    GameSessionLaunchKind m_pendingSessionLaunch = GameSessionLaunchKind::None;
    GameSessionLaunchKind m_activeSessionKind = GameSessionLaunchKind::None;
    double m_sessionStartTime = 0.0;
    double m_sessionStartLastUpdateTime = 0.0;
    double m_spaceStateBuildStartTime = 0.0;
    SessionStartStage m_sessionStartStage = SessionStartStage::Idle;

    std::function<void()> m_serviceUiTransitionCompletion;
    std::uint64_t m_serviceUiTransitionSerial = 0;
    std::uint64_t m_nextServiceUiTransitionSerial = 1;
    double m_serviceUiTransitionFailSafeDeadline = 0.0;

    double m_loadingUiProgress = 0.05;
    std::string m_loadingUiStageKey = "loading.boot";
    std::string m_loadingUiEnglishFallback = "BOOT";

    GameUiController m_gameUi;

    #ifdef _WIN32
        GameWebView m_gameWebView;
        bool m_systemMapPanelPrepared = false;
        bool m_systemMapPanelNavigationPending = false;
        bool m_systemMapPanelStateRequested = false;
        bool m_systemMapPanelPrewarmPending = false;
    #endif
};
