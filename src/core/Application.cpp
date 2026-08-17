#include <iostream>
#include "src/core/RuntimeTrace.h"
#include <GLFW/glfw3.h>
#include "Application.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
#include <utility>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "core/log.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "src/game/ui/GameUiHotkeyPolicy.h"
#include "src/game/host/LocalGameSession.h"
#include "src/game/session/RemoteGameSession.h"
#include "src/game/session/IGameSession.h"
#include "src/game/network/NetworkEndpoint.h"
#include "src/game/identity/ClientIdentityProfile.h"
#include "src/game/navigation/CoordinateDisplayService.h"
#include "src/game/ui/SystemMapUiCommandRouter.h"
#include "input/Input.h"
#include "render/HUD/TextRenderer.h"
#include <windows.h>
#include <filesystem>
#include "ui/html/HtmlUiPanelId.h"

#include "render/ViewportUtils.h"
#include "render/Renderer.h"
#include "debug/DebugSettings.h"

namespace
{
#ifdef _WIN32
DWORD foregroundProcessIdForTrace()
{
    const HWND foreground = GetForegroundWindow();
    if (!foreground)
        return 0;

    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);
    return pid;
}

std::uint64_t xprocTraceTickMs()
{
    return static_cast<std::uint64_t>(GetTickCount64());
}

using XprocTraceClock = std::chrono::steady_clock;

double xprocElapsedMs(const XprocTraceClock::time_point& begin)
{
    return std::chrono::duration<double, std::milli>(
        XprocTraceClock::now() - begin
    ).count();
}

void traceSlowMainPhase(
    const char* phase,
    const XprocTraceClock::time_point& begin,
    double thresholdMs = 100.0)
{
    const double durationMs = xprocElapsedMs(begin);
    if (durationMs < thresholdMs)
        return;

    std::cerr
        << "[M8E-XPROC][main] pid=" << GetCurrentProcessId()
        << " phase=" << phase
        << " duration_ms=" << durationMs
        << " uptime_ms=" << xprocTraceTickMs()
        << " foreground_pid=" << foregroundProcessIdForTrace()
        << " thread=" << std::this_thread::get_id()
        << "\n";
}
#endif
}

// =====================================================================================
// Constructor
// =====================================================================================
Application::Application()
    : m_running(false)
    , m_window(nullptr)
    , m_states(m_context)
{
}


// =====================================================================================
// Destructor
// =====================================================================================
Application::~Application()
{
}

static std::string getExecutablePath()
{
    char buffer[MAX_PATH];
    GetModuleFileNameA(nullptr, buffer, MAX_PATH);

    std::string path(buffer);
    size_t pos = path.find_last_of("\\/");

    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}




static std::string findGameUiFile(const std::string& relativeFile)
{
    namespace fs = std::filesystem;

    fs::path cwd = fs::current_path();

    std::vector<fs::path> candidates =
    {
        cwd / "assets" / "webui" / relativeFile,
        cwd.parent_path() / "assets" / "webui" / relativeFile,
        cwd.parent_path() / "src" / "assets" / "webui" / relativeFile
    };

    for (const auto& p : candidates)
    {
        if (fs::exists(p))
            return p.string();
    }

    std::cout << "[App] GameUI file not found: " << relativeFile << "\n";
    return (cwd / "assets" / "webui" / relativeFile).string();
}






static std::string findGameUiRoot()
{
    namespace fs = std::filesystem;

    fs::path cwd = fs::current_path();

    std::vector<fs::path> candidates =
    {
        cwd / "assets" / "webui",
        cwd.parent_path() / "assets" / "webui",
        cwd.parent_path() / "src" / "assets" / "webui"
    };

    for (const auto& p : candidates)
    {
        if (fs::exists(p / "main_menu.html") ||
            fs::exists(p / "system_map.html"))
        {
            std::cout << "[App] GameUI root: " << p.string() << "\n";
            return p.string();
        }
    }

    std::cout << "[App] GameUI root fallback used\n";
    return (cwd / "assets" / "webui").string();
}

static std::string makeGameUiHttpUrl(
    std::uint16_t localPort,
    const std::string& relativeFile,
    const std::string& locale = std::string()
)
{
    std::string url =
        "http://localhost:" + std::to_string(localPort) + "/" + relativeFile;
    if (!locale.empty())
        url += "?locale=" + locale;
    return url;
}


static constexpr float TargetGameAspect = 16.0f / 9.0f;


static int systemMapPanelWidth(int framebufferWidth)
{
    const float w = static_cast<float>(framebufferWidth);

    // 28% экрана, но с разумными границами.
    // 1280 -> ~358
    // 1920 -> ~538
    // 2560 -> ~716, но ограничим.
    return std::clamp(
        static_cast<int>(w * 0.28f),
        360,
        640
    );
}


void Application::configureClientIdentity(
    std::string profileName,
    const game::network::SessionHello& hello
)
{
    if (!game::identity::isValidAccountHandle(profileName))
        throw std::invalid_argument("invalid client account handle");
    if (!hello.authToken.valid() || hello.accountHandle != profileName)
        throw std::invalid_argument("invalid client authentication identity");

    m_clientIdentityProfileName = std::move(profileName);
    m_clientIdentityHello = hello;
    m_clientIdentityHello.intent = game::network::AuthenticationIntent::SignIn;
}

void Application::configureClientIdentityProfileHint(std::string profileName)
{
    if (!profileName.empty() && !game::identity::isValidAccountHandle(profileName))
        throw std::invalid_argument("invalid client account-handle hint");

    m_clientIdentityProfileName = std::move(profileName);
    m_clientIdentityHello = {};
}

