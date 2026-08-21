#include <iostream>
#include "src/core/RuntimeTrace.h"
#include <GLFW/glfw3.h>
#include "Application.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <thread>
#include <utility>
#include <string_view>
#include <stdexcept>
#include <nlohmann/json.hpp>

#include "core/log.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "src/game/ui/GameUiHotkeyPolicy.h"
#include "src/ui/presentation/PresentationFunctionKeyRouter.h"
#include "src/game/host/LocalGameSession.h"
#include "src/game/session/RemoteGameSession.h"
#include "src/game/session/IGameSession.h"
#include "src/game/network/NetworkEndpoint.h"
#include "src/game/identity/ClientIdentityProfile.h"
#include "src/game/navigation/CoordinateDisplayService.h"
#include "input/Input.h"
#include "render/HUD/TextRenderer.h"
#include <windows.h>
#include <dwmapi.h>
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

bool flushDesktopCompositor()
{
    // glfwSwapBuffers()/ShowWindow()/SetWindowPos submit work to surfaces that
    // DWM composes asynchronously. A logical presentation commit must not
    // retire the old child HWND until the new GL/WebView generation has reached
    // the desktop compositor.
    const HRESULT hr = DwmFlush();
    if (FAILED(hr))
    {
        std::cerr << "[Presentation] DwmFlush failed hr=0x"
                  << std::hex << static_cast<unsigned long>(hr)
                  << std::dec << "\n";
        return false;
    }
    return true;
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

int hexDigitValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool decodeWebComponent(std::string_view encoded, std::string& out)
{
    out.clear();
    out.reserve(encoded.size());

    for (std::size_t i = 0; i < encoded.size(); ++i)
    {
        const char c = encoded[i];
        if (c != '%')
        {
            out.push_back(c);
            continue;
        }

        if (i + 2 >= encoded.size())
            return false;

        const int hi = hexDigitValue(encoded[i + 1]);
        const int lo = hexDigitValue(encoded[i + 2]);
        if (hi < 0 || lo < 0)
            return false;

        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
    }

    return true;
}

bool parsePresentationSerialCommand(
    const std::string& command,
    std::string_view prefix,
    std::uint64_t& outSerial)
{
    if (command.size() < prefix.size() ||
        command.compare(0, prefix.size(), prefix) != 0)
    {
        return false;
    }

    const std::string payload = command.substr(prefix.size());
    if (payload.empty())
        return false;

    try
    {
        std::size_t consumed = 0;
        const unsigned long long parsed = std::stoull(payload, &consumed);
        if (consumed != payload.size() || parsed == 0)
            return false;
        outSerial = static_cast<std::uint64_t>(parsed);
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}


bool normalizeLocalPlayerDisplayName(std::string& value)
{
    const auto isTrimByte = [](unsigned char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    std::size_t first = 0;
    while (first < value.size() && isTrimByte(static_cast<unsigned char>(value[first])))
        ++first;

    std::size_t last = value.size();
    while (last > first && isTrimByte(static_cast<unsigned char>(value[last - 1])))
        --last;

    value = value.substr(first, last - first);
    if (value.empty() || value.size() > 192u)
        return false;

    for (const unsigned char c : value)
    {
        if (c < 0x20u || c == 0x7fu)
            return false;
    }

    return true;
}
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

void startDebugUiCompatibilityRedirect(
    std::uint16_t processLocalPort,
    const std::string& webUiRoot
)
{
    constexpr std::uint16_t CompatibilityPort = 8090;

    // The real WebView/WS endpoint remains process-local and ephemeral. This
    // best-effort launcher only restores the old human-facing debug bookmark.
    // The first graphical client that can bind 8090 redirects the browser to
    // its real endpoint; later clients keep their isolated ephemeral servers.
    if (processLocalPort == 0 || processLocalPort == CompatibilityPort)
        return;

    static bool attempted = false;
    static HtmlUiServer redirectServer;
    if (attempted)
        return;
    attempted = true;

    const std::string redirectHtml =
        "<!doctype html><meta charset=\"utf-8\">"
        "<title>Elite debug redirect</title>"
        "<script>"
        "location.replace(location.protocol+'//'+location.hostname+':" +
        std::to_string(processLocalPort) +
        "'+location.pathname+location.search+location.hash);"
        "</script>"
        "<p>Redirecting to the active Elite debug endpoint...</p>";

    for (const char* resource : {
        "/debug_control.html",
        "/attachment_editor.html",
        "/volume_viewer.html",
        "/frustum_debug.html",
        "/ship_core.html",
        "/structure_debug.html",
        "/system_map.html"
    })
    {
        redirectServer.setVirtualFile(
            resource,
            redirectHtml,
            "text/html; charset=utf-8"
        );
    }

    try
    {
        redirectServer.start(CompatibilityPort, webUiRoot);
        std::cout << "[App] debug compatibility URL: "
                  << "http://localhost:"
                  << CompatibilityPort
                  << "/debug_control.html"
                  << " -> process-local http://localhost:"
                  << processLocalPort
                  << "/debug_control.html\n";
    }
    catch (const std::exception& e)
    {
        // Port 8090 may already belong to another EliteGame instance. That is
        // expected in multi-process acceptance; never disturb the real server.
        std::cout << "[App] debug compatibility port "
                  << CompatibilityPort
                  << " unavailable: "
                  << e.what()
                  << "\n";
    }
}

static std::string makeGameUiHttpUrl(
    std::uint16_t localPort,
    const std::string& relativeFile,
    const std::string& locale = std::string(),
    std::uint64_t presentationSerial = 0
)
{
    std::string url =
        "http://localhost:" + std::to_string(localPort) + "/" + relativeFile;
    char separator = '?';
    if (!locale.empty())
    {
        url += separator;
        url += "locale=" + locale;
        separator = '&';
    }
    if (presentationSerial != 0)
    {
        url += separator;
        url += "presentation=" + std::to_string(presentationSerial);
    }
    return url;
}


static constexpr float TargetGameAspect = 16.0f / 9.0f;


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
    m_activeSessionKind = GameSessionLaunchKind::None;
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

    {
        std::string preferencesError;
        if (!ui::platform::ClientPreferencesStore::load(
                m_clientPreferences,
                &preferencesError))
        {
            std::cerr << "[ClientPreferences] ignored invalid preferences: "
                      << preferencesError << "\n";
            m_clientPreferences = {};
        }
    }

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

                if (!m_clientPreferences.preferredLocale.empty() &&
                    !m_localization.setLocale(m_clientPreferences.preferredLocale))
                {
                    std::cerr << "[ClientPreferences] preferred locale is not enabled: "
                              << m_clientPreferences.preferredLocale << "\n";
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
            std::cout << "[App] debug UI: http://localhost:"
                      << m_gameUiHttpPort
                      << "/debug_control.html\n";
            startDebugUiCompatibilityRedirect(
                m_gameUiHttpPort,
                webUiRoot
            );

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
    glfwSetWindowSizeCallback(m_window->nativeHandle(), window_size_callback);

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
        m_window->clientSize(uiW, uiH);

        const GameUiTarget initialUi = GameUiTarget::forMode(GameUiMode::MainMenu);
        m_gameUi.requestTarget(initialUi);
        const std::uint64_t initialNavigationSerial =
            m_gameUi.beginDocumentPreparation(0, initialUi);

        m_documentWebViews[0].start(
            m_window->nativeWin32Handle(),
            "EliteGame Document UI A",
            uiW,
            uiH,
            makeGameUiHttpUrl(
                m_gameUiHttpPort,
                "main_menu.html",
                m_localization.locale(),
                initialNavigationSerial)
        );
        m_documentWebViews[1].start(
            m_window->nativeWin32Handle(),
            "EliteGame Document UI B",
            uiW,
            uiH,
            makeGameUiHttpUrl(
                m_gameUiHttpPort,
                "presentation_blank.html",
                m_localization.locale(),
                0)
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
        m_htmlUi.setActivePanel(HtmlUiPanelId::None);
    #endif

    m_states.push(std::make_unique<MainMenuState>(m_states));
    // m_states.push(std::make_unique<SpaceState>(m_states));

    m_states.applyPendingChanges();

    // The GLFW HWND was created hidden. Present one fully defined dark
    // framebuffer before exposing it so Windows never composites the default
    // unpainted window background. The WebView child remains hidden until its
    // document-specific prepared handshake completes.
    {
        int bootstrapW = 1;
        int bootstrapH = 1;
        glfwGetFramebufferSize(m_window->nativeHandle(), &bootstrapW, &bootstrapH);
        glViewport(0, 0, bootstrapW, bootstrapH);
        glClearColor(0.002f, 0.006f, 0.014f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        m_window->swapBuffers();
        m_window->show();
    }
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

        updateGameUiPresentation();
        if (!m_running)
            break;


        #ifdef _WIN32
        {
            // F1-F12 edges are captured by Window::pollEvents from the Win32
            // message queue with a GetAsyncKeyState pressed-since-last-poll
            // fallback. Presentation code consumes events; it never polls
            // physical key levels itself. This survives short taps across a
            // temporarily slow WebView/compositor frame.
            auto* space = dynamic_cast<SpaceState*>(m_states.current());
            const bool sessionReady =
                space && m_activeSessionKind != GameSessionLaunchKind::None;

            Window::FunctionKeyPress press;
            while (m_window->pollFunctionKeyPress(press))
            {
                const int functionKey = press.functionKey;
                const bool ctrlDown = press.ctrlDown;
                const bool altDown = press.altDown;

                if (sessionReady && !ctrlDown && !altDown)
                {
                    if (const auto target =
                            ui::presentation::directTargetForFunctionKey(functionKey))
                    {
                        requestPresentationTarget(*target);
                    }
                }

                if (sessionReady && functionKey == 11 && ctrlDown && !altDown)
                {
                    game::navigation::CoordinateDisplayService::instance().cycle();
                }

                if (functionKey == 12)
                {
                    using game::ui::F12HotkeyAction;
                    const F12HotkeyAction action =
                        game::ui::resolveF12HotkeyAction(ctrlDown, altDown, sessionReady);
                    switch (action)
                    {
                        case F12HotkeyAction::CycleUiLanguage:
                            cycleUiLanguage();
                            break;
                        case F12HotkeyAction::CycleSkyCulture:
                            if (space) space->cycleSkyCulture();
                            break;
                        case F12HotkeyAction::ToggleConstellations:
                            if (space) space->toggleConstellationOverlay();
                            break;
                        case F12HotkeyAction::NavigateLocal:
                        case F12HotkeyAction::None:
                        default:
                            break;
                    }
                }
            }
        }
        #endif



        #ifdef _WIN32
        processDocumentWebViewCommands(0);
        processDocumentWebViewCommands(1);
        #endif

        if (!m_running)
            break;


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

#ifdef _WIN32
        const bool escapePressed = m_window->consumeEscapePressed();
#else
        const bool escapePressed = Input::instance().isKeyPressedOnce(GLFW_KEY_ESCAPE);
#endif
        if (escapePressed)
        {
            if (dynamic_cast<SpaceState*>(state) != nullptr &&
                m_activeSessionKind != GameSessionLaunchKind::None)
            {
                if (m_gameUi.committedTarget().mode == GameUiMode::SessionMenu ||
                    m_gameUi.requestedTarget().mode == GameUiMode::SessionMenu)
                {
                    resumeSessionFromMenu();
                }
                else
                {
                    showSessionMenu();
                }

                Input::instance().reset();
                continue;
            }

            if (state && state->onGlobalEscape())
            {
                m_states.applyPendingChanges();
                Input::instance().reset();
                continue;
            }
        }

#ifdef _WIN32
            xprocPhaseBegin = XprocTraceClock::now();
#endif
            const bool localPaused = localSessionMenuPaused();
            state->prepareFrame(localPaused ? 0.0f : dt);

            const GameUiMode visibleMode = m_gameUi.committedTarget().mode;
            const bool presentationConsumesGameplayInput =
                visibleMode == GameUiMode::ServicePanel ||
                visibleMode == GameUiMode::SessionMenu;
            if (localPaused)
            {
                Input::instance().reset();
            }
            else if (presentationConsumesGameplayInput)
            {
                // Service/session overlays consume manual controls, but the
                // simulation keeps running. Feed one neutral input sample per
                // frame so a multiplayer server never retains thrust/rotation
                // from the last flight frame merely because WebView owns focus.
                Input::instance().reset();
                state->handleInput();
            }
            else
            {
                state->handleInput();
            }

            // ESC in Local freezes the authoritative embedded session because
            // SpaceState::update owns LocalGameSession::advance(). Multiplayer
            // never takes this branch: its transport/world keeps advancing
            // behind the menu. F1-F12 do not set the pause flag.
            if (!localPaused)
                state->update(dt);

            if (auto* activeSpace = dynamic_cast<SpaceState*>(state))
                prepareRequestedPresentation(*activeSpace);

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

const GameUiMode sceneMode = sceneGameUiMode();
const bool flightMode = sceneMode == GameUiMode::Flight;
const bool systemMapMode = sceneMode == GameUiMode::SystemMap;
const bool serviceMode = sceneMode == GameUiMode::ServicePanel;
/*
    Копируем live-настройки из debug panel в Renderer.
*/
{
    const auto& debugRender = debug::get().render;
    PostProcessSettings& post = m_renderer.postProcessSettings();
    post.enabled = debugRender.postProcessEnabled;
    post.bloomThreshold = debugRender.postBloomThreshold;
    post.bloomKnee = debugRender.postBloomKnee;
    post.bloomIntensity = debugRender.postBloomIntensity;
    post.softening = debugRender.postSoftening;
    post.saturation = debugRender.postSaturation;
    post.contrast = debugRender.postContrast;
    post.vignette = debugRender.postVignette;
    post.grain = debugRender.postGrain;
    post.haze = debugRender.postHaze;
}

// Only 3D scene domains use the cinematic off-screen chain. F5-F8 are native
// 2D presentation and draw directly to the already-cleared default framebuffer.
int postProcessViewportWidth = lb.width;
if (systemMapMode)
{
    postProcessViewportWidth = static_cast<int>(
        static_cast<float>(lb.width) * 0.72f);
}

const bool scene3dActive = flightMode || systemMapMode;
const bool postProcessActive =
    scene3dActive &&
    m_renderer.beginPostProcess(
        fbW, fbH, lb.x, lb.y,
        postProcessViewportWidth, lb.height);

// Flight is the only domain that renders game cameras through renderUI().
if (top && flightMode)
{
    if (top->isModal() && below)
        below->renderUI();
    top->renderUI();
}

// Navigation scene is rendered before cinematic composition; its native STAR
// ATLAS side panel is intentionally rendered later as crisp 2D UI.
if (systemMapMode && top)
    top->renderHUD();

if (postProcessActive)
    m_renderer.endPostProcess(static_cast<float>(currentTime));

if (flightMode && top)
    top->renderHUD();

if ((systemMapMode || serviceMode))
{
    if (auto* activeSpace = dynamic_cast<SpaceState*>(top))
        activeSpace->renderInSessionPresentationOverlay();
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
        if (auto* activeSpace = dynamic_cast<SpaceState*>(m_states.current()))
            commitPreparedPresentationAfterSwap(*activeSpace);
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

    // Terminal presentation boundary comes first. Destroying the WebView child
    // while the GLFW top-level remains visible uncovers the last gameplay back
    // buffer for one DWM frame (the observed quit flash).
    if (m_window)
        m_window->hide();
#ifdef _WIN32
    for (auto& surface : m_documentWebViews)
        surface.setVisible(false);
#endif

    stopGameSession();
    m_states.clear();
    m_states.applyPendingChanges();

    #ifdef _WIN32
        for (auto& surface : m_documentWebViews)
            surface.stop();
    #endif


    m_htmlUi.stop();
    m_renderer.shutdown();

    delete m_window;
}


// GLFW framebuffer pixels belong to OpenGL only. Native child HWND geometry
// is synchronized from the parent client rect by window_size_callback below.
void Application::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app)
        app->handleFramebufferResize(width, height);
}

void Application::window_size_callback(GLFWwindow* window, int, int)
{
    Application* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (app)
        app->handleWindowResize();
}

void Application::handleFramebufferResize(int width, int height)
{
    const auto lb = makeLetterboxedViewport(width, height, TargetGameAspect);

    glViewport(lb.x, lb.y, lb.width, lb.height);

    if (GameState* currentState = m_states.current())
        currentState->handleResize(lb.width, lb.height);

    m_htmlUi.setViewport(lb.width, lb.height);
}

void Application::handleWindowResize()
{
#ifdef _WIN32
    // A GLFW top-level resize and a WebView2 child resize are two independent
    // compositor operations. Never expose their intermediate geometries.
    // Hide the already-prepared child during the live resize, keep updating
    // its native client-area bounds, and restore it only after geometry settles.
    m_gameUi.noteGeometryChange(glfwGetTime());

    for (auto& surface : m_documentWebViews)
        surface.setVisible(false);
    syncDocumentWebViewBounds();
#endif
}

void Application::syncDocumentWebViewBounds()
{
#ifdef _WIN32
    if (!m_window)
        return;

    int clientW = 1;
    int clientH = 1;
    m_window->clientSize(clientW, clientH);
    for (auto& webView : m_documentWebViews)
        webView.setBounds(0, 0, clientW, clientH);
#endif
}


void Application::updateGameUiPresentation()
{
#ifdef _WIN32
    if (!m_gameUi.consumeSettledGeometryChange(glfwGetTime()))
        return;

    syncDocumentWebViewBounds();

    const GameUiTarget committed = m_gameUi.committedTarget();
    if (committed.isFullScreenWebDocument())
    {
        auto& active = m_documentWebViews[m_activeDocumentSurface];
        active.setVisible(true);
        active.bringToFront();
        active.evalScript(
            "if(window.EliteUiKit&&window.EliteUiKit.revealPreparedDocument)"
            "window.EliteUiKit.revealPreparedDocument();"
        );
    }
#endif
}

void Application::navigateDocumentSurface(int surfaceIndex, GameUiTarget target)
{
#ifndef _WIN32
    (void)surfaceIndex;
    (void)target;
#else
    if (surfaceIndex < 0 || surfaceIndex > 1 || !target.isFullScreenWebDocument())
        return;

    std::string document;
    switch (target.mode)
    {
        case GameUiMode::MainMenu:
            document = "main_menu.html";
            break;
        case GameUiMode::Loading:
            document = "loading.html";
            break;
        case GameUiMode::SessionMenu:
            document =
                m_activeSessionKind == GameSessionLaunchKind::LocalNewGame
                    ? "local_session_menu.html"
                    : "multiplayer_session_menu.html";
            break;
        default:
            return;
    }

    const std::uint64_t serial =
        m_gameUi.beginDocumentPreparation(surfaceIndex, target);

    syncDocumentWebViewBounds();
    auto& surface = m_documentWebViews[surfaceIndex];
    surface.setVisible(false);

    std::string url = makeGameUiHttpUrl(
        m_gameUiHttpPort,
        document,
        m_localization.locale(),
        serial);
    if (target.mode == GameUiMode::Loading)
    {
        url += m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer
            ? "&session=remote"
            : "&session=local";
    }
    surface.navigate(url);
#endif
}

void Application::prepareFullScreenDocument(GameUiTarget target)
{
#ifdef _WIN32
    if (!target.isFullScreenWebDocument())
        return;

    // Browser surfaces are reserved for non-game-session documents only:
    // Main Menu, Loading and ESC Session Menu. F1-F12 never navigate WebView.
    const int staging = m_activeDocumentSurface == 0 ? 1 : 0;
    m_stagingDocumentSurface = staging;
    navigateDocumentSurface(staging, target);
#else
    (void)target;
#endif
}

void Application::commitFullScreenDocument(int surfaceIndex, GameUiTarget target)
{
#ifdef _WIN32
    if (surfaceIndex < 0 || surfaceIndex > 1 ||
        m_gameUi.requestedTarget() != target)
    {
        return;
    }

    const auto& state = m_gameUi.documentSurface(surfaceIndex);
    if (state.target != target || !state.prepared)
        return;

    syncDocumentWebViewBounds();
    auto& incoming = m_documentWebViews[surfaceIndex];

    // Reveal the prepared child before retiring the previous owner. Because
    // both are real sibling HWNDs, this is a presentation double buffer rather
    // than a navigate-then-pray sequence.
    incoming.setVisible(true);
    incoming.bringToFront();
    incoming.evalScript(
        "if(window.EliteUiKit&&window.EliteUiKit.revealPreparedDocument)"
        "window.EliteUiKit.revealPreparedDocument();"
    );

    // ShowWindow/SetWindowPos and WebView2 composition are asynchronous with
    // the GLFW/OpenGL parent. Do not retire the previous owner until DWM has
    // actually composed the prepared incoming child. Without this fence a
    // sibling hide can uncover an older parent/front-buffer generation for one
    // desktop frame even though our logical presentation state is correct.
    flushDesktopCompositor();

    if (m_activeDocumentSurface != surfaceIndex)
        m_documentWebViews[m_activeDocumentSurface].setVisible(false);

    m_activeDocumentSurface = surfaceIndex;
    m_stagingDocumentSurface = surfaceIndex == 0 ? 1 : 0;
    if (m_gameUi.commitRequested(target))
        m_gameUi.parkScene();

    if (target.mode == GameUiMode::SessionMenu)
        incoming.focus();

    if (m_afterPresentationCommit)
    {
        auto completion = std::move(m_afterPresentationCommit);
        m_afterPresentationCommit = {};
        completion();
    }
#endif
}


void Application::requestPresentationTarget(GameUiTarget target)
{
    if (!target.valid() || target.mode == GameUiMode::None)
        return;

    const bool sessionPresentation =
        target.mode == GameUiMode::Flight ||
        target.mode == GameUiMode::SystemMap ||
        target.mode == GameUiMode::ServicePanel ||
        target.mode == GameUiMode::SessionMenu;
    if (sessionPresentation && m_activeSessionKind == GameSessionLaunchKind::None)
        return;

    if (!m_gameUi.requestTarget(target))
        return;

    if (target.mode != GameUiMode::SessionMenu)
        setLocalSessionMenuPause(false);

    if (m_gameUi.committedTarget() == target)
        return;

    m_htmlUi.setActivePanel(HtmlUiPanelId::None);

#ifdef _WIN32
    if (target.isFullScreenWebDocument())
    {
        prepareFullScreenDocument(target);
        Input::instance().reset();
        return;
    }
#endif

    auto* space = dynamic_cast<SpaceState*>(m_states.current());
    if (!space)
        return;

    if (target.mode == GameUiMode::Flight)
    {
        ScreenLayout layout = ScreenLayout::Front_Main_Rear_Mini;
        switch (target.flight)
        {
            case FlightPresentationView::Front: layout = ScreenLayout::Front_Main_Rear_Mini; break;
            case FlightPresentationView::Rear: layout = ScreenLayout::Rear_Main_Front_Mini; break;
            case FlightPresentationView::FrontDrone: layout = ScreenLayout::Front_Main_Drone_Mini; break;
            case FlightPresentationView::Drone: layout = ScreenLayout::Drone_Main_Front_Mini; break;
        }
        space->setFlightScreenLayout(layout);
        m_gameUi.armSceneTarget(target);
        Input::instance().reset();
        return;
    }

    if (target.mode == GameUiMode::ServicePanel)
    {
        // F5-F8 are native scene-backed presentation targets. No browser
        // navigation, HWND visibility or DWM sibling ordering participates.
        m_gameUi.armSceneTarget(target);
        Input::instance().reset();
        return;
    }

    if (target.mode == GameUiMode::SystemMap)
    {
        // Navigation data is prepared by prepareRequestedPresentation() from
        // the latest accepted client snapshot. The side panel is rendered in
        // the same OpenGL frame, so there is no separate readiness handshake.
        Input::instance().reset();
    }
}

void Application::requestFlightView(FlightPresentationView view)
{
    m_lastFlightTarget = GameUiTarget::forFlight(view);
    requestPresentationTarget(m_lastFlightTarget);
}

void Application::requestLastFlightView()
{
    requestPresentationTarget(m_lastFlightTarget);
}

void Application::requestNavigationView(NavigationPresentationView view)
{
    requestPresentationTarget(GameUiTarget::forNavigation(view));
}

void Application::adoptNavigationView(NavigationPresentationView view)
{
    // Internal map drill/navigation already prepared the scene on the same
    // OpenGL surface. Synchronize the global presentation target without
    // re-running F9-F12 preparation. Never override a newer direct request.
    if (m_gameUi.requestPending() ||
        m_gameUi.committedTarget().mode != GameUiMode::SystemMap)
    {
        return;
    }

    const GameUiTarget target = GameUiTarget::forNavigation(view);
    if (m_gameUi.committedTarget() == target &&
        m_gameUi.sceneTarget() == target)
    {
        return;
    }

    m_gameUi.forceCommit(target);
}

void Application::requestServicePanel(ui::services::ServiceUiId service)
{
    if (service == ui::services::ServiceUiId::None)
        return;
    requestPresentationTarget(GameUiTarget::forService(service));
}

void Application::prepareRequestedPresentation(SpaceState& space)
{
    const GameUiTarget requested = m_gameUi.requestedTarget();
    if (requested.mode != GameUiMode::SystemMap || !m_gameUi.requestPending())
        return;

    PlayerNavigationMapLevel level = PlayerNavigationMapLevel::Galaxy;
    switch (requested.navigation)
    {
        case NavigationPresentationView::Galaxy: level = PlayerNavigationMapLevel::Galaxy; break;
        case NavigationPresentationView::System: level = PlayerNavigationMapLevel::System; break;
        case NavigationPresentationView::Detail: level = PlayerNavigationMapLevel::Detail; break;
        case NavigationPresentationView::Local: level = PlayerNavigationMapLevel::Local; break;
    }

    if (!space.preparePlayerNavigationMapLevel(level))
        return;

    m_gameUi.armSceneTarget(requested);
}

void Application::commitPreparedPresentationAfterSwap(SpaceState& space)
{
    (void)space;
    const GameUiTarget requested = m_gameUi.requestedTarget();
    const GameUiTarget scene = m_gameUi.sceneTarget();
    if (requested != scene ||
        (requested.mode != GameUiMode::Flight &&
         requested.mode != GameUiMode::SystemMap &&
         requested.mode != GameUiMode::ServicePanel))
    {
        return;
    }

    if (m_gameUi.committedTarget() == requested)
        return;

#ifdef _WIN32
    const bool outgoingFullScreen =
        m_gameUi.committedTarget().isFullScreenWebDocument();
    if (outgoingFullScreen)
    {
        // This is the only session handoff that still crosses native surfaces:
        // leaving the explicit ESC menu/loading document for the one OpenGL
        // in-session presentation surface. F1-F12 transitions among themselves
        // never execute WebView show/hide/z-order operations.
        flushDesktopCompositor();
        m_documentWebViews[m_activeDocumentSurface].setVisible(false);
    }
#endif

    if (m_gameUi.commitRequested(requested))
    {
        if (m_window)
            m_window->focus();

        if (m_afterPresentationCommit)
        {
            auto completion = std::move(m_afterPresentationCommit);
            m_afterPresentationCommit = {};
            completion();
        }
    }
}

void Application::cycleUiLanguage()
{
    const std::string locale = m_localization.cycleLocale();
    m_clientPreferences.preferredLocale = locale;
    {
        std::string preferencesError;
        if (!ui::platform::ClientPreferencesStore::save(
                m_clientPreferences,
                &preferencesError))
        {
            std::cerr << "[ClientPreferences] cannot persist locale: "
                      << preferencesError << "\n";
        }
    }

#ifdef _WIN32
    const std::string script =
        "localStorage.setItem('elite.ui.locale','" + locale + "');"
        "if (window.GameI18n) window.GameI18n.setLocale('" + locale + "');";
    for (auto& surface : m_documentWebViews)
        surface.evalScript(script);
#endif

    if (GameState* state = m_states.current())
        state->onUiLanguageChanged();

    std::cout << "[Localization] UI locale=" << locale << std::endl;
}

void Application::requestApplicationQuit()
{
    if (m_window)
        m_window->hide();
#ifdef _WIN32
    for (auto& surface : m_documentWebViews)
        surface.setVisible(false);
#endif
    m_gameUi.clearForShutdown();
    m_running = false;
}

GameUiMode Application::gameUiMode() const
{
    // Semantic presentation state: input/live-map policy must follow what the
    // user can actually see, never a destination being prepared underneath it.
    return m_gameUi.committedTarget().mode;
}

GameUiTarget Application::gameUiTarget() const
{
    return m_gameUi.committedTarget();
}

GameUiMode Application::sceneGameUiMode() const
{
    // Render-only state. This may temporarily differ from committedTarget while
    // a Flight/Navigation destination is prepared under an opaque outgoing UI.
    return m_gameUi.sceneTarget().mode;
}

GameUiTarget Application::sceneGameUiTarget() const
{
    return m_gameUi.sceneTarget();
}

bool Application::isGameUiOpen() const
{
    return m_gameUi.committedTarget().mode != GameUiMode::Flight &&
           m_gameUi.committedTarget().mode != GameUiMode::None;
}


void Application::setLocalSessionMenuPause(bool paused)
{
    m_localSessionPausedByMenu =
        paused && m_activeSessionKind == GameSessionLaunchKind::LocalNewGame;
}

bool Application::localSessionMenuPaused() const
{
    return m_localSessionPausedByMenu &&
           m_activeSessionKind == GameSessionLaunchKind::LocalNewGame;
}

void Application::processDocumentWebViewCommands(int surfaceIndex)
{
#ifdef _WIN32
    if (surfaceIndex < 0 || surfaceIndex > 1)
        return;

    auto& webView = m_documentWebViews[surfaceIndex];
    std::string webCommand;
    while (webView.pollCommand(webCommand))
    {
        std::uint64_t presentationSerial = 0;
        const GameUiTarget mainMenuTarget =
            GameUiTarget::forMode(GameUiMode::MainMenu);
        const GameUiTarget loadingTarget =
            GameUiTarget::forMode(GameUiMode::Loading);
        const GameUiTarget sessionMenuTarget =
            GameUiTarget::forMode(GameUiMode::SessionMenu);

        if (webCommand.rfind("main_menu_ready|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "main_menu_ready|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentLoaded(
                    surfaceIndex, mainMenuTarget, presentationSerial))
            {
                applyMainMenuView();
            }
            continue;
        }

        if (webCommand.rfind("main_menu_prepared|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "main_menu_prepared|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentPrepared(
                    surfaceIndex, mainMenuTarget, presentationSerial))
            {
                commitFullScreenDocument(surfaceIndex, mainMenuTarget);
            }
            continue;
        }

        if (webCommand.rfind("loading_ui_ready|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "loading_ui_ready|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentLoaded(
                    surfaceIndex, loadingTarget, presentationSerial))
            {
                setLoadingUiProgress(
                    m_loadingUiProgress,
                    m_loadingUiStageKey,
                    m_loadingUiEnglishFallback);
            }
            continue;
        }

        if (webCommand.rfind("loading_ui_prepared|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "loading_ui_prepared|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentPrepared(
                    surfaceIndex, loadingTarget, presentationSerial))
            {
                commitFullScreenDocument(surfaceIndex, loadingTarget);
            }
            continue;
        }

        if (webCommand.rfind("session_menu_ready|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "session_menu_ready|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentLoaded(
                    surfaceIndex, sessionMenuTarget, presentationSerial))
            {
                applySessionMenuView();
            }
            continue;
        }

        if (webCommand.rfind("session_menu_prepared|", 0) == 0)
        {
            if (parsePresentationSerialCommand(
                    webCommand, "session_menu_prepared|", presentationSerial) &&
                m_gameUi.acknowledgeDocumentPrepared(
                    surfaceIndex, sessionMenuTarget, presentationSerial))
            {
                commitFullScreenDocument(surfaceIndex, sessionMenuTarget);
            }
            continue;
        }

        // A hidden back surface is allowed to prepare documents but can never
        // issue player commands before it becomes the committed front surface.
        if (surfaceIndex != m_activeDocumentSurface)
            continue;

        constexpr const char* NewLocalGamePrefix = "new_local_game|";
        if (webCommand.rfind(NewLocalGamePrefix, 0) == 0)
        {
            const std::string encodedName = webCommand.substr(
                std::char_traits<char>::length(NewLocalGamePrefix));
            std::string localPlayerName;
            if (!decodeWebComponent(encodedName, localPlayerName) ||
                !normalizeLocalPlayerDisplayName(localPlayerName))
            {
                webView.evalScript(
                    "window.EliteUiKit.restoreDocument();"
                    "window.EliteUiKit.setBanner("
                    "'#local-player-name-error',"
                    "window.GameI18n.t('main.local_player_name_required','Enter a player name.'),"
                    "'error');");
                continue;
            }

            m_localPlayerDisplayName = std::move(localPlayerName);
            requestSessionStart(GameSessionLaunchKind::LocalNewGame);
            continue;
        }

        if (webCommand == "load_local_game")
        {
            std::cout << "[App] load local game not implemented yet\n";
            continue;
        }

        constexpr const char* MainRoutePrefix = "main_route|";
        if (webCommand.rfind(MainRoutePrefix, 0) == 0)
        {
            const std::string payload = webCommand.substr(
                std::char_traits<char>::length(MainRoutePrefix));
            const auto first = payload.find('|');
            const auto second = first == std::string::npos
                ? std::string::npos
                : payload.find('|', first + 1);
            if (first == std::string::npos || second == std::string::npos)
                continue;

            const std::string route = payload.substr(0, first);
            const std::string endpoint =
                payload.substr(first + 1, second - first - 1);
            const std::string account = payload.substr(second + 1);
            if (endpoint.size() <= 255u &&
                account.size() <= game::identity::AccountHandleMaxLength)
            {
                m_uiNavigationState.setConnectionDraft(endpoint, account);
            }

            if (route == "home")
                m_uiNavigationState.showMainMenuHome();
            else if (route == "multiplayer")
                m_uiNavigationState.showMultiplayerAuthorization();
            else if (route == "signin")
                m_uiNavigationState.showSignInPassword();
            else if (route == "register")
                m_uiNavigationState.showRegistration();
            else if (route == "recovery")
                m_uiNavigationState.showRecovery();
            else if (route == "account")
            {
                if (m_authenticatedRemoteAccountHandle.empty())
                    m_uiNavigationState.showMultiplayerAuthorization();
                else
                    m_uiNavigationState.showAccount();
            }
            else
                continue;

            applyMainMenuView();
            continue;
        }

        constexpr const char* SetLocalePrefix = "set_ui_locale|";
        if (webCommand.rfind(SetLocalePrefix, 0) == 0)
        {
            setUiLanguage(webCommand.substr(
                std::char_traits<char>::length(SetLocalePrefix)));
            if (m_gameUi.committedTarget().mode == GameUiMode::MainMenu)
                applyMainMenuView();
            continue;
        }

        if (webCommand == "session_cancel")
        {
            cancelPendingSessionStart();
            continue;
        }

        if (webCommand == "session_resume" || webCommand == "session_escape")
        {
            resumeSessionFromMenu();
            continue;
        }

        if (webCommand == "session_return_main")
        {
            returnSessionToMainMenu();
            continue;
        }

        if (webCommand == "session_quit" || webCommand == "exit")
        {
            requestApplicationQuit();
            continue;
        }

        if (webCommand == "multiplayer")
        {
            showMultiplayerConnectionForm();
            continue;
        }

        constexpr const char* MultiplayerSignInPrefix = "multiplayer_signin|";
        constexpr const char* MultiplayerRegisterPrefix = "multiplayer_register|";
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
                std::char_traits<char>::length(prefix));
            const auto separator = payload.rfind('|');
            if (separator == std::string::npos)
            {
                webView.evalScript("setAuthError('INVALID_ACCOUNT_HANDLE');");
                continue;
            }

            const std::string endpointText = payload.substr(0, separator);
            const std::string profileName = payload.substr(separator + 1);
            m_uiNavigationState.setConnectionDraft(endpointText, profileName);

            game::network::NetworkEndpoint endpoint;
            std::string endpointError;
            if (!game::network::parseNetworkEndpoint(
                    endpointText, endpoint, &endpointError))
            {
                std::cerr << "[App] invalid multiplayer endpoint: "
                          << endpointError << "\n";
                webView.evalScript("setAuthError('INVALID_SERVER_ADDRESS');");
                continue;
            }

            std::string identityError;
            const auto intent = isRegister
                ? game::network::AuthenticationIntent::Register
                : game::network::AuthenticationIntent::SignIn;
            if (!prepareRemoteIdentity(profileName, intent, identityError))
            {
                if (isSignIn && identityError == "LOCAL_CREDENTIAL_MISSING")
                    showPasswordSignInForm(identityError);
                else
                    webView.evalScript(
                        "setAuthError(" + nlohmann::json(identityError).dump() + ");");
                continue;
            }

            configureRemoteServer(endpoint.host, endpoint.port);
            requestSessionStart(GameSessionLaunchKind::RemoteMultiplayer);
            continue;
        }

        if (webCommand == "multiplayer_back")
        {
            showMainMenu();
            continue;
        }
    }
#else
    (void)surfaceIndex;
#endif
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

    startSessionNow(kind);
}

void Application::startSessionNow(GameSessionLaunchKind kind)
{
    m_pendingSessionLaunch = kind;
    setLocalSessionMenuPause(false);

#ifdef _WIN32
    m_uiNavigationState.clearTransientMessage();
    m_loadingUiProgress = 0.10;
    m_loadingUiStageKey = "loading.stage.opening";
    m_loadingUiEnglishFallback = "OPENING LOADING SCREEN";
    requestPresentationTarget(GameUiTarget::forMode(GameUiMode::Loading));
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

    m_sessionStartTime = glfwGetTime();
    m_sessionStartLastUpdateTime = m_sessionStartTime;
    m_sessionStartStage = SessionStartStage::WaitingForLoadingScreen;
}

void Application::showMultiplayerConnectionForm(const std::string& errorCode)
{
#ifdef _WIN32
    m_uiNavigationState.showMultiplayerAuthorization(errorCode);
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::MainMenu);
    if (m_gameUi.committedTarget() == target)
        applyMainMenuView();
    else
        requestPresentationTarget(target);
#else
    (void)errorCode;
#endif
}

void Application::showRegistrationForm(const std::string& errorCode)
{
#ifdef _WIN32
    m_uiNavigationState.showRegistration(errorCode);
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::MainMenu);
    if (m_gameUi.committedTarget() == target)
        applyMainMenuView();
    else
        requestPresentationTarget(target);
#else
    (void)errorCode;
#endif
}

