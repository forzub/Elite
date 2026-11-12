#include "src/game/system_map/SystemMapPlanetCarousel.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <map>
#include <unordered_map>

#include "render/Font.h"
#include "render/HUD/TextRenderer.h"
#include "src/render/HUD/HudPrimitiveBatch.h"
#include "src/render/ShaderUtils.h"
#include "src/render/navigation/NavigationOverlayVisualSettings.h"

namespace game::system_map
{
namespace
{
    using world::celestial::BodyType;
    using world::celestial::SystemMapBody;

    // Layout for a 1080 px-high viewport (scaled like the map buttons).
    constexpr float StripBottomPx = 84.0f;     // above the map button row and the breadcrumbs
    constexpr float StripHeightPx = 146.0f;
    constexpr float StripSideMarginPx = 220.0f;
    constexpr float MaxGlobePx = 96.0f;
    constexpr float MinGlobePx = 6.0f;
    constexpr float SlotGapPx = 18.0f;
    constexpr float MinSlotPx = 44.0f;
    constexpr float ArrowWidthPx = 30.0f;
    constexpr float LabelScale = 0.62f;
    constexpr float WheelStepPx = 90.0f;
    constexpr float ArrowStepFraction = 0.6f;
    constexpr double DragThresholdPx = 5.0;

    bool contains(const glm::vec4& r, double x, double y)
    {
        return x >= r.x && x <= r.z && y >= r.y && y <= r.w;
    }

    void rect(render::hud::HudPrimitiveBatch& batch, const glm::vec4& r, const glm::vec4& fill,
              const glm::vec4& line, float lineWidth)
    {
        const glm::vec2 a(r.x, r.y), b(r.z, r.y), c(r.z, r.w), d(r.x, r.w);
        if (fill.a > 0.0f)
        {
            batch.filledTriangle(a, b, c, fill);
            batch.filledTriangle(a, c, d, fill);
        }
        if (line.a > 0.0f)
        {
            batch.line(a, b, lineWidth, line);
            batch.line(b, c, lineWidth, line);
            batch.line(c, d, lineWidth, line);
            batch.line(d, a, lineWidth, line);
        }
    }

