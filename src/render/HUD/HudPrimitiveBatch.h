#pragma once

#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace render::hud
{

class HudPrimitiveBatch
{
public:
    ~HudPrimitiveBatch();

    void init();
    void begin(int screenWidth, int screenHeight);
    void line(
        const glm::vec2& aPx,
        const glm::vec2& bPx,
        float thicknessPx,
        const glm::vec4& color
    );
    void flush();

private:
    struct Vertex
    {
        glm::vec2 posNdc {0.0f};
        glm::vec4 color {1.0f};
    };

    glm::vec2 pxToNdc(const glm::vec2& value) const;
    void triangle(
        const glm::vec2& a,
        const glm::vec2& b,
        const glm::vec2& c,
        const glm::vec4& color
    );

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    int m_width = 1;
    int m_height = 1;
    std::vector<Vertex> m_vertices;
};

} // namespace render::hud
