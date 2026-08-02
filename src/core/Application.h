#pragma once

#include "StateContext.h"
#include "StateStack.h"
#include "window/Window.h"
#include "render/Renderer.h"
#include "render/RenderContext.h"
#include "ui/html/HtmlUiManager.h"
#include <string>
#include <memory>

#ifdef _WIN32
#include "ui/browser/GameWebView.h"
#endif

namespace game::host
{
class LocalGameSession;
}

enum class GameUiMode
{
    None,
    MainMenu,
    Loading,
    SystemMap
};


class GameUiController
{
public:
    GameUiMode mode() const
    {
        return m_mode;
    }

    

    GameUiMode loadedMode() const
    {
        return m_loadedMode;
    }

    bool isLoaded(GameUiMode mode) const
    {
        return m_loadedMode == mode;
    }

    void markLoaded(GameUiMode mode)
    {
        m_loadedMode = mode;
    }

    void clearLoaded()
    {
        m_loadedMode = GameUiMode::None;
    }

    bool isOpen() const
    {
        return m_mode != GameUiMode::None;
    }

    bool isMode(GameUiMode mode) const
    {
        return m_mode == mode;
    }

    void forceMode(GameUiMode mode)
    {
        m_mode = mode;
    }

    bool open(GameUiMode mode)
    {
        if (mode == GameUiMode::None)
            return close();

        if (m_mode == mode)
            return false;

        m_mode = mode;
        return true;
    }

    bool close()
    {
        if (m_mode == GameUiMode::None)
            return false;

        m_mode = GameUiMode::None;
        return true;
    }

    bool toggle(GameUiMode mode)
    {
        if (m_mode == mode)
            return close();

        return open(mode);
    }

    bool consumeF9Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f9Latch = false;
            return false;
        }

        if (m_f9Latch)
            return false;

        m_f9Latch = true;
        return true;
    }

    bool consumeF11Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f11Latch = false;
            return false;
        }

        if (m_f11Latch)
            return false;

        m_f11Latch = true;
        return true;
    }

    bool consumeF10Press(bool physicallyDown)
    {
        if (!physicallyDown)
        {
            m_f10Latch = false;
            return false;
        }

        if (m_f10Latch)
            return false;

        m_f10Latch = true;
        return true;
    }
        

private:
    GameUiMode m_mode = GameUiMode::None;
    GameUiMode m_loadedMode = GameUiMode::None;

    bool m_f9Latch = false;
    bool m_f11Latch = false;
    bool m_f10Latch = false;
};



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
    void openGameUi(GameUiMode mode);
    void closeGameUi();
    void toggleSystemMapUi();

    GameUiMode gameUiMode() const;
    bool isGameUiOpen() const;

    void evalGameUiScript(const std::string& script);

    void startLocalGameSession();
    void stopGameSession();
    game::host::LocalGameSession& localGameSession();
    const game::host::LocalGameSession& localGameSession() const;

private:
    void init();
    void mainLoop();
    void shutdown();
    void navigateGameUi(GameUiMode mode);

private:
    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    void handleResize(int width, int height);
    
    bool m_running;
    Renderer m_renderer;
    Window* m_window;
    StateContext m_context;
    std::unique_ptr<game::host::LocalGameSession> m_localGameSession;
    StateStack   m_states;
    RenderContext renderContext;
    HtmlUiManager m_htmlUi;

    bool m_pendingNewGameLoad = false;
    double m_newGameLoadStartTime = 0.0;

    GameUiController m_gameUi;
   
    #ifdef _WIN32
        GameWebView m_gameWebView;
    #endif
};