void Application::showPasswordSignInForm(const std::string& errorCode)
{
#ifdef _WIN32
    m_uiNavigationState.showSignInPassword(errorCode);
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::MainMenu);
    if (m_gameUi.committedTarget() == target)
        applyMainMenuView();
    else
        requestPresentationTarget(target);
#else
    (void)errorCode;
#endif
}

void Application::showMainMenu()
{
#ifdef _WIN32
    m_uiNavigationState.showMainMenuHome();
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::MainMenu);
    if (m_gameUi.committedTarget() == target)
        applyMainMenuView();
    else
        requestPresentationTarget(target);
#endif
}

void Application::applyMainMenuView()
{
#ifdef _WIN32
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::MainMenu);
    int surfaceIndex = -1;
    for (int i : {m_stagingDocumentSurface, m_activeDocumentSurface})
    {
        const auto& surface = m_gameUi.documentSurface(i);
        if (surface.target == target && surface.loaded)
        {
            surfaceIndex = i;
            break;
        }
    }
    if (surfaceIndex < 0)
        return;

    std::string endpoint = m_uiNavigationState.endpointDraft();
    if (endpoint.empty())
    {
        if (hasConfiguredRemoteServer())
        {
            endpoint = m_remoteServerHost + ":" +
                std::to_string(m_remoteServerPort);
        }
        else
        {
            game::network::NetworkEndpoint rememberedEndpoint;
            if (!m_clientPreferences.lastServerEndpoint.empty() &&
                game::network::parseNetworkEndpoint(
                    m_clientPreferences.lastServerEndpoint,
                    rememberedEndpoint))
            {
                endpoint = m_clientPreferences.lastServerEndpoint;
            }
            else
            {
                endpoint = "127.0.0.1:27351";
            }
        }
    }

    std::string accountHandle = m_uiNavigationState.accountHandleDraft();
    if (accountHandle.empty())
        accountHandle = m_clientIdentityProfileName;
    if (accountHandle.empty())
        accountHandle = m_clientPreferences.lastSuccessfulAccountFor(endpoint);

    nlohmann::json state = nlohmann::json::object();
    switch (m_uiNavigationState.route())
    {
        case ui::platform::UiShellRoute::MultiplayerAuthorization:
            state["view"] = "multiplayer";
            break;
        case ui::platform::UiShellRoute::SignInPassword:
            state["view"] = "signin";
            break;
        case ui::platform::UiShellRoute::Registration:
            state["view"] = "register";
            break;
        case ui::platform::UiShellRoute::Recovery:
            state["view"] = "recovery";
            break;
        case ui::platform::UiShellRoute::Account:
            state["view"] = "account";
            break;
        case ui::platform::UiShellRoute::MainMenuHome:
        default:
            state["view"] = "home";
            break;
    }

    state["endpoint"] = endpoint;
    state["accountHandle"] = accountHandle;
    state["errorCode"] = m_uiNavigationState.transientMessageCode();
    state["locale"] = m_localization.locale();
    state["localPlayerName"] = m_localPlayerDisplayName;
    state["accountAvailable"] = !m_authenticatedRemoteAccountHandle.empty();
    state["passwordRecoveryAvailable"] = false;
    state["authenticatedAccountHandle"] = m_authenticatedRemoteAccountHandle;
    state["authenticatedEndpoint"] = m_authenticatedRemoteEndpoint;
    state["locales"] = nlohmann::json::array();
    for (const std::string& locale : m_localization.localeOrder())
    {
        nlohmann::json localeState;
        localeState["id"] = locale;
        const auto metadataIt = m_localization.localeMetadata().find(locale);
        localeState["nativeName"] = metadataIt != m_localization.localeMetadata().end()
            ? metadataIt->second.nativeName
            : locale;
        state["locales"].push_back(std::move(localeState));
    }

    m_documentWebViews[surfaceIndex].evalScript(
        "window.applyMainMenuState(" + state.dump() + ");");
