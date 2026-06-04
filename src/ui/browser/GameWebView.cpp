#ifdef _WIN32

#include "ui/browser/GameWebView.h"

#include <filesystem>
#include <iostream>
#include <windows.h>

#include <webview/webview.h>

GameWebView::GameWebView() = default;

GameWebView::~GameWebView()
{
    stop();
}

void GameWebView::start(void* parentHwnd, const std::string& title, int width, int height, const std::string& htmlFile)
{
    if (m_running)
        return;

    m_parentHwnd = parentHwnd;
    m_running = true;

    m_thread = std::thread(
        [this, parentHwnd, title, width, height, htmlFile]()
        {
            threadMain(parentHwnd, title, width, height, htmlFile);
        }
    );
}

void GameWebView::stop()
{
    m_running = false;

    if (m_thread.joinable())
        m_thread.detach();
}





static bool isUri(const std::string& value)
{
    return value.rfind("http://", 0) == 0 ||
           value.rfind("https://", 0) == 0 ||
           value.rfind("file://", 0) == 0;
}







void GameWebView::setCommandCallback(CommandCallback cb)
{
    m_commandCallback = std::move(cb);
}

bool GameWebView::pollCommand(std::string& outCommand)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_commands.empty())
        return false;

    outCommand = std::move(m_commands.front());
    m_commands.pop();
    return true;
}




void GameWebView::resize(int width, int height)
{
    HWND hwnd = static_cast<HWND>(m_webviewHwnd);

    if (!hwnd)
        return;

    MoveWindow(hwnd, 0, 0, width, height, TRUE);
}



void GameWebView::setBounds(int x, int y, int width, int height)
{
    HWND hwnd = static_cast<HWND>(m_webviewHwnd);

    if (!hwnd)
        return;

    MoveWindow(hwnd, x, y, width, height, TRUE);
}



void GameWebView::setVisible(bool visible)
{
    HWND hwnd = static_cast<HWND>(m_webviewHwnd);

    if (!hwnd)
        return;

    EnableWindow(hwnd, visible ? TRUE : FALSE);
    ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);

    if (!visible)
    {
        SetWindowPos(
            hwnd,
            HWND_BOTTOM,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );

        HWND parent = static_cast<HWND>(m_parentHwnd);
        if (parent)
        {
            SetForegroundWindow(parent);
            SetActiveWindow(parent);
            SetFocus(parent);
        }
    }
    else
    {
        SetWindowPos(
            hwnd,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE
        );

        SetFocus(hwnd);
    }
}




void GameWebView::navigate(const std::string& htmlFile)
{
    void* raw = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_webviewMutex);
        raw = m_webviewObject;
    }

    if (!raw)
        return;

    auto* w = static_cast<webview::webview*>(raw);

    std::string uri;

    if (isUri(htmlFile))
    {
        uri = htmlFile;
    }
    else
    {
        std::filesystem::path htmlPath =
            std::filesystem::absolute(htmlFile);

        uri = filePathToUri(htmlPath.string());
    }

    std::cout << "[GameWebView] navigate request: " << uri << "\n";

    w->dispatch([w, uri]()
    {
        w->navigate(uri);
    });
}




void GameWebView::evalScript(const std::string& script)
{
    void* raw = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_webviewMutex);
        raw = m_webviewObject;
    }

    if (!raw)
        return;

    auto* w = static_cast<webview::webview*>(raw);

    w->dispatch([w, script]()
    {
        w->eval(script);
    });
}














void GameWebView::threadMain(void* parentHwnd, std::string title, int width, int height, std::string htmlFile)
{
    try
    {
        webview::webview w(false, nullptr);

        {
            std::lock_guard<std::mutex> lock(m_webviewMutex);
            m_webviewObject = &w;
        }

        w.set_title(title);
        w.set_size(width, height, WEBVIEW_HINT_NONE);


            HWND childHwnd = nullptr;

            try
            {
                childHwnd = reinterpret_cast<HWND>(w.window().value());
            }
            catch (const std::exception& e)
            {
                std::cout << "[GameWebView] w.window() failed: " << e.what() << "\n";
            }

            HWND parent = static_cast<HWND>(parentHwnd);

        m_webviewHwnd = childHwnd;

        std::cout << "[GameWebView] parent HWND: " << parent << "\n";
        std::cout << "[GameWebView] child HWND: " << childHwnd << "\n";

        if (childHwnd && parent)
        {
            LONG_PTR style = GetWindowLongPtr(childHwnd, GWL_STYLE);

            style &= ~WS_OVERLAPPEDWINDOW;
            style &= ~WS_POPUP;
            style |= WS_CHILD;

            SetWindowLongPtr(childHwnd, GWL_STYLE, style);
            SetParent(childHwnd, parent);

            SetWindowPos(
                childHwnd,
                HWND_TOP,
                0,
                0,
                width,
                height,
                SWP_FRAMECHANGED | SWP_SHOWWINDOW
            );

            std::cout << "[GameWebView] Attached to GLFW window\n";
        }
        else
        {
            std::cout << "[GameWebView] Attach failed: null HWND\n";
        }



        w.bind("gameCommand",
            [this, &w](const std::string& seq, const std::string& req, void* arg)
            {
                (void)arg;

                // req приходит как JSON-массив аргументов.
                // Например: ["new_game"]
                std::string command = req;

                if (command.size() >= 4 && command.front() == '[')
                {
                    auto firstQuote = command.find('"');
                    auto secondQuote = command.find('"', firstQuote + 1);

                    if (firstQuote != std::string::npos && secondQuote != std::string::npos)
                    {
                        command = command.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_commands.push(command);
                }

                w.resolve(seq, 0, "{}");
            },
            nullptr
        );

        std::string uri;

        if (isUri(htmlFile))
        {
            uri = htmlFile;

            std::cout << "[GameWebView] Navigate URI: " << uri << "\n";
            w.navigate(uri);
        }
        else
        {
            std::filesystem::path htmlPath =
                std::filesystem::absolute(htmlFile);

            std::cout << "[GameWebView] HTML path: " << htmlPath.string() << "\n";
            std::cout << "[GameWebView] HTML exists: "
                    << (std::filesystem::exists(htmlPath) ? "YES" : "NO")
                    << "\n";

            uri = filePathToUri(htmlPath.string());

            std::cout << "[GameWebView] Navigate file: " << uri << "\n";

            if (std::filesystem::exists(htmlPath))
            {
                w.navigate(uri);
            }
            else
            {
                w.set_html(R"HTML(
                    <!doctype html>
                    <html>
                    <body style="background:#111;color:#eee;font-family:Arial;padding:32px">
                        <h1>GameWebView works</h1>
                        <p>HTML file not found.</p>
                    </body>
                    </html>
                )HTML");
            }
        }

        w.run();

        std::cout << "[GameWebView] Closed\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "[GameWebView] Exception: " << e.what() << "\n";
    }


    {
        std::lock_guard<std::mutex> lock(m_webviewMutex);
        m_webviewObject = nullptr;
    }

    m_running = false;
}

std::string GameWebView::filePathToUri(const std::string& path)
{
    std::filesystem::path absPath = std::filesystem::absolute(path);
    std::string s = absPath.generic_string();

    // Минимальное экранирование пробелов.
    std::string encoded;
    encoded.reserve(s.size());

    for (char c : s)
    {
        if (c == ' ')
            encoded += "%20";
        else
            encoded += c;
    }

    if (encoded.size() >= 2 && encoded[1] == ':')
        return "file:///" + encoded;

    return "file://" + encoded;
}

#endif