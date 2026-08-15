#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
input_cpp = (ROOT / "src/input/Input.cpp").read_text(encoding="utf-8")
window_cpp = (ROOT / "src/window/Window.cpp").read_text(encoding="utf-8")

errors = []

if "for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)" not in input_cpp:
    errors.append("Input polling must start at GLFW_KEY_SPACE; 0..31 are invalid glfwGetKey tokens")

update_body = input_cpp.split("void Input::update", 1)[1].split("void Input::reset", 1)[0]
if "for (int key = 0; key <= GLFW_KEY_LAST; ++key)" in update_body:
    errors.append("Input::update must not poll GLFW key codes 0..31")

if "glfwGetWindowAttrib(m_window, GLFW_SRGB_CAPABLE)" in window_cpp:
    errors.append("GLFW_SRGB_CAPABLE is a framebuffer hint, not a glfwGetWindowAttrib token")

if "void Window::pollEvents()" not in window_cpp or "void Window::swapBuffers()" not in window_cpp:
    errors.append("Window::pollEvents contract boundary is missing")
else:
    poll_body = window_cpp.split("void Window::pollEvents()", 1)[1].split("void Window::swapBuffers()", 1)[0]

    if "#ifdef _WIN32" not in poll_body or "#else" not in poll_body:
        errors.append("Window::pollEvents must keep an explicit Win32/non-Windows split")
    else:
        win32_branch = poll_body.split("#ifdef _WIN32", 1)[1].split("#else", 1)[0]
        non_windows_branch = poll_body.split("#else", 1)[1]

        if "PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)" not in win32_branch:
            errors.append("Win32 client event pumping must use the local Win32 queue directly")
        if "TranslateMessage(&message)" not in win32_branch:
            errors.append("Win32 client event pump must translate queued keyboard messages")
        if "DispatchMessageW(&message)" not in win32_branch:
            errors.append("Win32 client event pump must dispatch messages into GLFW's installed WndProc")
        if "glfwPollEvents();" in win32_branch:
            errors.append("Win32 event pump must not call GLFW 3.4's unsafe foreign-active-window post-poll path")
        if "glfwPollEvents();" not in non_windows_branch:
            errors.append("non-Windows platforms must retain normal glfwPollEvents behavior")

if errors:
    for error in errors:
        print(f"[FAIL] {error}")
    raise SystemExit(1)

print("[PASS] GLFW input API + race-free Win32 event-pump contract")
