// The interstellar jump, client side (author 2026-11-08; rules navigation/rules/JumpRules.h, the
// server's part GameSimulationJump.cpp). The galaxy map's star menu: РАССЧИТАТЬ ПРЫЖОК (the checks
// the server repeats: distance, the 32 ly hop, the core, the charge, the charge-up time) -> START
// (only for the calculated star) -> the silent charge-up panel; the jump itself: a cartoon ripple.
#include "src/game/SpaceStateInternal.h"

#include <cmath>
#include <cstdio>

#include "src/game/navigation/rules/JumpRules.h"
#include "src/world/celestial/StarAtlasDatabase.h"

namespace
{
namespace jr = game::navigation::jump;

std::string minutes(double seconds)
{
    char b[32];
    const int s = static_cast<int>(std::max(0.0, seconds) + 0.5);
    std::snprintf(b, sizeof b, "%d:%02d", s / 60, s % 60);
    return b;
}
}

void SpaceState::handleJumpRequest(int kind, int systemId)
{
    if (!m_client)
        return;
    const auto& ships = m_client->world().ships();
    const auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;
    const auto& motion = it->second.transform.motion;
    if (kind == 3)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::RequestJump;
        command.index = -1;
        msg.payload = command;
        m_client->sendMessage(msg);
        return;
    }
    const auto* atlas = m_client->starAtlas();
    const auto* from = atlas ? atlas->findSystemSummary(motion.systemId) : nullptr;
    const auto* to = atlas ? atlas->findSystemSummary(systemId) : nullptr;
    if (!from || !to)
    {
        m_pilotNotice = "ПРЫЖОК: система не найдена";
        return;
    }
    const double d = glm::length(to->positionLy - from->positionLy);
    if (kind == 1)
    {
        const auto r = jr::check(d, motion.shipMassT, motion.jumpCoreRatingT, motion.jChargeT < 0.0 ? motion.jChargeCapacityT : motion.jChargeT);
        char b[256];
        if (r == jr::Refusal::None)
        {
            std::snprintf(b, sizeof b, "ПРЫЖОК %s: %.1f св. лет, заряд %.2f из %.2f т, зарядка %s - можно СТАРТ", to->name.c_str(), d,
                          jr::chargeT(d, motion.shipMassT), motion.jChargeT, minutes(jr::chargeUpSeconds(d)).c_str());
            m_jumpCalculatedSystemId = systemId;
        }
        else
        {
            std::snprintf(b, sizeof b, "ПРЫЖОК %s (%.1f св. лет): нельзя - %s", to->name.c_str(), d, jr::refusalText(r));
            m_jumpCalculatedSystemId = -1;
        }
        m_pilotNotice = b;
        std::cerr << "[Jump] calculated: " << b << std::endl;
        return;
    }
    if (kind == 2 && systemId == m_jumpCalculatedSystemId)
    {
        game::network::ClientMessage msg;
        ClientShipCommand command;
        command.type = ClientShipCommand::RequestJump;
        command.index = systemId;
        msg.payload = command;
        m_client->sendMessage(msg);
        m_jumpCalculatedSystemId = -1;
    }
}

