#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>
#include <cstdint>

struct Vertex2D
{
    glm::vec2 position;   // NDC или screen-space
    glm::vec4 color;      // RGBA
};

struct Mesh2D
{
    std::vector<Vertex2D> vertices;
    std::vector<uint32_t> indices;
};