bool Application::prepareRemoteIdentity(
    const std::string& profileName,
    game::network::AuthenticationIntent intent,
    std::string& outError
)
{
    game::identity::ClientIdentityProfile profile;
    const bool ok = intent == game::network::AuthenticationIntent::Register
        ? game::identity::ClientIdentityProfileStore::loadOrCreate(
              profileName, profile, &outError)
        : game::identity::ClientIdentityProfileStore::loadExisting(
              profileName, profile, &outError);

    if (!ok)
        return false;

    m_clientIdentityProfileName = profile.profileName;
    m_clientIdentityHello = profile.sessionHello();
    m_clientIdentityHello.intent = intent;
    return true;
}

void Application::configureRemoteServer(
    std::string host,
    std::uint16_t port)
{
    m_remoteServerHost = std::move(host);
    m_remoteServerPort = port;
}

bool Application::hasConfiguredRemoteServer() const
{
    return !m_remoteServerHost.empty() && m_remoteServerPort != 0;
}

void Application::startRemoteGameSession()
{
    if (!hasConfiguredRemoteServer())
        throw std::runtime_error("remote server endpoint is not configured");
    if (!game::identity::isValidAccountHandle(
            m_clientIdentityHello.accountHandle) ||
        !m_clientIdentityHello.authToken.valid())
    {
        throw std::runtime_error("remote authentication identity is not prepared");
    }

    game::session::RemoteGameSessionConfig config;
    config.host = m_remoteServerHost;
    config.port = m_remoteServerPort;
    config.identityHello = m_clientIdentityHello;
    m_gameSession =
        std::make_unique<game::session::RemoteGameSession>(
            std::move(config)
        );
}

void Application::startLocalGameSession()
{
    // Local play owns a private authoritative runtime and therefore uses its
    // own local bootstrap identity. Remote credential slots are neither read
    // nor created as a side effect of starting a local game.
    m_gameSession = std::make_unique<game::host::LocalGameSession>();
}

void Application::stopGameSession()
{
    m_gameSession.reset();
}

game::session::IGameSession& Application::gameSession()
{
    if (!m_gameSession)
        throw std::runtime_error("Game session is not running");

    return *m_gameSession;
}

const game::session::IGameSession& Application::gameSession() const
{
    if (!m_gameSession)
        throw std::runtime_error("Game session is not running");

    return *m_gameSession;
}


// =====================================================================================
// run
// =====================================================================================
void Application::run()
{
    LOG("[App] init");
    init();

    LOG("[App] main loop start");
    mainLoop();

    LOG("[App] shutdown");
    shutdown();
}



// =====================================================================================
// run
// =====================================================================================
Viewport Application::viewport() const
{
    int w = 1;
    int h = 1;

    glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

    const auto lb =
        makeLetterboxedViewport(w, h, TargetGameAspect);

    return toViewport(lb);
}





// =====================================================================================
// init
// =====================================================================================
void Application::init()
{
    std::cout << "Application init\n";

#ifdef _WIN32
    const auto xprocInitTotalBegin = XprocTraceClock::now();
    auto xprocInitStageBegin = xprocInitTotalBegin;
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=begin"
            << " uptime_ms=" << xprocTraceTickMs()
            << " thread=" << std::this_thread::get_id()
            << "\n";
#endif

    m_context.app           = this;

    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    m_window = new Window(1280, 720, "EliteGame");
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=window"
            << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
            << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
            << " uptime_ms=" << xprocTraceTickMs()
            << "\n";
    xprocInitStageBegin = XprocTraceClock::now();
#endif

    // m_window  = new Window(1920, 1080, "EliteGame");




    // ---------------------------------------------------
            std::string webUiRoot = findGameUiRoot();

            // Global localization is a client-only asset domain. All editable
            // translations live under assets/localization and are discovered
            // recursively; the server/protocol continues to use stable IDs.
            {
                const std::filesystem::path webRoot(webUiRoot);
                const std::filesystem::path localizationRoot =
                    webRoot.parent_path() / "localization";
                if (!m_localization.loadDirectory(localizationRoot.string()))
                {
                    std::cerr << "[Localization] core tables incomplete; English/key fallback remains active\n";
                }

                // WebUI consumes an in-memory bundle generated from the exact
                // same LocalizationService tables as native OpenGL UI.
                m_htmlUi.setVirtualFile(
                    "/localization/runtime_ui.json",
                    m_localization.webUiBundleJson(),
                    "application/json; charset=utf-8"
                );
            }

            m_gameUiHttpPort = m_htmlUi.start(0, webUiRoot);

#ifdef _WIN32
            if (core::runtimeTraceEnabled())
                std::cerr << "[App] client process pid=" << GetCurrentProcessId()
                          << " glfw_hwnd=" << m_window->nativeWin32Handle()
                          << " webui_port=" << m_gameUiHttpPort
                          << "\n";
#endif

            int w, h;
            glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);
            m_htmlUi.setViewport(w, h);
    // ---------------------------------------------------
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=localization-htmlui"
            << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
            << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
            << " uptime_ms=" << xprocTraceTickMs()
            << "\n";
    xprocInitStageBegin = XprocTraceClock::now();
#endif


    glfwSetWindowUserPointer(m_window->nativeHandle(), this);
    glfwSetFramebufferSizeCallback(m_window->nativeHandle(), framebuffer_size_callback);

    m_running = true;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    m_renderer.init();
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=renderer"
            << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
            << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
            << " uptime_ms=" << xprocTraceTickMs()
            << "\n";
    xprocInitStageBegin = XprocTraceClock::now();
#endif


    glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

    g_stateContext          = &m_context;

    TextRenderer::instance().init();
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=text-renderer"
            << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
            << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
            << " uptime_ms=" << xprocTraceTickMs()
            << "\n";
    xprocInitStageBegin = XprocTraceClock::now();
