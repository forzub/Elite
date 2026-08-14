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

if errors:
    for error in errors:
        print(f"[FAIL] {error}")
    raise SystemExit(1)

print("[PASS] GLFW input polling and framebuffer-hint usage are API-valid")