#endif
}

void Application::showSessionMenu()
{
#ifdef _WIN32
    if (!m_gameSession || m_activeSessionKind == GameSessionLaunchKind::None)
        return;

    const GameUiTarget committed = m_gameUi.committedTarget();
    if (committed.mode != GameUiMode::SessionMenu &&
        committed.mode != GameUiMode::None)
    {
        m_sessionResumeTarget = committed;
    }
    else if (m_gameUi.requestedTarget().mode != GameUiMode::SessionMenu)
    {
        m_sessionResumeTarget = m_gameUi.requestedTarget();
    }

    if (m_sessionResumeTarget.mode == GameUiMode::MainMenu ||
        m_sessionResumeTarget.mode == GameUiMode::Loading ||
        m_sessionResumeTarget.mode == GameUiMode::SessionMenu ||
        m_sessionResumeTarget.mode == GameUiMode::None)
    {
        m_sessionResumeTarget = m_lastFlightTarget;
    }

    setLocalSessionMenuPause(true);
    requestPresentationTarget(GameUiTarget::forMode(GameUiMode::SessionMenu));
    Input::instance().reset();
#endif
}

void Application::resumeSessionFromMenu()
{
#ifdef _WIN32
    if (m_activeSessionKind == GameSessionLaunchKind::None)
        return;

    setLocalSessionMenuPause(false);
    requestPresentationTarget(m_sessionResumeTarget);
    Input::instance().reset();
#endif
}

