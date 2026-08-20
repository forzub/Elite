#pragma once

#include <array>
#include <deque>

struct GLFWwindow;

class Window
{
public:
    struct FunctionKeyPress
    {
        int functionKey = 0;
        bool ctrlDown = false;
        bool altDown = false;
    };

    Window(int width, int height, const char* title);
    ~Window();

    bool shouldClose() const;
    void pollEvents();
    bool consumeEscapePressed();
    bool pollFunctionKeyPress(FunctionKeyPress& outPress);
    bool ownsForegroundInput() const;
    void swapBuffers();
    void show();
    void hide();
    void clientSize(int& width, int& height) const;

    void focus();

    GLFWwindow* nativeHandle() const;

    #ifdef _WIN32
        void* nativeWin32Handle() const;
    #endif


private:
    GLFWwindow* m_window;
    bool m_escapePressed = false;
    bool m_escapeDown = false;
    std::array<bool, 12> m_functionKeyDown{};
    std::deque<FunctionKeyPress> m_functionKeyPresses;
};
