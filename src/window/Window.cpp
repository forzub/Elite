#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "Window.h"
#include <iostream>
#include <stdexcept>


Window::Window(int width, int height, const char* title)
    : m_window(nullptr)
{
 
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


            if (glfwGetWindowAttrib(m_window, GLFW_SRGB_CAPABLE))
                std::cout << "sRGB framebuffer OK\n";
            else
                std::cout << "NO sRGB framebuffer\n";



    if (!m_window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

        glfwMakeContextCurrent(m_window);

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

GLFWwindow* Window::nativeHandle() const
{
    return m_window;
}



