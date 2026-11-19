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

std::string money2(double v)
{
    char b[48];
    std::snprintf(b, sizeof b, "%.2f", v);
    std::string s = b;
    const auto dot = s.find('.');
    const bool neg = !s.empty() && s[0] == '-';
    std::string head = s.substr(neg ? 1 : 0, dot - (neg ? 1 : 0)), out;
    for (std::size_t i = 0; i < head.size(); ++i)
    {
        if (i > 0 && (head.size() - i) % 3 == 0)
            out += ' ';
        out += head[i];
    }
    return (neg ? "-" : "") + out + s.substr(dot);
}

// F8 -> ФИНАНСЫ: the server's ledger as replicated to this session (docs/ECONOMY_BANKING.md)
void renderFinance(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, float listX, float listY, float listW, float listH,
                   float w, float h, int px, int small, int big)
{
    t.solidRectPx(listX, listY, listW, listH, Panel);
    if (!f.valid)
    {
        t.textDrawPx("нет данных банка (сервер без экономики)", listX + 12, listY + 12, px, Muted);
        return;
    }
    float y = listY + 10.0f;
    const float x = listX + 12.0f, right = listX + listW - 12.0f;
    const auto row = [&](const std::string& label, const std::string& value, const glm::vec4& col, int size) {
        if (y > listY + listH - static_cast<float>(size))
            return;
        t.textDrawPx(label, x, y, size, size == px ? Text : Muted);
        t.textDrawPx(value, right - t.measureTextPx(value, size), y, size, col);
        y += static_cast<float>(size) + 6.0f;
    };
    const auto header = [&](const std::string& s) {
        y += 4.0f;
        t.textDrawPx(s, x, y, px, Title);
        y += static_cast<float>(px) + 6.0f;
    };
    const bool good = f.standing == "good";
    row("Статус клиента", f.standingRu, good ? Good : f.standing == "grace" ? Warn : Bad, px);
    row("Чистые активы", money2(f.netWorthCr) + " Cr", f.netWorthCr >= 0.0 ? Good : Bad, px);
    header("Наличные");
    for (const auto& c : f.cash)
        row(c.label, money2(c.amount) + " " + c.currency + (c.currency == "CR" ? "" : "  (" + grouped(c.amountCr) + " Cr)"), Text, small);
    header("Счета");
    for (const auto& a : f.accounts)
    {
        row(a.label, money2(a.amount) + " " + a.currency + (a.currency == "CR" ? "" : "  (" + grouped(a.amountCr) + " Cr)"),
            a.amount >= 0.0 ? Text : Warn, px);
        row("  овердрафт " + grouped(a.limit) + " " + a.currency + ", ставка " + fmt("%.2f %%", a.annualRate * 100.0) +
                (a.state.empty() ? "" : "  ·  " + a.state),
            "", Muted, small);
    }
    if (!f.loans.empty())
    {
        header("Долги");
        for (const auto& l : f.loans)
        {
            row(l.label, money2(l.amount) + " " + l.currency, Warn, px);
            row("  ставка " + fmt("%.2f %%", l.annualRate * 100.0) + ", платёж " + grouped(l.limit) + " / 30 сут" +
                    (l.state.empty() ? "" : "  ·  " + l.state),
                "", Muted, small);
        }
    }
    header("Спасательный кредит");
    row("Остаток лимита (топливо, ремонт до лётного состояния; буксир — всегда)", grouped(f.lifelineLeftCr) + " Cr / 90 сут", Good, small);
    if (!f.notices.empty())
    {
        header("Последние операции");
        for (const auto& n : f.notices)
            row(n, "", Muted, small);
    }

    // right column: where the ship is, the fuel price here
    const float cx = listX + listW + 24.0f, cw = w - cx - 24.0f;
    float ry = listY;
    const auto line = [&](const std::string& label, const std::string& value, const glm::vec4& col) {
        if (ry > h - 40.0f - static_cast<float>(small))
            return;
        t.textDrawPx(label, cx, ry, small, Muted);
        t.textDrawPx(value, cx + cw - t.measureTextPx(value, small), ry, small, col);
        ry += static_cast<float>(small) + 6.0f;
    };
    t.textDrawPx("Здесь платят", cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line("Полития", f.localPolity.empty() ? "нет государства" : f.localPolity, Text);
    line("Банк", f.localBank, Text);
    line("Курс", "1 Cr = " + fmt("%.4f", f.localFxPerCr) + " " + f.localCurrency, Text);
    ry += 10.0f;
    t.textDrawPx("Топливо", cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    if (!f.fuelQuoteValid)
        line("Заряд прыжка", "здесь не продаётся", Bad);
    else
    {
        line("Где", f.fuelHubName, Text);
        line("Заряд прыжка, прейскурант", grouped(f.jumpChargeListPerT) + " " + f.localCurrency + "/т  (" + grouped(f.jumpChargeListCrPerT) +
                                              " Cr/т)", Text);
        line("К оплате", f.fuelFree ? "0 — топливо пока бесплатно" : grouped(f.jumpChargeChargedPerT) + " " + f.localCurrency + "/т",
             f.fuelFree ? Good : Text);
        line("Запас станции", fmt("%.1f т", f.jumpChargeStockT), Text);
        if (f.reactorFuelListPerKg > 0.0)
            line("Реакторное топливо (D/He-3)", grouped(f.reactorFuelListPerKg) + " " + f.localCurrency + "/кг", Text);
        ry += 6.0f;
        t.textDrawPx("Из чего цена заряда", cx, ry, px, Title);
        ry += static_cast<float>(px) + 6.0f;
        for (const auto& b : f.fuelBreakdown)
            line(b, "", Muted);
    }
    if (f.freeFuelListCr > 0.0)
        line("Получено бесплатно по прейскуранту", grouped(f.freeFuelListCr) + " Cr", Muted);
}

// F8 -> ГРУЗ: the server's manifest of the ship (docs/TRADE_DESIGN.md §8.3, read-only in step 1), the hull figures and
// the latest trade results
void renderCargo(TextRenderer& t, const game::simulation::ClientTradeSnapshot& s, const game::simulation::ClientWalletSnapshot& f,
                 float listX, float listY, float listW, float listH, float w, float h, int px, int small, int big)
{
    t.solidRectPx(listX, listY, listW, listH, Panel);
    if (!s.valid)
    {
        t.textDrawPx("нет данных груза (сервер без экономики)", listX + 12, listY + 12, px, Muted);
        return;
    }
    const auto& goods = game::economy::EconomyModel::instance().goods();
    const auto goodRu = [&](int g) {
        return g >= 0 && g < static_cast<int>(goods.size()) ? goods[static_cast<std::size_t>(g)].ru : std::string("?");
    };
    float y = listY + 10.0f;
    const float x = listX + 12.0f, right = listX + listW - 12.0f;
    t.textDrawPx("Манифест", x, y, px, Title);
    y += static_cast<float>(px) + 8.0f;
    if (s.cargo.empty())
    {
        t.textDrawPx("трюм пуст", x, y, px, Muted);
        y += static_cast<float>(px) + 6.0f;
    }
    for (const auto& c : s.cargo)
    {
        if (y > listY + listH - 2.0f * static_cast<float>(px))
            break;
        t.textDrawPx(goodRu(c.good), x, y, px, Text);
        const std::string mass = fmt("%.2f т", c.tonnes) + "  ·  C20 x " + std::to_string(c.c20);
        t.textDrawPx(mass, right - t.measureTextPx(mass, px), y, px, Text);
        y += static_cast<float>(px) + 4.0f;
        const std::string detail = "партия " + c.lotId + "  ·  " + (c.contractId.empty() ? std::string("свой товар") : "контракт " + c.contractId) +
                                   "  ·  погружено день " + std::to_string(c.loadedDay) + " (" + c.originHub + ")  ·  себест. " +
                                   grouped(c.costBasisCrPerT) + " Cr/т";
        t.textDrawPx(detail, x + 10, y, small, Muted);
        y += static_cast<float>(small) + 8.0f;
    }
    if (!s.results.empty())
    {
        y += 6.0f;
        t.textDrawPx("Последние сделки", x, y, px, Title);
        y += static_cast<float>(px) + 6.0f;
        for (auto it = s.results.rbegin(); it != s.results.rend(); ++it)
        {
            if (y > listY + listH - static_cast<float>(small))
                break;
            const std::string line = "#" + std::to_string(it->requestId) + "  " + (it->ok ? it->message : "ОТКАЗ: " + it->message);
            t.textDrawPx(line, x, y, small, it->ok ? Good : Bad);
            if (it->ok)
            {
                const std::string total = (it->totalCr >= 0.0 ? "+" : "") + grouped(it->totalCr) + " Cr";
                t.textDrawPx(total, right - t.measureTextPx(total, small), y, small, it->totalCr >= 0.0 ? Good : Warn);
            }
            y += static_cast<float>(small) + 6.0f;
        }
    }

    // right column: the hold and what the cargo does to the ship
    const float cx = listX + listW + 24.0f, cw = w - cx - 24.0f;
    float ry = listY;
    const auto line = [&](const std::string& label, const std::string& value, const glm::vec4& col) {
        if (ry > h - 40.0f - static_cast<float>(small))
            return;
        t.textDrawPx(label, cx, ry, small, Muted);
        t.textDrawPx(value, cx + cw - t.measureTextPx(value, small), ry, small, col);
        ry += static_cast<float>(small) + 6.0f;
    };
    t.textDrawPx("Трюм", cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line("Места C20", std::to_string(s.c20Used) + " занято / " + std::to_string(s.c20Slots), s.c20Used >= s.c20Slots ? Warn : Text);
    line("Груз нетто", fmt("%.2f", s.cargoNetT) + " / " + fmt("%.0f т", s.netCapacityT), Text);
    line("Тара C20 (1.7 т на контейнер)", fmt("%.1f т", s.cargoGrossT - s.cargoNetT), Text);
    line("Груз брутто", fmt("%.2f т", s.cargoGrossT), Text);
    ry += 10.0f;
    t.textDrawPx("Корабль", cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line("Масса сейчас", fmt("%.1f т", s.shipMassT), Text);
    line("Дальность прыжка на полном заряде", fmt("%.1f св. лет", s.jumpRangeLy), s.jumpRangeLy < s.jumpRangeEmptyLy - 0.05 ? Warn : Text);
    line("  с пустым трюмом", fmt("%.1f св. лет", s.jumpRangeEmptyLy), Muted);
    if (s.jumpRangeEmptyLy > 0.0 && s.jumpRangeLy < s.jumpRangeEmptyLy - 0.05)
        line("  груз сокращает прыжок на", fmt("%.1f %%", 100.0 * (1.0 - s.jumpRangeLy / s.jumpRangeEmptyLy)), Warn);
    ry += 10.0f;
    t.textDrawPx("Деньги", cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    if (f.valid)
        for (const auto& a : f.accounts)
            line(a.label, money2(a.amount) + " " + a.currency, a.amount >= 0.0 ? Text : Warn);
    if (s.vatReclaimCr > 0.0)
        line("Входной НДС к возврату", grouped(s.vatReclaimCr) + " Cr", Muted);
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
    const char* tabs[] = {"ДОСКА ЗАЯВОК", "РЫНОК", "ХАБЫ СИСТЕМЫ", "НАВИГ. ДАННЫЕ", "ФИНАНСЫ", "ГРУЗ"};
    float tx = 24.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float tw = t.measureTextPx(tabs[i], px) + 24.0f;
        t.solidRectPx(tx, tabY, tw, px * 1.8f, page.tab == i ? Selected : Panel);
        t.textDrawPx(tabs[i], tx + 12, tabY + px * 0.35f, px, page.tab == i ? Title : Text);
        m_hits.push_back({tx, tabY, tx + tw, tabY + px * 1.8f, 0, {ServicePanelActionType::TradeTab, std::to_string(i)}});
        tx += tw + 8.0f;
    }

    // ---- left list
    const float listX = 24.0f, listY = tabY + px * 2.4f, listW = w * 0.58f, listH = h - listY - 40.0f;
    if (page.tab == 3)
    {
        renderNavData(page.nav, listX, listY, listW, listH, w, h, px, small, big);
        t.endFrame();
        return;
    }
    if (page.tab == 4)
    {
        renderFinance(t, page.finance, listX, listY, listW, listH, w, h, px, small, big);
        t.endFrame();
        return;
    }
    if (page.tab == 5)
    {
        renderCargo(t, page.trade, page.finance, listX, listY, listW, listH, w, h, px, small, big);
        t.endFrame();
        return;
    }
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
            const bool sel = page.deal.valid && page.deal.good == q.good;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            t.textDrawPx(g.ru, listX + 10, y + 4, px, sel ? Title : Text);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::TradeSelectGood, std::to_string(q.good)}});
            std::string sub = g.groupRu + "  ·  запас " + grouped(q.stockT) + " т";
            if (i < page.marketLimits.size())
                sub += "  ·  хаб берёт ≤ " + grouped(page.marketLimits[i].hubTakesUpToT) + " т, отпускает ≤ " +
                       grouped(page.marketLimits[i].hubGivesUpToT) + " т";
            t.textDrawPx(sub, listX + 10, y + 6 + static_cast<float>(px), small, Muted);
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
    if (page.tab == 1)
        renderTradeDeal(page, cx, y, cw, h, px, small, big);
    else if (c.valid)
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
    if (page.tab == 1)
    {
        // the server's market: an empty list says why (not at a hub with a market); the newest trade result
        if (page.market.empty())
            t.textDrawPx(page.trade.marketNote.empty() ? std::string("рынок виден у хаба, где стоит корабль") : page.trade.marketNote,
                         listX + 12, listY + 12, px, Muted);
        std::string status = page.trade.valid ? "цены сервера, день " + std::to_string(page.trade.day) + "  ·  стиль " +
                                                    page.trade.fiscalStyleRu + ", НДС " + fmt("%.0f %%", page.trade.vatShare * 100.0) +
                                                    ", погрузка " + grouped(page.trade.handlingCrPerT) + " Cr/т" +
                                                    (page.trade.docked ? "  ·  у причала: сделки разрешены" : "  ·  сделки только у причала")
                                              : std::string();
        if (!page.trade.results.empty())
        {
            const auto& r = page.trade.results.back();
            status += "   |   #" + std::to_string(r.requestId) + " " + (r.ok ? r.message : "ОТКАЗ: " + r.message);
        }
        t.textDrawPx(status, 24, h - 26, small, Text);
    }
    if (page.tab != 1)
        t.textDrawPx("Клик по заявке или хабу — карточка; колесо — прокрутка списка", 24, h - 26, small, Muted);
    t.endFrame();
}

// F8 MARKET: the deal window of the selected row (TRADE_DESIGN §8.1, trade step 3). Every number comes from
// TradeDealView - the server's TradeQuoteModel on the replicated inputs; the panel only lays it out.
namespace
{
std::string signedCr(long long v)
{
    return (v > 0 ? "+" : v < 0 ? "-" : "") + grouped(static_cast<double>(v < 0 ? -v : v));
}

std::string tonnesText(double t)
{
    return fmt(t >= 100.0 ? "%.1f" : "%.2f", t);
}

std::string fill(std::string s, const std::vector<std::pair<std::string, std::string>>& args)
{
    for (const auto& [k, v] : args)
        for (std::size_t p = s.find(k); p != std::string::npos; p = s.find(k, p + v.size()))
            s.replace(p, k.size(), v);
    return s;
}
} // namespace

void ServicePanelsRenderer::renderTradeDeal(const game::presentation::TradePresentation& page, float cx, float y0, float cw, float h,
                                            int px, int small, int big)
{
    auto& t = TextRenderer::instance();
    const auto tx = [&](const std::string& key) -> std::string {
        const auto it = page.dealText.find(key);
        return it != page.dealText.end() ? it->second : key;
    };
    const auto& d = page.deal;
    const auto& s = page.trade;
    // textDrawPx takes the BASELINE: this panel lays out by the text's top (caps ~0.8 of the size above the baseline)
    const auto text = [&](const std::string& str, float x, float top, int size, const glm::vec4& col) {
        t.textDrawPx(str, x, top + static_cast<float>(size) * 0.8f, size, col);
    };
    // the key hint, wrapped at " · " to the panel width, bottom-aligned
    const auto keysHint = [&](float bottomY) {
        std::vector<std::string> lines(1);
        const std::string all = tx("keys"), sep = " · ";
        for (std::size_t p = 0; p <= all.size();)
        {
            const std::size_t e = std::min(all.find(sep, p), all.size());
            const std::string part = all.substr(p, e - p);
            if (!lines.back().empty() && t.measureTextPx(lines.back() + sep + part, small) > cw)
                lines.emplace_back();
            lines.back() += (lines.back().empty() ? "" : sep) + part;
            p = e + sep.size();
        }
        float ly = bottomY - static_cast<float>(lines.size()) * (static_cast<float>(small) + 6.0f);
        for (const auto& l : lines)
        {
            text(l, cx, ly, small, Muted);
            ly += static_cast<float>(small) + 6.0f;
        }
    };
    float y = y0;
    const float bottom = h - 40.0f;
    t.solidRectPx(cx - 10, y - 6, cw + 20, bottom - y + 6, Panel);
    m_zones.push_back({5, cx - 10, y - 6, cx + cw + 10, bottom});
    const std::string T = tx("units_t");
    if (!d.valid)
    {
        text(tx("hint"), cx, y + 6, px, Muted);
        keysHint(bottom - 8.0f);
        return;
    }
    const auto& goods = game::economy::EconomyModel::instance().goods();
    const auto& g = goods[static_cast<std::size_t>(d.good)];

    // ---- the good, the side, the units
    text(g.ru, cx, y, big, Title);
    y += static_cast<float>(big) + 12;
    const auto toggle = [&](const std::string& label, bool on, ServicePanelActionType type, const std::string& arg, float x) {
        const float bw = t.measureTextPx(label, px) + 24.0f, bh = px * 1.8f;
        t.solidRectPx(x, y, bw, bh, on ? Selected : Bg);
        t.solidRectPx(x, y + bh - 2, bw, 2, on ? Title : Line);
        text(label, x + 12, y + (bh - px) * 0.5f, px, on ? Title : Muted);
        m_hits.push_back({x, y, x + bw, y + bh, 0, {type, arg}});
        return x + bw + 6.0f;
    };
    float x = toggle(tx("buy"), d.buy, ServicePanelActionType::TradeDealSide, "buy", cx);
    (void)toggle(tx("sell"), !d.buy, ServicePanelActionType::TradeDealSide, "sell", x);
    {
        const std::string ut = T, uc = tx("units_c20");
        const float wc = t.measureTextPx(uc, px) + 24.0f, wt = t.measureTextPx(ut, px) + 24.0f;
        float ux = cx + cw - wc - 6.0f - wt;
        ux = toggle(ut, !d.unitsC20, ServicePanelActionType::TradeDealUnits, "t", ux);
        (void)toggle(uc, d.unitsC20, ServicePanelActionType::TradeDealUnits, "c20", ux);
    }
    y += px * 1.8f + 10;
    text(fill(tx("aboard"), {{"{t}", tonnesText(d.aboardT)}}) + "   ·   " +
                     fill(tx("account"), {{"{cr}", grouped(s.spendableCr)}, {"{used}", std::to_string(s.c20Used)},
                                          {"{slots}", std::to_string(s.c20Slots)}, {"{net}", tonnesText(s.cargoNetT)},
                                          {"{cap}", grouped(s.netCapacityT)}}),
                 cx, y, small, Muted);
    y += static_cast<float>(small) + 12;

    // ---- the quantity: [-] slider [+] [MAX]  value
    text(tx("quantity"), cx, y + 2, small, Muted);
    const std::string value = tonnesText(d.tonnes) + " " + T + "  (" + fill(tx("boxes"), {{"{n}", std::to_string(d.c20)}}) + ")";
    text(value, cx + cw - t.measureTextPx(value, px), y, px, Text);
    y += static_cast<float>(px) + 8;
    {
        const float bh = px * 1.8f, bw = bh * 1.2f;
        const auto button = [&](const std::string& label, ServicePanelActionType type, const std::string& arg, float bx, float w) {
            t.solidRectPx(bx, y, w, bh, Selected);
            const int size = label.size() == 1 ? big : px;   // "-" / "+" large enough to read
            text(label, bx + (w - t.measureTextPx(label, size)) * 0.5f, y + (bh - static_cast<float>(size)) * 0.5f, size, Title);
            m_hits.push_back({bx, y, bx + w, y + bh, 0, {type, arg}});
        };
        const std::string maxLabel = tx("max");
        const float mw = t.measureTextPx(maxLabel, px) + 20.0f;
        button("-", ServicePanelActionType::TradeDealStep, "-1", cx, bw);
        const float trackX = cx + bw + 10.0f, trackW = cw - 2.0f * bw - mw - 30.0f;
        button("+", ServicePanelActionType::TradeDealStep, "+1", trackX + trackW + 10.0f, bw);
        button(maxLabel, ServicePanelActionType::TradeDealMax, "", cx + cw - mw, mw);
        // the track: filled to tonnes / max; box ticks in C20 units
        const float ty = y + bh * 0.5f;
        t.solidRectPx(trackX, ty - 3, trackW, 6, Bg);
        const float f = d.maxT > 0.0 ? static_cast<float>(d.tonnes / d.maxT) : 0.0f;
        t.solidRectPx(trackX, ty - 3, trackW * f, 6, d.buy ? Warn : Good);
        if (d.unitsC20 && d.maxT > 0.0 && d.maxT / d.boxT <= 40.0)
            for (double b = d.boxT; b < d.maxT; b += d.boxT)
                t.solidRectPx(trackX + trackW * static_cast<float>(b / d.maxT), ty - 6, 1, 12, Line);
        t.solidRectPx(trackX + trackW * f - 4, y + 2, 8, bh - 4, Title);
        m_hits.push_back({trackX, y, trackX + trackW, y + bh, 0, {ServicePanelActionType::TradeDealSlider, {}}});
        y += bh + 8;
    }
    // the limit that decides the maximum
    {
        std::string why;
        if (d.limitKind == "hold_c20")
            why = fill(tx("why_hold_c20"), {{"{n}", std::to_string(d.c20Free)}});
        else if (d.limitKind == "payload")
            why = tx("why_payload");
        else if (d.limitKind == "hub")
            why = tx(d.buy ? "why_hub_buy" : "why_hub_sell");
        else if (d.limitKind == "money")
            why = fill(tx("why_money"), {{"{cr}", grouped(d.spendableCr)}});
        else
            why = tx("why_aboard");
        const bool atLimit = d.maxT <= 1.0e-6 || d.tonnes >= d.maxT - 1.0e-6;
        text(fill(tx("limit"), {{"{t}", tonnesText(d.maxT)}, {"{why}", why}}), cx, y, small, atLimit ? Warn : Muted);
        y += static_cast<float>(small) + 14;
    }

    // ---- the price and the breakdown (sums exactly to the total)
    const auto& q = d.quote;
    const auto row = [&](const std::string& label, const std::string& v, const glm::vec4& col, int size) {
        text(label, cx, y, size, size == px ? Text : Muted);
        text(v, cx + cw - t.measureTextPx(v, size), y, size, col);
        y += static_cast<float>(size) + 8;
    };
    const std::string perT = " Cr/" + T;
    row(tx(d.buy ? "list_ask" : "list_bid"), money2(q.listUnitCr) + perT, Text, small);
    row(tx("impact"), fmt("%+.2f %%", q.impactShare * 100.0) + "  ->  " + money2(q.unitCr) + perT, q.impactShare != 0.0 ? Warn : Text, small);
    t.solidRectPx(cx, y + 1, cw, 1, Line);
    y += 8;
    const std::string tt = tonnesText(d.tonnes);
    for (const auto& l : d.lines)
    {
        std::string label;
        if (l.kind == "goods")
            label = fill(tx(d.buy ? "goods_buy" : "goods_sell"), {{"{t}", tt}, {"{unit}", money2(q.unitCr)}});
        else if (l.kind == "handling")
            label = fill(tx(d.buy ? "handling_buy" : "handling_sell"), {{"{t}", tt}, {"{rate}", money2(s.handlingCrPerT)}});
        else if (l.kind == "vat")
            label = fill(tx(d.buy ? "vat_buy" : "vat_sell"), {{"{pct}", fmt("%.0f %%", s.vatShare * 100.0)}, {"{days}", std::to_string(s.vatRefundDays)}});
        else
            label = fill(tx("turnover"), {{"{pct}", fmt("%.0f %%", s.turnoverShare * 100.0)}});
        row(label, signedCr(l.shownCr), l.shownCr >= 0 ? Good : Text, small);
    }
    t.solidRectPx(cx, y + 1, cw, 1, Line);
    y += 8;
    row(tx(d.buy ? "total_buy" : "total_sell"), signedCr(d.shownTotalCr) + " Cr", d.shownTotalCr >= 0 ? Good : Warn, px);
    if (d.buy && d.vatRefundCr > 0.0)
        row(fill(tx("vat_refund"), {{"{cr}", grouped(d.vatRefundCr)}, {"{days}", std::to_string(d.vatRefundDays)}}), "", Muted, small);
    y += 6;

    // ---- Confirm / Cancel
    {
        const long long total = d.shownTotalCr < 0 ? -d.shownTotalCr : d.shownTotalCr;
        const std::string label = fill(tx(d.buy ? "confirm_buy" : "confirm_sell"), {{"{t}", tt}, {"{cr}", grouped(static_cast<double>(total))}});
        const float bh = px * 2.1f, bw = t.measureTextPx(label, px) + 32.0f;
        t.solidRectPx(cx, y, bw, bh, d.canConfirm ? Selected : Bg);
        t.solidRectPx(cx, y + bh - 2, bw, 2, d.canConfirm ? Title : Line);
        text(label, cx + 16, y + (bh - px) * 0.5f, px, d.canConfirm ? Title : Muted);
        m_hits.push_back({cx, y, cx + bw, y + bh, 0, {ServicePanelActionType::TradeDealConfirm, {}}});
        const std::string cancel = tx("cancel");
        const float cw2 = t.measureTextPx(cancel, px) + 28.0f;
        t.solidRectPx(cx + bw + 10, y, cw2, bh, Bg);
        text(cancel, cx + bw + 24, y + (bh - px) * 0.5f, px, Muted);
        m_hits.push_back({cx + bw + 10, y, cx + bw + 10 + cw2, y + bh, 0, {ServicePanelActionType::TradeDealCancel, {}}});
        y += bh + 10;
    }
    // why it cannot be confirmed / the server's answer
    if (!d.canConfirm && d.blockKind != "waiting")
    {
        const std::string why = d.blockKind == "quote" ? q.reason : tx(d.blockKind);
        text(why, cx, y, small, Bad);
        y += static_cast<float>(small) + 8;
    }
    using game::presentation::TradeDealAnswer;
    if (d.answer == TradeDealAnswer::Waiting)
        text(fill(tx("waiting"), {{"{id}", std::to_string(d.sentRequest)}}), cx, y, small, Muted);
    else if (d.answer == TradeDealAnswer::Ok)
    {
        const double v = d.result.totalCr < 0.0 ? -d.result.totalCr : d.result.totalCr;
        text(fill(tx(d.result.totalCr < 0.0 ? "ok_buy" : "ok_sell"), {{"{cr}", money2(v)}}) + "  ·  #" +
                         std::to_string(d.result.requestId),
                     cx, y, small, Good);
        y += static_cast<float>(small) + 6;
        text(d.result.message, cx, y, small, Muted);
    }
    else if (d.answer == TradeDealAnswer::Refused)
        text(fill(tx("refused"), {{"{reason}", d.result.message}}), cx, y, small, Bad);
    else if (d.answer == TradeDealAnswer::PriceMoved)
    {
        // a tiny move (a big stock) looks equal at 2 decimals: then show enough digits to see it
        const bool tiny = money2(q.unitCr) == money2(d.sentUnitCr);
        const std::string fresh = tiny ? fmt("%.6f", q.unitCr) : money2(q.unitCr), old = tiny ? fmt("%.6f", d.sentUnitCr) : money2(d.sentUnitCr);
        text(fill(tx("price_moved"), {{"{new}", fresh + perT}, {"{old}", old + perT}}), cx, y, px, Warn);
        y += static_cast<float>(px) + 8;
        const std::string retry = tx("retry");
        const float bh = px * 1.9f, bw = t.measureTextPx(retry, px) + 28.0f;
        t.solidRectPx(cx, y, bw, bh, Selected);
        text(retry, cx + 14, y + (bh - px) * 0.5f, px, Title);
        m_hits.push_back({cx, y, cx + bw, y + bh, 0, {ServicePanelActionType::TradeDealRetry, {}}});
    }
    keysHint(bottom - 8.0f);
}

// F8 tab 3: the colony's navigation data bank, the onboard computer's memory, the data carriers (JUMP_MECHANICS 10l)
void ServicePanelsRenderer::renderNavData(const game::presentation::NavDataPage& nav, float listX, float listY, float listW, float listH,
                                          float w, float h, int px, int small, int big)
{
    auto& t = TextRenderer::instance();
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({4, listX, listY, listX + listW, listY + listH});
    const float rowH = static_cast<float>(px) * 2.7f;
    if (nav.bankName.empty())
        t.textDrawPx("В этой системе нет банка навигационных данных (нет оператора маяков)", listX + 10, listY + 8, px, Muted);
    else
    {
        t.textDrawPx(nav.bankName, listX + 10, listY + 6, px, Title);
        const float maxScroll = std::max(0.0f, rowH * static_cast<float>(nav.bank.size()) - (listH - px * 2.0f));
        m_tradeListScroll = std::clamp(m_tradeListScroll, 0.0f, maxScroll);
        for (std::size_t i = 0; i < nav.bank.size(); ++i)
        {
            const auto& r = nav.bank[i];
            const float y = listY + px * 2.0f + rowH * static_cast<float>(i) - m_tradeListScroll;
            if (y < listY + px * 2.0f - 1.0f || y + rowH > listY + listH + 1.0f)
                continue;
            const bool sel = r.id == nav.selected;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            t.textDrawPx("трасса " + r.corridor + "  ·  " + r.place, listX + 10, y + 4, px, sel ? Title : r.open ? Text : Muted);
            char when[96];
            std::snprintf(when, sizeof when, "съёмка %.1f (%.1f г. назад)  ·  сейчас: ", r.surveyYear, r.ageYears);
            t.textDrawPx(std::string(when) + r.stateText + "  ·  " + r.tierText,
                         listX + 10, y + 6 + static_cast<float>(px), small, r.open ? Muted : Warn);
            const std::string price = grouped(r.priceCr) + " Cr";
            t.textDrawPx(price, listX + listW - t.measureTextPx(price, px) - 10, y + 4, px, r.open ? Good : Muted);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::NavDataSelect, r.id}});
            t.solidRectPx(listX, y + rowH - 1, listW, 1, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));
        }
    }

    // ---- right card: the selected record, the computer, the carriers
    const float cx = listX + listW + 24.0f, cw = w - cx - 24.0f;
    float y = listY;
    const auto button = [&](const std::string& label, bool enabled, ServicePanelActionType type, const std::string& text) {
        const float bw = t.measureTextPx(label, px) + 28.0f, bh = px * 1.9f;
        t.solidRectPx(cx, y, bw, bh, enabled ? Selected : Panel);
        t.textDrawPx(label, cx + 14, y + px * 0.4f, px, enabled ? Title : Muted);
        if (enabled)
            m_hits.push_back({cx, y, cx + bw, y + bh, 0, {type, text}});
        y += bh + 8.0f;
    };
    const game::presentation::NavDataRow* sel = nullptr;
    for (const auto& r : nav.bank)
        if (r.id == nav.selected)
            sel = &r;
    if (sel)
    {
        t.textDrawPx("Трасса " + sel->corridor, cx, y, big, Text);
        y += static_cast<float>(big) + 6;
        t.textDrawPx(sel->place + "  ·  " + sel->tierText, cx, y, small, sel->open ? Muted : Warn);
        y += static_cast<float>(small) + 6;
        t.textDrawPx("цена " + grouped(sel->priceCr) + " Cr - оплата появится вместе с рынком", cx, y, small, Muted);
        y += static_cast<float>(small) + 10;
        const bool can = sel->open && nav.atHub;
        if (!sel->open)
        {
            t.textDrawPx("Закрыто: " + sel->tierText, cx, y, px, Warn);
            y += static_cast<float>(px) + 10;
        }
        else if (!nav.atHub)
        {
            t.textDrawPx("Данные выдают только в хабе", cx, y, px, Warn);
            y += static_cast<float>(px) + 10;
        }
        button("В БОРТОВОЙ КОМПЬЮТЕР", can && nav.computerState == 1, ServicePanelActionType::NavDataLoad, sel->id);
        button("НА НОСИТЕЛЬ", can, ServicePanelActionType::NavDataCarrier, sel->id);
        y += 8;
    }
    t.textDrawPx("Бортовой компьютер", cx, y, px, Title);
    y += static_cast<float>(px) + 6;
    if (nav.computerState != 1)
    {
        t.textDrawPx("не работает - записи трасс недоступны", cx, y, small, Bad);
        y += static_cast<float>(small) + 6;
    }
    else if (nav.memory.empty())
    {
        t.textDrawPx("записей трасс нет", cx, y, small, Muted);
        y += static_cast<float>(small) + 6;
    }
    for (const auto& m : nav.memory)
    {
        if (y > h - 120.0f)
            break;
        t.textDrawPx(m.corridor + fmt("  ·  %.1f г.  ·  ", m.ageYears) + m.stateText + "  ·  " + m.tierText, cx, y, small, Text);
        y += static_cast<float>(small) + 5;
    }
    y += 8;
    t.textDrawPx("Носители данных на борту", cx, y, px, Title);
    y += static_cast<float>(px) + 6;
    if (nav.carriers.empty())
    {
        t.textDrawPx("нет", cx, y, small, Muted);
        y += static_cast<float>(small) + 6;
    }
    for (std::size_t i = 0; i < nav.carriers.size(); ++i)
    {
        if (y > h - 60.0f)
            break;
        t.textDrawPx(std::to_string(i + 1) + ". " + nav.carriers[i], cx, y, small, Text);
        y += static_cast<float>(small) + 4;
        button("ВСТАВИТЬ В КОМПЬЮТЕР", nav.computerState == 1, ServicePanelActionType::NavDataInsert, std::to_string(i + 1));
    }
    (void)cw;
    t.textDrawPx("Клик по записи - карточка; открытые данные выдаёт банк колонии, остальное - по допуску", 24, h - 26, small, Muted);
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
    // the wheel over the deal panel changes the quantity (TRADE_DESIGN §8: ←/→ or the wheel)
    if (scrollY != 0.0)
        for (const auto& z : m_zones)
            if (z.id == 5 && x >= z.x0 && x <= z.x1 && y >= z.y0 && y <= z.y1)
                return ServicePanelAction {ServicePanelActionType::TradeDealStep, scrollY > 0.0 ? "+1" : "-1"};
    const bool pressed = leftDown && !m_leftWasDown;
    m_leftWasDown = leftDown;
    if (!pressed)
        return std::nullopt;
    for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it)
        if (x >= it->x0 && x <= it->x1 && y >= it->y0 && y <= it->y1)
        {
            ServicePanelAction a = it->action;
            if (a.type == ServicePanelActionType::TradeDealSlider)
                a.text = std::to_string(std::clamp((x - it->x0) / std::max(1.0f, it->x1 - it->x0), 0.0f, 1.0f));
            return a;
        }
    return std::nullopt;
}

std::optional<ServicePanelAction> ServicePanelsRenderer::devClick(ServicePanelActionType type, const std::string& text, double fraction) const
{
    for (auto it = m_hits.rbegin(); it != m_hits.rend(); ++it)
        if (it->action.type == type && (text.empty() || it->action.text == text || type == ServicePanelActionType::TradeDealSlider))
        {
            ServicePanelAction a = it->action;
            if (type == ServicePanelActionType::TradeDealSlider)
                a.text = std::to_string(std::clamp(fraction, 0.0, 1.0));
            return a;
        }
    return std::nullopt;
}
} // namespace ui::presentation
