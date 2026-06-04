#pragma once

#ifdef _WIN32

#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
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

private:
    void threadMain(void* parentHwnd, std::string title, int width, int height, std::string htmlFile);
    static std::string filePathToUri(const std::string& path);

private:
    std::thread m_thread;
    std::atomic<bool> m_running{false};

    std::mutex m_mutex;
    std::queue<std::string> m_commands;

    CommandCallback m_commandCallback;
    void* m_parentHwnd = nullptr;
    void* m_webviewHwnd = nullptr;

    std::mutex m_webviewMutex;
    void* m_webviewObject = nullptr;
};

#endif