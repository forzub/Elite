#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "render/Font.h"
#include "render/types/Viewport.h"

class TextRenderer
{
public:
    static TextRenderer& instance();

    void init();

    void beginFrame();
    void endFrame();

    void textDrawBatched(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color
    );

    void textDraw(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec4& color
    );

    inline void textDraw(
        Font& font,
        const std::string& text,
        float x,
        float baselineY,
        const glm::vec3& color
    )
    {
        textDraw(font, text, x, baselineY, glm::vec4(color, 1.0f));
    }

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
    struct TextVertex
    {
        float x;
        float y;
        float u;
        float v;

        float r;
        float g;
        float b;
        float a;
    };

    struct TextBatch
    {
        unsigned int textureID = 0;
        std::vector<TextVertex> vertices;
    };

private:
    TextBatch& batchForTexture(unsigned int textureID);

private:
    unsigned int m_vao    = 0;
    unsigned int m_vbo    = 0;
    unsigned int m_shader = 0;

    bool  m_batchActive = false;
    float m_screenW = 1.0f;
    float m_screenH = 1.0f;

    int m_locTextSampler = -1;

    std::vector<TextBatch> m_batches;
};