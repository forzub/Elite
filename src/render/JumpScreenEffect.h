#pragma once

// The jump's screen effect (author 2026-11-09): the cockpit view shakes along the screen's x / y
// (no rotation), swells like a fish-eye lens as the jump core charges, is flung forward (the view
// shrinks, pinched) at the jump and springs back. Presentation only: it copies the frame drawn so
// far (scene + cockpit + its instruments) and draws it back distorted; nothing is drawn when the
// parameters are neutral.
#include <glm/glm.hpp>

namespace render
{
struct JumpScreenParams
{
    glm::vec2 shakePx {0.0f};   // the shake, screen pixels (x right, y down)
    float zoom = 1.0f;          // > 1 magnified, < 1 shrunk (a dark border shows)
    float bulge = 0.0f;         // fish-eye: > 0 the centre swells, < 0 it is pinched; the corners stay put
    float chroma = 0.0f;        // colour fringes (0..1)
    float vignette = 0.0f;      // darkened edges (0..1)

    bool neutral() const
    {
        return zoom == 1.0f && bulge == 0.0f && chroma == 0.0f && vignette == 0.0f && shakePx == glm::vec2(0.0f);
    }
};

class JumpScreenEffect
{
public:
    JumpScreenEffect() = default;
    ~JumpScreenEffect();
    JumpScreenEffect(const JumpScreenEffect&) = delete;
    JumpScreenEffect& operator=(const JumpScreenEffect&) = delete;

    // Captures the current viewport of the bound draw framebuffer and draws it back distorted.
    void apply(const JumpScreenParams& params);

private:
    bool init();

    unsigned int m_program = 0;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
    unsigned int m_texture = 0;
    int m_width = 0;
    int m_height = 0;
    bool m_failed = false;
};
} // namespace render
