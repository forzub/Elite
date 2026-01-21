#include "Application.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"

#include <iostream>
#include <GLFW/glfw3.h>
#include "input/Input.h"


// =====================================================================================
// Constructor
// =====================================================================================
Application::Application()
    : m_running(false)
    , m_window(nullptr)
{
}


// =====================================================================================
// Destructor
// =====================================================================================
Application::~Application()
{
}


// =====================================================================================
// run
// =====================================================================================
void Application::run()
{
    init();
    mainLoop();
    shutdown();
}


// =====================================================================================
// init
// =====================================================================================
void Application::init()
{
    std::cout << "Application init\n";

    m_window = new Window(1280, 720, "EliteGame");
    m_running = true;

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
        m_states.renderAll();
        // state->render();

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
