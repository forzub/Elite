#include "src/game/system_map/MapObjectOverlayRenderer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <GLFW/glfw3.h>

#include "render/HUD/TextRenderer.h"

namespace game::system_map
{
namespace
{
constexpr glm::vec4 kGlobalVelocityColor(0.25f, 0.66f, 1.00f, 0.96f);
constexpr glm::vec4 kLocalVelocityColor(0.32f, 0.95f, 0.46f, 0.96f);
constexpr glm::vec4 kPanelBackground(0.08f, 0.10f, 0.13f, 0.93f);
constexpr glm::vec4 kPanelHeader(0.15f, 0.17f, 0.22f, 0.96f);
constexpr glm::vec4 kPanelBorder(0.72f, 0.78f, 0.86f, 0.72f);
constexpr glm::vec4 kPanelText(0.93f, 0.95f, 0.98f, 1.0f);
constexpr glm::vec4 kPanelMuted(0.69f, 0.75f, 0.82f, 1.0f);

struct ScreenSpaceState
{
    GLint program = 0;
    GLint matrixMode = GL_MODELVIEW;
    GLint blendSrc = GL_ONE;
    GLint blendDst = GL_ZERO;
    GLboolean depthEnabled = GL_FALSE;
    GLboolean blendEnabled = GL_FALSE;
    GLfloat lineWidth = 1.0f;
};

ScreenSpaceState beginScreenSpace(const Viewport& viewport)
{
    ScreenSpaceState previous;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previous.program);
    glGetIntegerv(GL_MATRIX_MODE, &previous.matrixMode);
    glGetIntegerv(GL_BLEND_SRC_RGB, &previous.blendSrc);
    glGetIntegerv(GL_BLEND_DST_RGB, &previous.blendDst);
    previous.depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    previous.blendEnabled = glIsEnabled(GL_BLEND);
    glGetFloatv(GL_LINE_WIDTH, &previous.lineWidth);