#endif

     #ifdef _WIN32
        int uiW = 1280;
        int uiH = 720;
        glfwGetFramebufferSize(m_window->nativeHandle(), &uiW, &uiH);

        m_gameWebView.start(
            m_window->nativeWin32Handle(),
            "EliteGame UI",
            uiW,
            uiH,
            makeGameUiHttpUrl(m_gameUiHttpPort, "main_menu.html", m_localization.locale())
        );
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
                << " stage=webview"
                << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
                << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
                << " uptime_ms=" << xprocTraceTickMs()
                << "\n";
        xprocInitStageBegin = XprocTraceClock::now();
        m_gameUi.forceMode(GameUiMode::MainMenu);
        m_gameUi.clearLoaded();
        m_htmlUi.setActivePanel(HtmlUiPanelId::None);
    #endif

    m_states.push(std::make_unique<MainMenuState>(m_states));
    // m_states.push(std::make_unique<SpaceState>(m_states));

    m_states.applyPendingChanges();
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-XPROC][init] pid=" << GetCurrentProcessId()
            << " stage=main-menu-state"
            << " duration_ms=" << xprocElapsedMs(xprocInitStageBegin)
            << " total_ms=" << xprocElapsedMs(xprocInitTotalBegin)
            << " uptime_ms=" << xprocTraceTickMs()
            << "\n";
#endif

    // --connect is a lifecycle shortcut into multiplayer. It must never
    // masquerade as NEW GAME: the remote authoritative universe already
    // exists and this client only authenticates and attaches to it.
    if (hasConfiguredRemoteServer())
    {
        if (m_clientIdentityHello.authToken.valid())
        {
            std::cout << "[App] --connect shortcut: entering multiplayer endpoint="
                      << m_remoteServerHost << ':' << m_remoteServerPort << "\n";
            requestSessionStart(GameSessionLaunchKind::RemoteMultiplayer);
        }
        else
        {
            // Endpoint-only launch is not authorization. Keep the configured
            // endpoint as a UI default and require an explicit SIGN IN or
            // REGISTER before creating a remote game session.
            showMultiplayerConnectionForm();
        }
    }

}


// =====================================================================================
// mainLoop
// =====================================================================================

