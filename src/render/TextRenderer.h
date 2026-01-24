#pragma once

#include <string>
#include <glm/glm.hpp>

#include "render/Font.h"

class TextRenderer
{
public:
    static TextRenderer& instance();

    // ------------------------------------------------------------
    // lifecycle
    // ------------------------------------------------------------
    void init(); // ТОЛЬКО шейдеры и VAO/VBO
    void setViewportSize(int width, int height);

    int viewportWidth()  const { return m_viewportW; }
    int viewportHeight() const { return m_viewportH; }

    // ------------------------------------------------------------
    // NEW API (ПРАВИЛЬНЫЙ)
    // baseline = y
    // ------------------------------------------------------------
    void textDraw(
        const Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec3& color
    );

    // ------------------------------------------------------------
    // LEGACY API (НЕ ЛОМАЕМ ПРОЕКТ)
    // scale — ВРЕМЕННО
    // ------------------------------------------------------------
    void textDraw(
        const std::string& text,
        float x,
        float y,
        float scale,
        const glm::vec3& color
    );

private:
    TextRenderer() = default;

private:
    unsigned int m_vao    = 0;
    unsigned int m_vbo    = 0;
    unsigned int m_shader = 0;

    int m_viewportW = 1;
    int m_viewportH = 1;
};