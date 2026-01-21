#pragma once

#include "StateStack.h"
#include "window/Window.h"

class Application
{
public:
    Application();
    ~Application();

    void run();

private:
    void init();
    void mainLoop();
    void shutdown();

private:
    bool m_running;
    StateStack m_states;
    Window* m_window;
};
