#pragma once

#ifdef _WIN32

#include <functional>
#include <string>
#include <mutex>
#include <queue>

class GameWebView
{
public:
    using CommandCallback = std::function<void(const std::string& command)>;

public:
    GameWebView();
    ~GameWebView();

    void start(void* parentHwnd, const std::string& title, int width, int height, const std::string& htmlFile);
    void stop();

    void setCommandCallback(CommandCallback cb);

    bool pollCommand(std::string& outCommand);
    void resize(int width, int height);
    void setBounds(int x, int y, int width, int height);

    void navigate(const std::string& htmlFile);
    void evalScript(const std::string& script);

    void setVisible(bool visible);
    void bringToFront();
    void focus();

private:
    static std::string filePathToUri(const std::string& path);

private:
    bool m_running = false;
    bool m_comInitialized = false;

    std::mutex m_mutex;
    std::queue<std::string> m_commands;

    CommandCallback m_commandCallback;
    void* m_parentHwnd = nullptr;
    void* m_webviewHwnd = nullptr;
    void* m_webviewObject = nullptr;
};

#endif