void Application::applySessionMenuView()
{
#ifdef _WIN32
    const GameUiTarget target = GameUiTarget::forMode(GameUiMode::SessionMenu);
    int surfaceIndex = -1;
    for (int i : {m_stagingDocumentSurface, m_activeDocumentSurface})
    {
        const auto& surface = m_gameUi.documentSurface(i);
        if (surface.target == target && surface.loaded)
        {
            surfaceIndex = i;
            break;
        }
    }
    if (surfaceIndex < 0)
        return;

    const bool local =
        m_activeSessionKind == GameSessionLaunchKind::LocalNewGame;
    const bool safeZone = false;

    nlohmann::json state;
    if (local)
    {
        state["safeZone"] = safeZone;
        state["persistenceReady"] = false;
        state["canSave"] = false;
        state["canLoad"] = false;
        state["playerDisplayName"] = m_localPlayerDisplayName;
        m_documentWebViews[surfaceIndex].evalScript(
            "window.applyLocalSessionMenuState(" + state.dump() + ");");
    }
    else
    {
        state["accountHandle"] = m_clientIdentityProfileName;
        m_documentWebViews[surfaceIndex].evalScript(
            "window.applyMultiplayerSessionMenuState(" + state.dump() + ");");
    }
#endif
}

void Application::returnSessionToMainMenu()
{
#ifdef _WIN32
    setLocalSessionMenuPause(false);
    m_uiNavigationState.showMainMenuHome();
    m_afterPresentationCommit = [this]()
    {
        stopGameSession();
        m_pendingSessionLaunch = GameSessionLaunchKind::None;
        m_activeSessionKind = GameSessionLaunchKind::None;
        m_sessionStartStage = SessionStartStage::Idle;
        m_spaceStateBuildStartTime = 0.0;

        m_states.clear();
        m_states.applyPendingChanges();
        m_states.push(std::make_unique<MainMenuState>(m_states));
        m_states.applyPendingChanges();
    };
    requestPresentationTarget(GameUiTarget::forMode(GameUiMode::MainMenu));
#endif
}

