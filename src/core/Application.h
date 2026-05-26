#pragma once

#include "StateContext.h"
#include "StateStack.h"
#include "window/Window.h"
#include "render/Renderer.h"
#include "render/RenderContext.h"
#include "ui/html/HtmlUiManager.h"

#ifdef _WIN32
#include "ui/browser/GameWebView.h"
#endif



class Application
{
public:
    Application();
    ~Application();

    void run();
    Viewport viewport() const;
    
    HtmlUiManager& htmlUi() { return m_htmlUi; }
    const HtmlUiManager& htmlUi() const { return m_htmlUi; }

    void updatePendingNewGameLoad();

private:
    void init();
    void mainLoop();
    void shutdown();

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void handleResize(int width, int height);
    
    bool m_running;
    Renderer m_renderer;
    Window* m_window;
    StateContext m_context;
    StateStack   m_states;
    RenderContext renderContext;
    HtmlUiManager m_htmlUi;

    bool m_pendingNewGameLoad = false;
    double m_newGameLoadStartTime = 0.0;

    #ifdef _WIN32
        GameWebView m_gameWebView;
    #endif
};