    constexpr const char* GlobeVs = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aCorner;
uniform vec4 uRect;        // left, top, size (px), unused
uniform vec2 uViewport;    // px
out vec2 vLocal;           // -1..1 over the quad
out vec2 vPx;              // viewport px
void main()
{
    vec2 px = uRect.xy + aCorner * uRect.z;
    vLocal = aCorner * 2.0 - 1.0;
    vPx = px;
    vec2 ndc = vec2(px.x / uViewport.x * 2.0 - 1.0, 1.0 - px.y / uViewport.y * 2.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
)GLSL";

    // A lit globe: the map's equirect albedo (u = atan2(z, x) / 2pi + 0.5, v = 0 at the south pole) and
    // cloud cover, light from the left (the star side of the strip).
    constexpr const char* GlobeFs = R"GLSL(
#version 330 core
in vec2 vLocal;
in vec2 vPx;
uniform sampler2D uAlbedo;
uniform sampler2D uClouds;
uniform int uHasAlbedo;
uniform int uHasClouds;
uniform float uPhase;
uniform float uTilt;
uniform float uPixel;      // one px in quad units (edge smoothing)
uniform vec2 uClipX;       // visible x range of the strip content
uniform vec3 uRim;         // selection / hover ring colour (alpha in uRimAlpha)
uniform float uRimAlpha;
out vec4 fragColor;
void main()
{
    if (vPx.x < uClipX.x || vPx.x > uClipX.y)
        discard;
    float r = length(vLocal);
    float ring = 0.0;
    if (uRimAlpha > 0.0)
    {
        float rr = r * 1.14;   // ring sits just outside the globe (the quad is 14 % larger)
        ring = smoothstep(1.02, 1.02 + uPixel * 2.0, rr) * (1.0 - smoothstep(1.10, 1.10 + uPixel * 2.0, rr));
    }
    float rs = r * 1.14;
    float disc = 1.0 - smoothstep(1.0 - uPixel * 1.6, 1.0, rs);
    if (disc <= 0.0 && ring <= 0.0)
        discard;
    vec2 p = vLocal * 1.14;
    float z = sqrt(max(0.0, 1.0 - dot(p, p)));
    vec3 n = vec3(p.x, -p.y, z);
    // body frame: tilt about the view x axis, then spin about the body axis
    float ct = cos(uTilt), st = sin(uTilt);
    vec3 b = vec3(n.x, ct * n.y - st * n.z, st * n.y + ct * n.z);
    float cp = cos(uPhase), sp = sin(uPhase);
    vec3 d = vec3(cp * b.x + sp * b.z, b.y, -sp * b.x + cp * b.z);
    vec2 uv = vec2(atan(d.z, d.x) / 6.2831853 + 0.5, asin(clamp(d.y, -1.0, 1.0)) / 3.1415927 + 0.5);
    vec3 albedo = uHasAlbedo == 1 ? texture(uAlbedo, uv).rgb : vec3(0.42, 0.45, 0.50);
    if (uHasClouds == 1)
    {
        vec4 c = texture(uClouds, uv);
        albedo = mix(albedo, c.rgb, clamp(c.a, 0.0, 1.0));
    }
    vec3 light = normalize(vec3(-0.78, 0.30, 0.55));
    float lambert = max(dot(n, light), 0.0);
    float limb = pow(z, 0.25);
    vec3 col = albedo * (0.16 + 0.95 * lambert) * limb;
    vec4 globe = vec4(col, disc);
    vec4 rim = vec4(uRim, uRimAlpha * ring);
    fragColor = globe.a > 0.0 ? vec4(mix(rim.rgb, globe.rgb, globe.a), max(globe.a, rim.a)) : rim;
}
)GLSL";
} // namespace

SystemMapPlanetCarousel::SystemMapPlanetCarousel() = default;

SystemMapPlanetCarousel::~SystemMapPlanetCarousel()
{
    if (m_vbo)
        glDeleteBuffers(1, &m_vbo);
    if (m_vao)
        glDeleteVertexArrays(1, &m_vao);
    if (m_program)
        glDeleteProgram(m_program);
}

void SystemMapPlanetCarousel::reset()
{
    m_items.clear();
    m_visible = false;
    m_scroll = 0.0f;
    m_systemId = -1;
    m_hovered = -1;
    m_hoveredArrow = 0;
    m_dragging = false;
    m_pressInside = false;
    m_pressItem = -1;
}

void SystemMapPlanetCarousel::ensureFont()
{
    if (m_font)
        return;
    for (const char* path : {"assets/fonts/NotoSansCJK-Regular.otf", "src/assets/fonts/NotoSansCJK-Regular.otf",
                             "assets/fonts/Roboto-Medium.ttf", "src/assets/fonts/Roboto-Medium.ttf"})
    {
        if (std::filesystem::exists(path))
        {
            m_font = std::make_unique<Font>(path, 18);
            return;
        }
    }
}

void SystemMapPlanetCarousel::ensureGlobeProgram()
{
    if (m_program)
        return;
    m_program = compileShader(GlobeVs, GlobeFs);
    const float corners[] = {0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1};
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
}

void SystemMapPlanetCarousel::layout(const Viewport& viewport, const world::celestial::SystemMapSnapshot& system)
{
    const render::navigation::NavigationOverlayVisualSettings visuals;
    const float s = std::clamp(static_cast<float>(viewport.height) / visuals.referenceHeightPx,
                               visuals.minimumScreenScale, visuals.maximumScreenScale);
    m_screenScale = s;

    if (system.systemId != m_systemId)
    {
        m_systemId = system.systemId;
        m_scroll = 0.0f;
        m_hovered = -1;
    }

    // planets (incl. dwarf planets) only - moons belong to their planet's Detail map; grouped by the star
    // (or barycenter) they orbit in snapshot order, then by orbit size
    std::unordered_map<std::string, const SystemMapBody*> byId;
    for (const auto& b : system.bodies)
        byId[b.id] = &b;
    std::map<std::string, int> groupOrder;
    for (const auto& b : system.bodies)
        if (b.type == BodyType::Star || b.type == BodyType::Barycenter)
            groupOrder.emplace(b.id, static_cast<int>(groupOrder.size()));
    struct Row
    {
        int group;
        double orbit;
        const SystemMapBody* body;
    };
    std::vector<Row> rows;
    double maxRadius = 0.0;
    for (const auto& b : system.bodies)
    {
        if (b.type != BodyType::Planet || b.radiusKm <= 0.0)
            continue;
        const auto parent = byId.find(b.parentId);
        if (parent != byId.end() && parent->second->type == BodyType::Planet)
            continue; // a planet's satellite catalogued as a planet
        const auto g = groupOrder.find(b.parentId);
        // circumbinary planets (around a barycenter) come after the single-star groups
        const int group = g == groupOrder.end() ? 1000
                        : (parent != byId.end() && parent->second->type == BodyType::Barycenter ? 500 + g->second : g->second);
        rows.push_back({group, b.orbitRadiusAu > 0.0 ? b.orbitRadiusAu : glm::length(b.positionAu), &b});
        maxRadius = std::max(maxRadius, b.radiusKm);
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        return a.group != b.group ? a.group < b.group : a.orbit < b.orbit;
    });

