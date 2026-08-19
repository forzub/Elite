#include <glad/gl.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Window.h"
#include <iostream>
#include <cstdint>
#include <stdexcept>
#include "input/Input.h"

namespace
{
void glfwDiagnosticErrorCallback(int code, const char* description)
{
    std::cerr << "[GLFW] error code=" << code
              << " description=" << (description ? description : "<null>")
              << "\n";
}

void glfwDiagnosticCloseCallback(GLFWwindow*)
{
#ifdef _WIN32
    std::cerr << "[GLFW] window close requested pid="
              << GetCurrentProcessId() << "\n";
#else
    std::cerr << "[GLFW] window close requested\n";
#endif
}
}


Window::Window(int width, int height, const char* title)
    : m_window(nullptr)
{
 
    glfwSetErrorCallback(glfwDiagnosticErrorCallback);

    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_SAMPLES, 4);

    // Do not expose the native HWND before the first dark framebuffer exists.
    // A visible GLFW window is otherwise allowed to show the Win32 default
    // background for one compositor frame while OpenGL/WebView2 are still
    // initializing, which is the white startup rectangle seen by users.
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (m_window)
    {
        glfwSetWindowCloseCallback(m_window, glfwDiagnosticCloseCallback);
        // The service/account shell is height-responsive, but below this
        // baseline forms become a usability problem rather than a layout
        // problem. Keep the ordinary desktop client within the supported
        // interactive envelope and let WebUI scale proportionally above it.
        glfwSetWindowSizeLimits(
            m_window,
            800,
            600,
            GLFW_DONT_CARE,
            GLFW_DONT_CARE
        );
    }




    if (!m_window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

        glfwMakeContextCurrent(m_window);

        glfwSetScrollCallback(m_window, Input::scrollCallback);

        // 1️⃣ СНАЧАЛА загружаем функции OpenGL
        if (!gladLoadGL(glfwGetProcAddress))
            throw std::runtime_error("GLAD init failed");

        glEnable(GL_MULTISAMPLE);
        
        // 2️⃣ ТОЛЬКО ПОТОМ используем gl*
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
    
    
    glEnable(GL_DEPTH_TEST);
    

}

Window::~Window()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window);
}

void Window::pollEvents()
{
#ifdef _WIN32
    const ULONGLONG xprocBeginMs = GetTickCount64();
    std::uint64_t xprocMessageCount = 0;
    // GLFW 3.4 performs an unchecked GetActiveWindow() ->
    // GetPropW(hwnd, L"GLFW") -> _GLFWwindow* dereference after dispatching
    // messages. With embedded WebView2, Windows can report a GLFW HWND owned
    // by another EliteGame process as active on this input queue. The property
    // value is then a pointer in that foreign address space and GLFW crashes.
    //
    // Pump the Win32 queue directly instead. DispatchMessageW still invokes
    // GLFW's installed WndProc for this process, preserving ordinary window,
    // keyboard, mouse, focus, resize and close callbacks while avoiding the
    // unsafe GLFW 3.4 post-poll foreign-pointer path entirely.
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        ++xprocMessageCount;
        if (message.message == WM_QUIT)
        {
            // EliteGame owns one GLFW top-level window per client process.
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
            continue;
        }

        if ((message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN) &&
            message.wParam == VK_ESCAPE)
        {
            m_escapePressed = true;
        }

        TranslateMessage(&message);

        const ULONGLONG dispatchBeginMs = GetTickCount64();
        const UINT dispatchedMessage = message.message;
        const HWND dispatchedHwnd = message.hwnd;
        DWORD targetPid = 0;
        DWORD targetTid = 0;
        wchar_t className[128] = L"<thread-message>";
        if (dispatchedHwnd)
        {
            targetTid = GetWindowThreadProcessId(dispatchedHwnd, &targetPid);
            if (GetClassNameW(dispatchedHwnd, className, 128) <= 0)
                lstrcpyW(className, L"<unknown>");
        }

        DispatchMessageW(&message);

        const ULONGLONG dispatchDurationMs =
            GetTickCount64() - dispatchBeginMs;
        if (dispatchDurationMs >= 100)
        {
            std::wcerr
                << L"[M8E-XPROC][dispatch] pid=" << GetCurrentProcessId()
                << L" msg=0x" << std::hex << dispatchedMessage
                << L" hwnd=0x"
                << reinterpret_cast<std::uintptr_t>(dispatchedHwnd)
                << L" wparam=0x" << static_cast<std::uintptr_t>(message.wParam)
                << L" lparam=0x" << static_cast<std::uintptr_t>(message.lParam)
                << std::dec
                << L" class=" << className
                << L" target_pid=" << targetPid
                << L" target_tid=" << targetTid
                << L" duration_ms=" << dispatchDurationMs
                << L" uptime_ms=" << GetTickCount64()
                << L" tid=" << GetCurrentThreadId()
                << L"\n";
        }
    }

    // WebView2 may keep keyboard focus on a child HWND whose key routing does
    // not update GLFW's key table. Poll the physical Escape key for this
    // foreground process as a second source and edge-latch it exactly once.
    if (ownsForegroundInput())
    {
        const bool escapeDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (escapeDown && !m_escapeDown)
            m_escapePressed = true;
        m_escapeDown = escapeDown;
    }
    else
    {
        m_escapeDown = false;
    }

    const ULONGLONG xprocDurationMs = GetTickCount64() - xprocBeginMs;
    if (xprocDurationMs >= 100)
    {
        std::cerr
            << "[M8E-XPROC][win32-pump] pid=" << GetCurrentProcessId()
            << " duration_ms=" << xprocDurationMs
            << " messages=" << xprocMessageCount
            << " uptime_ms=" << GetTickCount64()
            << " tid=" << GetCurrentThreadId()
            << "\n";
    }