void SpaceState::renderHudJump(const Viewport& viewport)
{
    if (!m_client || viewport.width <= 0 || viewport.height <= 0)
        return;
    const auto& ships = m_client->world().ships();
    const auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;
    const auto& motion = it->second.transform.motion;
    m_systemMapRenderer.setJumpState(m_jumpCalculatedSystemId, motion.jumpState == 1 || motion.jumpState == 2 ? motion.jumpTargetSystemId : -1);

    // events: a refusal / a stop, the jump itself
    const double now = glfwGetTime();
    if (!m_jumpSerialsKnown)
    {
        m_jumpSerialsKnown = true;
        m_jumpSeenSerial = motion.jumpSerial;
        m_jumpSeenRefusalSerial = motion.jumpRefusalSerial;
    }
    if (motion.jumpRefusalSerial != m_jumpSeenRefusalSerial)
    {
        m_jumpSeenRefusalSerial = motion.jumpRefusalSerial;
        if (motion.jumpRefusal != 0)
            m_pilotNotice = std::string("ПРЫЖОК: ") + jr::refusalText(static_cast<jr::Refusal>(motion.jumpRefusal));
    }
    if (motion.jumpSerial != m_jumpSeenSerial)
    {
        m_jumpSeenSerial = motion.jumpSerial;
        m_jumpRippleStart = now;
        m_pilotNotice = "ПРЫЖОК: выход в системе " + motion.jumpTargetName;
        std::cerr << "[Jump] client: jumped to " << motion.jumpTargetName << std::endl;
    }

    // DEV diagnosis for 5 s after a jump: what the client draws against (celestial snapshot, the
    // player's position against its bodies)
    {
        static double lastDiag = -1.0;
        static const bool diag = std::getenv("ELITE_DEV_JUMP_DIAG") != nullptr;
        const double since = now - m_jumpRippleStart;
        if (diag && since >= 0.0 && since < 6.0 && now - lastDiag >= 1.0)
        {
            lastDiag = now;
            const auto snap = m_client->celestialSnapshot();
            const glm::dvec3 p = it->second.transform.motion.travelFrame.valid
                ? it->second.transform.motion.travelFrame.localToWorldPosition(it->second.transform.motion.localPositionMeters)
                : world::coordinates::fullMeters(it->second.transform.worldPosition);
            std::cerr << "[JumpDiag] t+" << since << " ship system " << motion.systemId << " pos " << p.x << "," << p.y << "," << p.z
                      << " snapshot system " << (snap ? snap->systemId : -99) << " bodies " << (snap ? snap->bodies.size() : 0);
            if (snap)
                for (const auto& b : snap->bodies)
                    if (glm::length(b.worldMeters - p) < 1.0e9)
                        std::cerr << " | " << b.id << " r " << b.radiusKm << " km at " << glm::length(b.worldMeters - p) / 1000.0 << " km";
            std::cerr << std::endl;
        }
    }

    auto& text = TextRenderer::instance();
    text.beginFrameForViewport(viewport.width, viewport.height);
    const float W = static_cast<float>(viewport.width);
    const float H = static_cast<float>(viewport.height);

    // ---- the charge-up panel (under the flight banner)
    if (motion.jumpState == 1 || motion.jumpState == 2)
    {
        const double frac = motion.jumpChargeDurationS > 0.0 ? std::clamp(motion.jumpChargeElapsedS / motion.jumpChargeDurationS, 0.0, 1.0) : 1.0;
        char line[256];
        if (motion.jumpState == 1)
            std::snprintf(line, sizeof line, "ПРЫЖОК -> %s  ·  %.1f св. лет  ·  зарядка %s / %s  ·  заряд -%.2f т", motion.jumpTargetName.c_str(),
                          motion.jumpDistanceLy, minutes(motion.jumpChargeElapsedS).c_str(), minutes(motion.jumpChargeDurationS).c_str(),
                          motion.jumpCostT);
        else
            std::snprintf(line, sizeof line, "ПРЫЖОК -> %s  ·  ЗАРЯЖЕНО  ·  отойдите от объектов (20 км) и выше J-порога",
                          motion.jumpTargetName.c_str());
        const std::string hint = "отмена: галактическая карта -> звезда -> ОТМЕНИТЬ ПРЫЖОК";
        const float width = std::max(text.measureTextPx(line, 16), text.measureTextPx(hint, 13)) + 40.0f;
        const float x = std::max(8.0f, (W - width) * 0.5f);
        const float y = 160.0f;
        text.solidRectPx(x, y, width, 58.0f, glm::vec4(0.02f, 0.03f, 0.06f, 0.85f));
        text.solidRectPx(x, y, width, 2.0f, glm::vec4(0.55f, 0.75f, 1.0f, 0.95f));
        text.textDrawPx(line, x + 20.0f, y + 22.0f, 16,
                        motion.jumpState == 2 ? glm::vec4(1.0f, 0.75f, 0.45f, 1.0f) : glm::vec4(0.70f, 0.85f, 1.0f, 1.0f));
        text.solidRectPx(x + 20.0f, y + 32.0f, width - 40.0f, 6.0f, glm::vec4(0.15f, 0.20f, 0.30f, 0.9f));
        text.solidRectPx(x + 20.0f, y + 32.0f, (width - 40.0f) * static_cast<float>(frac), 6.0f, glm::vec4(0.55f, 0.80f, 1.0f, 0.95f));
        text.textDrawPx(hint, x + 20.0f, y + 52.0f, 13, glm::vec4(0.75f, 0.80f, 0.90f, 0.9f));
    }

    // ---- the jump: a cartoon ripple - a flash, then rings running out from the centre (author:
    // "мультяшная" - drawn, not physical), 1.6 s
    const double t = now - m_jumpRippleStart;
    if (t >= 0.0 && t < 1.6)
    {
        const float flash = static_cast<float>(std::max(0.0, 1.0 - t / 0.5));
        if (flash > 0.0f)
            text.solidRectPx(0.0f, 0.0f, W, H, glm::vec4(0.85f, 0.92f, 1.0f, 0.85f * flash));
        const glm::vec2 c(W * 0.5f, H * 0.5f);
        const float maxR = std::sqrt(W * W + H * H) * 0.5f;
        for (int ring = 0; ring < 4; ++ring)
        {
            const double rt = t - 0.12 * ring;
            if (rt <= 0.0)
                continue;
            const float r = static_cast<float>(rt / 1.2) * maxR;
            const float alpha = static_cast<float>(std::max(0.0, 1.0 - rt / 1.3));
            const int dots = 96 + ring * 24;
            const float wobble = 0.04f * r;
            for (int k = 0; k < dots; ++k)
            {
                const float a = 6.2831853f * static_cast<float>(k) / static_cast<float>(dots);
                const float rr = r + wobble * std::sin(6.0f * a + static_cast<float>(t) * 9.0f + ring);
                const float px = c.x + rr * std::cos(a);
                const float py = c.y + rr * std::sin(a);
                text.solidRectPx(px - 3.0f, py - 3.0f, 6.0f, 6.0f,
                                 ring % 2 == 0 ? glm::vec4(0.55f, 0.85f, 1.0f, alpha) : glm::vec4(1.0f, 1.0f, 1.0f, alpha * 0.8f));
            }
        }
    }
    text.endFrame();
}
