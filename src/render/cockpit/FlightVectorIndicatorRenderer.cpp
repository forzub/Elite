#include "src/render/cockpit/FlightVectorIndicatorRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "src/render/HUD/TextRenderer.h"
#include "src/ui/hud/HudEdgeMapper.h"
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

// The same ShipHudProfile polygon used by ordinary navigation/world markers.
// Test the WHOLE glyph rather than its center; placing a 21-pixel reticle
// right on the polygon boundary still paints across the cockpit frame.
bool glyphFitsHud(
    const HudEdgeMapper& hudBoundary,
    const glm::vec2& position,
    float radius
)
{
    if (!hudBoundary.isInsideBoundary(position))
        return false;
    constexpr int Samples = 16;
    for (int i = 0; i < Samples; ++i)
    {
        const float radians =
            2.0f * Pi * static_cast<float>(i) /
            static_cast<float>(Samples);
        const glm::vec2 sample = position + radius *
            glm::vec2(std::cos(radians),std::sin(radians));
        if (!hudBoundary.isInsideBoundary(sample))
            return false;
    }
    return true;
}

bool projectEmergencyHudSight(
    const HudEdgeMapper& hudBoundary,
    const glm::vec2& center,
    const glm::vec2& projectedDeviation,
    float glyphRadius,
    glm::vec2& marker,
    bool& atEdge
)
{
    if (hudBoundary.boundaryPx().size() < 3 ||
        !glyphFitsHud(hudBoundary,center,glyphRadius) ||
        !std::isfinite(projectedDeviation.x) ||
        !std::isfinite(projectedDeviation.y))
        return false;

    const float distance = glm::length(projectedDeviation);
    const glm::vec2 direction = distance > 1.0e-5f
        ? projectedDeviation / distance
        : glm::vec2(0.0f,-1.0f);
    glm::vec2 edge;
    if (!hudBoundary.projectDirection(center,direction,edge))
        return false;

    // A radial inset alone fails near slanted/corner sections of the
    // nonrectangular HUD. Find the last point where the complete glyph
    // footprint fits inside the real boundary.
    float inside = 0.0f;
    float outside = glm::length(edge-center);
    for (int step = 0; step < 14; ++step)
    {
        const float middle = (inside+outside)*0.5f;
        if (glyphFitsHud(hudBoundary,center+direction*middle,glyphRadius))
            inside = middle;
        else
            outside = middle;
    }
    const float safeDistance = std::max(0.0f,inside-1.0f);
    marker = center + direction*std::min(distance,safeDistance);
    atEdge = distance > safeDistance;
    return true;
}

bool labelFitsHud(
    const HudEdgeMapper& hudBoundary,
    const glm::vec2& center,
    float textWidth
)
{
    const float halfWidth = textWidth*0.5f+5.0f;
    // The text rasterizer is baseline-based. Pad both sides of the
    // baseline to cover ascenders, descenders and glyph anti-aliasing.
    constexpr float HalfHeight = 17.0f;
    for (const glm::vec2& corner : {
        glm::vec2(-halfWidth,-HalfHeight),
        glm::vec2( halfWidth,-HalfHeight),
        glm::vec2( halfWidth, HalfHeight),
        glm::vec2(-halfWidth, HalfHeight)})
        if (!hudBoundary.isInsideBoundary(center+corner))
            return false;
    return true;
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

void FlightVectorIndicatorRenderer::renderBoresight(
    const Viewport& viewport
)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;

    if (!m_program)
        init();

    m_screenW = viewport.width;
    m_screenH = viewport.height;

    const glm::vec2 center(
        static_cast<float>(viewport.width) * 0.5f,
        static_cast<float>(viewport.height) * 0.5f
    );
    const glm::vec4 color(0.55f, 0.94f, 1.00f, 0.92f);

    // Open optical sight: the exact center stays unobscured while the four
    // short brackets make the hull-forward axis readable over bright scenery.
    beginBatch();
    emitCircle(center, 4.5f, 1.2f, color, 28);
    emitLine(center + glm::vec2(-18.0f, 0.0f),
             center + glm::vec2(-8.0f, 0.0f), 1.4f, color);
    emitLine(center + glm::vec2(8.0f, 0.0f),
             center + glm::vec2(18.0f, 0.0f), 1.4f, color);
    emitLine(center + glm::vec2(0.0f, -16.0f),
             center + glm::vec2(0.0f, -8.0f), 1.4f, color);
    emitLine(center + glm::vec2(0.0f, 8.0f),
             center + glm::vec2(0.0f, 16.0f), 1.4f, color);
    flushBatch();
}

