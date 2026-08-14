#include <glad/gl.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include "Window.h"
#include <iostream>
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (m_window)
        glfwSetWindowCloseCallback(m_window, glfwDiagnosticCloseCallback);




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
    glfwPollEvents();
}

void Window::swapBuffers()
{
    glfwSwapBuffers(m_window);
}


void Window::focus()
{
    if (!m_window)
        return;

    glfwFocusWindow(m_window);

#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);

    if (hwnd)
    {
        ShowWindow(hwnd, SW_SHOW);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        SetActiveWindow(hwnd);
        SetFocus(hwnd);
    }
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
