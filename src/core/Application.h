#pragma once

#include "StateContext.h"
#include "StateStack.h"
#include "window/Window.h"
#include "render/Renderer.h"


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
    Renderer m_renderer;
    Window* m_window;
    StateContext m_context;
    StateStack   m_states;
};