    ensureFont();
    const float labelScale = LabelScale * s;
    m_items.clear();
    float x = 0.0f;
    for (const auto& row : rows)
    {
        Item it;
        it.bodyId = row.body->id;
        it.name = row.body->name;
        it.body = row.body;
        it.diameterPx = std::max(MinGlobePx * s, static_cast<float>(row.body->radiusKm / maxRadius) * MaxGlobePx * s);
        const float label = m_font ? m_font->measureText(it.name) * labelScale : 40.0f * s;
        it.slotWidthPx = std::max({it.diameterPx, label, MinSlotPx * s}) + SlotGapPx * s;
        it.centerX = x + it.slotWidthPx * 0.5f;
        it.rotationPhase = static_cast<float>(row.body->rotationPhaseRad);
        it.axialTiltRad = static_cast<float>(glm::radians(std::clamp(row.body->axialTiltDeg, -90.0, 90.0)));
        x += it.slotWidthPx;
        m_items.push_back(std::move(it));
    }
    m_contentWidth = x;

    m_visible = !m_items.empty();
    if (!m_visible)
        return;
    const float bottom = static_cast<float>(viewport.height) - StripBottomPx * s;
    const float top = bottom - StripHeightPx * s;
    const float maxWidth = std::max(200.0f * s, static_cast<float>(viewport.width) - 2.0f * StripSideMarginPx * s);
    m_scrollable = m_contentWidth > maxWidth;
    const float inner = m_scrollable ? maxWidth - 2.0f * ArrowWidthPx * s : m_contentWidth;
    const float width = m_scrollable ? maxWidth : m_contentWidth;
    const float left = (static_cast<float>(viewport.width) - width) * 0.5f;
    m_strip = {left, top, left + width, bottom};
    const float contentLeft = m_scrollable ? left + ArrowWidthPx * s : left;
    m_content = {contentLeft, top, contentLeft + inner, bottom};
    m_leftArrow = {left, top, left + ArrowWidthPx * s, bottom};
    m_rightArrow = {left + width - ArrowWidthPx * s, top, left + width, bottom};
    m_scroll = std::clamp(m_scroll, 0.0f, maxScroll());
    m_globeRowY = top + 10.0f * s + MaxGlobePx * s * 0.5f;
    m_labelBaselineY = top + 10.0f * s + MaxGlobePx * s + 26.0f * s;
}

float SystemMapPlanetCarousel::maxScroll() const
{
    return m_scrollable ? std::max(0.0f, m_contentWidth - (m_content.z - m_content.x)) : 0.0f;
}

int SystemMapPlanetCarousel::itemAt(double x, double y) const
{
    if (!m_visible || !contains(m_content, x, y))
        return -1;
    const double cx = x - m_content.x + m_scroll;
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        const auto& it = m_items[i];
        if (std::abs(cx - it.centerX) <= it.slotWidthPx * 0.5f)
            return static_cast<int>(i);
    }
    return -1;
}