    glUseProgram(0);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(
        0.0,
        static_cast<double>(viewport.width),
        static_cast<double>(viewport.height),
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    return previous;
}

void endScreenSpace(const ScreenSpaceState& previous)
{
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glLineWidth(previous.lineWidth);
    glBlendFunc(previous.blendSrc, previous.blendDst);
    if (previous.blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (previous.depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glUseProgram(static_cast<GLuint>(previous.program));
    glMatrixMode(previous.matrixMode);
}

void drawLine(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::vec4& color,
    float width = 1.0f
)
{
    glColor4f(color.r, color.g, color.b, color.a);
    glLineWidth(width);
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
    glLineWidth(1.0f);
}

void drawRect(
    const glm::dvec2& topLeft,
    double width,
    double height,
    const glm::vec4& color
)
{
    glColor4f(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2d(topLeft.x, topLeft.y);
    glVertex2d(topLeft.x + width, topLeft.y);
    glVertex2d(topLeft.x + width, topLeft.y + height);
    glVertex2d(topLeft.x, topLeft.y + height);
    glEnd();
}

void drawRectOutline(
    const glm::dvec2& topLeft,
    double width,
    double height,
    const glm::vec4& color,
    float lineWidth = 1.0f
)
{
    glColor4f(color.r, color.g, color.b, color.a);
    glLineWidth(lineWidth);
    glBegin(GL_LINE_LOOP);
    glVertex2d(topLeft.x, topLeft.y);
    glVertex2d(topLeft.x + width, topLeft.y);
    glVertex2d(topLeft.x + width, topLeft.y + height);
    glVertex2d(topLeft.x, topLeft.y + height);
    glEnd();
    glLineWidth(1.0f);
}

void drawActiveObjectRing(
    const MapObjectOverlayItem& item
)
{
    constexpr int segments = 28;
    const double radius = 13.0 * item.glyphScale;
    glm::vec4 color = item.factionColor;
    color.a = 0.92f;

    glColor4f(color.r, color.g, color.b, color.a);
    glLineWidth(1.8f);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i)
    {
        const double angle =
            6.28318530717958647692 *
            static_cast<double>(i) /
            static_cast<double>(segments);
        glVertex2d(
            item.screenPx.x + std::cos(angle) * radius,
            item.screenPx.y + std::sin(angle) * radius
        );
    }
    glEnd();
    glLineWidth(1.0f);
}

void drawTriangle(
    const MapObjectOverlayItem& item
)
{
    const glm::dvec2 forward = normalizedScreenDirection(
        item.facingScreenDirection
    );
    const glm::dvec2 right(-forward.y, forward.x);
    const double size = 8.0 * item.glyphScale;

    const glm::dvec2 tip = item.screenPx + forward * size * 1.25;
    const glm::dvec2 left = item.screenPx - forward * size * 0.75 - right * size * 0.72;
    const glm::dvec2 rightPoint = item.screenPx - forward * size * 0.75 + right * size * 0.72;

    glColor4f(
        item.factionColor.r,
        item.factionColor.g,
        item.factionColor.b,
        item.factionColor.a
    );
    glBegin(GL_TRIANGLES);
    glVertex2d(tip.x, tip.y);
    glVertex2d(left.x, left.y);
    glVertex2d(rightPoint.x, rightPoint.y);
    glEnd();

    glColor4f(1.0f, 1.0f, 1.0f, 0.62f);
    glBegin(GL_LINE_LOOP);
    glVertex2d(tip.x, tip.y);
    glVertex2d(left.x, left.y);
    glVertex2d(rightPoint.x, rightPoint.y);
    glEnd();
}


void drawRoutePoint(
    const MapObjectOverlayItem& item
)
{
    // Route intent should read like a simple map pin, not like another ship.
    // Keep it deliberately cartoon-simple: green square + center dot.
    const double half = 7.0 * item.glyphScale;
    const glm::vec4 color(0.38f, 0.96f, 0.58f, 0.96f);
    drawRectOutline(
        item.screenPx - glm::dvec2(half),
        half * 2.0,
        half * 2.0,
        color,
        1.8f
    );
    const double dotHalf = std::max(1.5, 1.8 * item.glyphScale);
    drawRect(
        item.screenPx - glm::dvec2(dotHalf),
        dotHalf * 2.0,
        dotHalf * 2.0,
        color
    );
}

void drawHubCube(
    const MapObjectOverlayItem& item
)
{
    // Hubs are deliberately not arrowheads: a compact isometric cube makes
    // infrastructure distinguishable from self-propelled craft even when a
    // distant cluster collapses to only a handful of pixels. Hub Map keeps
    // its existing structural Hub geometry and therefore sets drawGlyph=false.
    const double half = 5.8 * item.glyphScale;
    const glm::dvec2 offset(half * 0.72, -half * 0.72);

    const glm::dvec2 front[4] = {
        item.screenPx + glm::dvec2(-half, -half),
        item.screenPx + glm::dvec2( half, -half),
        item.screenPx + glm::dvec2( half,  half),
        item.screenPx + glm::dvec2(-half,  half)
    };
    glm::dvec2 back[4];
    for (int i = 0; i < 4; ++i)
        back[i] = front[i] + offset;

    glm::vec4 face = item.factionColor;
    face.a *= 0.24f;
    glColor4f(face.r, face.g, face.b, face.a);
    glBegin(GL_QUADS);
    for (const auto& p : front)
        glVertex2d(p.x, p.y);
    glEnd();

    glm::vec4 line = item.factionColor;
    line.a = std::max(line.a, 0.92f);
    for (int i = 0; i < 4; ++i)
    {
        const int next = (i + 1) % 4;
        drawLine(front[i], front[next], line, 1.6f);
        drawLine(back[i], back[next], line, 1.2f);
        drawLine(front[i], back[i], line, 1.2f);
    }
}

std::vector<std::string> wrapLabelForWidth(
    TextRenderer& textRenderer,
    const std::string& label,
    int pixelSize,
    float maxWidthPx
)
{
    if (label.empty() ||
        textRenderer.measureTextPx(label, pixelSize) <= maxWidthPx)
    {
        return {label};
    }

    std::vector<std::string> words;
    std::string currentWord;
    for (char ch : label)
    {
        if (ch == ' ')
        {
            if (!currentWord.empty())
            {
                words.push_back(currentWord);
                currentWord.clear();
            }
        }
        else
        {
            currentWord.push_back(ch);
        }
    }
    if (!currentWord.empty())
        words.push_back(currentWord);

    // Languages without spaces stay intact. Their glyph metrics still decide
    // whether the label fits; we never split a UTF-8 code point byte-by-byte.
    if (words.size() < 2)
        return {label};

    std::vector<std::string> lines;
    std::string line;
    for (const auto& word : words)
    {
        const std::string candidate =
            line.empty() ? word : line + " " + word;
        if (!line.empty() &&
            textRenderer.measureTextPx(candidate, pixelSize) > maxWidthPx)
        {
            lines.push_back(line);
            line = word;
        }
        else
        {
            line = candidate;
        }
    }
    if (!line.empty())
        lines.push_back(line);

    return lines.empty() ? std::vector<std::string>{label} : lines;
}

void drawVelocityArrow(
    const MapObjectOverlayItem& item
)
{
    const double speed = glm::length(item.velocityArrowMps);
    if (speed <= 1.0e-6)
        return;

    const glm::dvec2 direction = normalizedScreenDirection(
        item.velocityScreenDirection
    );
    const glm::dvec2 perpendicular(-direction.y, direction.x);

    const double baseLength = item.wideVelocityArrow ? 42.0 : 19.0;
    const double magnitudeScale = mapObjectVelocityArrowLengthScale(
        speed,
        item.arrowVelocityMode
    );
    const double length =
        baseLength * item.glyphScale * magnitudeScale;
    const double headScale = 0.70 + 0.30 * magnitudeScale;
    const double head =
        (item.wideVelocityArrow ? 9.0 : 5.0) *
        item.glyphScale * headScale;
    const glm::dvec2 start = item.screenPx + direction * (10.0 * item.glyphScale);
    const glm::dvec2 end = start + direction * length;

    glm::vec4 color =
        item.arrowVelocityMode == MapObjectVelocityMode::Local
            ? kLocalVelocityColor
            : kGlobalVelocityColor;

    if (item.wideVelocityArrow)
        color.a *= 0.48f;

    drawLine(start, end, color, item.wideVelocityArrow ? 5.0f : 2.0f);
    drawLine(end, end - direction * head + perpendicular * head * 0.65, color, item.wideVelocityArrow ? 4.0f : 2.0f);
    drawLine(end, end - direction * head - perpendicular * head * 0.65, color, item.wideVelocityArrow ? 4.0f : 2.0f);
}

std::string formatSpeed(double speed)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(speed >= 100.0 ? 0 : 1) << speed;
    return out.str();
}

std::string formatAngle(double degrees)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << degrees << "°";
    return out.str();
}

const MapObjectOverlayItem* findItem(
    const MapObjectOverlayFrame& frame,
    const std::string& id
)
{
    for (const auto& item : frame.items)
    {
        if (item.objectId == id)
            return &item;
    }
    return nullptr;
}

} // namespace

std::string MapObjectOverlayRenderer::text(
    const std::string& locale,
    const char* key
)
{
    const bool ru = locale.rfind("ru", 0) == 0;
    const bool zh = locale.rfind("zh", 0) == 0;
    const bool es = locale.rfind("es", 0) == 0;
    const bool ja = locale.rfind("ja", 0) == 0;

    const std::string k(key);
    if (k == "type") return ru ? "Тип" : zh ? "类型" : es ? "Tipo" : ja ? "種類" : "Type";
    if (k == "name") return ru ? "Название" : zh ? "名称" : es ? "Nombre" : ja ? "名称" : "Name";
    if (k == "local_speed") return ru ? "Локальная скорость" : zh ? "局部速度" : es ? "Velocidad local" : ja ? "ローカル速度" : "Local speed";
    if (k == "global_speed") return ru ? "Глобальная скорость" : zh ? "全局速度" : es ? "Velocidad global" : ja ? "グローバル速度" : "Global speed";
    if (k == "azimuth") return ru ? "Азимут" : zh ? "方位角" : es ? "Acimut" : ja ? "方位角" : "Azimuth";
    if (k == "elevation") return ru ? "Элевация" : zh ? "仰角" : es ? "Elevación" : ja ? "仰角" : "Elevation";
    if (k == "owner") return ru ? "Фракция/владелец" : zh ? "阵营/所有者" : es ? "Facción/propietario" : ja ? "勢力/所有者" : "Faction/owner";
    if (k == "radius") return ru ? "Радиус" : zh ? "半径" : es ? "Radio" : ja ? "半径" : "Radius";
    if (k == "address") return ru ? "Адрес" : zh ? "地址" : es ? "Dirección" : ja ? "アドレス" : "Address";
    if (k == "set_waypoint") return ru ? "ДОБАВИТЬ В МАРШРУТ" : zh ? "加入航线" : es ? "AÑADIR A RUTA" : ja ? "ルートに追加" : "ADD WAYPOINT";
    if (k == "set_rendezvous") return ru ? "ВСТРЕЧА НА МАРШРУТЕ" : zh ? "航线会合" : es ? "ENCUENTRO EN RUTA" : ja ? "航路上で合流" : "ROUTE RENDEZVOUS";
    if (k == "cancel_waypoint") return ru ? "УБРАТЬ ИЗ МАРШРУТА" : zh ? "从航线移除" : es ? "QUITAR DE RUTA" : ja ? "ルートから削除" : "REMOVE WAYPOINT";
    if (k == "set_finish") return ru ? "СДЕЛАТЬ ФИНИШЕМ" : zh ? "设为终点" : es ? "FIJAR DESTINO" : ja ? "目的地に設定" : "SET AS FINISH";
    if (k == "cancel_finish") return ru ? "ОТМЕНИТЬ ФИНИШ" : zh ? "取消终点" : es ? "CANCELAR DESTINO" : ja ? "終点を解除" : "CANCEL FINISH";
    if (k == "set_intermediate") return ru ? "СДЕЛАТЬ ПРОМЕЖУТОЧНОЙ" : zh ? "设为中间点" : es ? "FIJAR INTERMEDIA" : ja ? "中継点に設定" : "SET INTERMEDIATE";
    if (k == "cancel_intermediate") return ru ? "ОТМЕНИТЬ ПРОМЕЖУТОЧНУЮ" : zh ? "取消中间点" : es ? "CANCELAR INTERMEDIA" : ja ? "中継点を解除" : "CANCEL INTERMEDIATE";
    if (k == "space_target") return ru ? "Точка пространства" : zh ? "空间目标" : es ? "Objetivo espacial" : ja ? "空間目標" : "Space target";
    if (k == "finish_target") return ru ? "Финиш" : zh ? "终点" : es ? "Destino" : ja ? "終点" : "Finish";
    if (k == "intermediate_target") return ru ? "Промежуточная" : zh ? "中间点" : es ? "Intermedia" : ja ? "中継点" : "Intermediate";
    return k;
}

void MapObjectOverlayRenderer::render(
    const Viewport& viewport,
    const MapObjectOverlayFrame& frame,
    MapObjectOverlayState& state,
    const std::string& locale
) const
{
    const ScreenSpaceState previousGlState = beginScreenSpace(viewport);

    // Trajectory seam: nothing is synthesized here. Only authoritative or
    // explicitly predicted samples supplied by a future producer are drawn.
    for (const auto& trajectory : frame.trajectories)
    {
        if (trajectory.points.size() < 2)
            continue;
        // Projection belongs to the producer; current trajectory model is a
        // data contract only. Rendering activates when projected samples are
        // added to the frame without changing object/card ownership.
    }

    auto& textRenderer = TextRenderer::instance();
    textRenderer.beginFrameForViewport(viewport.width, viewport.height);

    for (const auto& item : frame.items)
    {
        if (!item.visible)
            continue;

        if (item.routeDisplayIndex > 0)
        {
            if (state.isActive(item.objectId))
                drawActiveObjectRing(item);
            drawRoutePoint(item);
        }
        else if (item.drawGlyph)
        {
            if (state.isActive(item.objectId))
                drawActiveObjectRing(item);

            if (item.infoKind == MapObjectInfoKind::WaypointCandidate)
                drawRoutePoint(item);
            else if (item.kind == MapObjectGlyphKind::Hub)
                drawHubCube(item);
            else
                drawTriangle(item);
        }

        if (item.infoKind == MapObjectInfoKind::Tactical)
        {
            drawVelocityArrow(item);
        }

        const std::string track =
            item.routeDisplayIndex > 0
                ? std::to_string(item.routeDisplayIndex)
                : state.trackLabelFor(item.objectId);
        if (!track.empty() && track != "0")
        {
            const float trackWidth = textRenderer.measureTextPx(track, 12);
            textRenderer.textDrawPx(
                track,
                static_cast<float>(item.screenPx.x - trackWidth * 0.5f),
                static_cast<float>(item.screenPx.y + 4.0f),
                12,
                item.routeDisplayIndex > 0
                    ? glm::vec4(0.72f, 1.0f, 0.78f, 1.0f)
                    : glm::vec4(0.96f, 0.98f, 1.0f, 1.0f)
            );
        }
    }

    const auto panels = state.orderedPanels();
    for (const auto& panel : panels)
    {
        const auto* item = findItem(frame, panel.objectId);
        if (!item || !item->visible)
            continue;

        const double panelHeight =
            panel.collapsed
                ? MapObjectOverlayState::PanelCollapsedHeightPx
                : panel.expandedHeightPx;
        const glm::dvec2 panelCenter(
            panel.topLeftPx.x + MapObjectOverlayState::PanelWidthPx * 0.5,
            panel.topLeftPx.y + panelHeight * 0.5
        );
        drawLine(item->screenPx, panelCenter, glm::vec4(0.84f, 0.88f, 0.95f, 0.72f), 1.0f);

        drawRect(
            panel.topLeftPx,
            MapObjectOverlayState::PanelWidthPx,
            panelHeight,
            kPanelBackground
        );
        drawRect(
            panel.topLeftPx,
            MapObjectOverlayState::PanelWidthPx,
            MapObjectOverlayState::PanelHeaderHeightPx,
            kPanelHeader
        );
        const bool activePanel = state.isActive(item->objectId);
        glm::vec4 panelBorder =
            activePanel
                ? item->factionColor
                : kPanelBorder;
        if (activePanel)
            panelBorder.a = 1.0f;

        drawRectOutline(
            panel.topLeftPx,
            MapObjectOverlayState::PanelWidthPx,
            panelHeight,
            panelBorder,
            activePanel ? 2.0f : 1.0f
        );

        std::string panelTitle;
        if (item->infoKind == MapObjectInfoKind::Tactical)
        {
            const std::string track = state.trackLabelFor(item->objectId);
            panelTitle =
                (track == "0" ? std::string() : track + "  ") +
                (item->name.empty() ? item->typeName : item->name);
        }
        else if (item->infoKind == MapObjectInfoKind::WaypointCandidate)
        {
            panelTitle = item->name.empty() ? text(locale, "space_target") : item->name;
        }
        else
        {
            panelTitle = item->name.empty() ? item->typeName : item->name;
        }

        textRenderer.textDrawPx(
            panelTitle,
            static_cast<float>(panel.topLeftPx.x + 9.0),
            static_cast<float>(panel.topLeftPx.y + 18.0),
            12,
            glm::vec4(item->factionColor.r, item->factionColor.g, item->factionColor.b, 1.0f)
        );
        textRenderer.textDrawPx(
            panel.collapsed ? "+" : "−",
            static_cast<float>(panel.topLeftPx.x + MapObjectOverlayState::PanelWidthPx - 38.0),
            static_cast<float>(panel.topLeftPx.y + 18.0),
            13,
            kPanelText
        );
        textRenderer.textDrawPx(
            "×",
            static_cast<float>(panel.topLeftPx.x + MapObjectOverlayState::PanelWidthPx - 19.0),
            static_cast<float>(panel.topLeftPx.y + 18.0),
            13,
            kPanelText
        );

        if (panel.collapsed)
            continue;

        const double speed = glm::length(item->displayedVelocityMps);
        const double bearingSpeed = glm::length(item->stellarVelocityMps);
        const bool hasBearing = bearingSpeed > 1.0e-9;
        const auto [azimuth, elevation] = stellarAzimuthElevationDeg(item->stellarVelocityMps);
        const char* speedKey =
            item->velocityMode == MapObjectVelocityMode::Local
                ? "local_speed"
                : "global_speed";

        const double x = panel.topLeftPx.x + 9.0;
        constexpr double kValueColumnOffsetPx = 105.0;
        constexpr float kLabelColumnWidthPx = 96.0f;
        constexpr double kWrappedLineStepPx = 12.0;
        double y = panel.topLeftPx.y + 43.0;
        auto drawField = [&](const std::string& label, const std::string& value)
        {
            const std::string punctuated = label + ":";
            const auto lines = wrapLabelForWidth(
                textRenderer,
                punctuated,
                10,
                kLabelColumnWidthPx
            );

            for (std::size_t i = 0; i < lines.size(); ++i)
            {
                textRenderer.textDrawPx(
                    lines[i],
                    static_cast<float>(x),
                    static_cast<float>(y + kWrappedLineStepPx * i),
                    10,
                    kPanelMuted
                );
            }

            const double valueY =
                y + kWrappedLineStepPx *
                    static_cast<double>(lines.size() - 1u);
            textRenderer.textDrawPx(
                value,
                static_cast<float>(x + kValueColumnOffsetPx),
                static_cast<float>(valueY),
                10,
                kPanelText
            );
            y += 18.0 +
                kWrappedLineStepPx *
                    static_cast<double>(lines.size() - 1u);
        };

        const std::string typeValue =
            item->typeName.empty() ? "—" : item->typeName;
        drawField(text(locale, "type"), typeValue);

        if (item->infoKind == MapObjectInfoKind::Tactical)
        {
            drawField(text(locale, speedKey), formatSpeed(speed) + " m/s");
            drawField(text(locale, "azimuth"), hasBearing ? formatAngle(azimuth) : "—");
            drawField(text(locale, "elevation"), hasBearing ? formatAngle(elevation) : "—");
            if (!item->owner.empty())
                drawField(text(locale, "owner"), item->owner);
        }

        for (const auto& field : item->extraFields)
        {
            const std::string label = text(locale, field.labelKey.c_str());
            const std::string value = field.unit.empty()
                ? field.value
                : field.value + " " + field.unit;
            drawField(label, value);
        }

        constexpr double actionWidth = MapObjectOverlayState::PanelWidthPx - 16.0;
        constexpr double actionHeight = 23.0;
        constexpr double actionGap = 5.0;
        double actionTop = panel.topLeftPx.y + panelHeight - 8.0 - actionHeight;
        for (auto it = item->panelActions.rbegin(); it != item->panelActions.rend(); ++it)
        {
            if (!it->visible)
                continue;

            const glm::dvec2 actionTopLeft(
                panel.topLeftPx.x + 8.0,
                actionTop
            );
            glm::vec4 fill = glm::vec4(0.10f, 0.18f, 0.24f, 0.92f);
            glm::vec4 border = it->active
                ? item->factionColor
                : glm::vec4(item->factionColor.r, item->factionColor.g, item->factionColor.b, 0.55f);
            glm::vec4 textColor =
                it->enabled
                    ? glm::vec4(item->factionColor.r, item->factionColor.g, item->factionColor.b, 1.0f)
                    : glm::vec4(kPanelMuted.r, kPanelMuted.g, kPanelMuted.b, 0.82f);
            if (it->active)
                fill = glm::vec4(item->factionColor.r * 0.22f, item->factionColor.g * 0.22f, item->factionColor.b * 0.22f, 0.96f);
            drawRect(actionTopLeft, actionWidth, actionHeight, fill);
            drawRectOutline(actionTopLeft, actionWidth, actionHeight, border, it->active ? 1.4f : 1.0f);
            const std::string actionText = text(locale, it->labelKey.c_str());
            textRenderer.textDrawPx(
                actionText,
                static_cast<float>(actionTopLeft.x + 8.0),
                static_cast<float>(actionTopLeft.y + 16.0),
                10,
                textColor
            );
            actionTop -= actionHeight + actionGap;
        }
    }

    textRenderer.endFrame();
    endScreenSpace(previousGlState);
}

} // namespace game::system_map
