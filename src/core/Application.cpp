#include <iostream>
#include <GLFW/glfw3.h>
#include "Application.h"
#include <algorithm>

#include "core/Log.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "input/Input.h"
#include "render/HUD/TextRenderer.h"
#include <windows.h>
#include <filesystem>
#include "ui/html/HtmlUiPanelId.h"

#include "render/ViewportUtils.h"

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

static std::string makeGameUiHttpUrl(const std::string& relativeFile)
{
    return "http://localhost:8090/" + relativeFile;
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

            m_htmlUi.start(8090, webUiRoot);

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
            makeGameUiHttpUrl("main_menu.html")
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
        Input::instance().update(m_window->nativeHandle());
        
        
        #ifdef _WIN32
        {
            const bool f11Down =
                (GetAsyncKeyState(VK_F11) & 0x8000) != 0;

            if (m_gameUi.consumeF11Press(f11Down))
            {
                if (dynamic_cast<SpaceState*>(m_states.current()))
                {
                    toggleSystemMapUi();

                    Input::instance().reset();
                    m_window->swapBuffers();
                    continue;
                }
            }
        }
        #endif



        bool stateChangedFromWebView = false;

        #ifdef _WIN32
                std::string webCommand;
                while (m_gameWebView.pollCommand(webCommand))
                {
                    // std::cout << "[App] WebView command: " << webCommand << "\n";
                    if (webCommand.rfind("system_map_select:", 0) == 0)
                    {
                        const std::string idText =
                            webCommand.substr(std::string("system_map_select:").size());

                        try
                        {
                            const int systemId = std::stoi(idText);

                            if (auto* space = dynamic_cast<SpaceState*>(m_states.current()))
                            {
                                space->selectSystemMapSystem(systemId);
                            }
                        }
                        catch (...)
                        {
                            std::cout << "[App] bad system_map_select command: "
                                    << webCommand << "\n";
                        }

                        continue;
                    }

                    if (webCommand == "system_map_galaxy")
                    {
                        if (auto* space = dynamic_cast<SpaceState*>(m_states.current()))
                        {
                            space->setSystemMapGalaxyMode();
                        }

                        continue;
                    }

                    if (webCommand == "system_map_current_system")
                    {
                        if (auto* space = dynamic_cast<SpaceState*>(m_states.current()))
                        {
                            space->setSystemMapCurrentSystemMode();
                        }

                        continue;
                    }
                    
                    if (webCommand == "close_system_map")
                    {
                        closeGameUi();
                        continue;
                    }

                    if (webCommand == "new_game")
                    {
                        std::cout << "[App] new_game requested, switching to loading screen\n";

                        m_gameUi.forceMode(GameUiMode::Loading);
                        m_gameUi.markLoaded(GameUiMode::Loading);

                        m_htmlUi.setActivePanel(HtmlUiPanelId::None);

                        m_gameWebView.setVisible(true);
                        m_gameWebView.navigate(makeGameUiHttpUrl("loading.html"));
                        m_gameWebView.evalScript("setLoadingProgress(0.10, 'OPENING LOADING SCREEN');");

                        m_pendingNewGameLoad = true;
                        m_newGameLoadStartTime = glfwGetTime();

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

            // --- render world ---
            if (top)
            {
                if (top->isModal() && below) below->renderUI();
                top->renderUI();
            }
            if (top)top->renderHUD();

        m_renderer.endFrame();
        glDisable(GL_SCISSOR_TEST);

        m_states.applyPendingChanges();
        m_window->swapBuffers();
    }
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
            m_gameWebView.navigate(makeGameUiHttpUrl("main_menu.html"));
            break;

        case GameUiMode::Loading:
            m_gameWebView.navigate(makeGameUiHttpUrl("loading.html"));
            break;

        case GameUiMode::SystemMap:
            m_gameWebView.navigate(makeGameUiHttpUrl("system_map_panel.html"));
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
    const double elapsed = now - m_newGameLoadStartTime;

    if (elapsed < 0.10)
    {
        return;
    }

    m_gameWebView.evalScript("setLoadingProgress(0.25, 'PREPARING WORLD');");

    m_states.clear();
    m_states.push(std::make_unique<SpaceState>(m_states));

    m_gameWebView.evalScript("setLoadingProgress(0.80, 'APPLYING GAME STATE');");

    m_states.applyPendingChanges();

    m_gameWebView.evalScript("setLoadingProgress(1.00, 'READY');");

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

    std::cout << "[App] New game loaded, WebView hidden\n";
#endif
}