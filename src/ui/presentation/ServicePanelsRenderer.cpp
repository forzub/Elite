#include "src/ui/presentation/ServicePanelsRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <glad/gl.h>

#include "render/HUD/TextRenderer.h"

namespace ui::presentation
{
namespace
{
using game::presentation::RefitModuleView;

const glm::vec4 Bg {0.008f, 0.018f, 0.035f, 1.0f};
const glm::vec4 Panel {0.016f, 0.036f, 0.062f, 1.0f};
const glm::vec4 Line {0.32f, 0.62f, 0.86f, 0.55f};
const glm::vec4 Title {1.0f, 0.70f, 0.42f, 0.98f};
const glm::vec4 Text {0.86f, 0.92f, 0.98f, 0.98f};
const glm::vec4 Muted {0.45f, 0.60f, 0.73f, 0.92f};
const glm::vec4 Good {0.36f, 0.86f, 0.55f, 0.98f};
const glm::vec4 Warn {1.0f, 0.78f, 0.30f, 0.98f};
const glm::vec4 Bad {1.0f, 0.36f, 0.30f, 0.98f};
const glm::vec4 Selected {0.10f, 0.22f, 0.34f, 1.0f};

std::string fmt(const char* f, double v)
{
    char b[64];
    std::snprintf(b, sizeof b, f, v);
    return b;
}

std::string grouped(double v)
{
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.0f", v);
    std::string s = buf, out;
    const int n = static_cast<int>(s.size());
    for (int i = 0; i < n; ++i)
    {
        out.push_back(s[static_cast<std::size_t>(i)]);
        if ((n - i - 1) % 3 == 0 && i != n - 1 && s[0] != '-')
            out.push_back(' ');
    }
    return out;
}

glm::vec4 categoryColour(const std::string& c)
{
    if (c == "hull" || c == "armour") return {0.42f, 0.46f, 0.52f, 0.85f};
    if (c == "drive") return {0.90f, 0.42f, 0.12f, 0.95f};
    if (c == "reactor") return {0.86f, 0.22f, 0.22f, 0.95f};
    if (c == "service_reactor") return {0.78f, 0.42f, 0.42f, 0.95f};
    if (c == "coolant_loop" || c == "radiator") return {0.25f, 0.55f, 0.90f, 0.95f};
    if (c == "jump_core") return {0.62f, 0.36f, 0.86f, 0.95f};
    if (c == "tank") return {0.66f, 0.52f, 0.86f, 0.90f};
    if (c == "rcs") return {0.86f, 0.70f, 0.24f, 0.95f};
    if (c == "radar" || c == "comm" || c == "transponder") return {0.20f, 0.72f, 0.60f, 0.95f};
    if (c == "computer") return {0.24f, 0.78f, 0.62f, 0.95f};
    if (c == "life_support" || c == "escape_capsule" || c == "lifeboat" || c == "medbay") return {0.40f, 0.70f, 0.30f, 0.95f};
    if (c == "container_rack" || c == "train_module" || c == "bulk_hold") return {0.55f, 0.55f, 0.55f, 0.90f};
    if (c == "passenger_module") return {0.70f, 0.68f, 0.48f, 0.90f};
    if (c == "weapon") return {0.80f, 0.20f, 0.16f, 0.95f};
    return {0.55f, 0.58f, 0.62f, 0.9f};
}

glm::vec4 stateColour(const RefitModuleView& m)
{
    if (m.state == "detached") return {0.20f, 0.22f, 0.26f, 0.9f};
    if (m.failed()) return Bad;
    if (m.health01 < 0.7 || m.damage01 > 0.3) return {1.0f, 0.55f, 0.20f, 0.95f};
    if (m.damaged()) return Warn;
    return {0.30f, 0.66f, 0.86f, 0.85f};
}

const char* stateRu(const std::string& s)
{
    if (s == "attached") return "в строю";
    if (s == "disabled") return "отключён (выведен из строя)";
    if (s == "destroyed") return "разрушен";
    if (s == "detached") return "снесён / отстыкован";
    if (s == "hanging") return "висит на креплениях";
    return "нет данных";
}

const char* levelRu(const std::string& s)
{
    if (s == "crew") return "экипажем";
    if (s == "station") return "на станции";
    if (s == "yard") return "на верфи";
    if (s == "factory") return "только на заводе";
    return s.c_str();
}

struct Ctx
{
    TextRenderer& t;
    float w, h;
    int px, small, big;
};

// side view (z right = aft, y up) of boxes into a rect; returns the scale
struct View
{
    float x0, y0, x1, y1;
    float scale;
    float cz, cy;
    [[nodiscard]] float sx(float z) const { return (x0 + x1) * 0.5f + (z - cz) * scale; }
    [[nodiscard]] float sy(float y) const { return (y0 + y1) * 0.5f - (y - cy) * scale; }
};

View fitView(float x0, float y0, float x1, float y1, float lengthM, float heightM)
{
    View v {x0, y0, x1, y1, 1.0f, 0.0f, 0.0f};
    v.scale = std::min((x1 - x0) / std::max(1.0f, lengthM * 1.08f), (y1 - y0) / std::max(1.0f, heightM * 1.15f));
    return v;
}
} // namespace

// ============================================================ F6 SHIPYARD
void ServicePanelsRenderer::renderShipyard(const Viewport& viewport, const game::presentation::ShipyardPresentation& page)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    m_hits.clear();
    m_zones.clear();
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    auto& t = TextRenderer::instance();
    t.beginFrameForViewport(viewport.width, viewport.height);
    const float w = static_cast<float>(viewport.width), h = static_cast<float>(viewport.height);
    const int px = std::clamp(static_cast<int>(h * 0.018f), 12, 17);
    const int small = std::clamp(static_cast<int>(h * 0.015f), 11, 14);
    const int big = std::clamp(static_cast<int>(h * 0.034f), 20, 34);
    t.solidRectPx(0, 0, w, h, Bg);
    t.solidRectPx(0, 0, w, 2, Line);
    t.textDrawPx("ВЕРФЬ — покупка корабля", 24, 18, big, Title);
    t.textDrawPx("F6  /  модели, паспорта и предложения продавцов из каталога кораблей", 24, 18 + big + 6, small, Muted);