void Application::mainLoop()
{
    std::cout << "Application main loop start\n";
    double lastTime = glfwGetTime();


    while (m_running && !m_window->shouldClose() && !m_states.empty())
    {
        static int frame        = 0;
        double currentTime      = glfwGetTime();
        float dt                = static_cast<float>(currentTime - lastTime);

#ifdef _WIN32
        if (dt > 0.250f)
        {
            std::cerr
                << "[M8E-STARTUP][frame-gap] pid=" << GetCurrentProcessId()
                << " gap_ms=" << (static_cast<double>(dt) * 1000.0)
                << " uptime_ms=" << xprocTraceTickMs()
                << " foreground_pid=" << foregroundProcessIdForTrace()
                << " thread=" << std::this_thread::get_id()
                << "\n";
        }
#endif

        m_context.dt            = dt;
        lastTime                = currentTime;

        // Input::instance().update();
#ifdef _WIN32
        auto xprocPhaseBegin = XprocTraceClock::now();
#endif
        m_window->pollEvents();
#ifdef _WIN32
        traceSlowMainPhase("poll-events", xprocPhaseBegin);
#endif

        // A physical keyboard belongs to exactly one graphical client process.
        // Embedded WebView2 may own the child HWND focus, so GLFW_FOCUSED on
        // the parent is not a sufficient ownership test. Gate all gameplay
        // input by the foreground process instead; inactive EliteGame instances
        // immediately publish a neutral control state on their next frame.
        const bool ownsForegroundInput = m_window->ownsForegroundInput();
#ifdef _WIN32
        {
            static bool focusTraceInitialized = false;
            static bool previousOwnsForegroundInput = false;
            if (!focusTraceInitialized ||
                previousOwnsForegroundInput != ownsForegroundInput)
            {
                if (core::runtimeTraceEnabled())
                    std::cerr
                        << "[M8E-STARTUP][focus] pid=" << GetCurrentProcessId()
                        << " owns_foreground="
                        << (ownsForegroundInput ? "yes" : "no")
                        << " foreground_pid=" << foregroundProcessIdForTrace()
                        << " thread=" << std::this_thread::get_id()
                        << "\n";
                previousOwnsForegroundInput = ownsForegroundInput;
                focusTraceInitialized = true;
            }
        }
#endif
        if (ownsForegroundInput)
            Input::instance().update(m_window->nativeHandle());
        else
            Input::instance().reset();


        #ifdef _WIN32
        {
            /*
                Navigation function keys are polled at the Windows level so
                they remain deterministic even when the embedded WebView has
                keyboard focus.

                F9  = Galaxy
                F10 = current System / highest-level current sector
                F11 = current Details context
                F12 = current Hub / local spatial cube

                Ctrl+F10 is reserved for local flight-law switching and must
                reach PlayerInputMapper. Ctrl+F11 cycles coordinate display;
                Ctrl+F12 toggles gameplay constellations.
                Alt+F12 cycles the active sky culture.
                Ctrl+Alt+F12 cycles the global player-facing UI language.
            */
            const bool ctrlDown =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
            const bool altDown =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

            const bool f9Down =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
            const bool f10Down =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
            const bool f11Down =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
            const bool f12Down =
                ownsForegroundInput &&
                (GetAsyncKeyState(VK_F12) & 0x8000) != 0;

            auto* space = dynamic_cast<SpaceState*>(m_states.current());
            bool consumedNavigationHotkey = false;

            auto requestNavigationLevel =
                [&](PlayerNavigationMapLevel level, auto&& openLevel)
                {
                    // Function keys are direct selectors and same-level
                    // toggles. The result depends only on the map currently
                    // visible, never on how the player reached it.
                    const auto action = m_gameUi.navigationAction(
                        space->isPlayerNavigationMapLevel(level)
                    );

                    if (action == GameUiNavigationAction::Close)
                    {
                        closeGameUi();
                    }
                    else
                    {
                        if (!m_gameUi.isMode(GameUiMode::SystemMap))
                            openGameUi(GameUiMode::SystemMap);

                        openLevel();
                    }

                    consumedNavigationHotkey = true;
                };

            if (m_gameUi.consumeF9Press(f9Down) && space && !ctrlDown)
            {
                requestNavigationLevel(
                    PlayerNavigationMapLevel::Galaxy,
                    [&]() { space->setSystemMapGalaxyMode(); }
                );
            }

            // Ctrl+F10 belongs to PlayerInputMapper. Still latch the physical
            // F10 press here so releasing Ctrl before F10 cannot accidentally
            // turn the same chord into a plain-F10 map command.
            const bool f10PressedEdge =
                m_gameUi.consumeF10Press(f10Down);
            if (f10PressedEdge && space && !ctrlDown)
            {
                requestNavigationLevel(
                    PlayerNavigationMapLevel::System,
                    [&]() { space->setSystemMapPlayerSystemMode(); }
                );
            }

            if (m_gameUi.consumeF11Press(f11Down) && space)
            {
                if (ctrlDown)
                {
                    game::navigation::CoordinateDisplayService::instance()
                        .cycle();
                    consumedNavigationHotkey = true;
                }
                else
                {
                    requestNavigationLevel(
                        PlayerNavigationMapLevel::Detail,
                        [&]() { space->setSystemMapPlayerDetailMode(); }
                    );
                }
            }

            const bool f12PressedEdge =
                m_gameUi.consumeF12Press(f12Down);

            if (f12PressedEdge)
            {
                using game::ui::F12HotkeyAction;
                const F12HotkeyAction action =
                    game::ui::resolveF12HotkeyAction(
                        ctrlDown,
                        altDown,
                        space != nullptr
                    );

                switch (action)
                {
                    case F12HotkeyAction::CycleUiLanguage:
                        cycleUiLanguage();
                        consumedNavigationHotkey = true;
                        break;

                    case F12HotkeyAction::CycleSkyCulture:
                        space->cycleSkyCulture();
                        consumedNavigationHotkey = true;
                        break;

                    case F12HotkeyAction::ToggleConstellations:
                        space->toggleConstellationOverlay();
                        consumedNavigationHotkey = true;
                        break;

                    case F12HotkeyAction::NavigateLocal:
                        requestNavigationLevel(
                            PlayerNavigationMapLevel::Local,
                            [&]() { space->setSystemMapPlayerLocalMode(); }
                        );
                        break;

                    case F12HotkeyAction::None:
                    default:
                        break;
                }
            }

            if (consumedNavigationHotkey)
            {
                Input::instance().reset();
                m_window->swapBuffers();
                continue;
            }
        }
        #endif



        bool stateChangedFromWebView = false;

        #ifdef _WIN32
                std::string webCommand;
                while (m_gameWebView.pollCommand(webCommand))
                {
                    if (const auto mapCommand =
                            game::ui::parseSystemMapUiCommand(webCommand))
                    {
                        auto* space = dynamic_cast<SpaceState*>(
                            m_states.current()
                        );

                        game::ui::dispatchSystemMapUiCommand(
                            *mapCommand,
                            space,
                            [this]()
                            {
                                closeGameUi();
                            }
                        );

                        continue;
                    }

                    if (webCommand == "new_local_game")
                    {
                        std::cout << "[App] local new game requested\n";
                        requestSessionStart(GameSessionLaunchKind::LocalNewGame);
                        break;
                    }

                    if (webCommand == "load_local_game")
                    {
                        std::cout << "[App] load local game not implemented yet\n";
                    }

                    if (webCommand == "main_menu_ready")
                    {
                        if (m_gameUi.isMode(GameUiMode::MainMenu))
                        {
                            m_gameUi.markLoaded(GameUiMode::MainMenu);
                            applyMainMenuView();
                        }
                        continue;
                    }

                    if (webCommand == "multiplayer")
                    {
                        showMultiplayerConnectionForm();
                        continue;
                    }

                    constexpr const char* MultiplayerSignInPrefix =
                        "multiplayer_signin|";
                    constexpr const char* MultiplayerRegisterPrefix =
                        "multiplayer_register|";
                    const bool isSignIn =
                        webCommand.rfind(MultiplayerSignInPrefix, 0) == 0;
                    const bool isRegister =
                        webCommand.rfind(MultiplayerRegisterPrefix, 0) == 0;
                    if (isSignIn || isRegister)
                    {
                        const char* prefix = isRegister
                            ? MultiplayerRegisterPrefix
                            : MultiplayerSignInPrefix;
                        const std::string payload = webCommand.substr(
                            std::char_traits<char>::length(prefix)
                        );
                        const auto separator = payload.rfind('|');
                        if (separator == std::string::npos)
                        {
                            m_gameWebView.evalScript(
                                "setConnectionError('INVALID_ACCOUNT_HANDLE');"
                            );
                            continue;
                        }

                        const std::string endpointText = payload.substr(0, separator);
                        const std::string profileName = payload.substr(separator + 1);

                        game::network::NetworkEndpoint endpoint;
                        std::string endpointError;
                        if (!game::network::parseNetworkEndpoint(
                                endpointText,
                                endpoint,
                                &endpointError))
                        {
                            std::cerr << "[App] invalid multiplayer endpoint: "
                                      << endpointError << "\n";
                            m_gameWebView.evalScript(
                                "setConnectionError('INVALID_SERVER_ADDRESS');"
                            );
                            continue;
                        }

                        std::string identityError;
                        const auto intent = isRegister
                            ? game::network::AuthenticationIntent::Register
                            : game::network::AuthenticationIntent::SignIn;
                        if (!prepareRemoteIdentity(
                                profileName,
                                intent,
                                identityError))
                        {
                            m_gameWebView.evalScript(
                                "setConnectionError(" +
                                nlohmann::json(identityError).dump() +
                                ");"
                            );
                            continue;
                        }

                        configureRemoteServer(endpoint.host, endpoint.port);
                        std::cout << "[App] multiplayer "
                                  << (isRegister ? "register" : "sign-in")
                                  << " requested endpoint="
                                  << endpoint.host << ':' << endpoint.port
                                  << " credential_slot="
                                  << m_clientIdentityProfileName << "\n";
                        requestSessionStart(
                            GameSessionLaunchKind::RemoteMultiplayer
                        );
                        break;
                    }

                    if (webCommand == "multiplayer_back")
                    {
                        showMainMenu();
                        continue;
                    }

                    if (webCommand == "shipyard")
                    {
                        std::cout << "[App] shipyard not implemented yet\n";
                    }

                    if (webCommand == "exit")
                    {
                        m_states.clear();
                        m_states.applyPendingChanges();
                        m_running = false;
                        break;
                    }
                }
        #endif


#ifdef _WIN32
        xprocPhaseBegin = XprocTraceClock::now();
#endif
        updatePendingSessionStart();
#ifdef _WIN32
        traceSlowMainPhase("session-start-update", xprocPhaseBegin);
#endif

        if (m_pendingSessionLaunch != GameSessionLaunchKind::None)
        {
            /*
                The WebView loading/menu surface is a native child window and
                presents independently of the OpenGL back buffer. Swapping the
                hidden/covered GL surface here is therefore unnecessary.

                Real multi-process testing showed that a second EliteGame
                process doing heavy OpenGL startup can make SwapBuffers in
                another client wait inside the graphics driver. That creates
                an accidental cross-process serialization point and starves
                the Win32/WebView message pumps for seconds.

                While a session is connecting/synchronizing, keep this thread
                as a lightweight UI/network pump only. The next loop iteration
                still calls Window::pollEvents() and advances synchronization;
                SpaceState resumes normal OpenGL rendering once it is ready.
            */
#ifdef _WIN32
            xprocPhaseBegin = XprocTraceClock::now();
#endif
            std::this_thread::sleep_for(
                std::chrono::milliseconds(1)
            );
#ifdef _WIN32
            traceSlowMainPhase("pending-yield", xprocPhaseBegin);
#endif
            continue;
        }







        GameState* state = m_states.current();

         if (Input::instance().isKeyPressedOnce(GLFW_KEY_ESCAPE))
            {
                // 1. Если состояние само обрабатывает ESC
                if (state->onGlobalEscape())
                {
                    m_states.applyPendingChanges();
                    m_window->swapBuffers();
                    continue;
                }

                // 2. Если нет — но оно хочет confirm-exit
                if (state->wantsConfirmExit())
                {
                    // В твоей реализации это уже внутри onGlobalEscape SpaceState
                }
            }

#ifdef _WIN32
            xprocPhaseBegin = XprocTraceClock::now();
#endif
            state->prepareFrame(dt);
            state->handleInput();
            state->update(dt);
#ifdef _WIN32
            traceSlowMainPhase("state-update", xprocPhaseBegin);
            xprocPhaseBegin = XprocTraceClock::now();
#endif

        m_renderer.beginFrame();

        int fbW = 1;
        int fbH = 1;
        glfwGetFramebufferSize(m_window->nativeHandle(), &fbW, &fbH);

        const auto lb =
            makeLetterboxedViewport(fbW, fbH, TargetGameAspect);

        // Сначала очищаем весь framebuffer в чёрный.
        // Это создаёт black bars.
        glViewport(0, 0, fbW, fbH);
        glScissor(0, 0, fbW, fbH);
        glEnable(GL_SCISSOR_TEST);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT
        );

        // Теперь ограничиваем рендер игровой областью.
        glViewport(lb.x, lb.y, lb.width, lb.height);
        glScissor(lb.x, lb.y, lb.width, lb.height);

        glClearColor(0.002f, 0.006f, 0.014f, 1.0f);
        glClear(
            GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT
        );





GameState* top = m_states.current();
GameState* below = m_states.previous();

const bool systemMapMode =
    m_gameUi.isMode(GameUiMode::SystemMap);
/*
    Копируем live-настройки из debug panel в Renderer.

    Это дешёвая операция: только bool и несколько float.
    GL-ресурсы при этом не пересоздаются.
*/
{
    const auto& debugRender =
        debug::get().render;

    PostProcessSettings& post =
        m_renderer.postProcessSettings();

    post.enabled =
        debugRender.postProcessEnabled;

    post.bloomThreshold =
        debugRender.postBloomThreshold;

    post.bloomKnee =
        debugRender.postBloomKnee;

    post.bloomIntensity =
        debugRender.postBloomIntensity;

    post.softening =
        debugRender.postSoftening;

    post.saturation =
        debugRender.postSaturation;

    post.contrast =
        debugRender.postContrast;

    post.vignette =
        debugRender.postVignette;

    post.grain =
        debugRender.postGrain;

    post.haze =
        debugRender.postHaze;
}
// В режиме System Map справа находится нативная WebView-панель.
// Центрируем vignette/haze только по видимой OpenGL-части.
int postProcessViewportWidth = lb.width;

if (systemMapMode)
{
    postProcessViewportWidth =
        static_cast<int>(
            static_cast<float>(lb.width) * 0.72f
        );
}

const bool postProcessActive =
    m_renderer.beginPostProcess(
        fbW,
        fbH,
        lb.x,
        lb.y,
        postProcessViewportWidth,
        lb.height
    );

// Обычная 3D-сцена.
if (top)
{
    if (top->isModal() && below)
        below->renderUI();

    top->renderUI();
}

// В текущей архитектуре SystemMapRenderer вызывается из renderHUD().
// Поэтому в режиме карты его нужно выполнить ДО завершения post-process.
if (systemMapMode && top)
{
    top->renderHUD();
}

// Финальный cinematic composite.
if (postProcessActive)
{
    m_renderer.endPostProcess(
        static_cast<float>(currentTime)
    );
}

// В обычном игровом режиме HUD оставляем чистым и резким.
if (!systemMapMode && top)
{
    top->renderHUD();
}











        m_renderer.endFrame();
        glDisable(GL_SCISSOR_TEST);

        m_states.applyPendingChanges();
#ifdef _WIN32
        traceSlowMainPhase("render", xprocPhaseBegin);
        xprocPhaseBegin = XprocTraceClock::now();
#endif
        m_window->swapBuffers();
#ifdef _WIN32
        traceSlowMainPhase("swap-buffers", xprocPhaseBegin);
#endif
    }

    std::cerr << "[App] main loop exit pid=" << GetCurrentProcessId()
              << " running=" << (m_running ? "yes" : "no")
              << " window_should_close=" << (m_window->shouldClose() ? "yes" : "no")
              << " states_empty=" << (m_states.empty() ? "yes" : "no")
              << "\n";
}