void FlightVectorIndicatorRenderer::renderEmergencyThrustAxis(
    const Viewport& viewport,
    const glm::dvec3& workingAxisBody,
    const glm::dvec3& workingAxisCamera,
    float verticalFovDegrees,
    const std::string& localizedLabel,
    const HudEdgeMapper& hudBoundary
)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    const double norm = glm::length(workingAxisBody);
    const double viewNorm = glm::length(workingAxisCamera);
    if (!std::isfinite(norm) || norm < 1.0e-9 ||
        !std::isfinite(viewNorm) || viewNorm < 1.0e-9 ||
        !std::isfinite(verticalFovDegrees) ||
        verticalFovDegrees <= 1.0f || verticalFovDegrees >= 179.0f)
        return;
    const glm::dvec3 axis = workingAxisBody / norm;
    const glm::dvec3 viewAxis = workingAxisCamera / viewNorm;
    const double offset = std::atan2(
        std::hypot(axis.x,axis.y), -axis.z
    );
    if (!std::isfinite(offset) || offset < 0.01)
        return;

    if (!m_program)
        init();
    m_screenW = viewport.width;
    m_screenH = viewport.height;

    // The optical center always identifies the physical hull nose. This
    // additional sight identifies the actual propulsion reference. Project
    // its body-space deviation to the HUD plane and retain an edge pointer
    // if it falls outside the real, nonrectangular cockpit HUD boundary.
    const glm::vec2 center(
        static_cast<float>(viewport.width)*0.5f,
        static_cast<float>(viewport.height)*0.5f
    );
    // Exact perspective camera projection. An arbitrary 0.48*height
    // focal length displaced the pilot's amber sight by dozens of pixels,
    // so it did NOT actually indicate the physical thrust ray.
    const double fovRad=static_cast<double>(verticalFovDegrees)*
        (3.14159265358979323846/180.0);
    const double focal=
        (static_cast<double>(viewport.height)*0.5)/
        std::tan(fovRad*0.5);
    // Perspective projection can go outside the HUD for large gimbal
    // angles, and even behind the camera. Preserve the exact camera ray
    // before restricting only its visual position to the HUD polygon.
    const double depth=std::max(0.035,-viewAxis.z);
    glm::vec2 deviation(
        static_cast<float>(focal*viewAxis.x/depth),
        static_cast<float>(-focal*viewAxis.y/depth)
    );
    if(glm::length(deviation)<1.0f && viewAxis.z>0.0)
        deviation={0.0f,-static_cast<float>(focal)*3.0f};
    glm::vec2 marker;
    bool offscreen = false;
    if (!projectEmergencyHudSight(
            hudBoundary,center,deviation,24.0f,marker,offscreen))
        return;
    const glm::vec4 amber(1.0f,0.72f,0.28f,0.96f);
    beginBatch();
    if(offscreen)
    {
        // Chevron points towards the missing physical sight, like a
        // ship-target edge pointer, without suggesting it is onscreen.
        const float length=glm::length(deviation);
        const glm::vec2 direction=length>1.0e-6f
            ? deviation/length : glm::vec2(0.0f,-1.0f);
        const glm::vec2 normal(-direction.y,direction.x);
        const glm::vec2 base=marker-direction*18.0f;
        emitLine(marker,base+normal*9.0f,2.2f,amber);
        emitLine(marker,base-normal*9.0f,2.2f,amber);
        emitLine(base+normal*9.0f,base-normal*9.0f,1.3f,amber);
        emitCircle(base,5.0f,1.2f,amber,18);
    }
    else
    {
        emitCircle(marker,11.0f,1.7f,amber,32);
        emitLine(marker+glm::vec2(-21.0f,0.0f),
                 marker+glm::vec2(-13.0f,0.0f),2.0f,amber);
        emitLine(marker+glm::vec2(13.0f,0.0f),
                 marker+glm::vec2(21.0f,0.0f),2.0f,amber);
        emitLine(marker+glm::vec2(0.0f,-21.0f),
                 marker+glm::vec2(0.0f,-13.0f),2.0f,amber);
        emitLine(marker+glm::vec2(0.0f,13.0f),
                 marker+glm::vec2(0.0f,21.0f),2.0f,amber);
    }
    // Center gap leaves the apparent course unobscured.
    flushBatch();

    ensureFonts("assets/fonts/Roboto-Light.ttf",14,13);
    if (m_secondaryFont && !localizedLabel.empty())
    {
        char degrees[24];
        std::snprintf(degrees,sizeof(degrees)," %.0f deg",
            offset*180.0/3.14159265358979323846);
        const std::string label=localizedLabel+degrees;
        const float width = m_secondaryFont->measureText(label);
        const glm::vec2 positions[] {
            marker+glm::vec2(0.0f,34.0f),
            marker+glm::vec2(0.0f,-34.0f),
            marker+glm::vec2(-width*0.5f-29.0f,0.0f),
            marker+glm::vec2( width*0.5f+29.0f,0.0f)
        };
        for (const auto& position : positions)
            if (labelFitsHud(hudBoundary,position,width))
            {
                drawCenteredText(
                    *m_secondaryFont,label,position.x,position.y,
                    glm::vec3(1.0f,0.72f,0.28f)
                );
                break;
            }
    }
}

