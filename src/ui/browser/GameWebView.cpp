#ifdef _WIN32

#include "ui/browser/GameWebView.h"
#include "src/core/RuntimeTrace.h"

#include <filesystem>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <windows.h>
#include <objbase.h>

#include <webview/webview.h>

GameWebView::GameWebView() = default;

GameWebView::~GameWebView()
{
    stop();
}

static bool isUri(const std::string& value)
{
    return value.rfind("http://", 0) == 0 ||
           value.rfind("https://", 0) == 0 ||
           value.rfind("file://", 0) == 0;
}

static std::string hresultHex(HRESULT hr)
{
    std::ostringstream out;
    out << "0x" << std::hex << std::uppercase
        << static_cast<unsigned long>(hr);
    return out.str();
}

static bool currentProcessOwnsForegroundWindow()
{
    const HWND foreground = GetForegroundWindow();
    if (!foreground)
        return false;

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return foregroundPid == GetCurrentProcessId();
}

static std::filesystem::path configureProcessLocalWebView2UserDataFolder()
{
    // WebView2 otherwise gives multiple EliteGame.exe host processes the same
    // default user-data folder/session. A game client must own an independent
    // browser session just like it owns an independent HWND and HTTP endpoint.
    wchar_t localAppData[32768] = {};
    constexpr DWORD LocalAppDataCapacity =
        static_cast<DWORD>(sizeof(localAppData) / sizeof(localAppData[0]));
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        localAppData,
        LocalAppDataCapacity
    );

    std::filesystem::path root;
    if (length > 0 && length < LocalAppDataCapacity)
        root = std::filesystem::path(localAppData);
    else
        root = std::filesystem::temp_directory_path();

    const std::filesystem::path udf =
        root / "EliteGame" / "WebView2Sessions" /
        std::to_wstring(GetCurrentProcessId());

    std::filesystem::create_directories(udf);

    if (!SetEnvironmentVariableW(
            L"WEBVIEW2_USER_DATA_FOLDER",
            udf.c_str()))
    {
        throw std::runtime_error(
            "GameWebView failed to set process-local WEBVIEW2_USER_DATA_FOLDER"
        );
    }

    return udf;
}

void GameWebView::start(
    void* parentHwnd,
    const std::string& title,
    int width,
    int height,
    const std::string& htmlFile)
{
    const auto xprocStartBegin = std::chrono::steady_clock::now();
    if (m_running)
        return;

    if (!parentHwnd)
        throw std::runtime_error("GameWebView requires a valid parent HWND");

    const std::filesystem::path webView2Udf =
        configureProcessLocalWebView2UserDataFolder();

    if (core::runtimeTraceEnabled())
        std::cerr << "[GameWebView] pid=" << GetCurrentProcessId()
                  << " WebView2 UDF=" << webView2Udf.string() << "\n";

    // Embedded WebView2 must live on the same STA/UI thread that owns the
    // GLFW Win32 window. The webview backend initializes COM for windows it
    // owns itself, but when an existing HWND is supplied the caller owns both
    // COM and application lifecycle.
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(comResult))
    {
        throw std::runtime_error(
            "GameWebView CoInitializeEx(COINIT_APARTMENTTHREADED) failed: " +
            hresultHex(comResult)
        );
    }
    m_comInitialized = true;

    try
    {
        auto* w = new webview::webview(false, parentHwnd);

        HWND widgetHwnd = nullptr;
        try
        {
            widgetHwnd = reinterpret_cast<HWND>(w->widget().value());
        }
        catch (const std::exception& e)
        {
            delete w;
            throw std::runtime_error(
                std::string("GameWebView widget() failed: ") + e.what()
            );
        }

        if (!widgetHwnd)
        {
            delete w;
            throw std::runtime_error("GameWebView embedded widget HWND is null");
        }

        m_parentHwnd = parentHwnd;
        m_webviewHwnd = widgetHwnd;
        m_webviewObject = w;
        m_running = true;

        if (core::runtimeTraceEnabled())
            std::cerr << "[GameWebView] embedded pid=" << GetCurrentProcessId()
                      << " parent_hwnd=" << static_cast<HWND>(parentHwnd)
                      << " widget_hwnd=" << widgetHwnd
                      << " title=\"" << title << "\"\n";

        // The backend creates the widget as a real WS_CHILD of parentHwnd.
        // Do not create/re-style/re-parent a second top-level window.
        MoveWindow(widgetHwnd, 0, 0, width, height, TRUE);

        w->bind(
            "gameCommand",
            [this, w](const std::string& seq, const std::string& req, void* arg)
            {
                (void)arg;

                // req arrives as a JSON argument array, e.g. ["new_local_game"].
                std::string command = req;
                if (command.size() >= 4 && command.front() == '[')
                {
                    const auto firstQuote = command.find('"');
                    const auto secondQuote = command.find('"', firstQuote + 1);
                    if (firstQuote != std::string::npos &&
                        secondQuote != std::string::npos)
                    {
                        command = command.substr(
                            firstQuote + 1,
                            secondQuote - firstQuote - 1
                        );
                    }
                }

                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_commands.push(command);
                }

                w->resolve(seq, 0, "{}");
            },
            nullptr
        );

        std::string uri;
        if (isUri(htmlFile))
        {
            uri = htmlFile;
        }
        else
        {
            const std::filesystem::path htmlPath =
                std::filesystem::absolute(htmlFile);

            std::cout << "[GameWebView] HTML path: " << htmlPath.string() << "\n";
            std::cout << "[GameWebView] HTML exists: "
                      << (std::filesystem::exists(htmlPath) ? "YES" : "NO")
                      << "\n";

            if (!std::filesystem::exists(htmlPath))
            {
                w->set_html(R"HTML(
                    <!doctype html>
                    <html>
                    <body style="background:#111;color:#eee;font-family:Arial;padding:32px">
                        <h1>GameWebView works</h1>
                        <p>HTML file not found.</p>
                    </body>
                    </html>
                )HTML");
                return;
            }

            uri = filePathToUri(htmlPath.string());
        }

        std::cout << "[GameWebView] navigate initial: " << uri << "\n";
        w->navigate(uri);

        const double xprocStartMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - xprocStartBegin
        ).count();
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-XPROC][webview] pid=" << GetCurrentProcessId()
                << " op=start"
                << " duration_ms=" << xprocStartMs
                << " uptime_ms=" << GetTickCount64()
                << " tid=" << GetCurrentThreadId()
                << "\n";

        // No w->run() here. The embedded WebView belongs to the GLFW Win32 UI
        // thread and glfwPollEvents() is the application-owned message pump.
    }
    catch (...)
    {
        m_running = false;
        m_webviewObject = nullptr;
        m_webviewHwnd = nullptr;
        m_parentHwnd = nullptr;

        if (m_comInitialized)
        {
            CoUninitialize();
            m_comInitialized = false;
        }
        throw;
    }
}