void Application::cancelPendingSessionStart()
{
    if (m_pendingSessionLaunch == GameSessionLaunchKind::None ||
        m_sessionStartStage == SessionStartStage::BuildingSpaceState)
    {
        return;
    }

#ifdef _WIN32
    const bool remote =
        m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;
    m_afterPresentationCommit = [this]()
    {
        stopGameSession();
        m_pendingSessionLaunch = GameSessionLaunchKind::None;
        m_activeSessionKind = GameSessionLaunchKind::None;
        m_sessionStartStage = SessionStartStage::Idle;
        m_spaceStateBuildStartTime = 0.0;

        m_states.clear();
        m_states.applyPendingChanges();
        m_states.push(std::make_unique<MainMenuState>(m_states));
        m_states.applyPendingChanges();
    };

    if (remote)
        showMultiplayerConnectionForm();
    else
        showMainMenu();
#endif
}

void Application::setLoadingUiProgress(
    double progress,
    std::string stageKey,
    std::string englishFallback
)
{
    m_loadingUiProgress = std::clamp(progress, 0.0, 1.0);
    m_loadingUiStageKey = std::move(stageKey);
    m_loadingUiEnglishFallback = std::move(englishFallback);

#ifdef _WIN32
    const GameUiTarget loadingTarget = GameUiTarget::forMode(GameUiMode::Loading);
    for (int i = 0; i < 2; ++i)
    {
        const auto& surface = m_gameUi.documentSurface(i);
        if (surface.target != loadingTarget || !surface.loaded)
            continue;

        m_documentWebViews[i].evalScript(
            "if(window.setLoadingProgress){"
            "window.setLoadingProgress(" + std::to_string(m_loadingUiProgress) + "," +
            nlohmann::json(m_loadingUiStageKey).dump() + "," +
            nlohmann::json(m_loadingUiEnglishFallback).dump() +
            ");"
            "}"
        );
    }
#endif
}

