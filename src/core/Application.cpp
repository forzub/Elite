#include "Application.h"
#include "ui/MainMenuState.h"
#include "game/SpaceState.h"

#include <iostream>
#include <GLFW/glfw3.h>

Application::Application()
    : m_running(false)
    , m_window(nullptr)
{
}

Application::~Application()
{
}

void Application::run()
{
    init();
    mainLoop();
    shutdown();
}


void Application::init()
{
    std::cout << "Application init\n";

    m_window = new Window(1280, 720, "EliteGame");
    m_running = true;

    // m_states.push(std::make_unique<MainMenuState>(m_states));
    m_states.push(std::make_unique<SpaceState>(m_states));
    m_states.applyPendingChanges();
}


void Application::mainLoop()
{
    std::cout << "Application main loop start\n";
    double lastTime = glfwGetTime();

    while (m_running && !m_window->shouldClose() && !m_states.empty())
    {
        double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        m_window->pollEvents();

        GameState* state = m_states.current();
        state->handleInput();
        state->update(dt);
        state->render();

        m_states.applyPendingChanges();
        m_window->swapBuffers();
    }
}


void Application::shutdown()
{
    std::cout << "Application shutdown\n";

    m_states.clear();
    m_states.applyPendingChanges();

    delete m_window;
}
