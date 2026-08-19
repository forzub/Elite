#pragma once

struct GLFWwindow;

class Window
{
public:
    Window(int width, int height, const char* title);
    ~Window();

    bool shouldClose() const;
    void pollEvents();
    bool consumeEscapePressed();
    bool ownsForegroundInput() const;
    void swapBuffers();
    void show();

    void focus();

    GLFWwindow* nativeHandle() const;

    #ifdef _WIN32
        void* nativeWin32Handle() const;
    #endif


private:
    GLFWwindow* m_window;
    bool m_escapePressed = false;
    bool m_escapeDown = false;
};