// =====================================================================================
// shutdown
// =====================================================================================
void Application::shutdown()
{
    std::cout << "Application shutdown\n";

    m_states.clear();
    m_states.applyPendingChanges();

    #ifdef _WIN32
        m_gameWebView.stop();
    #endif


    m_htmlUi.stop();
    m_renderer.shutdown();

    delete m_window;
}


// Добавьте в начало файла или после других методов
void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // Получаем указатель на Application из GLFW window user pointer
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app)
    {
        app->handleResize(width, height);
    }
}

void Application::handleResize(int width, int height)
{
    const auto lb =
        makeLetterboxedViewport(width, height, TargetGameAspect);

    glViewport(lb.x, lb.y, lb.width, lb.height);

    GameState* currentState = m_states.current();
    if (currentState)
    {
        currentState->handleResize(lb.width, lb.height);
    }

    m_htmlUi.setViewport(lb.width, lb.height);

#ifdef _WIN32
    if (m_gameUi.isMode(GameUiMode::SystemMap))
    {
        const int panelW =
            systemMapPanelWidth(lb.width);

        m_gameWebView.setBounds(
            lb.x + std::max(0, lb.width - panelW),
            lb.y,
            panelW,
            lb.height
        );
    }
    else
    {
        m_gameWebView.setBounds(
            lb.x,
            lb.y,
            lb.width,
            lb.height
        );
    }
#endif
}





