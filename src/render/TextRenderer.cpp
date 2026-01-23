
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


// #include "render/TextRenderer.h"

// #include <glad/gl.h>
// #include <ft2build.h>
// #include FT_FREETYPE_H

// #include <iostream>

// // =====================================================================================
// // Singleton
// // =====================================================================================
// TextRenderer& TextRenderer::instance()
// {
//     static TextRenderer inst;
//     return inst;
// }

// // =====================================================================================
// // Init
// // =====================================================================================
// void TextRenderer::init(const std::string& fontPath)
// {
//     std::cout << "Loading font: " << fontPath << std::endl;

//     // ---------------- FreeType ----------------
//     FT_Library ft;
//     if (FT_Init_FreeType(&ft))
//     {
//         std::cerr << "FreeType init failed\n";
//         return;
//     }

//     FT_Face face;
//     if (FT_New_Face(ft, fontPath.c_str(), 0, &face))
//     {
//         std::cerr << "Font load failed\n";
//         FT_Done_FreeType(ft);
//         return;
//     }

//     FT_Set_Pixel_Sizes(face, 0, 48);
//     glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

//     for (unsigned char c = 32; c < 128; ++c)
//     {
//         if (FT_Load_Char(face, c, FT_LOAD_RENDER))
//             continue;

//         GLuint tex = 0;
//         glGenTextures(1, &tex);
//         glBindTexture(GL_TEXTURE_2D, tex);

//         glTexImage2D(
//             GL_TEXTURE_2D,
//             0,
//             GL_RED,
//             face->glyph->bitmap.width,
//             face->glyph->bitmap.rows,
//             0,
//             GL_RED,
//             GL_UNSIGNED_BYTE,
//             face->glyph->bitmap.buffer
//         );

//         // --- FIX: swizzle RED -> ALPHA ---
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

//         Character ch{
//             tex,
//             { face->glyph->bitmap.width, face->glyph->bitmap.rows },
//             { face->glyph->bitmap_left, face->glyph->bitmap_top },
//             static_cast<unsigned int>(face->glyph->advance.x)
//         };

//         m_chars[c] = ch;
//     }

//     FT_Done_Face(face);
//     FT_Done_FreeType(ft);

//     // ---------------- VAO / VBO ----------------
//     glGenVertexArrays(1, &m_vao);
//     glGenBuffers(1, &m_vbo);

//     glBindVertexArray(m_vao);
//     glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
//     glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

//     glEnableVertexAttribArray(0);
//     glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);

//     glBindBuffer(GL_ARRAY_BUFFER, 0);
//     glBindVertexArray(0);

//     // ---------------- Shaders ----------------
//     const char* vs = R"(
//         #version 330 core
//         layout (location = 0) in vec4 vertex;
//         out vec2 TexCoords;
//         void main()
//         {
//             gl_Position = vec4(vertex.xy, 0.0, 1.0);
//             TexCoords = vertex.zw;
//         }
//     )";

//     const char* fs = R"(
//         #version 330 core
//         in vec2 TexCoords;
//         out vec4 FragColor;
//         uniform sampler2D text;
//         uniform vec3 textColor;
//         void main()
//         {
//             float alpha = texture(text, TexCoords).a;
//             FragColor = vec4(textColor, alpha);
//         }
//     )";

//     auto compile = [](GLenum type, const char* src) -> GLuint
//     {
//         GLuint s = glCreateShader(type);
//         glShaderSource(s, 1, &src, nullptr);
//         glCompileShader(s);

//         GLint ok = 0;
//         glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
//         if (!ok)
//         {
//             char log[512];
//             glGetShaderInfoLog(s, 512, nullptr, log);
//             std::cerr << "Shader compile error:\n" << log << "\n";
//         }

//         return s;
//     };

//     GLuint v = compile(GL_VERTEX_SHADER, vs);
//     GLuint f = compile(GL_FRAGMENT_SHADER, fs);

//     m_shader = glCreateProgram();
//     glAttachShader(m_shader, v);
//     glAttachShader(m_shader, f);
//     glLinkProgram(m_shader);

//     glDeleteShader(v);
//     glDeleteShader(f);

//     glUseProgram(m_shader);
//     glUniform1i(glGetUniformLocation(m_shader, "text"), 0);
//     glUseProgram(0);
// }

