#include "src/render/cockpit/FlightVectorIndicatorRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "src/render/HUD/TextRenderer.h"
#include "src/render/ShaderUtils.h"

namespace render::cockpit
{
namespace
{
constexpr float Pi = 3.14159265358979323846f;

float clampRadius(float height)
{
    return std::clamp(height * 0.082f, 68.0f, 112.0f);
}
}

FlightVectorIndicatorRenderer::~FlightVectorIndicatorRenderer()
{
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_program)
        glDeleteProgram(m_program);
}

void FlightVectorIndicatorRenderer::init()
{
    if (m_program)
        return;

    m_program = compileShaderFromFiles(
        "assets/shaders/hud/hudPrimitive.vert",
        "assets/shaders/common/vertex_color.frag"
    );

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(Vertex) * 4096,
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(0)
    );

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(
        1,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(sizeof(glm::vec2))
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

glm::vec2 FlightVectorIndicatorRenderer::pxToNdc(const glm::vec2& p) const
{
    return {
        (2.0f * p.x / static_cast<float>(m_screenW)) - 1.0f,
        1.0f - (2.0f * p.y / static_cast<float>(m_screenH))
    };
}

void FlightVectorIndicatorRenderer::beginBatch()
{
    m_vertices.clear();
}

void FlightVectorIndicatorRenderer::flushBatch()
{
    if (!m_program || m_vertices.empty())
        return;

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_program);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(m_vertices.size() * sizeof(Vertex)),
        m_vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        static_cast<GLsizei>(m_vertices.size())
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

    m_vertices.clear();
}

void FlightVectorIndicatorRenderer::emitTriangle(
    const glm::vec2& a,
    const glm::vec2& b,
    const glm::vec2& c,
    const glm::vec4& color
)
{
    m_vertices.push_back({pxToNdc(a), color});
    m_vertices.push_back({pxToNdc(b), color});
    m_vertices.push_back({pxToNdc(c), color});
}

void FlightVectorIndicatorRenderer::emitLine(
    const glm::vec2& a,
    const glm::vec2& b,
    float thickness,
    const glm::vec4& color
)
{
    const glm::vec2 d = b - a;
    const float len = glm::length(d);
    if (len <= 0.0001f)
        return;

    const glm::vec2 n =
        glm::vec2(-d.y, d.x) / len * (thickness * 0.5f);

    const glm::vec2 p0 = a + n;
    const glm::vec2 p1 = b + n;
    const glm::vec2 p2 = b - n;
    const glm::vec2 p3 = a - n;

    emitTriangle(p0, p1, p2, color);
    emitTriangle(p0, p2, p3, color);
}

void FlightVectorIndicatorRenderer::emitCircle(
    const glm::vec2& center,
    float radius,
    float thickness,
    const glm::vec4& color,
    int segments
)
{
    const int count = std::max(12, segments);
    for (int i = 0; i < count; ++i)
    {
        const float a0 =
            2.0f * Pi * static_cast<float>(i) / static_cast<float>(count);
        const float a1 =
            2.0f * Pi * static_cast<float>(i + 1) / static_cast<float>(count);

        emitLine(
            center + glm::vec2(std::cos(a0), std::sin(a0)) * radius,
            center + glm::vec2(std::cos(a1), std::sin(a1)) * radius,
            thickness,
            color
        );
    }
}

void FlightVectorIndicatorRenderer::ensureFonts(
    const std::string& fontPath,
    int primaryPixelSize,
    int secondaryPixelSize
)
{
    const std::string resolvedPath = fontPath.empty()
        ? "assets/fonts/Roboto-Light.ttf"
        : fontPath;

    if (!m_primaryFont ||
        m_fontPath != resolvedPath ||
        m_primaryPixelSize != primaryPixelSize)
    {
        m_primaryFont = std::make_unique<Font>(
            resolvedPath,
            primaryPixelSize
        );
        m_primaryPixelSize = primaryPixelSize;
    }

    if (!m_secondaryFont ||
        m_fontPath != resolvedPath ||
        m_secondaryPixelSize != secondaryPixelSize)
    {
        m_secondaryFont = std::make_unique<Font>(
            resolvedPath,
            secondaryPixelSize
        );
        m_secondaryPixelSize = secondaryPixelSize;
    }

    m_fontPath = resolvedPath;
}

void FlightVectorIndicatorRenderer::drawCenteredText(
    Font& font,
    const std::string& text,
    float centerX,
    float baselineY,
    const glm::vec3& color
)
{
    if (text.empty())
        return;

    const float width = font.measureText(text);
    TextRenderer::instance().textDraw(
        font,
        text,
        centerX - width * 0.5f,
        baselineY,
        color
    );
}

