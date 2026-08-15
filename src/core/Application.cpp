#include <iostream>
#include <GLFW/glfw3.h>
#include "Application.h"
#include <algorithm>
#include <utility>
#include <stdexcept>

#include "core/log.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "src/game/ui/GameUiHotkeyPolicy.h"
#include "src/game/host/LocalGameSession.h"
#include "src/game/session/RemoteGameSession.h"
#include "src/game/session/IGameSession.h"
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
        cwd.parent_path() / "src" / "assets" / "webui" / relativeFile,
        fs::path("D:/__elite/work/src/assets/webui") / relativeFile
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
        cwd.parent_path() / "src" / "assets" / "webui",
        fs::path("D:/__elite/work/src/assets/webui")
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
    const game::network::SessionHello& hello
)
{
    if (!hello.authToken.valid())
        throw std::invalid_argument("invalid client authentication token");

    m_clientIdentityHello = hello;
}

void Application::configureRemoteServer(
    std::string host,
    std::uint16_t port)
{
    m_remoteServerHost = std::move(host);
    m_remoteServerPort = port;
}

void Application::startConfiguredGameSession()
{
    if (!m_remoteServerHost.empty() && m_remoteServerPort != 0)
    {
        game::session::RemoteGameSessionConfig config;
        config.host = m_remoteServerHost;
        config.port = m_remoteServerPort;
        config.identityHello = m_clientIdentityHello;
        m_gameSession =
            std::make_unique<game::session::RemoteGameSession>(
                std::move(config)
            );
        return;
    }

    startLocalGameSession();
}