// // =====================================================================================
// // Draw (NDC version, HUD-safe)
// // =====================================================================================
// void TextRenderer::textDraw(const std::string& text,
//                         float x,
//                         float y,
//                         float scale,
//                         const glm::vec3& color)
// {
//     glUseProgram(m_shader);
//     glUniform3f(
//         glGetUniformLocation(m_shader, "textColor"),
//         color.x, color.y, color.z
//     );

//     glActiveTexture(GL_TEXTURE0);
//     glBindVertexArray(m_vao);

//     // screen size 
//     float screenW = static_cast<float>(m_viewportW);
//     float screenH = static_cast<float>(m_viewportH);

//     for (char c : text)
//     {
//         const Character& ch = m_chars[c];

//         float xpos = x + ch.bearing.x * scale;
//         // float ypos = y - (ch.size.y - ch.bearing.y) * scale;
//         float ypos = y - ch.bearing.y * scale;

//         float w = ch.size.x * scale;
//         float h = ch.size.y * scale;

//         // pixel -> NDC
//         float x0 =  (2.0f * xpos / screenW) - 1.0f;
//         float y0 =  1.0f - (2.0f * ypos / screenH);
//         float x1 =  (2.0f * (xpos + w) / screenW) - 1.0f;
//         float y1 =  1.0f - (2.0f * (ypos + h) / screenH);

//         float vertices[6][4] = {
//             { x0, y1, 0.0f, 1.0f },
//             { x0, y0, 0.0f, 0.0f },
//             { x1, y0, 1.0f, 0.0f },

//             { x0, y1, 0.0f, 1.0f },
//             { x1, y0, 1.0f, 0.0f },
//             { x1, y1, 1.0f, 1.0f }
//         };

//         glBindTexture(GL_TEXTURE_2D, ch.textureID);
//         glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
//         glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

//         glDrawArrays(GL_TRIANGLES, 0, 6);

//         x += (ch.advance >> 6) * scale;
//     }

//     glBindVertexArray(0);
//     glBindTexture(GL_TEXTURE_2D, 0);
//     glUseProgram(0);
// }


// // ==================================================================================
// void TextRenderer::setViewportSize(int width, int height)
// {
//     m_viewportW = width > 0 ? width : 1;
//     m_viewportH = height > 0 ? height : 1;
// }


// // ==================================================================================
// void TextRenderer::drawWorldLabel(
//     const std::string& text,
//     const glm::vec3& worldPos,
//     const glm::mat4& view,
//     const glm::mat4& projection,
//     float scale,
//     const glm::vec3& color
// )
// {
//     glm::vec4 clip = projection * view * glm::vec4(worldPos, 1.0f);

//     if (clip.w <= 0.0f)
//         return; // за камерой

//     glm::vec3 ndc = glm::vec3(clip) / clip.w;

//     if (ndc.x < -1.0f || ndc.x > 1.0f ||
//         ndc.y < -1.0f || ndc.y > 1.0f)
//         return; // вне экрана

//     float x = (ndc.x * 0.5f + 0.5f) * m_viewportW;
//     float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * m_viewportH;

//     textDraw(text, x, y, scale, color);
// }


// // ==================================================================================
// float TextRenderer::measureText(const std::string& text, float scale) const
// {
//     float width = 0.0f;
    
//     for (char c : text)
//     {
//         auto it = m_chars.find(c);
//         if (it == m_chars.end())
//         continue;
        
//         width += (it->second.advance >> 6) * scale;
//     }
    
//     return width;
// }


// // ==================================================================================
// float TextRenderer::lineHeight(float scale) const
// {
//     return m_fontPixelSize * scale;
// }

// // ==================================================================================
// float TextRenderer::ascent(float scale) const
// {
//     return m_fontPixelSize * 0.8f * scale;
// }

#include "render/TextRenderer.h"

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
        uniform vec3 textColor;
        void main()
        {
            float alpha = texture(text, TexCoords).a;
            FragColor = vec4(textColor, alpha);
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
    const glm::vec3& color
)
{
    glUseProgram(m_shader);
    glUniform3f(
        glGetUniformLocation(m_shader, "textColor"),
        color.x, color.y, color.z
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
