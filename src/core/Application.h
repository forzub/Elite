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
#include "src/ui/presentation/GamePresentationCoordinator.h"
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

enum class GameSessionLaunchKind
{
    None,
    LocalNewGame,
    RemoteMultiplayer
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
    void requestPresentationTarget(GameUiTarget target);
    void requestFlightView(FlightPresentationView view);
    void requestLastFlightView();
    void requestNavigationView(NavigationPresentationView view);
    void adoptNavigationView(NavigationPresentationView view);
    void requestServicePanel(ui::services::ServiceUiId service);
    void prepareRequestedPresentation(SpaceState& space);
    void commitPreparedPresentationAfterSwap(SpaceState& space);

    // Committed presentation visible to the user. Never exposes a staged scene.
    GameUiMode gameUiMode() const;
    GameUiTarget gameUiTarget() const;
    // Internal OpenGL scene currently allowed to render behind an opaque/staging surface.
    GameUiMode sceneGameUiMode() const;
    GameUiTarget sceneGameUiTarget() const;
    bool isGameUiOpen() const;

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
    void prepareFullScreenDocument(GameUiTarget target);
    void navigateDocumentSurface(int surfaceIndex, GameUiTarget target);
    void commitFullScreenDocument(int surfaceIndex, GameUiTarget target);
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
    void updateGameUiPresentation();
    void syncDocumentWebViewBounds();
    void processDocumentWebViewCommands(int surfaceIndex);
    void setLocalSessionMenuPause(bool paused);
    bool localSessionMenuPaused() const;
    void requestApplicationQuit();
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
    static void window_size_callback(GLFWwindow* window, int width, int height);
    void handleFramebufferResize(int width, int height);
    void handleWindowResize();

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

    double m_loadingUiProgress = 0.05;
    std::string m_loadingUiStageKey = "loading.boot";
    std::string m_loadingUiEnglishFallback = "BOOT";

    GamePresentationCoordinator m_gameUi;
    GameUiTarget m_sessionResumeTarget =
        GameUiTarget::forFlight(FlightPresentationView::Front);
    GameUiTarget m_lastFlightTarget =
        GameUiTarget::forFlight(FlightPresentationView::Front);
    bool m_localSessionPausedByMenu = false;
    std::function<void()> m_afterPresentationCommit;

    #ifdef _WIN32
        GameWebView m_documentWebViews[2];
        int m_activeDocumentSurface = 0;
        int m_stagingDocumentSurface = 1;
    #endif
};