void Application::startLocalGameSession()
{
    game::host::LocalGameSessionConfig config;
    config.identityHello = m_clientIdentityHello;
    m_gameSession =
        std::make_unique<game::host::LocalGameSession>(config);
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

    m_context.app           = this;

    glfwWindowHint(GLFW_STENCIL_BITS, 8);
    m_window = new Window(1280, 720, "EliteGame");

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
            std::cerr << "[App] client process pid=" << GetCurrentProcessId()
                      << " glfw_hwnd=" << m_window->nativeWin32Handle()
                      << " webui_port=" << m_gameUiHttpPort
                      << "\n";
#endif

            int w, h;
            glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);
            m_htmlUi.setViewport(w, h);
    // ---------------------------------------------------


    glfwSetWindowUserPointer(m_window->nativeHandle(), this);
    glfwSetFramebufferSizeCallback(m_window->nativeHandle(), framebuffer_size_callback);

    m_running = true;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    m_renderer.init();


    glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

    g_stateContext          = &m_context;

    TextRenderer::instance().init();

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
        m_gameUi.forceMode(GameUiMode::MainMenu);
        m_gameUi.markLoaded(GameUiMode::MainMenu);
        m_htmlUi.setActivePanel(HtmlUiPanelId::None);
    #endif

    m_states.push(std::make_unique<MainMenuState>(m_states));
    // m_states.push(std::make_unique<SpaceState>(m_states));

    m_states.applyPendingChanges();

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

        m_context.dt            = dt;
        lastTime                = currentTime;

        // Input::instance().update();
        m_window->pollEvents();

        // A physical keyboard belongs to exactly one graphical client process.
        // Embedded WebView2 may own the child HWND focus, so GLFW_FOCUSED on
        // the parent is not a sufficient ownership test. Gate all gameplay
        // input by the foreground process instead; inactive EliteGame instances
        // immediately publish a neutral control state on their next frame.
        const bool ownsForegroundInput = m_window->ownsForegroundInput();
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

                    if (webCommand == "new_game")
                    {
                        std::cout << "[App] new_game requested, switching to loading screen\n";

                        m_gameUi.forceMode(GameUiMode::Loading);
                        m_gameUi.markLoaded(GameUiMode::Loading);

                        m_htmlUi.setActivePanel(HtmlUiPanelId::None);

                        m_gameWebView.setVisible(true);
                        m_gameWebView.navigate(makeGameUiHttpUrl(m_gameUiHttpPort, "loading.html", m_localization.locale()));
                        m_gameWebView.evalScript("setLoadingProgress(0.10, 'loading.stage.opening', 'OPENING LOADING SCREEN');");

                        m_pendingNewGameLoad = true;
                        m_newGameLoadStartTime = glfwGetTime();
                        m_newGameLoadLastUpdateTime = m_newGameLoadStartTime;
                        m_newGameLoadStage =
                            NewGameLoadStage::WaitingForLoadingScreen;

                        break;
                    }

                    if (webCommand == "load_game")
                    {
                        std::cout << "[App] load_game not implemented yet\n";
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


        updatePendingNewGameLoad();

        if (m_pendingNewGameLoad)
        {
            m_window->swapBuffers();
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

            state->prepareFrame(dt);
            state->handleInput();
            state->update(dt);

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
        m_window->swapBuffers();
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
            {
                navigateGameUi(GameUiMode::MainMenu);
                m_gameUi.markLoaded(GameUiMode::MainMenu);
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














void Application::updatePendingNewGameLoad()
{
#ifndef _WIN32
    return;
#else
    if (!m_pendingNewGameLoad)
        return;

    const double now = glfwGetTime();

    if (m_newGameLoadStage == NewGameLoadStage::WaitingForLoadingScreen)
    {
        if ((now - m_newGameLoadStartTime) < 0.10)
            return;

        m_gameWebView.evalScript(
            "setLoadingProgress(0.25, 'loading.stage.world', 'PREPARING WORLD');"
        );

        stopGameSession();
        startConfiguredGameSession();
        m_gameSession->beginSynchronization();

        m_newGameLoadLastUpdateTime = now;
        m_newGameLoadStage = NewGameLoadStage::SynchronizingSession;
        return;
    }

    if (m_newGameLoadStage != NewGameLoadStage::SynchronizingSession)
        return;

    const double elapsed = std::clamp(
        now - m_newGameLoadLastUpdateTime,
        0.0,
        0.05
    );
    m_newGameLoadLastUpdateTime = now;

    m_gameSession->updateSynchronization(elapsed);

    const auto sessionState = m_gameSession->state();
    if (sessionState == game::session::GameSessionState::WaitingForServer)
    {
        m_gameWebView.evalScript(
            "setLoadingProgress(0.45, 'loading.stage.waiting_server', 'WAITING FOR SERVER');"
        );
        return;
    }

    if (sessionState == game::session::GameSessionState::Synchronizing ||
        sessionState == game::session::GameSessionState::Created)
    {
        m_gameWebView.evalScript(
            "setLoadingProgress(0.55, 'loading.stage.sync', 'SYNCHRONIZING SESSION');"
        );
        return;
    }

    if (sessionState == game::session::GameSessionState::Failed)
    {
        std::cerr << "[App] Session synchronization failed: "
                  << m_gameSession->error() << std::endl;
        m_gameWebView.evalScript(
            "setLoadingProgress(1.00, 'loading.stage.failed', 'SESSION FAILED');"
        );
        stopGameSession();
        m_pendingNewGameLoad = false;
        m_newGameLoadStage = NewGameLoadStage::Idle;
        return;
    }

    m_gameWebView.evalScript(
        "setLoadingProgress(0.80, 'loading.stage.apply', 'APPLYING GAME STATE');"
    );

    // Keep the loading/menu state alive while synchronization is pending.
    // The main loop terminates when the state stack is empty, so replace it
    // only after the session has reached Ready.
    m_states.clear();
    m_states.applyPendingChanges();
    m_states.push(std::make_unique<SpaceState>(m_states));
    m_states.applyPendingChanges();

    m_gameWebView.evalScript("setLoadingProgress(1.00, 'loading.stage.ready', 'READY');");

    closeGameUi();
    m_gameUi.clearLoaded();
    Input::instance().reset();
    m_window->focus();

    glfwSetInputMode(
        m_window->nativeHandle(),
        GLFW_CURSOR,
        GLFW_CURSOR_NORMAL
    );

    m_pendingNewGameLoad = false;
    m_newGameLoadStage = NewGameLoadStage::Idle;

    std::cout << "[App] New game loaded, WebView hidden\n";
#endif
}
