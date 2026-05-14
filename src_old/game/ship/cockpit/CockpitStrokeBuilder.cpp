#include "CockpitStrokeBuilder.h"
#include <glm/geometric.hpp>


Mesh2D buildStrokeMesh(const CockpitStroke& stroke)
{
    Mesh2D mesh;
    const float t = stroke.thickness * 0.5f;

    for (size_t i = 0; i + 1 < stroke.path01.size(); ++i)
    {
        // --- SVG coords [0..1]
        glm::vec2 s0 = stroke.path01[i];
        glm::vec2 s1 = stroke.path01[i + 1];

        // --- SVG → NDC
        glm::vec2 p0 = { s0.x * 2.0f - 1.0f, 1.0f - s0.y * 2.0f };
        glm::vec2 p1 = { s1.x * 2.0f - 1.0f, 1.0f - s1.y * 2.0f };

        glm::vec2 dir = glm::normalize(p1 - p0);
        glm::vec2 n   = { -dir.y, dir.x };

        glm::vec2 a = p0 + n * t;
        glm::vec2 b = p0 - n * t;
        glm::vec2 c = p1 - n * t;
        glm::vec2 d = p1 + n * t;

        uint32_t base = (uint32_t)mesh.vertices.size();

        // 🔴 ВАЖНО: local01 ЗАПОЛНЯЕМ ОСОЗНАННО
        mesh.vertices.push_back({ a, s0, stroke.color }); // a
        mesh.vertices.push_back({ b, s0, stroke.color }); // b
        mesh.vertices.push_back({ c, s1, stroke.color }); // c
        mesh.vertices.push_back({ d, s1, stroke.color }); // d


        mesh.indices.insert(mesh.indices.end(), {
            base + 0, base + 1, base + 2,
            base + 0, base + 2, base + 3
        });
    }

    return mesh;
}