void Application::navigateGameUi(GameUiMode mode)
{
#ifndef _WIN32
    (void)mode;
    return;
#else
    switch (mode)
    {
        case GameUiMode::MainMenu:
            m_gameWebView.navigate(makeGameUiHttpUrl(m_gameUiHttpPort, "main_menu.html", m_localization.locale()));
            break;

        case GameUiMode::Loading:
            m_gameWebView.navigate(makeGameUiHttpUrl(m_gameUiHttpPort, "loading.html", m_localization.locale()));
            break;

        case GameUiMode::SystemMap:
            m_gameWebView.navigate(makeGameUiHttpUrl(m_gameUiHttpPort, "system_map_panel.html", m_localization.locale()));
            break;

        case GameUiMode::None:
        default:
            break;
    }
#endif
}




void Application::openGameUi(GameUiMode mode)
{
    if (!m_gameUi.open(mode))
        return;

    switch (mode)
    {
        case GameUiMode::SystemMap:
        {
            m_htmlUi.setActivePanel(HtmlUiPanelId::None);

#ifdef _WIN32
            int w = 1280;
            int h = 720;

            glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

            const auto lb =
                makeLetterboxedViewport(w, h, TargetGameAspect);

            const int panelW =
                systemMapPanelWidth(lb.width);

            if (!m_gameUi.isLoaded(GameUiMode::SystemMap))
            {
                navigateGameUi(GameUiMode::SystemMap);
                m_gameUi.markLoaded(GameUiMode::SystemMap);
            }

            m_gameWebView.setBounds(
                lb.x + std::max(0, lb.width - panelW),
                lb.y,
                panelW,
                lb.height
            );

            m_gameWebView.setVisible(true);
#endif
            break;
        }

        case GameUiMode::MainMenu:
        {
            m_htmlUi.setActivePanel(HtmlUiPanelId::None);

#ifdef _WIN32
            if (!m_gameUi.isLoaded(GameUiMode::MainMenu))
                navigateGameUi(GameUiMode::MainMenu);

            int w = 1280;
            int h = 720;
            glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

            const auto lb =
                makeLetterboxedViewport(w, h, TargetGameAspect);

            m_gameWebView.setBounds(lb.x, lb.y, lb.width, lb.height);
            m_gameWebView.setVisible(true);
#endif
            break;
        }

        case GameUiMode::Loading:
        {
            m_htmlUi.setActivePanel(HtmlUiPanelId::None);

#ifdef _WIN32
            if (!m_gameUi.isLoaded(GameUiMode::Loading))
            {
                navigateGameUi(GameUiMode::Loading);
                m_gameUi.markLoaded(GameUiMode::Loading);
            }

            int w = 1280;
            int h = 720;
            glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

            const auto lb =
                makeLetterboxedViewport(w, h, TargetGameAspect);

            m_gameWebView.setBounds(lb.x, lb.y, lb.width, lb.height);
            m_gameWebView.setVisible(true);
#endif
            break;
        }

        case GameUiMode::None:
        default:
            closeGameUi();
            break;
    }
}






void Application::evalGameUiScript(const std::string& script)
{
#ifdef _WIN32
    m_gameWebView.evalScript(script);
#else
    (void)script;
#endif
}







void Application::cycleUiLanguage()
{
    const std::string locale = m_localization.cycleLocale();

#ifdef _WIN32
    // C++ owns the global locale. WebUI receives the same value and stores it
    // only as a page-navigation convenience; it never owns gameplay state.
    m_gameWebView.evalScript(
        "localStorage.setItem('elite.ui.locale','" + locale + "');"
        "if (window.GameI18n) window.GameI18n.setLocale('" + locale + "');"
    );
#endif

    if (GameState* state = m_states.current())
        state->onUiLanguageChanged();

    std::cout << "[Localization] UI locale=" << locale << std::endl;
}