void Application::setUiLanguage(const std::string& locale)
{
    if (!m_localization.setLocale(locale))
        return;

    m_clientPreferences.preferredLocale = m_localization.locale();
    std::string preferencesError;
    if (!ui::platform::ClientPreferencesStore::save(
            m_clientPreferences,
            &preferencesError))
    {
        std::cerr << "[ClientPreferences] cannot persist locale: "
                  << preferencesError << "\n";
    }

#ifdef _WIN32
    const std::string activeLocale = m_localization.locale();
    const std::string script =
        "localStorage.setItem('elite.ui.locale'," +
        nlohmann::json(activeLocale).dump() + ");"
        "if (window.GameI18n) window.GameI18n.setLocale(" +
        nlohmann::json(activeLocale).dump() + ");";
    for (auto& surface : m_documentWebViews)
        surface.evalScript(script);
#endif

    if (GameState* state = m_states.current())
        state->onUiLanguageChanged();
}

void Application::updatePendingSessionStart()
{
    if (m_pendingSessionLaunch == GameSessionLaunchKind::None)
        return;

    const double now = glfwGetTime();

    const auto finishReadySession = [this]()
    {
        Input::instance().reset();

        glfwSetInputMode(
            m_window->nativeHandle(),
            GLFW_CURSOR,
            GLFW_CURSOR_NORMAL
        );

        const bool remote =
            m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;
        if (remote && hasConfiguredRemoteServer() &&
            game::identity::isValidAccountHandle(m_clientIdentityProfileName))
        {
            const std::string endpoint =
                m_remoteServerHost + ":" + std::to_string(m_remoteServerPort);
            m_clientPreferences.rememberSuccessfulMultiplayer(
                endpoint,
                m_clientIdentityProfileName
            );
            m_authenticatedRemoteAccountHandle = m_clientIdentityProfileName;
            m_authenticatedRemoteEndpoint = endpoint;

            std::string preferencesError;
            if (!ui::platform::ClientPreferencesStore::save(
                    m_clientPreferences,
                    &preferencesError))
            {
                std::cerr << "[ClientPreferences] cannot remember multiplayer account: "
                          << preferencesError << "\n";
            }
        }

        m_activeSessionKind = m_pendingSessionLaunch;
        m_pendingSessionLaunch = GameSessionLaunchKind::None;
        m_sessionStartStage = SessionStartStage::Idle;
        m_spaceStateBuildStartTime = 0.0;
        m_lastFlightTarget =
            GameUiTarget::forFlight(FlightPresentationView::Front);
        m_sessionResumeTarget = m_lastFlightTarget;
        setLocalSessionMenuPause(false);

        // Loading remains physically visible while the first complete flight
        // framebuffer is rendered. commitPreparedPresentationAfterSwap() then
        // retires the loading surface on the exact swap boundary.
        requestPresentationTarget(m_lastFlightTarget);

        std::cout << "[App] "
                  << (remote ? "multiplayer world entered" : "local game loaded")
                  << ", flight presentation requested\n";
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

#ifdef _WIN32
        setLoadingUiProgress(1.00, "loading.stage.ready", "READY");
#endif
        finishReadySession();
        return;
    }

    if (m_sessionStartStage == SessionStartStage::WaitingForLoadingScreen)
    {
        // The loading DOM is a presentation prerequisite. Do not start
        // networking/world bootstrap until the destination document has
        // acknowledged DOM/i18n/font readiness and received its first state.
        const GameUiTarget loadingTarget = GameUiTarget::forMode(GameUiMode::Loading);
        if (m_gameUi.committedTarget() != loadingTarget)
            return;

        const bool remote =
            m_pendingSessionLaunch == GameSessionLaunchKind::RemoteMultiplayer;

#ifdef _WIN32
        if (remote)
            setLoadingUiProgress(0.25, "loading.stage.connecting", "CONNECTING TO SERVER");
        else
            setLoadingUiProgress(0.25, "loading.stage.world", "PREPARING LOCAL WORLD");
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
        setLoadingUiProgress(0.45, "loading.stage.waiting_server", "WAITING FOR SERVER");
#endif
        return;
    }

    if (sessionState == game::session::GameSessionState::Synchronizing ||
        sessionState == game::session::GameSessionState::Created)
    {
#ifdef _WIN32
        setLoadingUiProgress(0.55, "loading.stage.sync", "SYNCHRONIZING SESSION");
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
        setLoadingUiProgress(1.00, "loading.stage.failed", "SESSION FAILED");
#endif

        m_afterPresentationCommit = [this]()
        {
            stopGameSession();
            m_pendingSessionLaunch = GameSessionLaunchKind::None;
            m_activeSessionKind = GameSessionLaunchKind::None;
            m_sessionStartStage = SessionStartStage::Idle;
            m_spaceStateBuildStartTime = 0.0;
        };

#ifdef _WIN32
        if (remoteLaunch)
        {
            const std::string message =
                sessionError.empty() ? "SESSION_UNAVAILABLE" : sessionError;
            if (m_uiNavigationState.route() ==
                ui::platform::UiShellRoute::Registration)
            {
                showRegistrationForm(message);
            }
            else if (message == "INVALID_CREDENTIAL")
            {
                showPasswordSignInForm(message);
            }
            else
            {
                showMultiplayerConnectionForm(message);
            }
        }
        else
        {
            showMainMenu();
        }
#endif
        return;
    }

#ifdef _WIN32
    setLoadingUiProgress(0.80, "loading.stage.apply", "APPLYING GAME STATE");
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