    // ---- right: every catalog model
    const float listX = w * 0.70f, listY = 18.0f + big + 40.0f, listW = w - listX - 20.0f, listH = h - listY - 56.0f;
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({1, listX, listY, listX + listW, listY + listH});
    const float rowH = static_cast<float>(px) * 2.6f;
    const float maxScroll = std::max(0.0f, rowH * static_cast<float>(page.models.size()) - listH);
    m_shipyardListScroll = std::clamp(m_shipyardListScroll, 0.0f, maxScroll);
    for (std::size_t i = 0; i < page.models.size(); ++i)
    {
        const auto& m = page.models[i];
        const float y = listY + rowH * static_cast<float>(i) - m_shipyardListScroll;
        if (y < listY - 1.0f || y + rowH > listY + listH + 1.0f)
            continue;
        if (m.selected)
            t.solidRectPx(listX, y, listW, rowH, Selected);
        t.textDrawPx(m.name, listX + 10, y + 4, px, m.selected ? Title : Text);
        t.textDrawPx(m.manufacturer, listX + 10, y + 6 + static_cast<float>(px), small, Muted);
        const std::string price = grouped(m.priceListCr / 1000.0) + " тыс. Cr";
        t.textDrawPx(price, listX + listW - t.measureTextPx(price, small) - 10, y + 4, small, Muted);
        t.solidRectPx(listX, y + rowH - 1, listW, 1, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));
        m_hits.push_back({listX, y, listX + listW, y + rowH, 1, {ServicePanelActionType::ShipyardSelectModel, m.passportId}});
    }

    // ---- left: the selected model card
    const auto& c = page.card;
    const float cx = 24.0f, cw = listX - 48.0f;
    float y = listY;
    if (c.valid)
    {
        t.textDrawPx(c.name, cx, y, big, Text);
        y += static_cast<float>(big) + 8;
        t.textDrawPx(c.manufacturer + "  ·  " + c.shipClass, cx, y, px, Muted);
        y += static_cast<float>(px) + 4;
        t.textDrawPx(c.purpose, cx, y, small, Muted);
        y += static_cast<float>(small) + 10;

        // preview: side view (top) and plan view (bottom) of the runtime primitive blocks
        const float pvH = std::min(h * 0.30f, 260.0f);
        t.solidRectPx(cx, y, cw, pvH, Panel);
        const float L = c.envelopeMeters.x, W = c.envelopeMeters.y, H = c.envelopeMeters.z;
        const auto side = fitView(cx + 8, y + 8, cx + cw - 8, y + pvH * 0.5f - 4, L, H);
        const auto plan = fitView(cx + 8, y + pvH * 0.5f + 4, cx + cw - 8, y + pvH - 8, L, W);
        const float s = std::min(side.scale, plan.scale);
        auto sideV = side;
        sideV.scale = s;
        auto planV = plan;
        planV.scale = s;
        for (const auto& b : c.preview)
        {
            const auto col = categoryColour(b.category);
            t.solidRectPx(sideV.sx(b.centerMeters.z - b.halfSizeMeters.z), sideV.sy(b.centerMeters.y + b.halfSizeMeters.y),
                          std::max(1.0f, 2 * b.halfSizeMeters.z * s), std::max(1.0f, 2 * b.halfSizeMeters.y * s), col);
            t.solidRectPx(planV.sx(b.centerMeters.z - b.halfSizeMeters.z), planV.sy(b.centerMeters.x + b.halfSizeMeters.x),
                          std::max(1.0f, 2 * b.halfSizeMeters.z * s), std::max(1.0f, 2 * b.halfSizeMeters.x * s), col);
        }
        t.textDrawPx("сбоку  ·  нос слева", cx + 10, y + 6, small, Muted);
        t.textDrawPx("сверху", cx + 10, y + pvH * 0.5f + 6, small, Muted);
        y += pvH + 12;

        // the main specs (two columns)
        const float colW = cw * 0.5f;
        const float lineH = static_cast<float>(px) * 1.55f;
        for (std::size_t i = 0; i < c.mainSpecs.size(); ++i)
        {
            const float x = cx + (i % 2 == 0 ? 0.0f : colW);
            const float yy = y + lineH * 2.0f * static_cast<float>(i / 2);
            t.textDrawPx(c.mainSpecs[i].labelRu, x, yy, small, Muted);
            t.textDrawPx(c.mainSpecs[i].value, x, yy + static_cast<float>(small) + 3, px, Text);
        }
        y += lineH * 2.0f * static_cast<float>((c.mainSpecs.size() + 1) / 2) + 6;

        // seller offers (concrete instances)
        t.textDrawPx("Предложения продавцов", cx, y, px, Title);
        t.textDrawPx("DEV: пробный склад до подключения экономики", cx + t.measureTextPx("Предложения продавцов", px) + 16, y + 2, small, Muted);
        y += static_cast<float>(px) + 8;
        const float cols[] = {0.0f, 0.30f, 0.44f, 0.56f, 0.70f, 0.80f};
        const char* heads[] = {"продавец", "состояние", "возраст", "наработка", "износ / повр.", "цена"};
        for (int k = 0; k < 6; ++k)
            t.textDrawPx(heads[k], cx + cw * cols[k], y, small, Muted);
        y += static_cast<float>(small) + 6;
        for (const auto& o : c.offers)
        {
            const char* cond = o.condition == "new" ? "новый" : o.condition == "good" ? "хорошее" : o.condition == "worn" ? "изношен" : "на разбор";
            t.textDrawPx(o.sellerNameRu, cx, y, small, Text);
            t.textDrawPx(cond, cx + cw * cols[1], y, small, o.condition == "wreck" ? Bad : o.condition == "worn" ? Warn : Good);
            t.textDrawPx(fmt("%.0f лет", o.ageYears), cx + cw * cols[2], y, small, Text);
            t.textDrawPx(grouped(o.operatingHours) + " ч", cx + cw * cols[3], y, small, Text);
            t.textDrawPx(fmt("%.0f%%", o.wear01 * 100.0) + " / " + fmt("%.0f%%", o.damage01 * 100.0), cx + cw * cols[4], y, small, Text);
            t.textDrawPx(grouped(o.priceCr) + " Cr", cx + cw * cols[5], y, small, Title);
            y += static_cast<float>(small) + 7;
        }
        y += 8;
        const std::string toggle = page.passportExpanded ? "[-] Паспорт (свернуть)" : "[+] Полный паспорт и оборудование";
        t.textDrawPx(toggle, cx, y, px, Line);
        m_hits.push_back({cx, y - 2, cx + t.measureTextPx(toggle, px) + 8, y + static_cast<float>(px) + 4, 0,
                          {ServicePanelActionType::ShipyardTogglePassport, {}}});
        y += static_cast<float>(px) + 8;
        if (page.passportExpanded)
        {
            const float bottom = h - 56.0f;
            m_zones.push_back({3, cx, y, cx + cw, bottom});
            const float rowHp = static_cast<float>(small) + 6;
            const float maxCard = std::max(0.0f, rowHp * static_cast<float>(c.passportSpecs.size()) - (bottom - y));
            m_cardScroll = std::clamp(m_cardScroll, 0.0f, maxCard);
            for (std::size_t i = 0; i < c.passportSpecs.size(); ++i)
            {
                const float yy = y + rowHp * static_cast<float>(i) - m_cardScroll;
                if (yy < y || yy + rowHp > bottom)
                    continue;
                t.textDrawPx(c.passportSpecs[i].labelRu, cx, yy, small, Muted);
                t.textDrawPx(c.passportSpecs[i].value, cx + cw * 0.30f, yy, small, Text);
            }
        }
    }
    t.endFrame();
    glEnable(GL_DEPTH_TEST);
}