#else
    glfwPollEvents();
#endif
}

bool Window::consumeEscapePressed()
{
#ifdef _WIN32
    const bool pressed = m_escapePressed;
    m_escapePressed = false;
    return pressed;
#else
    return false;
#endif
}


bool Window::ownsForegroundInput() const
{
#ifdef _WIN32
    const HWND foreground = GetForegroundWindow();
    if (!foreground || !m_window)
        return false;

    const HWND gameHwnd = glfwGetWin32Window(m_window);
    if (!gameHwnd)
        return false;

    // WebView2 can place keyboard focus on a child HWND owned by a helper
    // process. In that case comparing only foregroundPid with EliteGame's PID
    // incorrectly says that this client has no foreground input until the user
    // clicks the GLFW surface. Treat any foreground window rooted in our game
    // HWND as belonging to this graphical client, regardless of which WebView2
    // helper process owns the focused child.
    if (foreground == gameHwnd ||
        IsChild(gameHwnd, foreground) ||
        GetAncestor(foreground, GA_ROOT) == gameHwnd ||
        GetAncestor(foreground, GA_ROOTOWNER) == gameHwnd)
    {
        return true;
    }

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return foregroundPid == GetCurrentProcessId();
#else
    return m_window &&
        glfwGetWindowAttrib(m_window, GLFW_FOCUSED) == GLFW_TRUE;
#endif
}

void Window::swapBuffers()
{
#ifdef _WIN32
    const ULONGLONG xprocBeginMs = GetTickCount64();
#endif
    glfwSwapBuffers(m_window);
#ifdef _WIN32
    // WebView2 may keep keyboard focus on a child HWND whose key routing does
    // not update GLFW's key table. Poll the physical Escape key for this
    // foreground process as a second source and edge-latch it exactly once.
    if (ownsForegroundInput())
    {
        const bool escapeDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (escapeDown && !m_escapeDown)
            m_escapePressed = true;
        m_escapeDown = escapeDown;
    }
    else
    {
        m_escapeDown = false;
    }

    const ULONGLONG xprocDurationMs = GetTickCount64() - xprocBeginMs;
    if (xprocDurationMs >= 100)
    {
        std::cerr
            << "[M8E-XPROC][swap] pid=" << GetCurrentProcessId()
            << " duration_ms=" << xprocDurationMs
            << " uptime_ms=" << GetTickCount64()
            << " tid=" << GetCurrentThreadId()
            << "\n";
    }
#endif
}


void Window::show()
{
    if (!m_window)
        return;

    glfwShowWindow(m_window);
}

void Window::focus()
{
    if (!m_window)
        return;

#ifdef _WIN32
    // Never let a background EliteGame process steal foreground ownership from
    // another graphical client. Session/bootstrap completion is asynchronous
    // from the user's window switching, so unconditional SetForegroundWindow
    // here makes two independent clients appear mutually blocking.
    const HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foreground)
        GetWindowThreadProcessId(foreground, &foregroundPid);

    if (foregroundPid != GetCurrentProcessId())
        return;

    glfwFocusWindow(m_window);

    HWND hwnd = glfwGetWin32Window(m_window);
    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOW);
        SetActiveWindow(hwnd);
        SetFocus(hwnd);
    }
#else
    glfwFocusWindow(m_window);
#endif
}



GLFWwindow* Window::nativeHandle() const
{
    return m_window;
}


#ifdef _WIN32
void* Window::nativeWin32Handle() const
{
    return glfwGetWin32Window(m_window);
}
#endif