void Application::closeGameUi()
{
    if (!m_gameUi.close())
        return;

    m_htmlUi.setActivePanel(HtmlUiPanelId::None);

#ifdef _WIN32
    m_gameWebView.setVisible(false);
#endif
}

void Application::toggleSystemMapUi()
{
    if (m_gameUi.isMode(GameUiMode::SystemMap))
    {
        closeGameUi();
        return;
    }

    openGameUi(GameUiMode::SystemMap);
}

GameUiMode Application::gameUiMode() const
{
    return m_gameUi.mode();
}

bool Application::isGameUiOpen() const
{
    return m_gameUi.isOpen();
}














void Application::requestSessionStart(GameSessionLaunchKind kind)
{
    if (kind == GameSessionLaunchKind::None)
        return;

    if (kind == GameSessionLaunchKind::RemoteMultiplayer &&
        !hasConfiguredRemoteServer())
    {
        showMultiplayerConnectionForm();
        return;
    }

#ifdef _WIN32
    m_gameUi.forceMode(GameUiMode::Loading);
    m_gameUi.markLoaded(GameUiMode::Loading);
    m_mainMenuConnectionError.clear();
    m_htmlUi.setActivePanel(HtmlUiPanelId::None);

    m_gameWebView.setVisible(true);
    m_gameWebView.navigate(
        makeGameUiHttpUrl(
            m_gameUiHttpPort,
            "loading.html",
            m_localization.locale()
        )
    );
    m_gameWebView.evalScript(
        "setLoadingProgress(0.10, 'loading.stage.opening', 'OPENING LOADING SCREEN');"
    );
#endif

    if (core::runtimeTraceEnabled())
    {
        std::cerr << "[M8E-CONNECT][client-ui] session-start requested mode="
                  << (kind == GameSessionLaunchKind::RemoteMultiplayer
                          ? "remote"
                          : "local")
                  << " thread=" << std::this_thread::get_id();
        if (kind == GameSessionLaunchKind::RemoteMultiplayer)
            std::cerr << " endpoint=" << m_remoteServerHost << ':' << m_remoteServerPort;
        std::cerr << "\n";
    }

    m_pendingSessionLaunch = kind;
    m_sessionStartTime = glfwGetTime();
    m_sessionStartLastUpdateTime = m_sessionStartTime;
    m_sessionStartStage = SessionStartStage::WaitingForLoadingScreen;
}

void Application::showMultiplayerConnectionForm(
    const std::string& errorCode
)
{
#ifdef _WIN32
    m_mainMenuView = MainMenuView::MultiplayerAuthorization;
    m_mainMenuConnectionError = errorCode;
    m_gameUi.forceMode(GameUiMode::MainMenu);
    m_gameWebView.setVisible(true);

    if (m_gameUi.isLoaded(GameUiMode::MainMenu))
    {
        applyMainMenuView();
    }
    else
    {
        // main_menu.html is loaded asynchronously by WebView2. Do not eval
        // showMultiplayerForm() against the outgoing loading.html document.
        // The page sends main_menu_ready once its DOM and bridge are ready;
        // applyMainMenuView() then applies this pending sub-view atomically.
        navigateGameUi(GameUiMode::MainMenu);
    }
#else
    (void)errorCode;
#endif
}

void Application::showMainMenu()
{
#ifdef _WIN32
    m_mainMenuView = MainMenuView::MainActions;
    m_mainMenuConnectionError.clear();
    m_gameUi.forceMode(GameUiMode::MainMenu);
    m_gameWebView.setVisible(true);

    if (m_gameUi.isLoaded(GameUiMode::MainMenu))
        applyMainMenuView();
    else
        navigateGameUi(GameUiMode::MainMenu);
#endif
}

void Application::applyMainMenuView()
{
#ifdef _WIN32
    if (!m_gameUi.isLoaded(GameUiMode::MainMenu))
        return;

    if (m_mainMenuView == MainMenuView::MultiplayerAuthorization)
    {
        const std::string endpoint = hasConfiguredRemoteServer()
            ? m_remoteServerHost + ":" + std::to_string(m_remoteServerPort)
            : "127.0.0.1:27351";
        m_gameWebView.evalScript(
            "setMultiplayerDefaults(" +
            nlohmann::json(endpoint).dump() + "," +
            nlohmann::json(m_clientIdentityProfileName).dump() +
            ");showMultiplayerForm();setConnectionError(" +
            nlohmann::json(m_mainMenuConnectionError).dump() +
            ");"
        );
        return;
    }

    m_gameWebView.evalScript("showMainMenu();");
#endif
}