// ============================================================ F7 REPAIR & REFIT
void ServicePanelsRenderer::renderRepairRefit(const Viewport& viewport, const game::presentation::RepairRefitPresentation& page)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    m_hits.clear();
    m_zones.clear();
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    auto& t = TextRenderer::instance();
    t.beginFrameForViewport(viewport.width, viewport.height);
    const float w = static_cast<float>(viewport.width), h = static_cast<float>(viewport.height);
    const int px = std::clamp(static_cast<int>(h * 0.018f), 12, 17);
    const int small = std::clamp(static_cast<int>(h * 0.015f), 11, 14);
    const int big = std::clamp(static_cast<int>(h * 0.030f), 18, 30);
    t.solidRectPx(0, 0, w, h, Bg);
    t.solidRectPx(0, 0, w, 2, Line);
    if (!page.valid)
    {
        t.textDrawPx("РЕМОНТ И ПЕРЕОБОРУДОВАНИЕ", 24, 18, big, Title);
        t.textDrawPx("нет данных о кораблях игрока", 24, 24 + big, px, Muted);
        t.endFrame();
        glEnable(GL_DEPTH_TEST);
        return;
    }

    // ---- header: < ship >
    const auto& tab = page.ships[static_cast<std::size_t>(std::clamp(page.currentShip, 0, static_cast<int>(page.ships.size()) - 1))];
    const std::string head = tab.name + (tab.registration.empty() ? std::string() : "  [" + tab.registration + "]");
    const float arrowW = static_cast<float>(big) * 1.4f;
    t.textDrawPx("<", 24, 16, big, page.ships.size() > 1 ? Line : Muted);
    m_hits.push_back({16, 12, 24 + arrowW, 20 + static_cast<float>(big), 0, {ServicePanelActionType::RefitPreviousShip, {}}});
    t.textDrawPx(head, 24 + arrowW, 16, big, Title);
    const float headEnd = 24 + arrowW + t.measureTextPx(head, big) + 16;
    t.textDrawPx(">", headEnd, 16, big, page.ships.size() > 1 ? Line : Muted);
    m_hits.push_back({headEnd - 8, 12, headEnd + arrowW, 20 + static_cast<float>(big), 0, {ServicePanelActionType::RefitNextShip, {}}});
    t.textDrawPx("F7  /  ремонт и переоборудование  ·  " + page.modelName + "  ·  корабль " + std::to_string(page.currentShip + 1) + " из " +
                     std::to_string(page.ships.size()),
                 24, 22 + static_cast<float>(big), small, Muted);

    const float leftW = w * 0.60f, top = 30.0f + static_cast<float>(big + small);
    // ---- functional schematic: side view of every module placement
    const float schH = std::min(h * 0.34f, 320.0f);
    t.solidRectPx(16, top, leftW - 24, schH, Panel);
    const float L = page.envelopeMeters.z, H = page.envelopeMeters.y;
    const auto view = fitView(24, top + 8, leftW - 16, top + schH - 8, L, std::max(H, L * 0.18f));
    std::vector<const RefitModuleView*> placed;
    for (const auto& m : page.modules)
        if (m.hasPlacement)
            placed.push_back(&m);
    // big first, small last: small boxes stay visible and win the click
    std::stable_sort(placed.begin(), placed.end(), [](const auto* a, const auto* b) {
        return a->halfSizeMeters.x * a->halfSizeMeters.y * a->halfSizeMeters.z > b->halfSizeMeters.x * b->halfSizeMeters.y * b->halfSizeMeters.z;
    });
    for (const auto* m : placed)
    {
        const float x0 = view.sx(m->centerMeters.z - m->halfSizeMeters.z), x1 = view.sx(m->centerMeters.z + m->halfSizeMeters.z);
        const float y0 = view.sy(m->centerMeters.y + m->halfSizeMeters.y), y1 = view.sy(m->centerMeters.y - m->halfSizeMeters.y);
        const bool hull = m->category == "hull" || m->group == "structure";
        glm::vec4 col = hull ? glm::vec4(0.12f, 0.17f, 0.22f, 1.0f) : stateColour(*m);
        if (hull && m->damaged())
            col = glm::vec4(0.35f, 0.18f, 0.12f, 1.0f);
        t.solidRectPx(x0, y0, std::max(2.0f, x1 - x0), std::max(2.0f, y1 - y0), col);
        if (m->selected)
        {
            t.solidRectPx(x0 - 2, y0 - 2, std::max(2.0f, x1 - x0) + 4, 2, Title);
            t.solidRectPx(x0 - 2, y1, std::max(2.0f, x1 - x0) + 4, 2, Title);
        }
        m_hits.push_back({x0, y0, std::max(x0 + 4, x1), std::max(y0 + 4, y1), 0, {ServicePanelActionType::RefitSelectModule, m->moduleId}});
    }
    if (placed.empty())
        t.textDrawPx("у этого корпуса нет паспортных позиций модулей: выбор по списку ниже", 28, top + schH * 0.5f, small, Muted);
    t.textDrawPx("функциональная схема · сбоку, нос слева · цвет = состояние", 24, top + 6, small, Muted);

    // ---- group chips
    float gx = 16, gy = top + schH + 10;
    for (const auto& g : page.groups)
    {
        const std::string label = g.labelRu + " " + std::to_string(g.modules) + (g.failed ? "  отказ " + std::to_string(g.failed) : g.damaged ? "  повр. " + std::to_string(g.damaged) : "");
        const float cw = t.measureTextPx(label, small) + 16;
        if (gx + cw > leftW - 8)
        {
            gx = 16;
            gy += static_cast<float>(small) + 14;
        }
        t.solidRectPx(gx, gy, cw, static_cast<float>(small) + 10, g.selected ? Selected : Panel);
        t.textDrawPx(label, gx + 8, gy + 5, small, g.failed ? Bad : g.damaged ? Warn : Text);
        m_hits.push_back({gx, gy, gx + cw, gy + static_cast<float>(small) + 10, 0, {ServicePanelActionType::RefitSelectGroup, g.key}});
        gx += cw + 6;
    }
    gy += static_cast<float>(small) + 18;

    // ---- module list (selected group, or all damaged modules)
    std::string group;
    for (const auto& g : page.groups)
        if (g.selected)
            group = g.key;
    std::vector<const RefitModuleView*> rows;
    for (const auto& m : page.modules)
        if (group.empty() ? m.damaged() : m.group == group)
            rows.push_back(&m);
    const float listBottom = h - 52.0f;
    t.textDrawPx(group.empty() ? "повреждённые модули корабля" : "модули группы", 16, gy, small, Muted);
    gy += static_cast<float>(small) + 6;
    m_zones.push_back({2, 16, gy, leftW - 8, listBottom});
    const float rowH = static_cast<float>(px) + 10;
    m_refitListScroll = std::clamp(m_refitListScroll, 0.0f, std::max(0.0f, rowH * static_cast<float>(rows.size()) - (listBottom - gy)));
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const auto& m = *rows[i];
        const float y = gy + rowH * static_cast<float>(i) - m_refitListScroll;
        if (y < gy - 1 || y + rowH > listBottom)
            continue;
        if (m.selected)
            t.solidRectPx(16, y, leftW - 24, rowH, Selected);
        t.solidRectPx(20, y + 4, 6, rowH - 8, stateColour(m));
        t.textDrawPx(m.label, 34, y + 4, px, Text);
        const std::string st = std::string(stateRu(m.state)) + fmt("  %.0f%%", m.health01 * 100.0) + (m.damage01 > 0.001 ? fmt("  повр. %.0f%%", m.damage01 * 100.0) : "");
        t.textDrawPx(st, leftW - 24 - t.measureTextPx(st, small) - 8, y + 6, small, stateColour(m));
        m_hits.push_back({16, y, leftW - 8, y + rowH, 2, {ServicePanelActionType::RefitSelectModule, m.moduleId}});
    }
    if (rows.empty())
        t.textDrawPx(group.empty() ? "повреждений нет" : "—", 34, gy + 4, px, Good);

    // ---- right: the selected module and the consequences of servicing it
    const float rx = leftW + 12, rw = w - rx - 20;
    t.solidRectPx(rx - 4, top, rw + 8, h - top - 48, Panel);
    if (page.selectedModule >= 0)
    {
        const auto& m = page.modules[static_cast<std::size_t>(page.selectedModule)];
        const auto& f = page.facts;
        float y = top + 8;
        t.textDrawPx(m.label, rx + 6, y, px + 2, Title);
        y += static_cast<float>(px) + 8;
        t.textDrawPx(m.moduleId + (f.productId.empty() ? std::string() : "  ·  " + f.productId), rx + 6, y, small, Muted);
        y += static_cast<float>(small) + 8;
        const auto line = [&](const std::string& k, const std::string& v, glm::vec4 col = Text) {
            t.textDrawPx(k, rx + 6, y, small, Muted);
            t.textDrawPx(v, rx + rw * 0.42f, y, small, col);
            y += static_cast<float>(small) + 5;
        };
        line("состояние", stateRu(m.state), stateColour(m));
        line("целостность", fmt("%.1f%%", m.health01 * 100.0), stateColour(m));
        line("функц. повреждения", fmt("%.1f%%", m.damage01 * 100.0), m.damage01 > 0.3 ? Bad : m.damage01 > 0.01 ? Warn : Text);
        std::string causes;
        for (const auto& [name, v] : m.causes)
            causes += (causes.empty() ? "" : ", ") + name + fmt(" %.0f%%", v * 100.0);
        if (!causes.empty())
            line("причины", causes, Warn);
        line("износ", fmt("%.1f%%", m.wear01 * 100.0));
        line("масса", f.massTonnes > 0 ? fmt("%.2f т", f.massTonnes) : "—");
        if (f.powerMW > 0)
            line("мощность", fmt("%.1f МВт", f.powerMW));
        if (m.hasPlacement)
            line("положение", fmt("x %.1f", m.centerMeters.x) + fmt("  y %.1f", m.centerMeters.y) + fmt("  z %.1f м", m.centerMeters.z));
        line("ремонт", std::string(levelRu(f.repairLevel)) + (f.lineReplaceable ? ", сменный блок" : "") +
                           (f.replaceHours > 0 ? fmt(", замена %.0f ч", f.replaceHours) : ""));
        if (f.priceCr > 0)
            line("цена изделия", grouped(f.priceCr) + " Cr");
        y += 6;

        // actions
        const float bw = (rw - 24) / 3.0f, bh = static_cast<float>(px) + 14;
        const auto button = [&](float x, const char* label, ServicePanelActionType type, bool enabled, const std::string& arg = {}) {
            t.solidRectPx(x, y, bw - 6, bh, enabled ? glm::vec4(0.05f, 0.14f, 0.22f, 1.0f) : glm::vec4(0.03f, 0.05f, 0.07f, 1.0f));
            t.solidRectPx(x, y, bw - 6, 1, Line);
            t.textDrawPx(label, x + std::max(4.0f, (bw - 6 - t.measureTextPx(label, small)) * 0.5f), y + (bh - static_cast<float>(small)) * 0.5f, small,
                         enabled ? Text : Muted);
            if (enabled)
                m_hits.push_back({x, y, x + bw - 6, y + bh, 0, {type, arg}});
        };
        button(rx + 6, "Ремонтировать", ServicePanelActionType::RefitPreviewRepair, f.repairable);
        button(rx + 6 + bw, "Заменить", ServicePanelActionType::RefitPreviewReplace, true);
        button(rx + 6 + 2 * bw, "Применить (DEV)", ServicePanelActionType::RefitApply,
               page.previewForSelection && page.preview.ok && page.preview.applicable);
        y += bh + 10;

        // the preview: what changes in the whole ship
        if (page.previewForSelection)
        {
            const auto& p = page.preview;
            const bool repair = p.operation.kind == game::ship::refit::RefitOperationKind::Repair;
            t.textDrawPx(std::string(repair ? "Ремонт" : "Замена") + (p.operation.productId.empty() ? "" : " на " + p.operation.productId) +
                             ": что изменится в корабле",
                         rx + 6, y, px, Title);
            y += static_cast<float>(px) + 6;
            if (!p.ok)
            {
                t.textDrawPx(p.error, rx + 6, y, small, Bad);
                y += static_cast<float>(small) + 6;
            }
            else
            {
                for (const auto& dlt : p.deltas)
                {
                    if (y > h - 120)
                        break;
                    t.textDrawPx(dlt.labelRu, rx + 6, y, small, Muted);
                    std::string v;
                    if (dlt.flag)
                        v = std::string(dlt.before > 0.5 ? "есть" : "нет") + "  ->  " + (dlt.after > 0.5 ? "есть" : "нет");
                    else
                        v = fmt(std::abs(dlt.after) >= 100 ? "%.0f" : "%.2f", dlt.before) + "  ->  " +
                            fmt(std::abs(dlt.after) >= 100 ? "%.0f" : "%.2f", dlt.after) + " " + dlt.unit;
                    const bool better = dlt.key.find("damage") != std::string::npos || dlt.key.find("wear") != std::string::npos ||
                                                dlt.key.find("coolant") != std::string::npos || dlt.key == "emergency_cooling" ||
                                                dlt.key == "mass"
                                            ? dlt.after < dlt.before
                                            : dlt.after > dlt.before;
                    t.textDrawPx(v, rx + rw * 0.52f, y, small, better ? Good : Bad);
                    y += static_cast<float>(small) + 5;
                }
                if (p.deltas.empty())
                {
                    t.textDrawPx("для корабля ничего не меняется", rx + 6, y, small, Muted);
                    y += static_cast<float>(small) + 5;
                }
                y += 4;
                t.textDrawPx("стоимость " + std::string(levelRu(p.cost.repairLevel)) + fmt(": %.1f ч", p.cost.labourHours) + ", работа " +
                                 grouped(p.cost.labourCr) + " + материалы " + grouped(p.cost.materialCr) + " = " + grouped(p.cost.totalCr) + " Cr",
                             rx + 6, y, small, Title);
                y += static_cast<float>(small) + 5;
                t.textDrawPx(p.applyNote, rx + 6, y, small, Muted);
                y += static_cast<float>(small) + 8;
            }
        }
        else
        {
            t.textDrawPx("выберите «Ремонтировать» или «Заменить»: сервер прогонит корабль до и после", rx + 6, y, small, Muted);
            y += static_cast<float>(small) + 8;
        }
        if (page.applyResultFresh)
        {
            t.textDrawPx(page.applyResult.empty() ? "применено: ShipCore изменён" : "не применено: " + page.applyResult, rx + 6, y, small,
                         page.applyResult.empty() ? Good : Bad);
            y += static_cast<float>(small) + 8;
        }

        // compatible replacements
        if (!page.replacements.empty() && y < h - 90)
        {
            t.textDrawPx("совместимые изделия (предпросмотр замены)", rx + 6, y, small, Muted);
            y += static_cast<float>(small) + 5;
            for (const auto& r : page.replacements)
            {
                if (y > h - 70)
                    break;
                const std::string label = (r.sameProduct ? "новый того же типа: " : "") + r.name + "  ·  " + grouped(r.priceCr) + " Cr";
                t.textDrawPx(label, rx + 12, y, small, r.sameProduct ? Text : Line);
                m_hits.push_back({rx + 6, y - 2, rx + rw, y + static_cast<float>(small) + 3, 0,
                                  {ServicePanelActionType::RefitPreviewReplace, r.sameProduct ? std::string() : r.productId}});
                y += static_cast<float>(small) + 5;
            }
        }
    }
    t.endFrame();
    glEnable(GL_DEPTH_TEST);
}

