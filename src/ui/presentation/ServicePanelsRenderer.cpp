#include "src/core/BuildFlavor.h"
#include "src/ui/presentation/ServicePanelsRenderer.h"

#include <algorithm>
#include <functional>
#include <cmath>
#include <cstdio>

#include <glad/gl.h>

#include "render/HUD/TextRenderer.h"
#include "src/game/localization/LocalizationService.h"

namespace ui::presentation
{
namespace
{
using game::presentation::RefitModuleView;
using game::localization::tr;

// a producer's string (a Message from the server or a presentation model, or a plain name) in the UI language
std::string rd(const std::string& s)
{
    return game::localization::renderMessage(s);
}

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
    if (c == "life_support" || c == "escape_capsule" || c == "cockpit_capsule" || c == "lifeboat" || c == "medbay") return {0.40f, 0.70f, 0.30f, 0.95f};
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

std::string stateText(const std::string& s)
{
    if (s == "attached") return tr("service.panel.refit.state.attached", "in service");
    if (s == "disabled") return tr("service.panel.refit.state.disabled", "disabled (out of action)");
    if (s == "destroyed") return tr("service.panel.refit.state.destroyed", "destroyed");
    if (s == "detached") return tr("service.panel.refit.state.detached", "torn off / undocked");
    if (s == "hanging") return tr("service.panel.refit.state.hanging", "hanging on its mounts");
    return tr("service.panel.refit.state.unknown", "no data");
}

std::string levelText(const std::string& s)
{
    if (s == "crew") return tr("service.panel.refit.level.crew", "by the crew");
    if (s == "station") return tr("service.panel.refit.level.station", "at a station");
    if (s == "yard") return tr("service.panel.refit.level.yard", "at a shipyard");
    if (s == "factory") return tr("service.panel.refit.level.factory", "factory only");
    return s;
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
    t.textDrawPx(tr("service.panel.shipyard.title", "SHIPYARD — buy a ship"), 24, 18, big, Title);
    t.textDrawPx(tr("service.panel.shipyard.subtitle", "F6  /  models, passports and seller offers from the ship catalogue"), 24, 18 + big + 6,
                 small, Muted);

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
        const std::string price = tr("service.panel.shipyard.price_thousands", {{"k", grouped(m.priceListCr / 1000.0)}}, "{k} k Cr");
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
        t.textDrawPx(tr("service.panel.shipyard.view_side", "side  ·  nose left"), cx + 10, y + 6, small, Muted);
        t.textDrawPx(tr("service.panel.shipyard.view_top", "top"), cx + 10, y + pvH * 0.5f + 6, small, Muted);
        y += pvH + 12;

        // the main specs (two columns)
        const float colW = cw * 0.5f;
        const float lineH = static_cast<float>(px) * 1.55f;
        for (std::size_t i = 0; i < c.mainSpecs.size(); ++i)
        {
            const float x = cx + (i % 2 == 0 ? 0.0f : colW);
            const float yy = y + lineH * 2.0f * static_cast<float>(i / 2);
            t.textDrawPx(rd(c.mainSpecs[i].labelRu), x, yy, small, Muted);
            t.textDrawPx(rd(c.mainSpecs[i].value), x, yy + static_cast<float>(small) + 3, px, Text);
        }
        y += lineH * 2.0f * static_cast<float>((c.mainSpecs.size() + 1) / 2) + 6;

        // seller offers (concrete instances)
        const std::string offersTitle = tr("service.panel.shipyard.offers", "Seller offers");
        t.textDrawPx(offersTitle, cx, y, px, Title);
        if (!core::isPreviewBuild())   // HantedPreview: no developer notes on the panels
            t.textDrawPx(tr("service.panel.shipyard.offers_dev", "DEV: trial stock until the economy is connected"),
                         cx + t.measureTextPx(offersTitle, px) + 16, y + 2, small, Muted);
        y += static_cast<float>(px) + 8;
        const float cols[] = {0.0f, 0.30f, 0.44f, 0.56f, 0.70f, 0.80f};
        const std::string heads[] = {tr("service.panel.shipyard.col.seller", "seller"), tr("service.panel.shipyard.col.condition", "condition"),
                                     tr("service.panel.shipyard.col.age", "age"), tr("service.panel.shipyard.col.hours", "hours run"),
                                     tr("service.panel.shipyard.col.wear", "wear / dmg."), tr("service.panel.shipyard.col.price", "price")};
        for (int k = 0; k < 6; ++k)
            t.textDrawPx(heads[k], cx + cw * cols[k], y, small, Muted);
        y += static_cast<float>(small) + 6;
        for (const auto& o : c.offers)
        {
            const std::string cond = o.condition == "new"    ? tr("service.panel.shipyard.condition.new", "new")
                                     : o.condition == "good" ? tr("service.panel.shipyard.condition.good", "good")
                                     : o.condition == "worn" ? tr("service.panel.shipyard.condition.worn", "worn")
                                                             : tr("service.panel.shipyard.condition.scrap", "for scrap");
            t.textDrawPx(rd(o.sellerNameRu), cx, y, small, Text);
            t.textDrawPx(cond, cx + cw * cols[1], y, small, o.condition == "wreck" ? Bad : o.condition == "worn" ? Warn : Good);
            t.textDrawPx(tr("service.panel.shipyard.age_years", {{"years", fmt("%.0f", o.ageYears)}}, "{years} yr"), cx + cw * cols[2], y, small, Text);
            t.textDrawPx(tr("service.panel.shipyard.hours_run", {{"hours", grouped(o.operatingHours)}}, "{hours} h"), cx + cw * cols[3], y, small, Text);
            t.textDrawPx(fmt("%.0f%%", o.wear01 * 100.0) + " / " + fmt("%.0f%%", o.damage01 * 100.0), cx + cw * cols[4], y, small, Text);
            t.textDrawPx(grouped(o.priceCr) + " Cr", cx + cw * cols[5], y, small, Title);
            // step 14: a new ship can be bought at a berth (the server decides); used ones later (wear not carried yet)
            if (o.condition == "new")
            {
                const std::string buy = tr("service.panel.shipyard.buy", "BUY");
                const float bx = cx + cw - t.measureTextPx(buy, small) - 16, bw = t.measureTextPx(buy, small) + 14;
                t.solidRectPx(bx - 4, y - 2, bw, static_cast<float>(small) + 6, Selected);
                t.textDrawPx(buy, bx + 3, y, small, Warn);
                m_hits.push_back({bx - 4, y - 2, bx - 4 + bw, y + static_cast<float>(small) + 4, 0, {ServicePanelActionType::ShipyardBuy, o.offerId}});
            }
            y += static_cast<float>(small) + 7;
        }
        y += 8;
        const std::string toggle = page.passportExpanded ? tr("service.panel.shipyard.passport_collapse", "[-] Passport (collapse)")
                                                         : tr("service.panel.shipyard.passport_expand", "[+] Full passport and equipment");
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
                t.textDrawPx(rd(c.passportSpecs[i].labelRu), cx, yy, small, Muted);
                t.textDrawPx(rd(c.passportSpecs[i].value), cx + cw * 0.30f, yy, small, Text);
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
        t.textDrawPx(tr("service.panel.refit.title", "REPAIR & REFIT"), 24, 18, big, Title);
        t.textDrawPx(tr("service.panel.refit.no_ships", "no data on the player's ships"), 24, 24 + big, px, Muted);
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
    // base 2: another owned ship (the controlled one is listed first) - the pilot can move to it
    if (page.currentShip > 0)
    {
        const std::string board = tr("service.panel.refit.board", "BOARD THIS SHIP");
        const float bx = headEnd + arrowW + 16, bw = t.measureTextPx(board, small) + 20;
        t.solidRectPx(bx, 18, bw, static_cast<float>(small) * 1.8f, Selected);
        t.textDrawPx(board, bx + 10, 18 + small * 0.4f, small, Warn);
        m_hits.push_back({bx, 18, bx + bw, 18 + small * 1.8f, 0, {ServicePanelActionType::RefitBoard, std::to_string(tab.id.value)}});
    }
    t.textDrawPx(tr("service.panel.refit.subtitle",
                    {{"model", page.modelName}, {"n", std::to_string(page.currentShip + 1)}, {"total", std::to_string(page.ships.size())}},
                    "F7  /  repair & refit  ·  {model}  ·  ship {n} of {total}"),
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
        t.textDrawPx(tr("service.panel.refit.no_placements", "this hull has no passport module positions: pick from the list below"), 28,
                     top + schH * 0.5f, small, Muted);
    t.textDrawPx(tr("service.panel.refit.schematic", "functional diagram · side view, nose left · colour = condition"), 24, top + 6, small, Muted);

    // ---- group chips
    float gx = 16, gy = top + schH + 10;
    for (const auto& g : page.groups)
    {
        const std::string groupName = tr("service.panel.refit.group." + g.key, g.labelRu);
        const std::string label =
            g.failed    ? tr("service.panel.refit.chip_failed", {{"group", groupName}, {"n", std::to_string(g.modules)}, {"failed", std::to_string(g.failed)}},
                             "{group} {n}  failed {failed}")
            : g.damaged ? tr("service.panel.refit.chip_damaged", {{"group", groupName}, {"n", std::to_string(g.modules)}, {"damaged", std::to_string(g.damaged)}},
                             "{group} {n}  dmg. {damaged}")
                        : tr("service.panel.refit.chip", {{"group", groupName}, {"n", std::to_string(g.modules)}}, "{group} {n}");
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
    t.textDrawPx(group.empty() ? tr("service.panel.refit.list_damaged", "damaged modules of the ship") : tr("service.panel.refit.list_group", "modules of the group"),
                 16, gy, small, Muted);
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
        const std::string st = m.damage01 > 0.001
                                   ? tr("service.panel.refit.row_state_damaged",
                                        {{"state", stateText(m.state)}, {"health", fmt("%.0f", m.health01 * 100.0)}, {"damage", fmt("%.0f", m.damage01 * 100.0)}},
                                        "{state}  {health}%  dmg. {damage}%")
                                   : tr("service.panel.refit.row_state", {{"state", stateText(m.state)}, {"health", fmt("%.0f", m.health01 * 100.0)}},
                                        "{state}  {health}%");
        t.textDrawPx(st, leftW - 24 - t.measureTextPx(st, small) - 8, y + 6, small, stateColour(m));
        m_hits.push_back({16, y, leftW - 8, y + rowH, 2, {ServicePanelActionType::RefitSelectModule, m.moduleId}});
    }
    if (rows.empty())
        t.textDrawPx(group.empty() ? tr("service.panel.refit.no_damage", "no damage") : std::string("—"), 34, gy + 4, px, Good);

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
        line(tr("service.panel.refit.label.state", "condition"), stateText(m.state), stateColour(m));
        line(tr("service.panel.refit.label.integrity", "integrity"), fmt("%.1f%%", m.health01 * 100.0), stateColour(m));
        line(tr("service.panel.refit.label.function_damage", "functional damage"), fmt("%.1f%%", m.damage01 * 100.0),
             m.damage01 > 0.3 ? Bad : m.damage01 > 0.01 ? Warn : Text);
        std::string causes;
        for (const auto& [name, v] : m.causes)
            causes += (causes.empty() ? "" : ", ") +
                      tr("service.panel.refit.cause_item", {{"cause", tr("service.panel.refit.cause." + name, name)}, {"pct", fmt("%.0f", v * 100.0)}},
                         "{cause} {pct}%");
        if (!causes.empty())
            line(tr("service.panel.refit.label.causes", "causes"), causes, Warn);
        line(tr("service.panel.refit.label.wear", "wear"), fmt("%.1f%%", m.wear01 * 100.0));
        line(tr("service.panel.refit.label.mass", "mass"),
             f.massTonnes > 0 ? tr("service.panel.refit.tonnes", {{"t", fmt("%.2f", f.massTonnes)}}, "{t} t") : std::string("—"));
        if (f.powerMW > 0)
            line(tr("service.panel.refit.label.power", "power"), tr("service.panel.refit.megawatts", {{"mw", fmt("%.1f", f.powerMW)}}, "{mw} MW"));
        if (m.hasPlacement)
            line(tr("service.panel.refit.label.position", "position"),
                 tr("service.panel.refit.position",
                    {{"x", fmt("%.1f", m.centerMeters.x)}, {"y", fmt("%.1f", m.centerMeters.y)}, {"z", fmt("%.1f", m.centerMeters.z)}},
                    "x {x}  y {y}  z {z} m"));
        {
            std::string repair = levelText(f.repairLevel);
            if (f.lineReplaceable)
                repair += tr("service.panel.refit.repair_lru", ", line-replaceable unit");
            if (f.replaceHours > 0)
                repair += tr("service.panel.refit.repair_swap", {{"hours", fmt("%.0f", f.replaceHours)}}, ", swap {hours} h");
            line(tr("service.panel.refit.label.repair", "repair"), repair);
        }
        if (f.priceCr > 0)
            line(tr("service.panel.refit.label.price", "unit price"), grouped(f.priceCr) + " Cr");
        y += 6;

        // actions
        const float bw = (rw - 24) / 3.0f, bh = static_cast<float>(px) + 14;
        const auto button = [&](float x, const std::string& label, ServicePanelActionType type, bool enabled, const std::string& arg = {}) {
            t.solidRectPx(x, y, bw - 6, bh, enabled ? glm::vec4(0.05f, 0.14f, 0.22f, 1.0f) : glm::vec4(0.03f, 0.05f, 0.07f, 1.0f));
            t.solidRectPx(x, y, bw - 6, 1, Line);
            t.textDrawPx(label, x + std::max(4.0f, (bw - 6 - t.measureTextPx(label, small)) * 0.5f), y + (bh - static_cast<float>(small)) * 0.5f, small,
                         enabled ? Text : Muted);
            if (enabled)
                m_hits.push_back({x, y, x + bw - 6, y + bh, 0, {type, arg}});
        };
        button(rx + 6, tr("service.panel.refit.repair", "Repair"), ServicePanelActionType::RefitPreviewRepair, f.repairable);
        button(rx + 6 + bw, tr("service.panel.refit.replace", "Replace"), ServicePanelActionType::RefitPreviewReplace, true);
        if (!core::isPreviewBuild())   // HantedPreview: the free DEV apply is not offered (nor sent, nor accepted)
        button(rx + 6 + 2 * bw, tr("service.panel.refit.apply_dev", "Apply (DEV)"),
               ServicePanelActionType::RefitApply,
               page.previewForSelection && page.preview.ok && page.preview.applicable);
        y += bh + 10;

        // the preview: what changes in the whole ship
        if (page.previewForSelection)
        {
            const auto& p = page.preview;
            const bool repair = p.operation.kind == game::ship::refit::RefitOperationKind::Repair;
            const std::string previewTitle =
                p.operation.productId.empty()
                    ? (repair ? tr("service.panel.refit.preview_repair", "Repair: what changes in the ship")
                              : tr("service.panel.refit.preview_replace", "Replace: what changes in the ship"))
                    : (repair ? tr("service.panel.refit.preview_repair_with", {{"product", p.operation.productId}}, "Repair with {product}: what changes in the ship")
                              : tr("service.panel.refit.preview_replace_with", {{"product", p.operation.productId}},
                                   "Replace with {product}: what changes in the ship"));
            t.textDrawPx(previewTitle, rx + 6, y, px, Title);
            y += static_cast<float>(px) + 6;
            if (!p.ok)
            {
                t.textDrawPx(rd(p.error), rx + 6, y, small, Bad);
                y += static_cast<float>(small) + 6;
            }
            else
            {
                for (const auto& dlt : p.deltas)
                {
                    if (y > h - 120)
                        break;
                    t.textDrawPx(rd(dlt.labelRu), rx + 6, y, small, Muted);
                    std::string v;
                    const std::string yes = tr("service.panel.refit.flag_yes", "yes"), no = tr("service.panel.refit.flag_no", "no");
                    if (dlt.flag)
                        v = (dlt.before > 0.5 ? yes : no) + "  ->  " + (dlt.after > 0.5 ? yes : no);
                    else
                        v = fmt(std::abs(dlt.after) >= 100 ? "%.0f" : "%.2f", dlt.before) + "  ->  " +
                            fmt(std::abs(dlt.after) >= 100 ? "%.0f" : "%.2f", dlt.after) + " " + rd(dlt.unit);
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
                    t.textDrawPx(tr("service.panel.refit.no_change", "nothing changes for the ship"), rx + 6, y, small, Muted);
                    y += static_cast<float>(small) + 5;
                }
                y += 4;
                t.textDrawPx(tr("service.panel.refit.cost",
                                {{"level", levelText(p.cost.repairLevel)}, {"hours", fmt("%.1f", p.cost.labourHours)}, {"labour", grouped(p.cost.labourCr)},
                                 {"material", grouped(p.cost.materialCr)}, {"total", grouped(p.cost.totalCr)}},
                                "cost {level}: {hours} h, labour {labour} + materials {material} = {total} Cr"),
                             rx + 6, y, small, Title);
                y += static_cast<float>(small) + 5;
                t.textDrawPx(rd(p.applyNote), rx + 6, y, small, Muted);
                y += static_cast<float>(small) + 8;
            }
        }
        else
        {
            t.textDrawPx(tr("service.panel.refit.choose_hint", "choose “Repair” or “Replace”: the server runs the ship before and after"), rx + 6, y,
                         small, Muted);
            y += static_cast<float>(small) + 8;
        }
        if (page.applyResultFresh)
        {
            t.textDrawPx(page.applyResult.empty() ? tr("service.panel.refit.applied", "applied: ShipCore changed")
                                                  : tr("service.panel.refit.not_applied", {{"reason", rd(page.applyResult)}}, "not applied: {reason}"),
                         rx + 6, y, small, page.applyResult.empty() ? Good : Bad);
            y += static_cast<float>(small) + 8;
        }

        // compatible replacements
        if (!page.replacements.empty() && y < h - 90)
        {
            t.textDrawPx(tr("service.panel.refit.replacements", "compatible products (replacement preview)"), rx + 6, y, small, Muted);
            y += static_cast<float>(small) + 5;
            for (const auto& r : page.replacements)
            {
                if (y > h - 70)
                    break;
                const std::string label = r.sameProduct ? tr("service.panel.refit.replacement_same", {{"name", r.name}, {"price", grouped(r.priceCr)}},
                                                             "new of the same type: {name}  ·  {price} Cr")
                                                        : r.name + "  ·  " + grouped(r.priceCr) + " Cr";
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
        return tr("service.panel.trade.price_per_kg", {{"price", grouped(cr / 1000.0)}}, "{price} Cr/kg");
    return tr("service.panel.trade.price_per_t", {{"price", grouped(cr)}}, "{price} Cr/t");
}

std::string ageText(double hours)
{
    if (hours < 1.0)
        return tr("service.panel.trade.age_now", "just now");
    if (hours < 48.0)
        return tr("service.panel.trade.age_hours", {{"h", fmt("%.0f", hours)}}, "{h} h ago");
    return tr("service.panel.trade.age_days", {{"d", fmt("%.1f", hours / 24.0)}}, "{d} d ago");
}

std::string channelText(const std::string& c)
{
    if (c == "open") return tr("service.panel.trade.channel.open", "open");
    if (c == "licensed") return tr("service.panel.trade.channel.licensed", "licensed");
    if (c == "restricted") return tr("service.panel.trade.channel.restricted", "restricted");
    if (c == "black") return tr("service.panel.trade.channel.black", "black");
    if (c == "closed") return tr("service.panel.trade.channel.closed", "closed");
    return c;
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

std::string fill(std::string s, const std::vector<std::pair<std::string, std::string>>& args);

// F8 -> ФИНАНСЫ: the server's ledger as replicated to this session (docs/ECONOMY_BANKING.md); `text` = the
// "service.trade.finance.<key>" strings in the UI language
void renderFinance(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text,
                   float listX, float listY, float listW, float listH, float w, float h, int px, int small, int big)
{
    const auto tx = [&](const std::string& key) -> std::string {
        const auto it = text.find(key);
        return it != text.end() ? it->second : key;
    };
    t.solidRectPx(listX, listY, listW, listH, Panel);
    if (!f.valid)
    {
        t.textDrawPx(tr("service.panel.finance.no_data", "no bank data (server without economy)"), listX + 12, listY + 12, px, Muted);
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
    row(tr("service.panel.finance.standing", "Client standing"), rd(f.standingRu), good ? Good : f.standing == "grace" ? Warn : Bad, px);
    row(tr("service.panel.finance.net_worth", "Net worth"), money2(f.netWorthCr) + " Cr", f.netWorthCr >= 0.0 ? Good : Bad, px);
    if (f.heldCr > 0.005)   // contract deposits (trade step 5): the client's money, blocked until hand-in / failure
        row(tr("service.panel.finance.held", "  of it blocked (contract deposits)"), money2(f.heldCr) + " Cr", Warn, small);
    if (f.garnishDebtCr > 0.005 || f.lastWithheldDay >= 0)   // income garnish over the protected minimum (TRADE_DESIGN §1.3)
        row(tx("garnish"),
            fill(tx("garnish_value"), {{"{debt}", grouped(f.garnishDebtCr)}, {"{withheld}", grouped(f.lastWithheldCr)},
                                       {"{protected}", grouped(f.protectedCr)}}),
            f.garnishDebtCr > 0.005 ? Warn : Good, small);
    header(tr("service.panel.finance.cash", "Cash"));
    for (const auto& c : f.cash)
        row(rd(c.label), money2(c.amount) + " " + c.currency + (c.currency == "CR" ? "" : "  (" + grouped(c.amountCr) + " Cr)"), Text, small);
    header(tr("service.panel.finance.accounts", "Accounts"));
    for (const auto& a : f.accounts)
    {
        row(rd(a.label), money2(a.amount) + " " + a.currency + (a.currency == "CR" ? "" : "  (" + grouped(a.amountCr) + " Cr)"),
            a.amount >= 0.0 ? Text : Warn, px);
        row(tr("service.panel.finance.overdraft", {{"limit", grouped(a.limit)}, {"currency", a.currency}, {"rate", fmt("%.2f", a.annualRate * 100.0)}},
               "  overdraft {limit} {currency}, rate {rate} %") +
                (a.state.empty() ? "" : "  ·  " + rd(a.state)),
            "", Muted, small);
    }
    if (!f.loans.empty())
    {
        header(tr("service.panel.finance.loans", "Debts"));
        for (const auto& l : f.loans)
        {
            row(rd(l.label), money2(l.amount) + " " + l.currency, Warn, px);
            row(tr("service.panel.finance.loan_terms", {{"rate", fmt("%.2f", l.annualRate * 100.0)}, {"payment", grouped(l.limit)}},
                   "  rate {rate} %, payment {payment} / 30 d") +
                    (l.state.empty() ? "" : "  ·  " + rd(l.state)),
                "", Muted, small);
        }
    }
    header(tr("service.panel.finance.lifeline", "Lifeline credit"));
    row(tr("service.panel.finance.lifeline_left", "Limit left (fuel, repair to flying condition; tow — always)"),
        tr("service.panel.finance.lifeline_value", {{"cr", grouped(f.lifelineLeftCr)}}, "{cr} Cr / 90 d"), Good, small);
    if (!f.notices.empty())
    {
        header(tr("service.panel.finance.notices", "Recent operations"));
        for (const auto& n : f.notices)
            row(rd(n), "", Muted, small);
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
    t.textDrawPx(tr("service.panel.finance.paying_here", "Paying here"), cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line(tr("service.panel.finance.polity", "Polity"), f.localPolity.empty() ? tr("service.panel.finance.no_state", "no state") : rd(f.localPolity), Text);
    line(tr("service.panel.finance.bank", "Bank"), rd(f.localBank), Text);
    line(tr("service.panel.finance.fx_rate", "Rate"), "1 Cr = " + fmt("%.4f", f.localFxPerCr) + " " + f.localCurrency, Text);
    ry += 10.0f;
    t.textDrawPx(tr("service.panel.finance.fuel", "Fuel"), cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    if (!f.fuelQuoteValid)
        line(tr("service.panel.finance.jump_charge", "Jump charge"), tr("service.panel.finance.not_sold", "not sold here"), Bad);
    else
    {
        line(tr("service.panel.finance.where", "Where"), rd(f.fuelHubName), Text);
        line(tr("service.panel.finance.jump_charge_list", "Jump charge, list price"),
             tr("service.panel.finance.jump_charge_list_value",
                {{"price", grouped(f.jumpChargeListPerT)}, {"currency", f.localCurrency}, {"cr", grouped(f.jumpChargeListCrPerT)}},
                "{price} {currency}/t  ({cr} Cr/t)"),
             Text);
        line(tr("service.panel.finance.to_pay", "To pay"),
             f.fuelFree ? tr("service.panel.finance.fuel_free", "0 — fuel is free for now")
                        : tr("service.panel.finance.per_tonne", {{"price", grouped(f.jumpChargeChargedPerT)}, {"currency", f.localCurrency}},
                             "{price} {currency}/t"),
             f.fuelFree ? Good : Text);
        line(tr("service.panel.finance.station_stock", "Station stock"),
             tr("service.panel.finance.tonnes", {{"t", fmt("%.1f", f.jumpChargeStockT)}}, "{t} t"), Text);
        if (f.reactorFuelListPerKg > 0.0)
            line(tr("service.panel.finance.reactor_fuel", "Reactor fuel (D/He-3)"),
                 tr("service.panel.finance.per_kg", {{"price", grouped(f.reactorFuelListPerKg)}, {"currency", f.localCurrency}}, "{price} {currency}/kg"),
                 Text);
        ry += 6.0f;
        t.textDrawPx(tr("service.panel.finance.breakdown", "What the charge price is made of"), cx, ry, px, Title);
        ry += static_cast<float>(px) + 6.0f;
        for (const auto& b : f.fuelBreakdown)
            line(rd(b), "", Muted);
    }
    if (f.freeFuelListCr > 0.0)
        line(tr("service.panel.finance.free_fuel", "Received free at list price"), grouped(f.freeFuelListCr) + " Cr", Muted);
}

// F8 -> ГРУЗ: the server's manifest of the ship (docs/TRADE_DESIGN.md §8.3, read-only in step 1), the hull figures and
// the latest trade results
void renderCargo(TextRenderer& t, const game::simulation::ClientTradeSnapshot& s, const game::simulation::ClientWalletSnapshot& f,
                 float listX, float listY, float listW, float listH, float w, float h, int px, int small, int big,
                 const std::function<void(float, float, float, float, const std::string&)>& addBlackButton)
{
    t.solidRectPx(listX, listY, listW, listH, Panel);
    if (!s.valid)
    {
        t.textDrawPx(tr("service.panel.cargo.no_data", "no cargo data (server without economy)"), listX + 12, listY + 12, px, Muted);
        return;
    }
    const auto& goods = game::economy::EconomyModel::instance().goods();
    const auto goodRu = [&](int g) {
        return g >= 0 && g < static_cast<int>(goods.size()) ? goods[static_cast<std::size_t>(g)].ru : std::string("?");
    };
    float y = listY + 10.0f;
    const float x = listX + 12.0f, right = listX + listW - 12.0f;
    t.textDrawPx(tr("service.panel.cargo.manifest", "Manifest"), x, y, px, Title);
    y += static_cast<float>(px) + 8.0f;
    if (s.cargo.empty())
    {
        t.textDrawPx(tr("service.panel.cargo.empty", "hold empty"), x, y, px, Muted);
        y += static_cast<float>(px) + 6.0f;
    }
    // cargo insurance (step 13): the active policy; the button insures what is aboard now (at a berth)
    {
        const std::string pol = s.insuredUntilDay >= 0
                                    ? tr("service.panel.cargo.insured", {{"cr", grouped(s.insuredCr)}, {"day", std::to_string(s.insuredUntilDay)}},
                                         "Cargo insurance: {cr} Cr until day {day}")
                                    : tr("service.panel.cargo.not_insured", "Cargo insurance: none");
        t.textDrawPx(pol, x, y, small, s.insuredUntilDay >= 0 ? Good : Muted);
        if (s.docked && s.insuranceQuoteCr > 0.0)
        {
            const std::string label = tr("service.panel.cargo.insure", {{"value", grouped(s.insuranceValueCr)}, {"premium", grouped(s.insuranceQuoteCr)}},
                                         "INSURE {value} Cr  ·  premium {premium} Cr");
            const float bw = t.measureTextPx(label, small) + 20.0f, bh = small * 1.7f;
            t.solidRectPx(right - bw, y - small * 0.3f, bw, bh, Selected);
            t.textDrawPx(label, right - bw + 10, y, small, Warn);
            addBlackButton(right - bw, y - small * 0.3f, right, y - small * 0.3f + bh, "#insure");
        }
        y += static_cast<float>(small) * 1.8f + 6.0f;
    }
    for (const auto& c : s.cargo)
    {
        if (y > listY + listH - 2.0f * static_cast<float>(px))
            break;
        t.textDrawPx(c.good >= 0 ? goodRu(c.good) : (c.contractId.empty() ? std::string("?") : tr("service.panel.cargo.contract_cargo", "contract cargo")), x,
                     y, px, Text);
        const std::string mass = tr("service.panel.cargo.mass", {{"t", fmt("%.2f", c.tonnes)}, {"n", std::to_string(c.c20)}}, "{t} t  ·  C20 x {n}");
        t.textDrawPx(mass, right - t.measureTextPx(mass, px), y, px, Text);
        y += static_cast<float>(px) + 4.0f;
        const std::vector<game::localization::MessageParam> lotParams {{"lot", c.lotId}, {"contract", c.contractId},
                                                                        {"day", std::to_string(c.loadedDay)}, {"hub", rd(c.originHub)},
                                                                        {"cost", grouped(c.costBasisCrPerT)}};
        const std::string detail =
            c.contractId.empty()
                ? tr("service.panel.cargo.lot_own", lotParams, "lot {lot}  ·  own goods  ·  loaded day {day} ({hub})  ·  cost basis {cost} Cr/t")
                : tr("service.panel.cargo.lot_contract", lotParams, "lot {lot}  ·  contract {contract}  ·  loaded day {day} ({hub})  ·  cost basis {cost} Cr/t");
        t.textDrawPx(detail, x + 10, y, small, Muted);
        y += static_cast<float>(small) + 8.0f;
        // TRADE step 9: contraband here - the black market; restricted - open until licences (step 11)
        if (c.legalityHere == "banned" && c.contractId.empty())
        {
            t.textDrawPx(tr("service.panel.cargo.banned", "BANNED HERE: customs will seize it, fine 2 x value"), x + 10, y, small, Bad);
            if (c.blackBidCrPerT > 0.0 && c.good >= 0)
            {
                const std::string label = tr("service.panel.cargo.black_market", {{"cr", grouped(c.tonnes * c.blackBidCrPerT)}}, "BLACK MARKET  +{cr} Cr");
                const float bw = t.measureTextPx(label, small) + 20.0f, bh = small * 1.7f;
                const float bx = right - bw;
                t.solidRectPx(bx, y - small * 0.3f, bw, bh, Selected);
                t.textDrawPx(label, bx + 10, y, small, Warn);
                addBlackButton(bx, y - small * 0.3f, bx + bw, y - small * 0.3f + bh, goods[static_cast<std::size_t>(c.good)].id);
            }
            y += static_cast<float>(small) * 1.8f + 6.0f;
        }
        else if (c.legalityHere == "restricted")
        {
            t.textDrawPx(tr("service.panel.cargo.restricted", "restricted here: a licence will be needed (traded openly for now)"), x + 10, y, small, Warn);
            y += static_cast<float>(small) + 6.0f;
        }
    }
    if (!s.results.empty())
    {
        y += 6.0f;
        t.textDrawPx(tr("service.panel.cargo.recent_deals", "Recent deals"), x, y, px, Title);
        y += static_cast<float>(px) + 6.0f;
        for (auto it = s.results.rbegin(); it != s.results.rend(); ++it)
        {
            if (y > listY + listH - static_cast<float>(small))
                break;
            const std::string line = it->ok ? "#" + std::to_string(it->requestId) + "  " + rd(it->message)
                                            : tr("service.panel.cargo.result_refused", {{"id", std::to_string(it->requestId)}, {"message", rd(it->message)}},
                                                 "#{id}  REFUSED: {message}");
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
    const auto tonnes = [](const std::string& v) { return tr("service.panel.cargo.tonnes", {{"t", v}}, "{t} t"); };
    const auto lightYears = [](double ly) { return tr("service.panel.cargo.light_years", {{"ly", fmt("%.1f", ly)}}, "{ly} ly"); };
    t.textDrawPx(tr("service.panel.cargo.hold", "Hold"), cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line(tr("service.panel.cargo.c20_slots", "C20 slots"),
         tr("service.panel.cargo.c20_used", {{"used", std::to_string(s.c20Used)}, {"slots", std::to_string(s.c20Slots)}}, "{used} used / {slots}"),
         s.c20Used >= s.c20Slots ? Warn : Text);
    line(tr("service.panel.cargo.net", "Net cargo"), fmt("%.2f", s.cargoNetT) + " / " + tonnes(fmt("%.0f", s.netCapacityT)), Text);
    line(tr("service.panel.cargo.tare", "C20 tare (1.7 t per container)"), tonnes(fmt("%.1f", s.cargoGrossT - s.cargoNetT)), Text);
    line(tr("service.panel.cargo.gross", "Gross cargo"), tonnes(fmt("%.2f", s.cargoGrossT)), Text);
    ry += 10.0f;
    t.textDrawPx(tr("service.panel.cargo.ship", "Ship"), cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    line(tr("service.panel.cargo.mass_now", "Mass now"), tonnes(fmt("%.1f", s.shipMassT)), Text);
    line(tr("service.panel.cargo.jump_full", "Jump range on a full charge"), lightYears(s.jumpRangeLy),
         s.jumpRangeLy < s.jumpRangeEmptyLy - 0.05 ? Warn : Text);
    line(tr("service.panel.cargo.jump_empty", "  with an empty hold"), lightYears(s.jumpRangeEmptyLy), Muted);
    if (s.jumpRangeEmptyLy > 0.0 && s.jumpRangeLy < s.jumpRangeEmptyLy - 0.05)
        line(tr("service.panel.cargo.jump_cut", "  cargo shortens the jump by"), fmt("%.1f %%", 100.0 * (1.0 - s.jumpRangeLy / s.jumpRangeEmptyLy)), Warn);
    ry += 10.0f;
    t.textDrawPx(tr("service.panel.cargo.money", "Money"), cx, ry, big, Text);
    ry += static_cast<float>(big) + 6.0f;
    if (f.valid)
        for (const auto& a : f.accounts)
            line(rd(a.label), money2(a.amount) + " " + a.currency, a.amount >= 0.0 ? Text : Warn);
    if (s.vatReclaimCr > 0.0)
        line(tr("service.panel.cargo.vat_reclaim", "Input VAT to reclaim"), grouped(s.vatReclaimCr) + " Cr", Muted);
}
// F5 state app -> НАЛОГИ (TRADE_DESIGN §5.2, step 7): the server's tax book of the residence polity as replicated
void renderTaxes(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text,
                 float x, float y, float w, float h, float /*screenW*/, float /*screenH*/, int px, int small, int big)
{
    const auto tx = [&](const char* key) -> std::string {
        const auto it = text.find(key);
        return it == text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(x, y, w, h, Panel);
    if (!f.valid)
    {
        t.textDrawPx(tx("tax_no_data"), x + 12, y + 10, px, Muted);
        return;
    }
    float ry = y + 10.0f;
    const auto line = [&](const std::string& label, const std::string& value, const glm::vec4& col) {
        t.textDrawPx(label, x + 12, ry, small, Muted);
        t.textDrawPx(value, x + w * 0.55f, ry, small, col);
        ry += static_cast<float>(small) + 7.0f;
    };
    const auto pct = [](double v) { return fmt("%.0f %%", v * 100.0); };
    t.textDrawPx(tx("tax_residence") + ": " + rd(f.taxResidence), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    line(tx("tax_style"), rd(f.taxStyleRu), Text);
    line(tx("tax_rate"), pct(f.taxRate) + (f.taxAdminFeeShare > 0.0 ? "  + " + pct(f.taxAdminFeeShare) + " " + tx("tax_admin_fee") : ""), Text);
    line(tx("tax_rules"), std::to_string(f.taxPeriodDays) + " " + tx("tax_days") + ", " + tx("tax_declare") + " " + std::to_string(f.taxDeclareDays) + " " +
                              tx("tax_days"), Muted);
    ry += 8.0f;
    t.textDrawPx(tx("tax_open_period") + (f.taxPeriodStartDay >= 0 ? "  " + std::to_string(f.taxPeriodStartDay) + "-" +
                                                                     std::to_string(f.taxPeriodStartDay + f.taxPeriodDays - 1) : std::string()),
                 x + 12, ry, px, Text);
    ry += static_cast<float>(px) + 6.0f;
    line(tx("tax_revenue"), grouped(f.taxRevenueCr) + " Cr", Text);
    line(tx("tax_expenses"), (f.taxExpensesCr > 0.0 ? "-" : "") + grouped(f.taxExpensesCr) + " Cr", Text);
    const double profit = f.taxRevenueCr - f.taxExpensesCr;
    line(tx("tax_profit"), (profit >= 0.0 ? "" : "-") + grouped(std::fabs(profit)) + " Cr", profit >= 0.0 ? Good : Warn);
    if (f.taxWithheldCreditCr > 0.0)
        line(tx("tax_withheld_credit"), "-" + grouped(f.taxWithheldCreditCr) + " Cr", Text);
    if (f.taxWithheldLostCr > 0.0)
        line(tx("tax_withheld_lost"), grouped(f.taxWithheldLostCr) + " Cr", Warn);
    line(tx("tax_estimate"), grouped(f.taxEstimateCr) + " Cr", f.taxEstimateCr > 0.0 ? Warn : Muted);
    ry += 8.0f;
    t.textDrawPx(tx("tax_last_period"), x + 12, ry, px, Text);
    ry += static_cast<float>(px) + 6.0f;
    if (f.taxAssessedPeriodStartDay < 0)
    {
        line(tx("tax_none_yet"), "", Muted);
        return;
    }
    line(tx("tax_period"), std::to_string(f.taxAssessedPeriodStartDay) + "-" + std::to_string(f.taxAssessedPeriodStartDay + f.taxPeriodDays - 1), Text);
    line(tx("tax_profit"), (f.taxAssessedProfitCr >= 0.0 ? "" : "-") + grouped(std::fabs(f.taxAssessedProfitCr)) + " Cr", Text);
    line(tx("tax_assessed"), grouped(f.taxAssessedCr + f.taxAssessedFeeCr) + " Cr", Text);
    if (!f.taxSettled)
        line(tx("tax_due"), tx("tax_until_day") + " " + std::to_string(f.taxDueDay) + "  (" + tx("tax_in") + " " + std::to_string(f.taxDueDay - f.day) +
                                " " + tx("tax_days") + ")", Warn);
    else if (f.taxAssessedCr + f.taxAssessedFeeCr <= 0.0)
        line(tx("tax_status"), tx("tax_nothing"), Good);
    else
    {
        line(tx("tax_paid"), grouped(f.taxPaidCr) + " Cr", Good);
        if (f.taxDebtCr > 0.0)
            line(tx("tax_debt"), grouped(f.taxDebtCr) + " Cr  + " + tx("tax_penalty") + " " + grouped(f.taxPenaltyCr) + " Cr", Bad);
    }
}
// F5 state app -> ЛИЦЕНЗИИ (TRADE_DESIGN §4, step 11): the pilot licence categories (like a driving licence in Дія),
// the licences held, the offers of this hub's polity with a buy button at a berth
void renderLicences(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text,
                    bool docked, float x, float y, float w, float h, int px, int small,
                    const std::function<void(float, float, float, float, const std::string&)>& addBuyButton)
{
    const auto tx = [&](const char* key) -> std::string {
        const auto it = text.find(key);
        return it == text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(x, y, w, h, Panel);
    float ry = y + 10.0f;
    const float right = x + w - 12.0f;
    t.textDrawPx(tx("lic_pilot"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    for (const auto& c : f.pilotCategories)
    {
        const std::string mark = c.granted ? "+" : "−";
        t.textDrawPx(mark, x + 14, ry, px, c.granted ? Good : Muted);
        t.textDrawPx(rd(c.ru) + "  ·  " + tx("lic_control") + ": " + c.control + (c.note.empty() ? std::string() : "  (" + rd(c.note) + ")"), x + 34, ry,
                     small, c.granted ? Text : Muted);
        ry += static_cast<float>(small) + 7.0f;
    }
    ry += 8.0f;
    t.textDrawPx(tx("lic_held"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.licences.empty())
    {
        t.textDrawPx(tx("lic_none"), x + 14, ry, small, Muted);
        ry += static_cast<float>(small) + 7.0f;
    }
    for (const auto& l : f.licences)
    {
        t.textDrawPx(rd(l.ru) + "  ·  " + rd(l.polity), x + 14, ry, small, l.active ? Text : Muted);
        const std::string until = (l.active ? tx("lic_until") : tx("lic_expired")) + " " + std::to_string(l.expiresDay);
        t.textDrawPx(until, right - t.measureTextPx(until, small), ry, small, l.active ? Good : Bad);
        ry += static_cast<float>(small) + 7.0f;
    }
    ry += 8.0f;
    t.textDrawPx(tx("lic_offers") + ": " + rd(f.licencePolity), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (!docked)
    {
        t.textDrawPx(tx("lic_dock_hint"), x + 14, ry, small, Warn);
        ry += static_cast<float>(small) + 7.0f;
    }
    for (const auto& o : f.licenceOffers)
    {
        if (ry > y + h - 3.0f * static_cast<float>(small))
            break;
        t.textDrawPx(rd(o.ru), x + 14, ry, small, Text);
        const std::string fee = grouped(o.feeCr) + " Cr";
        const bool covers = !o.goodsRu.empty();
        if (docked)
        {
            const std::string label = (o.held ? tx("lic_extend") : tx("lic_buy")) + "  " + fee;
            const float bw = t.measureTextPx(label, small) + 20.0f, bh = small * 1.7f;
            t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
            t.textDrawPx(label, right - bw + 10, ry, small, o.held ? Good : Warn);
            addBuyButton(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, o.type);
        }
        else
            t.textDrawPx(fee, right - t.measureTextPx(fee, small), ry, small, Muted);
        ry += static_cast<float>(small) + 4.0f;
        if (covers)
            t.textDrawPx(tx("lic_covers") + ": " + rd(o.goodsRu), x + 24, ry, small, Muted);
        ry += static_cast<float>(small) + 10.0f;
    }
}
// F5 state app -> СЛУЖБА ЗАНЯТОСТИ (TRADE_DESIGN §11b, author: the hiring board is the state employment service): the
// pilot's job (with a quit button) and this hub's vacancies (a hire button at a berth, the reason when not eligible)
void renderJobs(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text,
                bool docked, float x, float y, float w, float h, int px, int small,
                const std::function<void(float, float, float, float, const std::string&)>& addButton)
{
    const auto tx = [&](const char* key) -> std::string {
        const auto it = text.find(key);
        return it == text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(x, y, w, h, Panel);
    float ry = y + 10.0f;
    const float right = x + w - 12.0f;
    t.textDrawPx(tx("job_current"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (!f.employed)
    {
        t.textDrawPx(tx("job_none"), x + 14, ry, small, Muted);
        ry += static_cast<float>(small) + 7.0f;
    }
    else
    {
        t.textDrawPx(rd(f.jobRoleRu) + "  ·  " + rd(f.jobEmployerRu), x + 14, ry, small, Text);
        const std::string quit = tx("job_quit");
        const float bw = t.measureTextPx(quit, small) + 20.0f, bh = small * 1.7f;
        t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
        t.textDrawPx(quit, right - bw + 10, ry, small, Warn);
        addButton(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, std::string());
        ry += static_cast<float>(small) + 7.0f;
        t.textDrawPx(tx("job_wage") + ": " + grouped(f.jobWageCr) + " Cr / 30 " + tx("job_days") + "  ·  " + tx("job_ship") + ": " + rd(f.jobShipRu) +
                         "  ·  " + tx("job_since") + " " + std::to_string(f.jobSinceDay),
                     x + 14, ry, small, Muted);
        ry += static_cast<float>(small) + 7.0f;
    }
    ry += 8.0f;
    t.textDrawPx(tx("job_vacancies"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.vacancies.empty())
    {
        t.textDrawPx(tx("job_no_vacancies"), x + 14, ry, small, Muted);
        return;
    }
    if (!docked)
    {
        t.textDrawPx(tx("job_dock_hint"), x + 14, ry, small, Warn);
        ry += static_cast<float>(small) + 7.0f;
    }
    for (const auto& v : f.vacancies)
    {
        if (ry > y + h - 3.0f * static_cast<float>(small))
            break;
        t.textDrawPx(rd(v.roleRu) + "  ·  " + rd(v.employerRu), x + 14, ry, small, Text);
        if (docked && v.eligible && !f.employed)
        {
            const std::string hire = tx("job_hire");
            const float bw = t.measureTextPx(hire, small) + 20.0f, bh = small * 1.7f;
            t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
            t.textDrawPx(hire, right - bw + 10, ry, small, Good);
            addButton(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, v.id);
        }
        ry += static_cast<float>(small) + 4.0f;
        std::string line = grouped(v.wageCr) + " Cr / 30 " + tx("job_days") + "  ·  " + tx("job_ship") + ": " + rd(v.shipRu) + "  ·  " + rd(v.requirementRu);
        if (v.termDays > 0)
            line += "  ·  " + tx("job_term") + " " + std::to_string(v.termDays) + " " + tx("job_days");
        t.textDrawPx(line, x + 24, ry, small, Muted);
        ry += static_cast<float>(small) + 4.0f;
        if (!v.eligible)
        {
            t.textDrawPx(rd(v.whyNotRu), x + 24, ry, small, Bad);
            ry += static_cast<float>(small) + 4.0f;
        }
        ry += 6.0f;
    }
}
// F5 state app -> МЧС (TRADE_DESIGN §4.2, step 12): the licence here, the SOS calls of this system, the active call
void renderRescue(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text,
                  float x, float y, float w, float h, int px, int small,
                  const std::function<void(float, float, float, float, const std::string&)>& addButton)
{
    const auto tx = [&](const char* key) -> std::string {
        const auto it = text.find(key);
        return it == text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(x, y, w, h, Panel);
    float ry = y + 10.0f;
    const float right = x + w - 12.0f;
    t.textDrawPx(f.sarLicence ? tx("sos_licence_yes") : tx("sos_licence_no"), x + 12, ry, small, f.sarLicence ? Good : Warn);
    ry += static_cast<float>(small) + 10.0f;
    if (!f.activeSosId.empty())
    {
        for (const auto& c : f.sos)
            if (c.id == f.activeSosId)
                t.textDrawPx(tx("sos_active") + ": " + rd(c.kindRu) + " — " + tx("sos_fly_to") + " " + rd(c.hubName) + " (+" + grouped(c.rewardCr) + " Cr)",
                             x + 12, ry, px, Title);
        ry += static_cast<float>(px) + 10.0f;
    }
    t.textDrawPx(tx("sos_list"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.sos.empty())
    {
        t.textDrawPx(tx("sos_none"), x + 14, ry, small, Muted);
        return;
    }
    for (const auto& c : f.sos)
    {
        if (ry > y + h - 2.0f * static_cast<float>(small))
            break;
        const bool open = c.status == "open";
        t.textDrawPx(rd(c.kindRu) + "  ·  " + rd(c.hubName) + "  ·  +" + grouped(c.rewardCr) + " Cr  ·  " + tx("sos_until") + " " + std::to_string(c.expiresDay),
                     x + 14, ry, small, open ? Text : Muted);
        if (open && f.sarLicence && f.activeSosId.empty())
        {
            const std::string take = tx("sos_take");
            const float bw = t.measureTextPx(take, small) + 20.0f, bh = small * 1.7f;
            t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
            t.textDrawPx(take, right - bw + 10, ry, small, Good);
            addButton(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, c.id);
        }
        else if (!open)
        {
            const std::string st = tx(("sos_status_" + c.status).c_str());
            t.textDrawPx(st, right - t.measureTextPx(st, small), ry, small, c.mine ? Good : Muted);
        }
        ry += static_cast<float>(small) + 9.0f;
    }
}
// F5 -> СТРОЙКА (TRADE_DESIGN §6.2, step 16): the contractors' offers for the hub's planned module and the player's
// contracts (progress, the stage s1..s4, paid of the price, status, income once let to the operator)
void renderBuild(TextRenderer& t, const game::simulation::ClientWalletSnapshot& f, const std::map<std::string, std::string>& text, bool docked,
                 float x, float y, float w, float h, int px, int small,
                 const std::function<void(float, float, float, float, const std::string&)>& addButton,
                 const std::function<void(float, float, float, float, const std::string&)>& addLease,
                 const std::function<void(float, float, float, float, ServicePanelActionType, const std::string&)>& addMine)
{
    const auto tx = [&](const char* key) -> std::string {
        const auto it = text.find(key);
        return it == text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(x, y, w, h, Panel);
    float ry = y + 10.0f;
    const float right = x + w - 12.0f;
    // the units glued after a number in the lines built from the service.government.* fragments
    const std::string unitT = " " + tr("service.panel.unit.tonnes", "t"), unitM = " " + tr("service.panel.unit.metres", "m");
    // step 16 part 2: the player's mines and the sites offered here
    if (!f.mines.empty())
    {
        t.textDrawPx(tx("mine_mine"), x + 12, ry, px, Title);
        ry += static_cast<float>(px) + 6.0f;
        for (const auto& m : f.mines)
        {
            t.textDrawPx(rd(m.nameRu) + "  ·  " + rd(m.hubName) + "  ·  " + rd(m.goodRu) + "  ·  " + rd(m.statusRu), x + 14, ry, small, Text);
            ry += static_cast<float>(small) + 5.0f;
            t.textDrawPx(tx("mine_reserve") + " " + grouped(m.reserveLeftT) + " / " + grouped(m.reserveT) + unitT + "  ·  " + tx("mine_today") + " " +
                             grouped(m.lastOutputT) + unitT + "  ·  " + tx("mine_sold") + " " + grouped(m.soldT) + unitT + "  ·  " + tx("mine_revenue") + " " +
                             grouped(m.revenueCr) + " Cr  ·  " + tx("mine_rent") + " " + grouped(m.rentYearCr) + " Cr/" + tx("mine_year"),
                         x + 24, ry, small, m.lastOutputT > 0.0 ? Good : Muted);
            ry += static_cast<float>(small) + 5.0f;
            // part 3: the sale mode and the stockpile
            t.textDrawPx(tx("mine_stock") + " " + grouped(m.stockT) + unitT + "  ·  " + (m.sellLocal ? tx("mine_selling") : tx("mine_keeping")), x + 24, ry, small,
                         Text);
            float bx = right;
            const auto button = [&](const std::string& label, ServicePanelActionType type, const std::string& arg) {
                const float bw = t.measureTextPx(label, small) + 20.0f, bh = small * 1.7f;
                bx -= bw;
                t.solidRectPx(bx, ry - small * 0.3f, bw, bh, Selected);
                t.textDrawPx(label, bx + 10, ry, small, Good);
                addMine(bx, ry - small * 0.3f, bx + bw, ry - small * 0.3f + bh, type, arg);
                bx -= 8.0f;
            };
            button(m.sellLocal ? tx("mine_keep") : tx("mine_sell"), ServicePanelActionType::GovernmentMineSale, (m.sellLocal ? "0|" : "1|") + m.id);
            if (m.here && m.stockT >= 0.01)
                button(tx("mine_load"), ServicePanelActionType::GovernmentMineLoad, m.id);
            ry += static_cast<float>(small) + 10.0f;
        }
    }
    if (!f.siteOffers.empty())
    {
        t.textDrawPx(tx("mine_offers"), x + 12, ry, px, Title);
        ry += static_cast<float>(px) + 6.0f;
        for (const auto& s : f.siteOffers)
        {
            t.textDrawPx(rd(s.nameRu) + "  ·  " + rd(s.goodRu) + "  ·  r " + grouped(s.radiusM) + unitM + "  ·  " + tx("mine_grade") + " " +
                             std::to_string(s.grade).substr(0, 4) + "  ·  " + tx("mine_reserve") + " " + grouped(s.reserveT) + unitT + "  ·  " + tx("mine_rent") + " " + grouped(s.rentYearCr) + " Cr/" + tx("mine_year") +
                             "  ·  " + tx("mine_lease_cost") + " " + grouped(s.leaseCostCr) + " Cr",
                         x + 14, ry, small, Text);
            {
                const std::string lease = tx("mine_lease");
                const float bw = t.measureTextPx(lease, small) + 20.0f, bh = small * 1.7f;
                t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
                t.textDrawPx(lease, right - bw + 10, ry, small, Good);
                addLease(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, s.id);
            }
            ry += static_cast<float>(small) + 9.0f;
        }
        ry += 6.0f;
    }
    t.textDrawPx(tx("build_mine"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.buildProjects.empty())
    {
        t.textDrawPx(tx("build_none"), x + 14, ry, small, Muted);
        ry += static_cast<float>(small) + 9.0f;
    }
    for (const auto& p : f.buildProjects)
    {
        t.textDrawPx(rd(p.projectRu) + "  ·  " + rd(p.hubName) + "  ·  " + rd(p.contractorRu), x + 14, ry, small, Text);
        ry += static_cast<float>(small) + 5.0f;
        std::string line = tx("build_stage") + " s" + std::to_string(p.stage) + "  ·  " + std::to_string(static_cast<int>(p.progress * 100.0 + 0.5)) +
                           " %  ·  " + tx("build_paid") + " " + grouped(p.paidCr) + " / " + grouped(p.priceCr) + " Cr  ·  " + rd(p.statusRu);
        if (p.incomeCrPerDay > 0.0)
            line += "  ·  +" + grouped(p.incomeCrPerDay) + " Cr/" + tx("build_day");
        t.textDrawPx(line, x + 24, ry, small, p.incomeCrPerDay > 0.0 ? Good : Muted);
        ry += static_cast<float>(small) + 10.0f;
    }
    ry += 6.0f;
    t.textDrawPx(tx("build_offers"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.buildOffers.empty())
    {
        t.textDrawPx(tx("build_no_offers"), x + 14, ry, small, Muted);
        return;
    }
    for (const auto& o : f.buildOffers)
    {
        if (ry > y + h - 2.0f * static_cast<float>(small))
            break;
        t.textDrawPx(rd(o.projectRu) + "  ·  " + rd(o.contractorRu) + "  ·  " + grouped(o.priceCr) + " Cr  ·  " + std::to_string(o.workDays) + " " +
                         tx("build_days") + "  ·  " + tx("build_reliability") + " " + std::to_string(static_cast<int>(o.reliability * 100.0 + 0.5)) + " %",
                     x + 14, ry, small, Text);
        {
            const std::string sign = tx("build_sign");
            const float bw = t.measureTextPx(sign, small) + 20.0f, bh = small * 1.7f;
            t.solidRectPx(right - bw, ry - small * 0.3f, bw, bh, Selected);
            t.textDrawPx(sign, right - bw + 10, ry, small, Good);
            addButton(right - bw, ry - small * 0.3f, right, ry - small * 0.3f + bh, o.id);
        }
        ry += static_cast<float>(small) + 9.0f;
    }
    if (!docked)
        t.textDrawPx(tx("build_dock_hint"), x + 14, ry + 4.0f, small, Muted);   // a hint only: the server takes it in the hub's frame too
}
// a bot that has quit: the server's state text is "ушёл" (legacy plain string) or a Message whose key ends in "quit"
bool botQuit(const std::string& stateRu)
{
    std::string key;
    std::vector<game::localization::MessageParam> params;
    if (game::localization::parseMessage(stateRu, key, params))
        return key.size() >= 4 && key.compare(key.size() - 4, 4, "quit") == 0;
    return stateRu == "ушёл";   // the legacy plain-text producer (a comparison, never shown)
}

// F8 -> ФЛОТ (TRADE_DESIGN §6.4, step 15): the agency's candidates (a CV, not the traits) and the owner's bots - state,
// loop, trips, profit, the delivered reports; hire puts the bot on the first owned ship the player does not fly
void renderFleet(TextRenderer& t, const game::presentation::TradePresentation& page, float x, float y, float w, float h, int px, int small,
                 const std::function<void(float, float, float, float, ServicePanelActionType, const std::string&)>& addButton)
{
    const auto& f = page.finance;
    t.solidRectPx(x, y, w, h, Panel);
    float ry = y + 10.0f;
    const float right = x + w - 12.0f;
    const auto button = [&](const std::string& label, float bxRight, ServicePanelActionType type, const std::string& id, const glm::vec4& col) {
        const float bw = t.measureTextPx(label, small) + 20.0f, bh = small * 1.7f;
        t.solidRectPx(bxRight - bw, ry - small * 0.3f, bw, bh, Selected);
        t.textDrawPx(label, bxRight - bw + 10, ry, small, col);
        addButton(bxRight - bw, ry - small * 0.3f, bxRight, ry - small * 0.3f + bh, type, id);
        return bw;
    };
    t.textDrawPx(tr("service.panel.fleet.my_pilots", "My pilots"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (f.bots.empty())
    {
        t.textDrawPx(tr("service.panel.fleet.none_yet", "none yet"), x + 14, ry, small, Muted);
        ry += static_cast<float>(small) + 7.0f;
    }
    for (const auto& b : f.bots)
    {
        if (ry > y + h * 0.62f)
            break;
        const bool gone = botQuit(b.stateRu);
        t.textDrawPx(rd(b.name) + "  ·  " + rd(b.shipRu) + "  ·  " + rd(b.stateRu), x + 14, ry, small, gone ? Muted : Text);
        if (!gone)
        {
            const float w1 = button(tr("service.panel.fleet.fire", "FIRE"), right, ServicePanelActionType::BotFire, b.id, Warn);
            button(tr("service.panel.fleet.loop", "LOOP"), right - w1 - 8.0f, ServicePanelActionType::BotLoop, b.id, Good);
        }
        ry += static_cast<float>(small) + 5.0f;
        {
            const std::vector<game::localization::MessageParam> stats {
                {"loop", b.loopRu.empty() ? tr("service.panel.fleet.no_order", "no order") : rd(b.loopRu)},
                {"trips", std::to_string(b.trips)},
                {"profit", (b.profitCr >= 0.0 ? "+" : "") + grouped(b.profitCr)},
                {"wage", grouped(b.wageCr)},
                {"unpaid", std::to_string(b.unpaidDays)}};
            t.textDrawPx(b.unpaidDays > 0
                             ? tr("service.panel.fleet.bot_stats_unpaid", stats,
                                  "{loop}  ·  trips {trips}  ·  result {profit} Cr  ·  wage {wage} Cr / 30 d  ·  UNPAID {unpaid} d")
                             : tr("service.panel.fleet.bot_stats", stats, "{loop}  ·  trips {trips}  ·  result {profit} Cr  ·  wage {wage} Cr / 30 d"),
                         x + 24, ry, small, b.unpaidDays > 0 ? Bad : Muted);
        }
        ry += static_cast<float>(small) + 5.0f;
        const std::size_t from = b.reports.size() > 3 ? b.reports.size() - 3 : 0;
        for (std::size_t k = from; k < b.reports.size(); ++k)
        {
            t.textDrawPx(rd(b.reports[k]), x + 24, ry, small, Muted);
            ry += static_cast<float>(small) + 3.0f;
        }
        ry += 6.0f;
    }
    ry += 6.0f;
    t.textDrawPx(tr("service.panel.fleet.agency", "Hub agency: candidates"), x + 12, ry, px, Title);
    ry += static_cast<float>(px) + 6.0f;
    if (page.botShipEntity == 0)
    {
        t.textDrawPx(tr("service.panel.fleet.need_ship", "you need an own ship you are not flying now (F6: buy)"), x + 14, ry, small, Warn);
        ry += static_cast<float>(small) + 7.0f;
    }
    for (const auto& c : f.botCandidates)
    {
        if (ry > y + h - 2.0f * static_cast<float>(small))
            break;
        t.textDrawPx(tr("service.panel.fleet.candidate",
                        {{"name", rd(c.name)}, {"years", std::to_string(c.yearsShown)}, {"skill", fmt("%.0f", c.skillShown * 100.0)}, {"wage", grouped(c.wageCr)}},
                        "{name}  ·  per CV: {years} yr experience, skill {skill} %  ·  {wage} Cr / 30 d"),
                     x + 14, ry, small, Text);
        if (page.botShipEntity != 0)
            button(tr("service.panel.fleet.hire", {{"ship", rd(page.botShipName)}}, "HIRE for {ship}"), right, ServicePanelActionType::BotHire, c.id, Good);
        ry += static_cast<float>(small) + 9.0f;
    }
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
        t.textDrawPx(tr("service.panel.trade.title", "TRADE"), 24, 18, big, Title);
        t.textDrawPx(tr("service.panel.trade.no_data", "no economy data or no hub nearby"), 24, 18 + big + 10, px, Muted);
        t.endFrame();
        return;
    }
    t.textDrawPx(tr("service.panel.trade.title_hub", {{"hub", rd(page.hubName)}}, "TRADE — {hub}"), 24, 18, big, Title);
    t.textDrawPx(page.atHub ? tr("service.panel.trade.subtitle_at_hub", {{"day", std::to_string(page.day)}},
                                 "F8  /  you are at the hub  ·  day {day}  ·  market, contracts, cargo, finance, system hubs  ·  navigation data — F5")
                            : tr("service.panel.trade.subtitle_nearest", {{"day", std::to_string(page.day)}},
                                 "F8  /  nearest hub  ·  day {day}  ·  market, contracts, cargo, finance, system hubs  ·  navigation data — F5"),
                 24, 18 + big + 6, small, Muted);
    if (!page.events.empty())
    {
        std::string list;
        for (std::size_t i = 0; i < page.events.size(); ++i)
            list += (i ? ", " : "") + rd(page.events[i].ru);
        t.textDrawPx(tr("service.panel.trade.events", {{"list", list}}, "Events: {list}"), 24, 18 + big + 8 + small + 4, small, Warn);
    }

    // ---- tabs
    const float tabY = 18.0f + big + 18.0f + 2.0f * small;
    const auto tabIt = page.contractText.find("tab");
    const std::string contractsTab = tabIt == page.contractText.end() ? tr("service.panel.trade.tab.contracts", "CONTRACTS") : tabIt->second;
    // by id: 0 contracts, 1 market, 2 hubs, 4 finance, 5 cargo; drawn in TradeTabOrder, keys 1..5 = that order
    const std::string tabs[] = {contractsTab,
                                tr("service.panel.trade.tab.market", "MARKET"),
                                tr("service.panel.trade.tab.hubs", "SYSTEM HUBS"),
                                std::string(),
                                tr("service.panel.trade.tab.finance", "FINANCE"),
                                tr("service.panel.trade.tab.cargo", "CARGO"),
                                tr("service.panel.trade.tab.fleet", "FLEET")};
    float tx = 24.0f;
    int key = 1;
    for (const int i : game::presentation::TradeTabOrder)
    {
        const std::string label = std::to_string(key++) + "  " + tabs[i];
        const float tw = t.measureTextPx(label, px) + 24.0f;
        t.solidRectPx(tx, tabY, tw, px * 1.8f, page.tab == i ? Selected : Panel);
        t.textDrawPx(label, tx + 12, tabY + px * 0.35f, px, page.tab == i ? Title : Text);
        m_hits.push_back({tx, tabY, tx + tw, tabY + px * 1.8f, 0, {ServicePanelActionType::TradeTab, std::to_string(i)}});
        tx += tw + 8.0f;
    }

    // ---- left list
    const float listX = 24.0f, listY = tabY + px * 2.4f, listW = w * 0.58f, listH = h - listY - 40.0f;
    if (page.tab == 6)
    {
        renderFleet(t, page, listX, listY, listW, listH, px, small,
                    [this](float x0, float y0, float x1, float y1, ServicePanelActionType type, const std::string& id) {
                        m_hits.push_back({x0, y0, x1, y1, 0, {type, id}});
                    });
        t.endFrame();
        return;
    }
    if (page.tab == 4)
    {
        renderFinance(t, page.finance, page.financeText, listX, listY, listW, listH, w, h, px, small, big);
        t.endFrame();
        return;
    }
    if (page.tab == 5)
    {
        renderCargo(t, page.trade, page.finance, listX, listY, listW, listH, w, h, px, small, big,
                    [this](float x0, float y0, float x1, float y1, const std::string& good) {
                        m_hits.push_back({x0, y0, x1, y1, 0, {good == "#insure" ? ServicePanelActionType::CargoInsure
                                                                                : ServicePanelActionType::CargoBlackSell, good}});
                    });
        t.endFrame();
        return;
    }
    if (page.tab == 0)
    {
        renderTradeContracts(page, listX, listY, listW, listH, w, h, px, small, big);
        t.endFrame();
        return;
    }
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({4, listX, listY, listX + listW, listY + listH});
    const float rowH = static_cast<float>(px) * 2.7f;
    const auto& goods = game::economy::EconomyModel::instance().goods();
    std::size_t rows = page.tab == 1 ? page.market.size() : page.hubs.size();
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
        if (page.tab == 1)
        {
            const auto& q = page.market[i];
            const auto& g = goods[static_cast<std::size_t>(q.good)];
            const bool sel = page.deal.valid && page.deal.good == q.good;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            t.textDrawPx(g.ru, listX + 10, y + 4, px, sel ? Title : Text);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::TradeSelectGood, std::to_string(q.good)}});
            std::string sub = tr("service.panel.trade.market_stock", {{"group", g.groupRu}, {"stock", grouped(q.stockT)}}, "{group}  ·  stock {stock} t");
            if (i < page.marketLimits.size())
                sub += tr("service.panel.trade.market_limits",
                          {{"takes", grouped(page.marketLimits[i].hubTakesUpToT)}, {"gives", grouped(page.marketLimits[i].hubGivesUpToT)}},
                          "  ·  hub takes ≤ {takes} t, gives ≤ {gives} t") +
                       (page.marketLimits[i].regimeNote.empty() ? std::string() : "  ·  " + rd(page.marketLimits[i].regimeNote));
            t.textDrawPx(sub, listX + 10, y + 6 + static_cast<float>(px), small, Muted);
            const std::string prices = tr("service.panel.trade.prices", {{"buy", priceText(q.buyCr)}, {"sell", priceText(q.sellCr)}}, "buy {buy}   sell {sell}");
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
            t.textDrawPx(hub->id == page.hubId ? tr("service.panel.trade.hub_here", {{"name", hub->name}}, "{name}  (here)") : hub->name, listX + 10, y + 4,
                         px, sel ? Title : Text);
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
        line(tr("service.panel.trade.card.owner", "Owner"), c.info.ownerName, Text);
        if (!c.info.operatorName.empty())
            line(tr("service.panel.trade.card.operator", "Operator"), c.info.operatorName, Text);
        line(tr("service.panel.trade.card.distance", "Distance"), c.distanceText.empty() ? tr("service.panel.trade.card.no_data", "no data") : rd(c.distanceText),
             Text);
        line(tr("service.panel.trade.card.channel", "Block channel"), channelText(c.channel), c.channel == "open" ? Good : c.channel == "licensed" ? Text : Warn);
        line(tr("service.panel.trade.card.prices", "Prices"), c.connected ? ageText(c.infoAgeHours) : tr("service.panel.trade.card.no_link", "no link"),
             c.connected ? (c.infoAgeHours > 72.0 ? Warn : Text) : Bad);
        if (!c.knownEvents.empty())
        {
            std::string ev;
            for (std::size_t i = 0; i < c.knownEvents.size(); ++i)
                ev += (i ? ", " : "") + rd(c.knownEvents[i].ru);
            line(tr("service.panel.trade.card.events", "Events"), ev, Warn);
        }

        // show on the map
        const std::string mapLabel = tr("service.panel.trade.card.show_on_map", "SHOW ON MAP");
        const float bw = t.measureTextPx(mapLabel, px) + 28.0f, bh = px * 1.9f;
        t.solidRectPx(cx, y + 4, bw, bh, Selected);
        t.textDrawPx(mapLabel, cx + 14, y + 4 + px * 0.4f, px, Title);
        m_hits.push_back({cx, y + 4, cx + bw, y + 4 + bh, 0, {ServicePanelActionType::TradeShowOnMap, c.info.id}});
        y += bh + 16;

        if (!c.knownPrices.empty())
        {
            t.textDrawPx(tr("service.panel.trade.card.known_prices", "Last known prices"), cx, y, px, Title);
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
            t.textDrawPx(page.trade.marketNote.empty() ? tr("service.panel.trade.market_hint", "the market is seen at the hub where the ship is")
                                                       : rd(page.trade.marketNote),
                         listX + 12, listY + 12, px, Muted);
        std::string status;
        if (page.trade.valid)
        {
            const std::vector<game::localization::MessageParam> params {{"day", std::to_string(page.trade.day)},
                                                                       {"style", rd(page.trade.fiscalStyleRu)},
                                                                       {"vat", fmt("%.0f", page.trade.vatShare * 100.0)},
                                                                       {"handling", grouped(page.trade.handlingCrPerT)}};
            status = page.trade.docked
                         ? tr("service.panel.trade.status_docked", params,
                              "server prices, day {day}  ·  style {style}, VAT {vat} %, handling {handling} Cr/t  ·  at a berth: deals allowed")
                         : tr("service.panel.trade.status_undocked", params,
                              "server prices, day {day}  ·  style {style}, VAT {vat} %, handling {handling} Cr/t  ·  deals only at a berth");
        }
        if (!page.trade.results.empty())
        {
            const auto& r = page.trade.results.back();
            status += "   |   " + (r.ok ? "#" + std::to_string(r.requestId) + " " + rd(r.message)
                                       : tr("service.panel.trade.last_refused", {{"id", std::to_string(r.requestId)}, {"message", rd(r.message)}},
                                            "#{id} REFUSED: {message}"));
        }
        t.textDrawPx(status, 24, h - 26, small, Text);
    }
    if (page.tab != 1)
        t.textDrawPx(tr("service.panel.trade.footer", "Click a posting or hub — card; wheel — scroll the list"), 24, h - 26, small, Muted);
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
        else if (l.kind == "duty")
            label = fill(tx("duty"), {{"{pct}", fmt("%.1f %%", d.dutyShare * 100.0)}});
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
        const std::string why = d.blockKind == "quote" ? rd(q.reason) : tx(d.blockKind);
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
        text(rd(d.result.message), cx, y, small, Muted);
    }
    else if (d.answer == TradeDealAnswer::Refused)
        text(fill(tx("refused"), {{"{reason}", rd(d.result.message)}}), cx, y, small, Bad);
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

// F5 state app: the colony's navigation data bank, the onboard computer's memory, the data carriers (JUMP_MECHANICS 10l)
void ServicePanelsRenderer::renderNavData(const game::presentation::NavDataPage& nav, float listX, float listY, float listW, float listH,
                                          float w, float h, int px, int small, int big)
{
    auto& t = TextRenderer::instance();
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({4, listX, listY, listX + listW, listY + listH});
    const float rowH = static_cast<float>(px) * 2.7f;
    if (nav.bankName.empty())
        t.textDrawPx(tr("service.panel.nav.no_bank", "This system has no navigation data bank (no beacon operator)"), listX + 10, listY + 8, px, Muted);
    else
    {
        t.textDrawPx(rd(nav.bankName), listX + 10, listY + 6, px, Title);
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
            t.textDrawPx(tr("service.panel.nav.row", {{"corridor", rd(r.corridor)}, {"place", rd(r.place)}}, "lane {corridor}  ·  {place}"), listX + 10, y + 4,
                         px, sel ? Title : r.open ? Text : Muted);
            t.textDrawPx(tr("service.panel.nav.row_survey",
                            {{"year", fmt("%.1f", r.surveyYear)}, {"age", fmt("%.1f", r.ageYears)}, {"state", rd(r.stateText)}, {"tier", rd(r.tierText)}},
                            "survey {year} ({age} yr ago)  ·  now: {state}  ·  {tier}"),
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
        t.textDrawPx(tr("service.panel.nav.card_title", {{"corridor", rd(sel->corridor)}}, "Lane {corridor}"), cx, y, big, Text);
        y += static_cast<float>(big) + 6;
        t.textDrawPx(rd(sel->place) + "  ·  " + rd(sel->tierText), cx, y, small, sel->open ? Muted : Warn);
        y += static_cast<float>(small) + 6;
        t.textDrawPx(tr("service.panel.nav.price", {{"price", grouped(sel->priceCr)}}, "price {price} Cr - payment comes with the market"), cx, y, small,
                     Muted);
        y += static_cast<float>(small) + 10;
        const bool can = sel->open && nav.atHub;
        if (!sel->open)
        {
            t.textDrawPx(tr("service.panel.nav.closed", {{"tier", rd(sel->tierText)}}, "Closed: {tier}"), cx, y, px, Warn);
            y += static_cast<float>(px) + 10;
        }
        else if (!nav.atHub)
        {
            t.textDrawPx(tr("service.panel.nav.only_at_hub", "Data is issued only at a hub"), cx, y, px, Warn);
            y += static_cast<float>(px) + 10;
        }
        button(tr("service.panel.nav.to_computer", "TO ONBOARD COMPUTER"), can && nav.computerState == 1, ServicePanelActionType::NavDataLoad, sel->id);
        button(tr("service.panel.nav.to_carrier", "TO A CARRIER"), can, ServicePanelActionType::NavDataCarrier, sel->id);
        y += 8;
    }
    t.textDrawPx(tr("service.panel.nav.computer", "Onboard computer"), cx, y, px, Title);
    y += static_cast<float>(px) + 6;
    if (nav.computerState != 1)
    {
        t.textDrawPx(tr("service.panel.nav.computer_down", "not working - lane records unavailable"), cx, y, small, Bad);
        y += static_cast<float>(small) + 6;
    }
    else if (nav.memory.empty())
    {
        t.textDrawPx(tr("service.panel.nav.no_records", "no lane records"), cx, y, small, Muted);
        y += static_cast<float>(small) + 6;
    }
    for (const auto& m : nav.memory)
    {
        if (y > h - 120.0f)
            break;
        t.textDrawPx(tr("service.panel.nav.memory_row",
                        {{"corridor", rd(m.corridor)}, {"age", fmt("%.1f", m.ageYears)}, {"state", rd(m.stateText)}, {"tier", rd(m.tierText)}},
                        "{corridor}  ·  {age} yr  ·  {state}  ·  {tier}"),
                     cx, y, small, Text);
        y += static_cast<float>(small) + 5;
    }
    y += 8;
    t.textDrawPx(tr("service.panel.nav.carriers", "Data carriers aboard"), cx, y, px, Title);
    y += static_cast<float>(px) + 6;
    if (nav.carriers.empty())
    {
        t.textDrawPx(tr("service.panel.nav.none", "none"), cx, y, small, Muted);
        y += static_cast<float>(small) + 6;
    }
    for (std::size_t i = 0; i < nav.carriers.size(); ++i)
    {
        if (y > h - 60.0f)
            break;
        t.textDrawPx(std::to_string(i + 1) + ". " + rd(nav.carriers[i]), cx, y, small, Text);
        y += static_cast<float>(small) + 4;
        button(tr("service.panel.nav.insert", "INSERT INTO COMPUTER"), nav.computerState == 1, ServicePanelActionType::NavDataInsert, std::to_string(i + 1));
    }
    (void)cw;
    t.textDrawPx(tr("service.panel.nav.footer", "Click a record - card; open data comes from the colony bank, the rest by clearance"), 24, h - 26, small,
                 Muted);
}

namespace
{
std::string serving_label(const game::presentation::GovernmentPresentation& page)
{
    if (page.wallet.employed && page.wallet.jobKind == "military")
        return rd(page.wallet.jobEmployerRu);
    const auto it = page.text.find("galley_not_enlisted");
    return it == page.text.end() ? std::string("galley_not_enlisted") : it->second;
}
} // namespace

// F5 Government services (TRADE_DESIGN §11a, author 2026-11-20): everything of the state. Two apps as tabs: the state
// services app (localized like Дія / Госуслуги) and Галера+ (military service). The app's sections so far: navigation
// data; licences (trade step 11), taxes (step 7), fines and debts to the state, citizenship come as they are built.
void ServicePanelsRenderer::renderGovernment(const Viewport& viewport, const game::presentation::GovernmentPresentation& page)
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
    const auto text = [&](const char* key) -> std::string {
        const auto it = page.text.find(key);
        return it == page.text.end() ? std::string(key) : it->second;
    };
    t.solidRectPx(0, 0, w, h, Bg);
    t.solidRectPx(0, 0, w, 2, Line);
    t.textDrawPx(page.tab == 1 ? text("galley") : text("app"), 24, 18, big, Title);
    t.textDrawPx("F5  /  " + text(page.tab == 1 ? "galley_subtitle" : "app_subtitle"), 24, 18 + big + 6, small, Muted);

    const float tabY = 18.0f + big + 18.0f + 2.0f * small;
    const std::string tabs[] = {text("app"), text("galley")};
    float tx = 24.0f;
    for (int i = 0; i < 2; ++i)
    {
        const std::string label = std::to_string(i + 1) + "  " + tabs[i];
        const float tw = t.measureTextPx(label, px) + 24.0f;
        t.solidRectPx(tx, tabY, tw, px * 1.8f, page.tab == i ? Selected : Panel);
        t.textDrawPx(label, tx + 12, tabY + px * 0.35f, px, page.tab == i ? Title : Text);
        m_hits.push_back({tx, tabY, tx + tw, tabY + px * 1.8f, 0, {ServicePanelActionType::GovernmentTab, std::to_string(i)}});
        tx += tw + 8.0f;
    }

    const float listX = 24.0f, listY = tabY + px * 2.4f, listW = w * 0.58f, listH = h - listY - 40.0f;
    if (page.tab == 0)
    {
        // the app's sections (below the selected tab's box)
        const std::string sections[] = {text("taxes"), text("licences"), text("nav_data"), text("jobs"), text("rescue"), text("build")};
        float sx = listX;
        const float sy = listY + px * 0.4f;
        for (int i = 0; i < 6; ++i)
        {
            const float sw = t.measureTextPx(sections[i], small) + 20.0f;
            t.solidRectPx(sx, sy, sw, small * 1.7f, page.section == i ? Selected : Panel);
            t.textDrawPx(sections[i], sx + 10, sy + small * 0.35f, small, page.section == i ? Title : Text);
            m_hits.push_back({sx, sy, sx + sw, sy + small * 1.7f, 0, {ServicePanelActionType::GovernmentSection, std::to_string(i)}});
            sx += sw + 6.0f;
        }
        const float y0 = listY + px * 2.6f;
        if (page.section == 5)
            renderBuild(t, page.wallet, page.text, page.docked, listX, y0, listW, listH - (y0 - listY), px, small,
                        [this](float x0, float y0b, float x1, float y1, const std::string& id) {
                            m_hits.push_back({x0, y0b, x1, y1, 0, {ServicePanelActionType::GovernmentSignBuild, id}});
                        },
                        [this](float x0, float y0b, float x1, float y1, const std::string& id) {
                            m_hits.push_back({x0, y0b, x1, y1, 0, {ServicePanelActionType::GovernmentLeaseSite, id}});
                        },
                        [this](float x0, float y0b, float x1, float y1, ServicePanelActionType type, const std::string& arg) {
                            m_hits.push_back({x0, y0b, x1, y1, 0, {type, arg}});
                        });
        else if (page.section == 4)
            renderRescue(t, page.wallet, page.text, listX, y0, listW, listH - (y0 - listY), px, small,
                         [this](float x0, float y0b, float x1, float y1, const std::string& id) {
                             m_hits.push_back({x0, y0b, x1, y1, 0, {ServicePanelActionType::GovernmentTakeSos, id}});
                         });
        else if (page.section == 3)
            renderJobs(t, page.wallet, page.text, page.docked, listX, y0, listW, listH - (y0 - listY), px, small,
                       [this](float x0, float y0b, float x1, float y1, const std::string& id) {
                           m_hits.push_back({x0, y0b, x1, y1, 0, {id.empty() ? ServicePanelActionType::GovernmentQuit
                                                                             : ServicePanelActionType::GovernmentHire, id}});
                       });
        else if (page.section == 2)
            renderNavData(page.nav, listX, y0, listW, listH - (y0 - listY), w, h, px, small, big);
        else if (page.section == 1)
            renderLicences(t, page.wallet, page.text, page.docked, listX, y0, listW, listH - (y0 - listY), px, small,
                           [this](float x0, float y0b, float x1, float y1, const std::string& type) {
                               m_hits.push_back({x0, y0b, x1, y1, 0, {ServicePanelActionType::GovernmentBuyLicence, type}});
                           });
        else
            renderTaxes(t, page.wallet, page.text, listX, y0, listW, listH - (y0 - listY), w, h, px, small, big);
        t.endFrame();
        return;
    }
    // Галера+: not enlisted - the forms are there, the fields empty (author: "пока игрок не нанялся - там пустые графы")
    t.solidRectPx(listX, listY, listW, listH, Panel);
    float y = listY + 10.0f;
    t.textDrawPx(serving_label(page), listX + 12, y, px, page.wallet.employed && page.wallet.jobKind == "military" ? Good : Muted);
    y += px * 2.2f;
    const bool serving = page.wallet.employed && page.wallet.jobKind == "military";
    const std::string values[] = {serving ? rd(page.wallet.jobUnitRu) : std::string("—"), serving ? rd(page.wallet.jobShipRu) : std::string("—"),
                                  serving ? rd(page.wallet.jobRankRu) : std::string("—"),
                                  serving ? std::to_string(page.wallet.jobSinceDay) + " - " + std::to_string(page.wallet.jobTermEndDay) : std::string("—")};
    int fieldIndex = 0;
    for (const char* key : {"galley_unit", "galley_ship", "galley_rank", "galley_term"})
    {
        t.textDrawPx(text(key), listX + 12, y, small, Muted);
        t.textDrawPx(values[fieldIndex++], listX + listW * 0.4f, y, small, serving ? Text : Muted);
        y += small + 10.0f;
    }
    y += 10.0f;
    t.textDrawPx(text("galley_reports"), listX + 12, y, px, Text);
    y += px + 10.0f;
    for (const char* key : {"galley_report_transfer_ship", "galley_report_transfer_unit", "galley_report_leave", "galley_report_discharge"})
    {
        t.textDrawPx(text(key), listX + 24, y, small, Muted);
        y += small + 8.0f;
    }
    t.textDrawPx(text("galley_hint"), 24, h - 26, small, Muted);
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
// F8 CONTRACTS (TRADE_DESIGN §8.2, trade step 5): the server's shared board of this hub and the player's contracts, with
// their timers. Every number is the server's (session.trade board / contracts); the tab only lays them out and asks
// "take?" / "abandon?" before the request goes.
namespace
{
std::string ctext(const game::presentation::TradePresentation& page, const std::string& key)
{
    const auto it = page.contractText.find(key);
    return it == page.contractText.end() ? key : it->second;
}

std::string cfill(std::string s, const std::vector<std::pair<std::string, std::string>>& args)
{
    for (const auto& [k, v] : args)
        for (std::size_t p = s.find(k); p != std::string::npos; p = s.find(k, p + v.size()))
            s.replace(p, k.size(), v);
    return s;
}

// "2 сут 05 ч" / "17 ч" / "просрочен"
std::string leftText(const game::presentation::TradePresentation& page, double days)
{
    if (days <= 0.0)
        return ctext(page, "overdue");
    const int d = static_cast<int>(std::floor(days));
    const int h = static_cast<int>(std::floor((days - d) * 24.0));
    char hh[8];
    std::snprintf(hh, sizeof hh, "%02d", h);
    return d > 0 ? cfill(ctext(page, "days_hours"), {{"{d}", std::to_string(d)}, {"{h}", hh}})
                 : cfill(ctext(page, "hours"), {{"{h}", std::to_string(h)}});
}

std::string kindText(const game::presentation::TradePresentation& page, const std::string& kind)
{
    return ctext(page, "kind_" + kind);
}

std::string loadText(const game::presentation::TradePresentation& page, const game::simulation::ClientContractRow& r)
{
    if (r.people > 0)
        return cfill(ctext(page, "people"), {{"{n}", std::to_string(r.people)}, {"{m}", std::to_string(r.c20)}});
    return rd(r.goodRu) + ", " + cfill(ctext(page, "cargo"), {{"{t}", fmt("%.1f", r.tonnes)}, {"{c20}", std::to_string(r.c20)}});
}

glm::vec4 statusColour(const std::string& s)
{
    if (s == "open" || s == "active")
        return Text;
    if (s == "delivered")
        return Good;
    if (s == "late" || s == "cancelled")
        return Warn;
    if (s == "failed" || s == "abandoned")
        return Bad;
    return Muted;
}
} // namespace

void ServicePanelsRenderer::renderTradeContracts(const game::presentation::TradePresentation& page, float listX, float listY, float listW,
                                                 float listH, float w, float h, int px, int small, int big)
{
    auto& t = TextRenderer::instance();
    const auto& s = page.trade;
    t.solidRectPx(listX, listY, listW, listH, Panel);
    m_zones.push_back({4, listX, listY, listX + listW, listY + listH});
    if (!s.valid)
    {
        t.textDrawPx(ctext(page, "take_hint"), listX + 12, listY + 12, px, Muted);
        return;
    }
    // ---- the list: the board of this hub, then my contracts
    const float rowH = static_cast<float>(px) * 2.7f, headH = static_cast<float>(px) * 1.8f;
    const float total = headH * 2.0f + rowH * static_cast<float>(std::max<std::size_t>(1, s.board.size()) + std::max<std::size_t>(1, s.contracts.size()));
    std::vector<const game::simulation::ClientContractRow*> boardRows;
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& r : s.board)
            if ((r.status == "open") == (pass == 0))
                boardRows.push_back(&r);
    if (page.selectedContract != m_tradeContractShown)
    {
        // a new selection (click, keys, a DEV pick): scroll so that its row is in the list
        m_tradeContractShown = page.selectedContract;
        float at = -1.0f;
        for (std::size_t i = 0; i < boardRows.size(); ++i)
            if (boardRows[i]->id == page.selectedContract)
                at = headH + rowH * static_cast<float>(i);
        for (std::size_t i = 0; i < s.contracts.size() && at < 0.0f; ++i)
            if (s.contracts[i].id == page.selectedContract)
                at = headH * 2.0f + rowH * static_cast<float>(std::max<std::size_t>(1, s.board.size()) + i);
        if (at >= 0.0f && (at < m_tradeListScroll || at + rowH > m_tradeListScroll + listH))
            m_tradeListScroll = at - listH * 0.4f;
    }
    m_tradeListScroll = std::clamp(m_tradeListScroll, 0.0f, std::max(0.0f, total - listH));
    float y = listY - m_tradeListScroll;
    const auto visible = [&](float y0, float hh) { return y0 >= listY - 1.0f && y0 + hh <= listY + listH + 1.0f; };
    const auto header = [&](const std::string& text) {
        if (visible(y, headH))
        {
            t.textDrawPx(text, listX + 10, y + 4, px, Title);
            t.solidRectPx(listX, y + headH - 2, listW, 1, Line);
        }
        y += headH;
    };
    const auto row = [&](const game::simulation::ClientContractRow& r, bool mine) {
        if (visible(y, rowH))
        {
            const bool sel = r.id == page.selectedContract;
            if (sel)
                t.solidRectPx(listX, y, listW, rowH, Selected);
            const bool closed = mine ? r.status != "active" : r.status != "open";
            const std::string what = kindText(page, r.kind) + ": " + loadText(page, r);
            t.textDrawPx(what, listX + 10, y + 4, px, closed ? Muted : sel ? Title : Text);
            const std::string reward = grouped(r.rewardCr) + " Cr";
            t.textDrawPx(reward, listX + listW - t.measureTextPx(reward, px) - 10, y + 4, px, closed ? Muted : Good);
            std::string sub = "-> " + rd(r.destinationName) + "  ·  ";
            if (mine)
            {
                sub += ctext(page, "status_" + r.status);
                if (r.status == "active")
                    sub += "  ·  " + cfill(ctext(page, "deadline"), {{"{d}", std::to_string(r.deadlineDay)},
                                                                   {"{left}", leftText(page, r.deadlineDay + 1.0 - s.dayNow)}}) +
                           "  ·  " + ctext(page, r.aboard ? "aboard" : "not_aboard");
                else if (r.paidCr > 0.0)
                    sub += "  ·  +" + grouped(r.paidCr) + " Cr";
            }
            else if (r.status == "open")
                sub += cfill(ctext(page, "on_board"), {{"{left}", leftText(page, r.expiresDay - s.dayNow)}}) + "  ·  " +
                       ctext(page, "deposit") + " " + grouped(r.depositCr) + " Cr";
            else
                sub += ctext(page, "status_" + r.status);
            t.textDrawPx(sub, listX + 10, y + 6 + static_cast<float>(px), small, mine ? statusColour(r.status) : Muted);
            m_hits.push_back({listX, y, listX + listW, y + rowH, 4, {ServicePanelActionType::TradeContractSelect, r.id}});
            t.solidRectPx(listX, y + rowH - 1, listW, 1, glm::vec4(0.2f, 0.3f, 0.4f, 0.5f));
        }
        y += rowH;
    };
    header(cfill(ctext(page, "board"), {{"{hub}", s.hubName.empty() ? s.boardHubId : rd(s.hubName)}}));
    if (s.board.empty())
    {
        if (visible(y, rowH))
            t.textDrawPx(s.boardHubId.empty() ? ctext(page, "take_hint") : ctext(page, "empty_board"), listX + 10, y + 6, small, Muted);
        y += rowH;
    }
    for (const auto* r : boardRows)
        row(*r, false);
    header(ctext(page, "mine"));
    if (s.contracts.empty())
    {
        if (visible(y, rowH))
            t.textDrawPx(ctext(page, "empty_mine"), listX + 10, y + 6, small, Muted);
        y += rowH;
    }
    for (const auto& r : s.contracts)
        row(r, true);

    // ---- the card of the selected posting / contract
    const float cx = listX + listW + 24.0f, cw = w - cx - 24.0f;
    float ry = listY;
    const game::simulation::ClientContractRow* sel = nullptr;
    bool mine = false;
    for (const auto& r : s.board)
        if (r.id == page.selectedContract)
            sel = &r;
    for (const auto& r : s.contracts)
        if (r.id == page.selectedContract)
        {
            sel = &r;
            mine = true;
        }
    const auto line = [&](const std::string& label, const std::string& value, const glm::vec4& col) {
        if (ry > h - 60.0f - static_cast<float>(small))
            return;
        t.textDrawPx(label, cx, ry, small, Muted);
        t.textDrawPx(value, cx + cw - t.measureTextPx(value, small), ry, small, col);
        ry += static_cast<float>(small) + 6.0f;
    };
    if (!sel)
        t.textDrawPx(ctext(page, "select_hint"), cx, ry, small, Muted);
    else
    {
        t.textDrawPx(kindText(page, sel->kind), cx, ry, big, Text);
        ry += static_cast<float>(big) + 6.0f;
        t.textDrawPx(sel->id, cx, ry, small, Muted);
        ry += static_cast<float>(small) + 10.0f;
        line(ctext(page, "route"), rd(sel->originName) + " -> " + rd(sel->destinationName), Text);
        line(ctext(page, "load"), loadText(page, *sel), Text);
        line(ctext(page, "issuer"), rd(sel->issuerName), Text);
        if (!sel->cause.empty())
            line(ctext(page, "cause"), rd(sel->cause), Muted);
        ry += 6.0f;
        line(ctext(page, "reward"), grouped(sel->rewardCr) + " Cr", Good);
        line(ctext(page, "advance"), grouped(sel->advanceCr) + " Cr", Text);
        line(ctext(page, "deposit"), grouped(sel->depositCr) + " Cr", sel->depositCr > 0.0 ? Warn : Text);
        line(ctext(page, "penalty"), grouped(sel->penaltyCr) + " Cr", Warn);
        line(ctext(page, "late_rule"), fmt("%.0f %%", sel->lateSharePerDay * 100.0), Muted);
        line(ctext(page, "deadline_label"),
             cfill(ctext(page, "day"), {{"{d}", std::to_string(sel->deadlineDay)}}) + "  (" + leftText(page, sel->deadlineDay + 1.0 - s.dayNow) + ")",
             sel->deadlineDay + 1.0 - s.dayNow < 1.0 ? Warn : Text);
        if (!mine && sel->status == "open")
            line(ctext(page, "board_left"), leftText(page, sel->expiresDay - s.dayNow), Text);
        if (mine)
        {
            line(ctext(page, "status_" + sel->status), sel->status == "active" ? ctext(page, sel->aboard ? "aboard" : "not_aboard") : "",
                 statusColour(sel->status));
            if (sel->paidCr > 0.0)
                line(ctext(page, "payout"), "+" + grouped(sel->paidCr) + " Cr", Good);
            if (sel->penaltyPaidCr > 0.0)
                line(ctext(page, "penalty_paid"), "-" + grouped(sel->penaltyPaidCr) + " Cr", Bad);
            if (sel->lateDays > 0)
                line(cfill(ctext(page, "late"), {{"{d}", std::to_string(sel->lateDays)}}), "", Warn);
        }
        ry += 10.0f;
        // the buttons: take (open posting, docked here) / abandon (my active contract), each asked first
        const float bh = static_cast<float>(px) * 1.9f;
        const auto button = [&](const std::string& label, ServicePanelActionType type, const std::string& text, const glm::vec4& fill,
                                float x) {
            const float bw = t.measureTextPx(label, px) + 28.0f;
            t.solidRectPx(x, ry, bw, bh, fill);
            t.textDrawPx(label, x + 14, ry + px * 0.4f, px, Title);
            m_hits.push_back({x, ry, x + bw, ry + bh, 0, {type, text}});
            return x + bw + 12.0f;
        };
        const bool canTake = !mine && sel->status == "open" && s.docked && s.hubId == sel->originHub;
        const bool canAbandon = mine && sel->status == "active";
        if (page.contractConfirm && (canTake || canAbandon))
        {
            float bx = button(ctext(page, canTake ? "confirm_take" : "confirm_abandon"), ServicePanelActionType::TradeContractConfirm, {},
                              canTake ? glm::vec4(0.10f, 0.36f, 0.20f, 1.0f) : glm::vec4(0.40f, 0.12f, 0.10f, 1.0f), cx);
            button(ctext(page, "cancel"), ServicePanelActionType::TradeContractCancel, {}, Selected, bx);
            ry += bh + 8.0f;
        }
        else if (canTake)
        {
            button(ctext(page, "take"), ServicePanelActionType::TradeContractTake, sel->id, Selected, cx);
            ry += bh + 8.0f;
        }
        else if (canAbandon)
        {
            button(ctext(page, "abandon"), ServicePanelActionType::TradeContractAbandon, sel->id, Selected, cx);
            ry += bh + 8.0f;
        }
        else if (!mine && sel->status == "open")
        {
            t.textDrawPx(ctext(page, "take_hint"), cx, ry, small, Muted);
            ry += static_cast<float>(small) + 6.0f;
        }
        if (canTake && sel->depositCr > 0.0)
            t.textDrawPx(ctext(page, "no_deposit_hint"), cx, ry, small, Muted);
    }
    // ---- the server's answer to the last take / abandon
    std::string status = ctext(page, "keys");
    glm::vec4 col = Muted;
    for (const auto& r : s.results)
        if (page.contractRequest != 0 && r.requestId == page.contractRequest)
        {
            status = "#" + std::to_string(r.requestId) + "  " + (r.ok ? rd(r.message) : ctext(page, "refused") + " " + rd(r.message));
            col = r.ok ? Good : Bad;
        }
    if (page.contractRequest != 0 && status == ctext(page, "keys"))
    {
        status = ctext(page, "waiting");
        col = Warn;
    }
    t.textDrawPx(status, 24, h - 26, small, col);
}

} // namespace ui::presentation
