#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Window.h"

#include <stdexcept>
#include "input/Input.h"

Window::Window(int width, int height, const char* title)
    : m_window(nullptr)
{
    fprintf(stderr, "[Window] ctor start\n");

    fprintf(stderr, "[Window] glfwInit...\n");
    if (!glfwInit())
    {
        fprintf(stderr, "[Window] glfwInit FAILED\n");
        throw std::runtime_error("Failed to initialize GLFW");
    }
    fprintf(stderr, "[Window] glfwInit OK\n");

    fprintf(stderr, "[Window] set hints\n");
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);

    fprintf(stderr, "[Window] create window...\n");
    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    fprintf(stderr, "[Window] glfwCreateWindow returned %p\n", (void*)m_window);

    if (!m_window)
    {
        fprintf(stderr, "[Window] window creation FAILED\n");
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    fprintf(stderr, "[Window] make context current\n");
    

    
        glfwMakeContextCurrent(m_window);

        // 1️⃣ СНАЧАЛА загружаем функции OpenGL
        if (!gladLoadGL(glfwGetProcAddress))
            throw std::runtime_error("GLAD init failed");

        // 2️⃣ ТОЛЬКО ПОТОМ используем gl*
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        glViewport(0, 0, fbWidth, fbHeight);
    


    fprintf(stderr, "[Window] glViewport OK\n");
    fprintf(stderr, "[Window] glEnable(GL_DEPTH_TEST)\n");
    
    glEnable(GL_DEPTH_TEST);
    
    fprintf(stderr, "[Window] glEnable OK\n");
    fprintf(stderr, "[Window] set key callback\n");
   
    glfwSetKeyCallback(
        m_window,
        [](GLFWwindow*, int key, int, int action, int)
        {
            if (action == GLFW_PRESS)
                Input::instance().setKey(key, true);
            else if (action == GLFW_RELEASE)
                Input::instance().setKey(key, false);
        }
    );
    fprintf(stderr, "[Window] key callback OK\n");

    fprintf(stderr, "[Window] set cursor pos callback\n");
    glfwSetCursorPosCallback(
        m_window,
        [](GLFWwindow*, double xpos, double ypos)
        {
            static double lastX = xpos;
            static double lastY = ypos;

            double dx = xpos - lastX;
            double dy = lastY - ypos;

            lastX = xpos;
            lastY = ypos;

            Input::instance().setMouseDelta(dx, dy);
        }
    );
    fprintf(stderr, "[Window] cursor callback OK\n");

    fprintf(stderr, "[Window] set mouse button callback\n");
    glfwSetMouseButtonCallback(
        m_window,
        [](GLFWwindow*, int button, int action, int)
        {
            if (action == GLFW_PRESS)
                Input::instance().setMouseButton(button, true);
            else if (action == GLFW_RELEASE)
                Input::instance().setMouseButton(button, false);
        }
    );
    fprintf(stderr, "[Window] mouse callback OK\n");

    fprintf(stderr, "[Window] ctor END\n");
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