glm::vec2 SystemMapPlanetCarousel::itemScreenCenter(const std::string& bodyId) const
{
    if (!m_visible)
        return glm::vec2(-1.0f);
    // "#N" = the N-th globe of the strip (0-based; the DEV environment is not UTF-8, ids may be Cyrillic)
    const int index = !bodyId.empty() && bodyId[0] == '#' ? std::atoi(bodyId.c_str() + 1) : -1;
    for (std::size_t i = 0; i < m_items.size(); ++i)
        if (const auto& it = m_items[i]; it.bodyId == bodyId || static_cast<int>(i) == index)
        {
            const float cx = m_content.x + it.centerX - m_scroll;
            if (cx < m_content.x || cx > m_content.z)
                return glm::vec2(-1.0f);
            return glm::vec2(cx, m_globeRowY);
        }
    return glm::vec2(-1.0f);
}

bool SystemMapPlanetCarousel::handlePointer(double x, double y, bool leftDown, double& wheelY,
                                            std::string& clickedBodyId)
{
    clickedBodyId.clear();
    const bool pressed = leftDown && !m_leftWasDown;
    const bool released = !leftDown && m_leftWasDown;
    m_leftWasDown = leftDown;
    const bool over = m_visible && contains(m_strip, x, y);

    m_hovered = over ? itemAt(x, y) : -1;
    m_hoveredArrow = 0;
    if (over && m_scrollable)
    {
        if (contains(m_leftArrow, x, y))
            m_hoveredArrow = -1;
        else if (contains(m_rightArrow, x, y))
            m_hoveredArrow = 1;
    }

    if (pressed && over)
    {
        m_pressInside = true;
        m_pressX = x;
        m_pressScroll = m_scroll;
        m_dragging = false;
        m_pressItem = m_hovered;
        if (m_hoveredArrow != 0)
        {
            const float step = (m_content.z - m_content.x) * ArrowStepFraction;
            m_scroll = std::clamp(m_scroll + static_cast<float>(m_hoveredArrow) * step, 0.0f, maxScroll());
            m_pressInside = false;
        }
    }
    if (m_pressInside && leftDown && m_scrollable)
    {
        if (std::abs(x - m_pressX) > DragThresholdPx)
            m_dragging = true;
        if (m_dragging)
            m_scroll = std::clamp(m_pressScroll - static_cast<float>(x - m_pressX), 0.0f, maxScroll());
    }
    bool taken = over || m_pressInside;
    if (released && m_pressInside)
    {
        if (!m_dragging && m_pressItem >= 0 && m_pressItem == m_hovered)
            clickedBodyId = m_items[static_cast<std::size_t>(m_pressItem)].bodyId;
        m_pressInside = false;
        m_dragging = false;
        m_pressItem = -1;
    }
    if (over && wheelY != 0.0)
    {
        if (m_scrollable)
            m_scroll = std::clamp(m_scroll - static_cast<float>(wheelY) * WheelStepPx * m_screenScale, 0.0f, maxScroll());
        wheelY = 0.0;
    }
    return taken;
}