void Application::updatePendingSessionStart()
{
    if (m_pendingSessionLaunch == GameSessionLaunchKind::None)
        return;

    const double now = glfwGetTime();

    const auto finishReadySession = [this]()
    {
#ifdef _WIN32
        m_gameWebView.evalScript(
            "setLoadingProgress(1.00, 'loading.stage.ready', 'READY');"
        );
#endif

        closeGameUi();
        m_gameUi.clearLoaded();
        Input::instance().reset();
        m_window->focus();

        glfwSetInputMode(
            m_window->nativeHandle(),
            GLFW_CURSOR,
            GLFW_CURSOR_NORMAL
        );

        const bool remote =
            m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;
        m_pendingSessionLaunch = GameSessionLaunchKind::None;
        m_sessionStartStage = SessionStartStage::Idle;
        m_spaceStateBuildStartTime = 0.0;

        std::cout << "[App] "
                  << (remote ? "multiplayer world entered" : "local game loaded")
                  << ", UI hidden\n";
        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-CONNECT][client-ui] session-ready mode="
                      << (remote ? "remote" : "local")
                      << " thread=" << std::this_thread::get_id() << "\n";
    };

    if (m_sessionStartStage == SessionStartStage::BuildingSpaceState)
    {
        auto* space = dynamic_cast<SpaceState*>(m_states.current());
        if (!space)
        {
            throw std::runtime_error(
                "Deferred SpaceState startup lost the pending SpaceState"
            );
        }

        if (!space->advanceStartupInitialization())
            return;

        const double spaceStateMs =
            (glfwGetTime() - m_spaceStateBuildStartTime) * 1000.0;
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-STARTUP][client-ui] stage=space-state-apply-end"
                << " duration_ms=" << spaceStateMs
    #ifdef _WIN32
                << " pid=" << GetCurrentProcessId()
                << " foreground_pid=" << foregroundProcessIdForTrace()
    #endif
                << " thread=" << std::this_thread::get_id()
                << "\n";

        finishReadySession();
        return;
    }

    if (m_sessionStartStage == SessionStartStage::WaitingForLoadingScreen)
    {
        if ((now - m_sessionStartTime) < 0.10)
            return;

        const bool remote =
            m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;

#ifdef _WIN32
        m_gameWebView.evalScript(
            remote
                ? "setLoadingProgress(0.25, 'loading.stage.connecting', 'CONNECTING TO SERVER');"
                : "setLoadingProgress(0.25, 'loading.stage.world', 'PREPARING LOCAL WORLD');"
        );
#endif

        stopGameSession();
        if (remote)
            startRemoteGameSession();
        else
            startLocalGameSession();

        using SyncClock = std::chrono::steady_clock;
        const auto syncBegin = SyncClock::now();
        m_gameSession->beginSynchronization();
        const double syncBeginMs =
            std::chrono::duration<double, std::milli>(
                SyncClock::now() - syncBegin
            ).count();
        if (core::runtimeTraceEnabled())
            std::cerr << "[M8E-CONNECT][client-ui] beginSynchronization returned mode="
                      << (remote ? "remote" : "local")
                      << " duration_ms=" << syncBeginMs
                      << " thread=" << std::this_thread::get_id() << "\n";

        m_sessionStartLastUpdateTime = now;
        m_sessionStartStage = SessionStartStage::SynchronizingSession;
        return;
    }

    if (m_sessionStartStage != SessionStartStage::SynchronizingSession)
        return;

    const double elapsed = std::clamp(
        now - m_sessionStartLastUpdateTime,
        0.0,
        0.05
    );
    m_sessionStartLastUpdateTime = now;

    m_gameSession->updateSynchronization(elapsed);

    const auto sessionState = m_gameSession->state();
    if (sessionState == game::session::GameSessionState::WaitingForServer)
    {
#ifdef _WIN32
        m_gameWebView.evalScript(
            "setLoadingProgress(0.45, 'loading.stage.waiting_server', 'WAITING FOR SERVER');"
        );
#endif
        return;
    }

    if (sessionState == game::session::GameSessionState::Synchronizing ||
        sessionState == game::session::GameSessionState::Created)
    {
#ifdef _WIN32
        m_gameWebView.evalScript(
            "setLoadingProgress(0.55, 'loading.stage.sync', 'SYNCHRONIZING SESSION');"
        );
#endif
        return;
    }

    if (sessionState == game::session::GameSessionState::Failed)
    {
        const bool remoteLaunch =
            m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;
        const std::string sessionError = m_gameSession->error();

        std::cerr << "[App] Session synchronization failed: "
                  << sessionError << std::endl;
#ifdef _WIN32
        m_gameWebView.evalScript(
            "setLoadingProgress(1.00, 'loading.stage.failed', 'SESSION FAILED');"
        );
#endif
        stopGameSession();
        m_pendingSessionLaunch = GameSessionLaunchKind::None;
        m_sessionStartStage = SessionStartStage::Idle;
        m_spaceStateBuildStartTime = 0.0;

#ifdef _WIN32
        // Loading is a transient document. Once synchronization has failed,
        // never leave the user stranded on loading.html: force a real page
        // navigation back to the menu before invoking menu-page functions.
        // clearLoaded() is required because MainMenu may have been marked
        // loaded before loading.html replaced the WebView document.
        m_gameUi.clearLoaded();

        if (remoteLaunch)
        {
            const std::string message =
                sessionError.empty() ? "SESSION_UNAVAILABLE" : sessionError;
            showMultiplayerConnectionForm(message);
        }
        else
        {
            showMainMenu();
        }
#endif
        return;
    }

#ifdef _WIN32
    m_gameWebView.evalScript(
        "setLoadingProgress(0.80, 'loading.stage.apply', 'APPLYING GAME STATE');"
    );
#endif

    // Keep the loading/menu state alive while synchronization is pending.
    // The main loop terminates when the state stack is empty, so replace it
    // only after the session has reached Ready. Heavy OpenGL startup remains
    // on the owning render thread, but it is advanced one major stage per main
    // loop iteration. That gives Win32/WebView a message-pump turn between
    // shader, scene, map and HUD initialization instead of one 2-3 second
    // uninterrupted UI-thread stall near the end of loading.
    m_states.clear();
    m_states.applyPendingChanges();

    m_spaceStateBuildStartTime = glfwGetTime();
#ifdef _WIN32
    if (core::runtimeTraceEnabled())
        std::cerr
            << "[M8E-STARTUP][client-ui] pid=" << GetCurrentProcessId()
            << " stage=space-state-apply-begin"
            << " foreground_pid=" << foregroundProcessIdForTrace()
            << " thread=" << std::this_thread::get_id()
            << "\n";
#endif

    m_states.push(std::make_unique<SpaceState>(
        m_states,
        SpaceState::StartupMode::Deferred
    ));
    m_states.applyPendingChanges();
    m_sessionStartStage = SessionStartStage::BuildingSpaceState;
}
