#include <iostream>
#include <GLFW/glfw3.h>
#include "Application.h"

#include "core/Log.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"
#include "input/Input.h"
#include "render/HUD/TextRenderer.h"
#include <windows.h>
#include <filesystem>
#include "ui/html/HtmlUiPanelId.h"


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
    int w, h;
    glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);
    return { w, h };
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
            std::string exeDir = getExecutablePath();
            std::string webUiRoot = exeDir + "/assets/webui";

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
            findGameUiFile("main_menu.html")
        );
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



        bool stateChangedFromWebView = false;

        #ifdef _WIN32
                std::string webCommand;
                while (m_gameWebView.pollCommand(webCommand))
                {
                    std::cout << "[App] WebView command: " << webCommand << "\n";

                    if (webCommand == "new_game")
                    {
                        std::cout << "[App] new_game requested, switching to loading screen\n";

                        m_gameWebView.setVisible(true);
                        m_gameWebView.navigate(findGameUiFile("loading.html"));
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
           
            glClear(GL_COLOR_BUFFER_BIT |
            GL_DEPTH_BUFFER_BIT |
            GL_STENCIL_BUFFER_BIT);

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
    // Обновляем вьюпорт в контексте
    glViewport(0, 0, width, height);
    
    GameState* currentState = m_states.current();
    if (currentState)
    {
        currentState->handleResize(width, height);
    }

    m_htmlUi.setViewport(width, height);

    #ifdef _WIN32
        m_gameWebView.resize(width, height);
    #endif
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

    m_htmlUi.setActivePanel(HtmlUiPanelId::None);

    Input::instance().reset();

    m_gameWebView.setVisible(false);

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