void SystemMapPlanetCarousel::drawGlobe(const Viewport& viewport, float cx, float cy, float diameter, GLuint albedo,
                                        GLuint clouds, float rotationPhase, float tilt, bool selected, bool hovered)
{
    const float quad = diameter * 1.14f; // the shader's ring room: globe radius = 1 / 1.14 of the quad
    glUniform4f(glGetUniformLocation(m_program, "uRect"), cx - quad * 0.5f, cy - quad * 0.5f, quad, 0.0f);
    glUniform2f(glGetUniformLocation(m_program, "uViewport"), static_cast<float>(viewport.width),
                static_cast<float>(viewport.height));
    glUniform1i(glGetUniformLocation(m_program, "uHasAlbedo"), albedo ? 1 : 0);
    glUniform1i(glGetUniformLocation(m_program, "uHasClouds"), clouds ? 1 : 0);
    glUniform1f(glGetUniformLocation(m_program, "uPhase"), rotationPhase);
    glUniform1f(glGetUniformLocation(m_program, "uTilt"), tilt);
    glUniform1f(glGetUniformLocation(m_program, "uPixel"), 2.0f / std::max(quad, 1.0f));
    glUniform2f(glGetUniformLocation(m_program, "uClipX"), m_content.x, m_content.z);
    const glm::vec3 rim = selected ? glm::vec3(0.96f, 0.80f, 0.40f) : glm::vec3(0.64f, 0.84f, 0.96f);
    glUniform3f(glGetUniformLocation(m_program, "uRim"), rim.r, rim.g, rim.b);
    glUniform1f(glGetUniformLocation(m_program, "uRimAlpha"), selected ? 0.95f : (hovered ? 0.75f : 0.0f));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, albedo);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, clouds);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void SystemMapPlanetCarousel::draw(const Viewport& viewport, const world::celestial::SystemMapSnapshot& system,
                                   const std::string& selectedBodyId, const TextureSource& textures)
{
    layout(viewport, system);
    if (!m_visible)
        return;
    const float s = m_screenScale;

    if (!m_batch)
        m_batch = std::make_unique<render::hud::HudPrimitiveBatch>();
    m_batch->begin(viewport.width, viewport.height);
    rect(*m_batch, m_strip, glm::vec4(0.02f, 0.06f, 0.09f, 0.62f), glm::vec4(0.44f, 0.62f, 0.74f, 0.45f), 1.2f * s);
    if (m_scrollable)
    {
        const auto arrow = [&](const glm::vec4& r, int dir, bool enabled) {
            const glm::vec4 col = !enabled ? glm::vec4(0.30f, 0.36f, 0.40f, 0.35f)
                                : m_hoveredArrow == dir ? glm::vec4(0.74f, 0.90f, 1.0f, 0.98f)
                                                        : glm::vec4(0.56f, 0.76f, 0.88f, 0.84f);
            const float cx = (r.x + r.z) * 0.5f, cy = (r.y + r.w) * 0.5f, h = 13.0f * s, w = 9.0f * s;
            m_batch->filledTriangle({cx + dir * w, cy}, {cx - dir * w, cy - h}, {cx - dir * w, cy + h}, col);
        };
        arrow(m_leftArrow, -1, m_scroll > 0.5f);
        arrow(m_rightArrow, 1, m_scroll < maxScroll() - 0.5f);
    }
    m_batch->flush();

    ensureGlobeProgram();
    if (m_program)
    {
        const GLboolean depth = glIsEnabled(GL_DEPTH_TEST);
        const GLboolean blend = glIsEnabled(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_program);
        glUniform1i(glGetUniformLocation(m_program, "uAlbedo"), 0);
        glUniform1i(glGetUniformLocation(m_program, "uClouds"), 1);
        glBindVertexArray(m_vao);
        for (std::size_t i = 0; i < m_items.size(); ++i)
        {
            const auto& it = m_items[i];
            // every planet's textures are requested (even scrolled out): that is the prewarm of the
            // Detail map; drawing only what is in the strip
            GLuint albedo = 0, clouds = 0;
            textures(*it.body, albedo, clouds);
            const float cx = m_content.x + it.centerX - m_scroll;
            if (cx + it.diameterPx < m_content.x || cx - it.diameterPx > m_content.z)
                continue;
            drawGlobe(viewport, cx, m_globeRowY, it.diameterPx, albedo, clouds, it.rotationPhase, it.axialTiltRad,
                      it.bodyId == selectedBodyId, static_cast<int>(i) == m_hovered);
        }
        glBindVertexArray(0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
        if (depth)
            glEnable(GL_DEPTH_TEST);
        if (!blend)
            glDisable(GL_BLEND);
    }

    if (!m_font)
        return;
    TextRenderer& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float scale = LabelScale * s;
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        const auto& it = m_items[i];
        const float cx = m_content.x + it.centerX - m_scroll;
        const float w = m_font->measureText(it.name) * scale;
        // a label is drawn only whole inside the strip (text is not clipped)
        if (cx - w * 0.5f < m_content.x - 1.0f || cx + w * 0.5f > m_content.z + 1.0f)
            continue;
        glm::vec4 color {0.56f, 0.76f, 0.88f, 0.84f};
        if (it.bodyId == selectedBodyId)
            color = {0.98f, 0.86f, 0.48f, 1.0f};
        else if (static_cast<int>(i) == m_hovered)
            color = {0.74f, 0.90f, 1.0f, 0.98f};
        text.textDraw(*m_font, it.name, cx - w * 0.5f, m_labelBaselineY, color, scale);
    }
    text.endFrame();
}
} // namespace game::system_map
