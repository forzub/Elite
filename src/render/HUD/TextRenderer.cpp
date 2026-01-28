
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


#include "render/HUD/TextRenderer.h"

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

// ================================================================
// init
// ================================================================
void TextRenderer::init()
{
    // ---------------- VAO / VBO ----------------
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // ---------------- shaders ----------------
    const char* vs = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex;
        out vec2 TexCoords;
        void main()
        {
            gl_Position = vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* fs = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 FragColor;

        uniform sampler2D text;
        uniform vec4 textColor;   // ← теперь vec4

        void main()
        {
            float glyphAlpha = texture(text, TexCoords).a;

            // итоговая альфа = глиф * visibility
            FragColor = vec4(
                textColor.rgb,
                glyphAlpha * textColor.a
            );
        }
    )";

    auto compile = [](GLenum type, const char* src) -> GLuint
    {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);

        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(s, 512, nullptr, log);
            std::cerr << "Shader error:\n" << log << "\n";
        }
        return s;
    };

    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);

    m_shader = glCreateProgram();
    glAttachShader(m_shader, v);
    glAttachShader(m_shader, f);
    glLinkProgram(m_shader);

    glDeleteShader(v);
    glDeleteShader(f);

    glUseProgram(m_shader);
    glUniform1i(glGetUniformLocation(m_shader, "text"), 0);
    glUseProgram(0);
}

// ================================================================
// viewport
// ================================================================
void TextRenderer::setViewportSize(int w, int h)
{
    m_viewportW = (w > 0) ? w : 1;
    m_viewportH = (h > 0) ? h : 1;
}

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
    glUseProgram(m_shader);
        glUniform4f(
        glGetUniformLocation(m_shader, "textColor"),
        color.r, color.g, color.b, color.a
    );

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_vao);

    float screenW = static_cast<float>(m_viewportW);
    float screenH = static_cast<float>(m_viewportH);

    for (char c : text)
    {
        const Character& ch = font.glyph(c);

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