void FlightVectorIndicatorRenderer::emitShipGlyph(
    const game::presentation::FlightVectorIndicatorPresentation& presentation,
    const glm::vec2& center,
    float scale,
    const glm::vec4& color
)
{
    // Flattened prism: model X=right, Y=nose/forward, Z=up.
    const std::array<glm::vec3, 6> model = {
        glm::vec3( 0.00f,  1.10f,  0.18f),
        glm::vec3(-0.62f, -0.70f,  0.18f),
        glm::vec3( 0.62f, -0.70f,  0.18f),
        glm::vec3( 0.00f,  1.10f, -0.18f),
        glm::vec3(-0.62f, -0.70f, -0.18f),
        glm::vec3( 0.62f, -0.70f, -0.18f)
    };

    std::array<glm::vec2, 6> projected {};
    for (std::size_t i = 0; i < model.size(); ++i)
    {
        const glm::vec3 p =
            presentation.shipModelToIndicatorBasis * model[i];

        // Small oblique depth term keeps roll/pitch visible without turning the
        // cockpit instrument into a perspective camera.
        projected[i] = center + glm::vec2(
            (p.x + p.z * 0.28f) * scale,
            (-p.y + p.z * 0.16f) * scale
        );
    }

    auto edge = [&](int a, int b, float alpha)
    {
        glm::vec4 c = color;
        c.a *= alpha;
        emitLine(projected[a], projected[b], 1.8f, c);
    };

    // Fill the lower (model -Z) triangular face. The silhouette used to be
    // symmetric enough that a rolled or inverted ship was hard to read at a
    // glance; the solid belly gives an immediate top/bottom reference while
    // preserving the lightweight wireframe style of the instrument.
    glm::vec4 belly = color;
    belly.a *= 0.34f;
    emitTriangle(projected[3], projected[4], projected[5], belly);

    edge(0, 1, 1.0f);
    edge(1, 2, 1.0f);
    edge(2, 0, 1.0f);

    edge(3, 4, 0.55f);
    edge(4, 5, 0.55f);
    edge(5, 3, 0.55f);

    edge(0, 3, 0.7f);
    edge(1, 4, 0.7f);
    edge(2, 5, 0.7f);

    // Small center mark gives a stable visual pivot while the hull rotates.
    emitCircle(center, std::max(2.0f, scale * 0.075f), 1.3f, color, 18);
}

void FlightVectorIndicatorRenderer::render(
    const game::presentation::FlightVectorIndicatorPresentation& presentation,
    const Viewport& viewport
)
{
    if (!presentation.visible || viewport.width <= 0 || viewport.height <= 0)
        return;

    if (!m_program)
        init();

    m_screenW = viewport.width;
    m_screenH = viewport.height;

    const float radius = clampRadius(static_cast<float>(viewport.height));
    const glm::vec2 center(
        static_cast<float>(viewport.width) - radius * 1.55f,
        static_cast<float>(viewport.height) - radius * 1.72f
    );

    const glm::vec4 dim(0.20f, 0.68f, 0.86f, 0.34f);
    const glm::vec4 normal(0.30f, 0.84f, 1.00f, 0.78f);
    const glm::vec4 bright(0.55f, 0.94f, 1.00f, 0.95f);
    const glm::vec4 action(1.00f, 0.72f, 0.28f, 0.92f);

    beginBatch();

    emitCircle(center, radius, 1.4f, dim, 72);
    emitCircle(center, radius * 0.94f, 0.8f, glm::vec4(dim.r, dim.g, dim.b, 0.16f), 72);

    const float axisBottom = center.y + radius * 0.48f;
    const float axisTop = center.y - radius * 0.68f;
    const float axisLength = axisBottom - axisTop;

    emitLine(
        {center.x, axisBottom},
        {center.x, axisTop},
        1.4f,
        dim
    );

    const float fraction = std::clamp(presentation.speedFraction01, 0.0f, 1.0f);
    const float fillY = axisBottom - axisLength * fraction;

    emitLine(
        {center.x, axisBottom},
        {center.x, fillY},
        4.0f,
        bright
    );

    const float head = std::max(4.0f, radius * 0.055f);
    emitTriangle(
        {center.x, fillY - head},
        {center.x - head * 0.72f, fillY + head * 0.35f},
        {center.x + head * 0.72f, fillY + head * 0.35f},
        bright
    );

    // 25/50/75/100 percent guide ticks.
    for (int i = 1; i <= 4; ++i)
    {
        const float y = axisBottom - axisLength * (static_cast<float>(i) / 4.0f);
        emitLine(
            {center.x - radius * 0.055f, y},
            {center.x + radius * 0.055f, y},
            1.0f,
            dim
        );
    }

    const glm::vec2 glyphCenter(
        center.x,
        center.y + radius * 0.10f
    );
    emitShipGlyph(
        presentation,
        glyphCenter,
        radius * 0.34f,
        normal
    );

    if (!presentation.actionText.empty())
    {
        emitCircle(
            center,
            radius * 0.86f,
            1.5f,
            glm::vec4(action.r, action.g, action.b, 0.45f),
            72
        );
    }

    flushBatch();

    const int primaryPx = std::max(14, static_cast<int>(radius * 0.18f));
    const int secondaryPx = std::max(12, static_cast<int>(radius * 0.135f));
    ensureFonts(
        presentation.fontPath,
        primaryPx,
        secondaryPx
    );

    if (!m_primaryFont || !m_secondaryFont)
        return;

    drawCenteredText(
        *m_primaryFont,
        presentation.speedText,
        center.x,
        center.y + radius * 0.62f,
        glm::vec3(0.55f, 0.94f, 1.0f)
    );

    drawCenteredText(
        *m_secondaryFont,
        presentation.modeText,
        center.x,
        center.y + radius + secondaryPx * 0.25f,
        glm::vec3(0.32f, 0.78f, 0.96f)
    );

    if (!presentation.actionText.empty())
    {
        drawCenteredText(
            *m_secondaryFont,
            presentation.actionText,
            center.x,
            center.y - radius * 0.88f,
            glm::vec3(1.0f, 0.72f, 0.28f)
        );
    }
}

} // namespace render::cockpit
