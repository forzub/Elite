#include <iostream>
#include <GLFW/glfw3.h>

#include "Application.h"

#include "core/Log.h"

#include "ui/MainMenuState.h"

#include "game/SpaceState.h"

#include "input/Input.h"

#include "render/HUD/TextRenderer.h"

#include <windows.h>





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

    m_context.app = this;   

    m_window = new Window(1280, 720, "EliteGame");
    m_running = true;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);

    int w, h;
    glfwGetFramebufferSize(m_window->nativeHandle(), &w, &h);

    g_stateContext = &m_context;

    TextRenderer::instance().init();

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
        static int frame = 0;
        double currentTime = glfwGetTime();
        
        float dt = static_cast<float>(currentTime - lastTime);
        m_context.dt = dt;

        lastTime = currentTime;
        
        // Input::instance().update();
        m_window->pollEvents();
        Input::instance().update(m_window->nativeHandle());

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

    delete m_window;
}
