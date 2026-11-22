#pragma once

// Screen-thick lines that are WORLD objects for the depth test (author 2026-11-22: «рендер зон - что бы они были видны
// как обычные объекты, т.е. их было бы не видно изнутри зданий и за зданиями»): each end keeps its clip-space position,
// the thickness is added in screen pixels, and the fragments are tested against the scene's depth buffer (LEQUAL, no
// depth write) - a building, a hull, the ground in front hides the line exactly, per pixel.
#include <vector>

#include <glm/glm.hpp>

#include <glad/gl.h>

namespace render::cockpit
{
class DepthLineBatch
{
public:
    ~DepthLineBatch();
    void init();
    void begin(int width, int height);
    // a, b: clip-space ends (w > 0), thickness in pixels
    void line(const glm::vec4& a, const glm::vec4& b, float thicknessPx, const glm::vec4& color);
    void flush();

private:
    struct Vertex
    {
        glm::vec4 clip;
        glm::vec4 color;
    };
    unsigned int m_program = 0, m_vao = 0, m_vbo = 0;
    int m_width = 1, m_height = 1;
    std::vector<Vertex> m_vertices;
};
} // namespace render::cockpit
