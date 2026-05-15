#include "core/StateContext.h"

#include "render/HUD/TextRenderer.h"
#include "render/ShaderUtils.h"

#include <glad/gl.h>

#include <cstdint>
#include <iostream>

namespace
{
uint32_t nextUtf8Codepoint(
    const std::string& text,
    size_t& i
)
{
    if (i >= text.size())
        return 0;

    const unsigned char c0 =
        static_cast<unsigned char>(text[i]);

    if (c0 < 0x80)
    {
        ++i;
        return c0;
    }

    if ((c0 & 0xE0) == 0xC0 && i + 1 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        i += 2;

        return
            ((c0 & 0x1F) << 6) |
            (c1 & 0x3F);
    }

    if ((c0 & 0xF0) == 0xE0 && i + 2 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        const unsigned char c2 =
            static_cast<unsigned char>(text[i + 2]);

        i += 3;

        return
            ((c0 & 0x0F) << 12) |
            ((c1 & 0x3F) << 6) |
            (c2 & 0x3F);
    }

    if ((c0 & 0xF8) == 0xF0 && i + 3 < text.size())
    {
        const unsigned char c1 =
            static_cast<unsigned char>(text[i + 1]);

        const unsigned char c2 =
            static_cast<unsigned char>(text[i + 2]);

        const unsigned char c3 =
            static_cast<unsigned char>(text[i + 3]);

        i += 4;

        return
            ((c0 & 0x07) << 18) |
            ((c1 & 0x3F) << 12) |
            ((c2 & 0x3F) << 6) |
            (c3 & 0x3F);
    }

    ++i;
    return static_cast<uint32_t>('?');
}
}

TextRenderer& TextRenderer::instance()
{
    static TextRenderer inst;
    return inst;
}

void TextRenderer::init()
{
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(TextVertex) * 4096,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TextVertex),
        reinterpret_cast<void*>(0)
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(TextVertex),
        reinterpret_cast<void*>(sizeof(float) * 4)
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    m_shader = compileShaderFromFiles(
        "assets/shaders/hud/textRenderer.vert",
        "assets/shaders/hud/textRenderer.frag"
    );

    if (!m_shader)
    {
        std::cerr << "[TextRenderer] Failed to load text shaders\n";
        return;
    }

    m_locTextSampler =
        glGetUniformLocation(m_shader, "text");

    glUseProgram(m_shader);
    glUniform1i(m_locTextSampler, 0);
    glUseProgram(0);
}

void TextRenderer::beginFrame()
{
    if (m_batchActive)
        return;

    extern StateContext* g_stateContext;
    const Viewport& vp = g_stateContext->viewport();

    m_screenW = static_cast<float>(vp.width);
    m_screenH = static_cast<float>(vp.height);

    m_batches.clear();

    m_batchActive = true;
}

void TextRenderer::endFrame()
{
    if (!m_batchActive)
        return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    for (TextBatch& batch : m_batches)
    {
        if (batch.vertices.empty())
            continue;

        glBindTexture(GL_TEXTURE_2D, batch.textureID);

        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(
                batch.vertices.size() * sizeof(TextVertex)
            ),
            batch.vertices.data(),
            GL_DYNAMIC_DRAW
        );

        glDrawArrays(
            GL_TRIANGLES,
            0,
            static_cast<GLsizei>(batch.vertices.size())
        );
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    m_batches.clear();
    m_batchActive = false;
}

TextRenderer::TextBatch& TextRenderer::batchForTexture(
    unsigned int textureID
)
{
    for (TextBatch& batch : m_batches)
    {
        if (batch.textureID == textureID)
            return batch;
    }

    TextBatch batch;
    batch.textureID = textureID;
    m_batches.push_back(std::move(batch));

    return m_batches.back();
}

void TextRenderer::textDrawBatched(
    Font& font,
    const std::string& text,
    float x,
    float baselineY,
    const glm::vec4& color
)
{
    if (!m_batchActive)
    {
        beginFrame();
        textDrawBatched(font, text, x, baselineY, color);
        endFrame();
        return;
    }

    TextBatch& batch =
        batchForTexture(font.atlasTexture());

    size_t i = 0;

    while (i < text.size())
    {
        const uint32_t cp =
            nextUtf8Codepoint(text, i);

        Character& ch = font.glyph(cp);

        const float advance =
            static_cast<float>(ch.advance >> 6);

        if (ch.size.x <= 0 || ch.size.y <= 0)
        {
            x += advance;
            continue;
        }

        const float xpos =
            x + static_cast<float>(ch.bearing.x);

        const float ypos =
            baselineY - static_cast<float>(ch.bearing.y);

        const float w =
            static_cast<float>(ch.size.x);

        const float h =
            static_cast<float>(ch.size.y);

        const float x0 =
            (2.0f * xpos / m_screenW) - 1.0f;

        const float y0 =
            1.0f - (2.0f * ypos / m_screenH);

        const float x1 =
            (2.0f * (xpos + w) / m_screenW) - 1.0f;

        const float y1 =
            1.0f - (2.0f * (ypos + h) / m_screenH);

        const float u0 = ch.uv0.x;
        const float v0 = ch.uv0.y;
        const float u1 = ch.uv1.x;
        const float v1 = ch.uv1.y;

        const TextVertex verts[6] = {
            { x0, y1, u0, v1, color.r, color.g, color.b, color.a },
            { x0, y0, u0, v0, color.r, color.g, color.b, color.a },
            { x1, y0, u1, v0, color.r, color.g, color.b, color.a },

            { x0, y1, u0, v1, color.r, color.g, color.b, color.a },
            { x1, y0, u1, v0, color.r, color.g, color.b, color.a },
            { x1, y1, u1, v1, color.r, color.g, color.b, color.a }
        };

        batch.vertices.insert(
            batch.vertices.end(),
            std::begin(verts),
            std::end(verts)
        );

        x += advance;
    }
}

void TextRenderer::textDraw(
    Font& font,
    const std::string& text,
    float x,
    float baselineY,
    const glm::vec4& color
)
{
    beginFrame();

    textDrawBatched(
        font,
        text,
        x,
        baselineY,
        color
    );

    endFrame();
}

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
        fallbackFont = new Font("assets/fonts/Roboto-Light.ttf", 48);

    textDraw(
        *fallbackFont,
        text,
        x,
        y,
        glm::vec4(color, 1.0f)
    );
}