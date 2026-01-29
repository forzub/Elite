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

    GLFWwindow* nativeHandle() const;
    

private:
    GLFWwindow* m_window;
};