// ============================================================ F8 TRADE
namespace
{
std::string priceText(double cr)
{
    // expensive goods per kg (trade_goods unit rule: >= 50 000 Cr/t)
    if (cr >= 50000.0)
        return grouped(cr / 1000.0) + " Cr/кг";
    return grouped(cr) + " Cr/т";
}

std::string ageText(double hours)
{
    if (hours < 1.0)
        return "только что";
    if (hours < 48.0)
        return fmt("%.0f ч назад", hours);
    return fmt("%.1f сут назад", hours / 24.0);
}

const char* channelRu(const std::string& c)
{
    if (c == "open") return "открытый";
    if (c == "licensed") return "по лицензии";
    if (c == "restricted") return "ограниченный";
    if (c == "black") return "чёрный";
    if (c == "closed") return "закрыт";
    return c.c_str();
}
} // namespace

void ServicePanelsRenderer::renderTrade(const Viewport& viewport, const game::presentation::TradePresentation& page)
{
    if (viewport.width <= 0 || viewport.height <= 0)
        return;
    m_hits.clear();
    m_zones.clear();
    glViewport(viewport.x, viewport.y, viewport.width, viewport.height);
    glScissor(viewport.x, viewport.y, viewport.width, viewport.height);
    glDisable(GL_DEPTH_TEST);
    auto& t = TextRenderer::instance();
    t.beginFrameForViewport(viewport.width, viewport.height);
    const float w = static_cast<float>(viewport.width), h = static_cast<float>(viewport.height);
    const int px = std::clamp(static_cast<int>(h * 0.018f), 12, 17);
    const int small = std::clamp(static_cast<int>(h * 0.015f), 11, 14);
    const int big = std::clamp(static_cast<int>(h * 0.034f), 20, 34);
    t.solidRectPx(0, 0, w, h, Bg);
    t.solidRectPx(0, 0, w, 2, Line);
    if (!page.valid)
    {
        t.textDrawPx("ТОРГОВЛЯ", 24, 18, big, Title);
        t.textDrawPx("нет данных экономики или хаба поблизости", 24, 18 + big + 10, px, Muted);
        t.endFrame();
        return;
    }
    t.textDrawPx("ТОРГОВЛЯ — " + page.hubName, 24, 18, big, Title);
    t.textDrawPx(std::string(page.atHub ? "F8  /  вы в хабе" : "F8  /  ближайший хаб") + "  ·  день " + std::to_string(page.day) +
                     "  ·  доска объявлений, рынок, хабы системы", 24, 18 + big + 6, small, Muted);
    if (!page.events.empty())
    {
        std::string ev = "События: ";
        for (std::size_t i = 0; i < page.events.size(); ++i)
            ev += (i ? ", " : "") + page.events[i].ru;
        t.textDrawPx(ev, 24, 18 + big + 8 + small + 4, small, Warn);
    }

    // ---- tabs
    const float tabY = 18.0f + big + 18.0f + 2.0f * small;
    const char* tabs[] = {"ДОСКА ЗАЯВОК", "РЫНОК", "ХАБЫ СИСТЕМЫ"};
    float tx = 24.0f;
    for (int i = 0; i < 3; ++i)
    {
        const float tw = t.measureTextPx(tabs[i], px) + 24.0f;
        t.solidRectPx(tx, tabY, tw, px * 1.8f, page.tab == i ? Selected : Panel);
        t.textDrawPx(tabs[i], tx + 12, tabY + px * 0.35f, px, page.tab == i ? Title : Text);
        m_hits.push_back({tx, tabY, tx + tw, tabY + px * 1.8f, 0, {ServicePanelActionType::TradeTab, std::to_string(i)}});
        tx += tw + 8.0f;
    }

    // ---- left list
    const float listX = 24.0f, listY = tabY + px * 2.4f, listW = w * 0.58f, listH = h - listY - 40.0f;
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({4, listX, listY, listX + listW, listY + listH});
    const float rowH = static_cast<float>(px) * 2.7f;
    const auto& goods = game::economy::EconomyModel::instance().goods();
    std::size_t rows = page.tab == 0 ? page.board.size() : page.tab == 1 ? page.market.size() : page.hubs.size();
    const float maxScroll = std::max(0.0f, rowH * static_cast<float>(rows) - listH);
    m_tradeListScroll = std::clamp(m_tradeListScroll, 0.0f, maxScroll);
    std::size_t visibleRow = 0;
    for (std::size_t i = 0; i < rows; ++i)
    {
        const float y = listY + rowH * static_cast<float>(visibleRow) - m_tradeListScroll;
        if (page.tab == 1 && !page.market[i].traded)
            continue;
        ++visibleRow;
        if (y < listY - 1.0f || y + rowH > listY + listH + 1.0f)
            continue;
        if (page.tab == 0)
        {
            const auto& r = page.board[i];
            const bool sel = r.entry.destinationHub == page.selectedHub;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            std::string what = r.entry.kindRu;
            if (r.entry.kind == "cargo")
                what += ": " + r.goodRu + ", " + std::to_string(r.entry.containersC20) + " x C20";
            else if (r.entry.people > 0)
                what += ": " + std::to_string(r.entry.people) + " чел.";
            t.textDrawPx(what, listX + 10, y + 4, px, sel ? Title : Text);
            const std::string dest = "-> " + r.destinationName + (r.destinationSystem.empty() ? "" : "  (" + r.destinationSystem + ")") +
                                     "  ·  срок " + std::to_string(r.daysLeft) + " сут  ·  канал " + channelRu(r.entry.channel);
            t.textDrawPx(dest, listX + 10, y + 6 + static_cast<float>(px), small, Muted);
            const std::string reward = grouped(r.entry.rewardCr) + " Cr";
            t.textDrawPx(reward, listX + listW - t.measureTextPx(reward, px) - 10, y + 4, px, Good);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::TradeSelectHub, r.entry.destinationHub}});
        }
        else if (page.tab == 1)
        {
            const auto& q = page.market[i];
            const auto& g = goods[static_cast<std::size_t>(q.good)];
            t.textDrawPx(g.ru, listX + 10, y + 4, px, Text);
            t.textDrawPx(g.groupRu + "  ·  запас " + grouped(q.stockT) + " т", listX + 10, y + 6 + static_cast<float>(px), small, Muted);
            const std::string prices = "купить " + priceText(q.buyCr) + "   продать " + priceText(q.sellCr);
            t.textDrawPx(prices, listX + listW - t.measureTextPx(prices, small) - 10, y + 4, small, Text);
            const std::string drift = fmt("%+.0f %%", q.drift * 100.0);
            t.textDrawPx(drift, listX + listW - t.measureTextPx(drift, small) - 10, y + 6 + static_cast<float>(px), small,
                         q.drift > 0.05 ? Warn : q.drift < -0.05 ? Good : Muted);
        }
        else
        {
            const auto* hub = page.hubs[i];
            const bool sel = hub->id == page.selectedHub;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            t.textDrawPx(hub->name + (hub->id == page.hubId ? "  (здесь)" : ""), listX + 10, y + 4, px, sel ? Title : Text);
            t.textDrawPx(hub->typeRu + "  ·  " + hub->ownerName, listX + 10, y + 6 + static_cast<float>(px), small, Muted);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::TradeSelectHub, hub->id}});
        }
        t.solidRectPx(listX, y + rowH - 1, listW, 1, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));
    }

    // ---- right card: the selected hub
    const float cx = listX + listW + 24.0f, cw = w - cx - 24.0f;
    float y = listY;
    const auto& c = page.card;
    if (c.valid)
    {
        t.textDrawPx(c.info.name, cx, y, big, Text);
        y += static_cast<float>(big) + 6;
        t.textDrawPx(c.info.typeRu + "  ·  " + c.info.systemName, cx, y, small, Muted);
        y += static_cast<float>(small) + 8;
        const auto line = [&](const std::string& label, const std::string& value, const glm::vec4& col) {
            t.textDrawPx(label, cx, y, small, Muted);
            t.textDrawPx(value, cx + cw * 0.36f, y, small, col);
            y += static_cast<float>(small) + 6;
        };
        line("Владелец", c.info.ownerName, Text);
        if (!c.info.operatorName.empty())
            line("Оператор", c.info.operatorName, Text);
        line("Расстояние", c.distanceText.empty() ? "нет данных" : c.distanceText, Text);
        line("Канал блоков", channelRu(c.channel), c.channel == "open" ? Good : c.channel == "licensed" ? Text : Warn);
        line("Цены", c.connected ? ageText(c.infoAgeHours) : "нет связи", c.connected ? (c.infoAgeHours > 72.0 ? Warn : Text) : Bad);
        if (!c.knownEvents.empty())
        {
            std::string ev;
            for (std::size_t i = 0; i < c.knownEvents.size(); ++i)
                ev += (i ? ", " : "") + c.knownEvents[i].ru;
            line("События", ev, Warn);
        }

        // show on the map
        const std::string mapLabel = "ПОКАЗАТЬ НА КАРТЕ";
        const float bw = t.measureTextPx(mapLabel, px) + 28.0f, bh = px * 1.9f;
        t.solidRectPx(cx, y + 4, bw, bh, Selected);
        t.textDrawPx(mapLabel, cx + 14, y + 4 + px * 0.4f, px, Title);
        m_hits.push_back({cx, y + 4, cx + bw, y + 4 + bh, 0, {ServicePanelActionType::TradeShowOnMap, c.info.id}});
        y += bh + 16;

        if (!c.knownPrices.empty())
        {
            t.textDrawPx("Последние известные цены", cx, y, px, Title);
            y += static_cast<float>(px) + 6;
            for (const auto& q : c.knownPrices)
            {
                if (y > h - 40.0f - static_cast<float>(small))
                    break;
                t.textDrawPx(goods[static_cast<std::size_t>(q.good)].ru, cx, y, small, Text);
                const std::string p = priceText(q.sellCr);
                t.textDrawPx(p, cx + cw - t.measureTextPx(p, small), y, small, Muted);
                y += static_cast<float>(small) + 5;
            }
        }
        if (!c.info.description.empty() && y < h - 80.0f)
        {
            y += 6;
            t.textDrawPx(c.info.purpose, cx, y, small, Muted);
        }
    }
    t.textDrawPx("Клик по заявке или хабу — карточка; колесо — прокрутка списка", 24, h - 26, small, Muted);
    t.endFrame();
}

std::optional<ServicePanelAction> ServicePanelsRenderer::handleInput(const Viewport& viewport, double mouseX, double mouseY, bool leftDown,
                                                                      double scrollY)
{
    const float x = static_cast<float>(mouseX - viewport.x), y = static_cast<float>(mouseY - viewport.y);
    if (scrollY != 0.0)
        for (const auto& z : m_zones)
            if (x >= z.x0 && x <= z.x1 && y >= z.y0 && y <= z.y1)
            {
                float& s = z.id == 1 ? m_shipyardListScroll : z.id == 2 ? m_refitListScroll
                         : z.id == 4 ? m_tradeListScroll : m_cardScroll;
                s -= static_cast<float>(scrollY) * 40.0f;
                s = std::max(0.0f, s);
            }
    const bool pressed = leftDown && !m_leftWasDown;
    m_leftWasDown = leftDown;
    if (!pressed)
        return std::nullopt;
    for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it)
        if (x >= it->x0 && x <= it->x1 && y >= it->y0 && y <= it->y1)
            return it->action;
    return std::nullopt;
}
} // namespace ui::presentation
