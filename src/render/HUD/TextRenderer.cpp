
// // 1. Получение экземпляра
// // TextRenderer& tr = TextRenderer::instance();

// // 2. Инициализация (ОБЯЗАТЕЛЬНО)
// // TextRenderer::instance().init(fontPath);

// // Где вызывать
// // один раз в Application::init() после создания OpenGL-контекста (Window)
// // Параметры
// // const std::string& fontPath - абсолютный путь к .ttf

// // 3. Рисование текста (HUD / UI)
// // TextRenderer::instance().draw(
// //     text,
// //     x,
// //     y,
// //     scale,
// //     color
// // );

// // text	    Строка ASCII	    символы 32–127
// // x	        X-позиция (px)	    0 … screenWidth
// // y	        Y-позиция (px)	    0 … screenHeight
// // scale	    Масштаб	> 0.0f      (обычно 0.2 – 1.0)
// // color	    Цвет RGB	        0.0 – 1.0

// // Включён blending:

// // glEnable(GL_BLEND);
// // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#include "core/Log.h"
#include "core/StateContext.h"

#include "render/HUD/TextRenderer.h"
#include "render/ShaderUtils.h"

#include <glad/gl.h>
#include <iostream>




// ================================================================
// singleton
// ================================================================
TextRenderer& TextRenderer::instance()
{
    static TextRenderer inst;
    return inst;
}



void TextRenderer::init()
{
    // -------------------------------------------------
    // VAO / VBO
    // -------------------------------------------------
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float) * 6 * 4,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        4,
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(float),
        nullptr
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // -------------------------------------------------
    // Shader program (FROM FILES)
    // -------------------------------------------------
    m_shader = compileShaderFromFiles(
        "assets/shaders/hud/textRenderer.vert",
        "assets/shaders/hud/textRenderer.frag"
    );

    if (!m_shader)
    {
        std::cerr << "[TextRenderer] Failed to load text shaders\n";
        return;
    }

    // -------------------------------------------------
    // Shader defaults
    // -------------------------------------------------
    glUseProgram(m_shader);
    glUniform1i(
        glGetUniformLocation(m_shader, "text"),
        0
    );
    glUseProgram(0);
}

// ================================================================
// viewport
// ================================================================
// void TextRenderer::setViewportSize(int w, int h)
// {
//     m_viewportW = (w > 0) ? w : 1;
//     m_viewportH = (h > 0) ? h : 1;
// }






// ================================================================
// NEW API — Font based
// baselineY = baseline
// ================================================================
void TextRenderer::textDraw(
    const Font& font,
    const std::string& text,
    float x,
    float baselineY,
    const glm::vec4& color   // ← RGBA
)
{
    LOG("[TextRenderer] textDraw begin");
    LOG("  shader = " << m_shader);
    LOG("  vao = " << m_vao << " vbo = " << m_vbo);

    

    glUseProgram(m_shader);
        glUniform4f(
        glGetUniformLocation(m_shader, "textColor"),
        color.r, color.g, color.b, color.a
    );

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_vao);

    // float screenW = static_cast<float>(m_viewportW);
    // float screenH = static_cast<float>(m_viewportH);

    extern StateContext* g_stateContext;
    const Viewport& vp = g_stateContext->viewport();

    float screenW = static_cast<float>(vp.width);
    float screenH = static_cast<float>(vp.height);

    LOG("[TextRenderer] text length = " << text.size());

    for (char c : text)
    {
        LOG("  glyph char = '" << c << "' (" << int(c) << ")");
        const Character& ch = font.glyph(c);
        LOG("    textureID = " << ch.textureID);

        float xpos = x + ch.bearing.x;
        float ypos = baselineY - ch.bearing.y;

        float w = static_cast<float>(ch.size.x);
        float h = static_cast<float>(ch.size.y);

        float x0 =  (2.0f * xpos / screenW) - 1.0f;
        float y0 =  1.0f - (2.0f * ypos / screenH);
        float x1 =  (2.0f * (xpos + w) / screenW) - 1.0f;
        float y1 =  1.0f - (2.0f * (ypos + h) / screenH);

        float vertices[6][4] = {
            { x0, y1, 0.0f, 1.0f },
            { x0, y0, 0.0f, 0.0f },
            { x1, y0, 1.0f, 0.0f },
            { x0, y1, 0.0f, 1.0f },
            { x1, y0, 1.0f, 0.0f },
            { x1, y1, 1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, ch.textureID);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += static_cast<float>(ch.advance >> 6);
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

// ================================================================
// LEGACY API — scale based (НЕ ЛОМАЕМ ПРОЕКТ)
// ================================================================
void TextRenderer::textDraw(
    const std::string& text,
    float x,
    float y,
    float scale,
    const glm::vec3& color
)
{
    static Font* fallbackFont = nullptr;

    if (!fallbackFont)
    {
        // один раз — временный костыль
        fallbackFont = new Font("assets/fonts/Roboto-Light.ttf", 48);
    }

    // эмулируем старый scale через baseline
    textDraw(
        *fallbackFont,
        text,
        x,
        y,
        color
    );
}