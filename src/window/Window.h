#pragma once

struct GLFWwindow;

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    bool shouldClose() const;
    void pollEvents();
    void swapBuffers();

    void focus();

    GLFWwindow* nativeHandle() const;

    #ifdef _WIN32
        void* nativeWin32Handle() const;
    #endif
    

private:
    GLFWwindow* m_window;
};
