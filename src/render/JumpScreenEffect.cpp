#include "src/render/JumpScreenEffect.h"

#include <glad/gl.h>

#include <algorithm>
#include <cmath>
#include <iostream>

#include "src/render/ShaderUtils.h"

namespace render
{
namespace
{
const char* const VertexSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;
out vec2 vUv;
void main()
{
    vUv = aPos * 0.5 + 0.5;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

// p: the pixel's offset from the screen centre in units of the screen height. The fish-eye scales
// the sample radius by f(r) = (1 + k r^2) / (1 + k rc^2): the corners (rc) map onto themselves, the
// centre is magnified by (1 + k rc^2) for k > 0 (the swell) and shrunk for k < 0 (the fling).
const char* const FragmentSource = R"(#version 330 core
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uFrame;
uniform vec2 uShake;      // uv units
uniform float uZoom;
uniform float uBulge;
uniform float uChroma;
uniform float uVignette;
uniform float uAspect;

vec2 warp(vec2 uv, float k)
{
    vec2 p = (uv - 0.5) * vec2(uAspect, 1.0);
    float rc2 = 0.25 * (uAspect * uAspect + 1.0);
    float f = (1.0 + k * dot(p, p)) / (1.0 + k * rc2);
    p = p * f / uZoom;
    return p / vec2(uAspect, 1.0) + 0.5 + uShake;
}

float inside(vec2 uv)
{
    vec2 e = step(vec2(0.0), uv) * step(uv, vec2(1.0));
    return e.x * e.y;
}

void main()
{
    float spread = 0.06 * uChroma;
    vec2 uvG = warp(vUv, uBulge);
    vec2 uvR = warp(vUv, uBulge + spread);
    vec2 uvB = warp(vUv, uBulge - spread);
    vec3 col = vec3(texture(uFrame, uvR).r, texture(uFrame, uvG).g, texture(uFrame, uvB).b);
    // outside the captured frame (the view shrunk): the deep blue of the jump field
    col = mix(vec3(0.004, 0.010, 0.028), col, inside(uvG));
    vec2 c = (vUv - 0.5) * vec2(uAspect, 1.0);
    float r = length(c) / sqrt(0.25 * (uAspect * uAspect + 1.0));
    col *= 1.0 - uVignette * smoothstep(0.35, 1.0, r);
    FragColor = vec4(col, 1.0);
}
)";
} // namespace

JumpScreenEffect::~JumpScreenEffect()
{
    if (m_texture)
        glDeleteTextures(1, &m_texture);
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_program)
        glDeleteProgram(m_program);
}

bool JumpScreenEffect::init()
{
    if (m_program)
        return true;
    if (m_failed)
        return false;
    m_program = compileShader(VertexSource, FragmentSource);
    if (!m_program)
    {
        m_failed = true;
        std::cerr << "[JumpFx] screen effect shader failed - the jump plays without the lens" << std::endl;
        return false;
    }
    const float verts[] = {-1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f};
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glGenTextures(1, &m_texture);
    return true;
}

void JumpScreenEffect::apply(const JumpScreenParams& params)
{
    if (params.neutral() || !init())
        return;
    GLint vp[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, vp);
    const int w = vp[2];
    const int h = vp[3];
    if (w <= 0 || h <= 0)
        return;

    GLint drawFbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
    GLint readFbo = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(drawFbo));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    if (w != m_width || h != m_height)
    {
        m_width = w;
        m_height = h;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, vp[0], vp[1], w, h);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFbo));

    // the shake must never uncover the frame's edge: a slight zoom-in covers it
    const glm::vec2 shakeUv(params.shakePx.x / static_cast<float>(w), -params.shakePx.y / static_cast<float>(h));
    const float cover = 1.0f + 2.0f * std::max(std::abs(shakeUv.x), std::abs(shakeUv.y));

    const GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blend = glIsEnabled(GL_BLEND);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(m_program);
    glUniform1i(glGetUniformLocation(m_program, "uFrame"), 0);
    glUniform2f(glGetUniformLocation(m_program, "uShake"), shakeUv.x, shakeUv.y);
    glUniform1f(glGetUniformLocation(m_program, "uZoom"), std::max(0.2f, params.zoom * cover));
    glUniform1f(glGetUniformLocation(m_program, "uBulge"), std::clamp(params.bulge, -0.9f, 2.0f));
    glUniform1f(glGetUniformLocation(m_program, "uChroma"), std::clamp(params.chroma, 0.0f, 1.0f));
    glUniform1f(glGetUniformLocation(m_program, "uVignette"), std::clamp(params.vignette, 0.0f, 1.0f));
    glUniform1f(glGetUniformLocation(m_program, "uAspect"), static_cast<float>(w) / static_cast<float>(h));
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (depth)
        glEnable(GL_DEPTH_TEST);
    if (blend)
        glEnable(GL_BLEND);
    if (scissor)
        glEnable(GL_SCISSOR_TEST);
}
} // namespace render