void FlightVectorIndicatorRenderer::renderEmergencyVelocityVector(
    const Viewport& viewport,
    const glm::dvec3& velocityCamera,
    float verticalFovDegrees,
    const HudEdgeMapper& hudBoundary
)
{
    if(viewport.width<=0 || viewport.height<=0 ||
       !std::isfinite(verticalFovDegrees) ||
       verticalFovDegrees<=1.0f || verticalFovDegrees>=179.0f)
        return;
    const double length=glm::length(velocityCamera);
    if(!std::isfinite(length) || length<=1.0e-9) return;
    const glm::dvec3 axis=velocityCamera/length;
    if(!m_program) init();
    m_screenW=viewport.width;
    m_screenH=viewport.height;

    const glm::vec2 center(
        viewport.width*0.5f,viewport.height*0.5f);
    const double focal=
        (static_cast<double>(viewport.height)*0.5)/
        std::tan(static_cast<double>(verticalFovDegrees)*
            (3.14159265358979323846/360.0));
    const double depth=std::max(0.035,-axis.z);
    glm::vec2 deviation(
        static_cast<float>(focal*axis.x/depth),
        static_cast<float>(-focal*axis.y/depth));
    if(glm::length(deviation)<1.0f && axis.z>0.0)
        deviation={0.0f,-static_cast<float>(focal)*3.0f};
    glm::vec2 marker;
    bool atEdge = false;
    if (!projectEmergencyHudSight(
            hudBoundary,center,deviation,23.0f,marker,atEdge))
        return;
    const glm::vec4 green(0.24f,1.0f,0.54f,0.92f);
    beginBatch();
    if(atEdge || axis.z>=0.0)
    {
        const float dl=glm::length(deviation);
        const glm::vec2 dir=dl>1.0e-6f
            ? deviation/dl : glm::vec2(0.0f,-1.0f);
        const glm::vec2 transverse(-dir.y,dir.x);
        emitLine(marker,marker-dir*16.0f+transverse*8.0f,2.0f,green);
        emitLine(marker,marker-dir*16.0f-transverse*8.0f,2.0f,green);
    }
    else
    {
        // Distinct flight-path symbol. Amber thrust shows acceleration
        // direction; green V shows where the ship is already traveling.
        emitCircle(marker,7.0f,1.5f,green,28);
        emitLine(marker+glm::vec2(-20.0f,0.0f),
                 marker+glm::vec2(-9.0f,0.0f),1.8f,green);
        emitLine(marker+glm::vec2(9.0f,0.0f),
                 marker+glm::vec2(20.0f,0.0f),1.8f,green);
        emitLine(marker+glm::vec2(0.0f,8.0f),
                 marker+glm::vec2(0.0f,16.0f),1.8f,green);
    }
    flushBatch();
    ensureFonts("assets/fonts/Roboto-Light.ttf",14,13);
    if (m_secondaryFont)
    {
        const float width = m_secondaryFont->measureText("V");
        const glm::vec2 positions[] {
            marker+glm::vec2(18.0f,-16.0f),
            marker+glm::vec2(-18.0f,-16.0f),
            marker+glm::vec2(18.0f,16.0f),
            marker+glm::vec2(-18.0f,16.0f)
        };
        for (const auto& position : positions)
            if (labelFitsHud(hudBoundary,position,width))
            {
                drawCenteredText(
                    *m_secondaryFont,"V",position.x,position.y,
                    glm::vec3(green)
                );
                break;
            }
    }
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

    const glm::vec4 freeRing(0.19f, 0.92f, 0.37f, 0.96f);
    const glm::vec4 ring = presentation.assistedFreeRotation ? freeRing : dim;
    emitCircle(center, radius,
        presentation.assistedFreeRotation ? 4.0f : 1.4f, ring, 72);
    emitCircle(center, radius * 0.94f,
        presentation.assistedFreeRotation ? 1.7f : 0.8f,
        presentation.assistedFreeRotation
            ? glm::vec4(freeRing.r,freeRing.g,freeRing.b,0.35f)
            : glm::vec4(dim.r,dim.g,dim.b,0.16f), 72);

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

    if (!presentation.modeText.empty())
    {
        const float modeBaselineY =
            center.y + radius + secondaryPx * 0.25f;
        const float modeTextWidth =
            m_secondaryFont->measureText(presentation.modeText);
        const float padX = std::max(7.0f, secondaryPx * 0.48f);
        const float padY = std::max(3.0f, secondaryPx * 0.24f);
        const float panelHeight = secondaryPx + padY * 2.0f;

        TextRenderer::instance().solidRectPx(
            center.x - modeTextWidth * 0.5f - padX,
            modeBaselineY - secondaryPx - padY,
            modeTextWidth + padX * 2.0f,
            panelHeight,
            glm::vec4(0.01f, 0.025f, 0.04f, 0.72f)
        );
    }

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