void GameWebView::stop()
{
    if (!m_running && !m_webviewObject)
    {
        if (m_comInitialized)
        {
            CoUninitialize();
            m_comInitialized = false;
        }
        return;
    }

    if (core::runtimeTraceEnabled())
        std::cerr << "[GameWebView] stop pid=" << GetCurrentProcessId()
                  << " parent_hwnd=" << static_cast<HWND>(m_parentHwnd)
                  << " widget_hwnd=" << static_cast<HWND>(m_webviewHwnd)
                  << "\n";

    auto* w = static_cast<webview::webview*>(m_webviewObject);

    m_running = false;
    m_webviewObject = nullptr;
    m_webviewHwnd = nullptr;
    m_parentHwnd = nullptr;

    // Embedded mode means destruction removes only the WebView child. The
    // parent GLFW HWND and application lifecycle remain owned by Application.
    delete w;

    if (m_comInitialized)
    {
        CoUninitialize();
        m_comInitialized = false;
    }
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
        if (parent && currentProcessOwnsForegroundWindow())
        {
            SetActiveWindow(parent);
            SetFocus(parent);
        }
    }
    else
    {
        // Showing/loading UI in a background client must not activate that
        // process. This is especially important while another EliteGame window
        // is already in gameplay and a second client is authenticating.
        SetWindowPos(
            hwnd,
            HWND_TOP,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
        );

        if (currentProcessOwnsForegroundWindow())
            SetFocus(hwnd);
    }
}

void GameWebView::navigate(const std::string& htmlFile)
{
    auto* w = static_cast<webview::webview*>(m_webviewObject);
    if (!w)
        return;

    std::string uri;
    if (isUri(htmlFile))
    {
        uri = htmlFile;
    }
    else
    {
        const std::filesystem::path htmlPath =
            std::filesystem::absolute(htmlFile);
        uri = filePathToUri(htmlPath.string());
    }

    std::cout << "[GameWebView] navigate request: " << uri << "\n";
    const auto xprocBegin = std::chrono::steady_clock::now();
    w->navigate(uri);
    const double xprocDurationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - xprocBegin
    ).count();
    if (xprocDurationMs >= 100.0)
    {
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-XPROC][webview] pid=" << GetCurrentProcessId()
                << " op=navigate"
                << " duration_ms=" << xprocDurationMs
                << " uptime_ms=" << GetTickCount64()
                << " tid=" << GetCurrentThreadId()
                << "\n";
    }
}

void GameWebView::evalScript(const std::string& script)
{
    auto* w = static_cast<webview::webview*>(m_webviewObject);
    if (!w)
        return;

    const auto xprocBegin = std::chrono::steady_clock::now();
    w->eval(script);
    const double xprocDurationMs = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - xprocBegin
    ).count();
    if (xprocDurationMs >= 100.0)
    {
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-XPROC][webview] pid=" << GetCurrentProcessId()
                << " op=eval"
                << " duration_ms=" << xprocDurationMs
                << " uptime_ms=" << GetTickCount64()
                << " tid=" << GetCurrentThreadId()
                << "\n";
    }
}

std::string GameWebView::filePathToUri(const std::string& path)
{
    const std::filesystem::path absPath = std::filesystem::absolute(path);
    const std::string s = absPath.generic_string();

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
