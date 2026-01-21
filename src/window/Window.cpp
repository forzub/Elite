#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Window.h"

#include <stdexcept>


Window::Window(int width, int height, const char* title)
    : m_window(nullptr)
{
 
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    if (!m_window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    

    
        glfwMakeContextCurrent(m_window);

        // 1️⃣ СНАЧАЛА загружаем функции OpenGL
        if (!gladLoadGL(glfwGetProcAddress))
            throw std::runtime_error("GLAD init failed");

        // 2️⃣ ТОЛЬКО ПОТОМ используем gl*
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
    


    
    glEnable(GL_DEPTH_TEST);
    
   
    // glfwSetKeyCallback(
    //     m_window,
    //     [](GLFWwindow*, int key, int, int action, int)
    //     {
    //         if (action == GLFW_PRESS)
    //             Input::instance().setKey(key, true);
    //         else if (action == GLFW_RELEASE)
    //             Input::instance().setKey(key, false);
    //     }
    // );

    // glfwSetCursorPosCallback(
    //     m_window,
    //     [](GLFWwindow*, double xpos, double ypos)
    //     {
    //         static double lastX = xpos;
    //         static double lastY = ypos;

    //         double dx = xpos - lastX;
    //         double dy = lastY - ypos;

    //         lastX = xpos;
    //         lastY = ypos;

    //         Input::instance().setMouseDelta(dx, dy);
    //     }
    // );

    // glfwSetMouseButtonCallback(
    //     m_window,
    //     [](GLFWwindow*, int button, int action, int)
    //     {
    //         if (action == GLFW_PRESS)
    //             Input::instance().setMouseButton(button, true);
    //         else if (action == GLFW_RELEASE)
    //             Input::instance().setMouseButton(button, false);
    //     }
    // );

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

GLFWwindow* Window::nativeHandle() const
{
    return m_window;
}



