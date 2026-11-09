// The interstellar jump, client side (author 2026-11-08; rules navigation/rules/JumpRules.h, the
// server's part GameSimulationJump.cpp). The galaxy map's star menu: РАССЧИТАТЬ ПРЫЖОК (the checks
// the server repeats: distance, the 32 ly hop, the core, the charge, the charge-up time) -> START
// (only for the calculated star) -> START; the charge-up, the jump and the exit report are presented
// on the HUD terminal and the cockpit view (updateJumpPresentation below).
#include "src/game/SpaceStateInternal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "src/audio/AudioEngine.h"
#include "src/core/DevFrameCapture.h"

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
        if (m_jumpCalculatedSystemId == systemId && !systemHasRescueService(systemId))
            m_pilotNotice += " · ВНИМАНИЕ: в системе нет спасательной службы - SOS услышат только случайные корабли";
        std::cerr << "[Jump] calculated: " << m_pilotNotice << std::endl;
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

// ---- the jump's presentation (author 2026-11-09). Timeline:
//   charging (state 1): the terminal prints the procedure and a font progress bar of the core's
//     charge; the hum rises (AudioEngine::setJumpHum - sounds are off for now); the cockpit shakes
//     along the screen's x / y, the amplitude growing with the charge; from ~60 % the view swells
//     like a fish-eye lens; charged and waiting (state 2) it holds there;
//   the jump (jumpSerial changes; the server's jump is instant): the view is flung forward - the
//     cockpit's edges stay put and its centre is drawn away like rubber (author 2026-11-09: not the
//     whole view shrinking) in 0.3 s; the old sky goes dark and every bright star sends a ray from
//     its old place to its place in the new sky; the ray reaches it and collapses into the new
//     point (0.2..1.8 s); the new sky comes up; the view springs back with 2-3 damped fish-eye
//     swings (1.9..3.3 s);
//   the exit (jumpExitSerial): the terminal's report - the system, the exit orbit, the coordinates
//     and how far from the calculated point the ship came out (the planned 3 sigma box; outside it
//     a warning: the ship was dragged off).
namespace
{
constexpr double FlingS = 0.14;   // sharp (author 2026-11-09)
constexpr double RaysStartS = 0.20;
constexpr double RaysS = 1.60;
constexpr double ReturnStartS = 1.90;
constexpr double ReturnS = 0.90;   // the springing back: a sharp snap and 2-3 damped swings
constexpr double FxEndS = ReturnStartS + ReturnS;
constexpr std::size_t MaxRays = 220;
constexpr double AuM = 1.495978707e11;

float smooth01(double x)
{
    const double t = std::clamp(x, 0.0, 1.0);
    return static_cast<float>(t * t * (3.0 - 2.0 * t));
}
float easeOutCubic(double x)
{
    const double t = 1.0 - std::clamp(x, 0.0, 1.0);
    return static_cast<float>(1.0 - t * t * t);
}
float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

// the cockpit's state at a charge fraction: the shake (px at 1080 lines), the swell
struct ChargeLook
{
    float shakePx = 0.0f;
    float zoom = 1.0f;
    float bulge = 0.0f;
};
ChargeLook chargeLook(double frac, bool waiting)
{
    ChargeLook c;
    const double f = std::clamp(frac, 0.0, 1.0);
    c.shakePx = static_cast<float>((0.6 + 5.4 * f * f) * (waiting ? 0.75 : 1.0));
    const float swell = smooth01((f - 0.6) / 0.4);
    // half the first try's lens (author 2026-11-09: the cockpit must feel whole, not a screen
    // under a fish-eye filter)
    // second cut (author 2026-11-09: "it bends to the centre too much - a picture, not a cockpit")
    c.zoom = 1.0f + 0.015f * swell;
    c.bulge = 0.06f * swell;
    return c;
}

glm::vec2 shakeAt(double t, float amplitude)
{
    constexpr double Tau = 6.283185307179586;
    const double x = 0.55 * std::sin(Tau * 13.1 * t) + 0.30 * std::sin(Tau * 23.7 * t + 1.3) + 0.15 * std::sin(Tau * 41.3 * t + 2.1);
    const double y = 0.55 * std::sin(Tau * 11.3 * t + 0.7) + 0.30 * std::sin(Tau * 27.9 * t + 2.4) + 0.15 * std::sin(Tau * 37.1 * t + 0.2);
    return glm::vec2(static_cast<float>(x), static_cast<float>(y)) * amplitude;
}

std::string bar(double frac, int cells)
{
    const int full = static_cast<int>(std::clamp(frac, 0.0, 1.0) * cells + 1.0e-9);
    return "[" + std::string(static_cast<std::size_t>(full), '#') + std::string(static_cast<std::size_t>(cells - full), '-') + "]";
}

std::string distanceText(double meters)
{
    char b[64];
    const double km = meters / 1000.0;
    if (km < 10.0)
        std::snprintf(b, sizeof b, "%.1f км", km);
    else if (meters < 0.01 * AuM)
    {
        // 1 234 567 км
        std::string digits = std::to_string(static_cast<long long>(km + 0.5));
        std::string out;
        for (std::size_t i = 0; i < digits.size(); ++i)
        {
            if (i > 0 && (digits.size() - i) % 3 == 0)
                out += ' ';
            out += digits[i];
        }
        std::snprintf(b, sizeof b, "%s км", out.c_str());
    }
    else
        std::snprintf(b, sizeof b, "%.3f а.е.", meters / AuM);
    return b;
}
} // namespace

void SpaceState::addJumpTerminalLine(const std::string& text, bool warning)
{
    m_jumpTerminalLines.emplace_back(text, warning ? game::presentation::DockingTerminalSeverity::Warning
                                                   : game::presentation::DockingTerminalSeverity::Info);
    while (m_jumpTerminalLines.size() > 12)
        m_jumpTerminalLines.erase(m_jumpTerminalLines.begin());
}

void SpaceState::updateJumpPresentation()
{
    m_jumpScreen = render::JumpScreenParams {};
    m_jumpRays01 = -1.0f;
    m_jumpTerminalLive.clear();
    m_sceneRenderer.setStarfieldBrightness(1.0f);
    audio::AudioEngine::instance().setJumpHum(0.0f);
    if (!m_client)
        return;
    const auto& ships = m_client->world().ships();
    const auto it = ships.find(m_playerId.value);
    if (it == ships.end())
        return;
    const auto& motion = it->second.transform.motion;
    const double now = glfwGetTime();

    glm::dvec3 observer(0.0);
    const bool observerValid = resolvePlayerGalacticPositionLy(observer);
    if (observerValid)
    {
        m_jumpObserverPrevLy = m_jumpObserverValid ? m_jumpObserverLy : observer;
        m_jumpObserverLy = observer;
    }
    m_jumpObserverValid = observerValid;

    // ---- events
    if (!m_jumpSerialsKnown)
    {
        m_jumpSerialsKnown = true;
        m_jumpSeenSerial = motion.jumpSerial;
        m_jumpSeenRefusalSerial = motion.jumpRefusalSerial;
        m_jumpSeenExitSerial = motion.jumpExitSerial;
        m_jumpSeenState = motion.jumpState;
    }
    if (motion.jumpRefusalSerial != m_jumpSeenRefusalSerial)
    {
        m_jumpSeenRefusalSerial = motion.jumpRefusalSerial;
        if (motion.jumpRefusal != 0 && static_cast<jr::Refusal>(motion.jumpRefusal) != jr::Refusal::NearObjects)
            m_pilotNotice = std::string("ПРЫЖОК: ") + jr::refusalText(static_cast<jr::Refusal>(motion.jumpRefusal));
    }
    if (motion.jumpState != m_jumpSeenState)
    {
        const std::uint8_t was = m_jumpSeenState;
        m_jumpSeenState = motion.jumpState;
        char b[256];
        if (motion.jumpState == 1 && was != 1)
        {
            m_jumpTerminalLines.clear();
            std::snprintf(b, sizeof b, "ПРЫЖОК: начата процедура - цель %s, %.1f св. лет", motion.jumpTargetName.c_str(), motion.jumpDistanceLy);
            addJumpTerminalLine(b);
            std::snprintf(b, sizeof b, "ЯДРО: зарядка %s, расход заряда %.2f т", minutes(motion.jumpChargeDurationS).c_str(), motion.jumpCostT);
            addJumpTerminalLine(b);
            addJumpTerminalLine("отмена: карта галактики -> звезда -> ОТМЕНИТЬ ПРЫЖОК");
        }
        else if (motion.jumpState == 2)
            addJumpTerminalLine("ЯДРО ЗАРЯЖЕНО: прыжок ждёт - отойдите от объектов (20 км) и выше J-порога", true);
        // 1/2 -> 0 with no jump made (a jump can pass 3 between two snapshots: then the serial moved)
        else if (motion.jumpState == 0 && (was == 1 || was == 2) && motion.jumpSerial == m_jumpSeenSerial)
            addJumpTerminalLine(std::string("ПРЫЖОК ПРЕРВАН: ") + jr::refusalText(static_cast<jr::Refusal>(motion.jumpRefusal ? motion.jumpRefusal : 8)), true);
    }
    if (motion.jumpSerial != m_jumpSeenSerial)
    {
        m_jumpSeenSerial = motion.jumpSerial;
        m_jumpFxStart = now;
        addJumpTerminalLine("ПРЫЖОК: переход в систему " + motion.jumpTargetName);
        std::cerr << "[Jump] client: jumped to " << motion.jumpTargetName << std::endl;
        // the rays: the brightest stars of either sky, their directions from the old and the new place
        m_jumpRays.clear();
        const glm::dvec3 from = m_jumpObserverPrevLy;
        const glm::dvec3 to = m_jumpObserverLy;
        if (m_jumpObserverValid && glm::length(to - from) > 1.0e-3)
        {
            struct Pick
            {
                float mag;
                JumpStarRay ray;
            };
            std::vector<Pick> picks;
            for (const auto& s : m_sceneRenderer.starfield().getRealStars())
            {
                if (s.isGameSystem && !s.isAstronomicalCatalogStar)
                    continue;
                const glm::dvec3 p(s.positionLy);
                const double d0 = glm::length(p - from);
                const double d1 = glm::length(p - to);
                if (d0 < 0.05 || d1 < 0.05)
                    continue;
                // apparent magnitude m = M + 5 log10(d_pc) - 5
                const auto mag = [&](double dLy) { return s.absoluteMagnitude + 5.0 * std::log10(dLy / 3.26156) - 5.0; };
                const float m = static_cast<float>(std::min(mag(d0), mag(d1)));
                Pick pk;
                pk.mag = m;
                pk.ray.from = glm::vec3((p - from) / d0);
                pk.ray.to = glm::vec3((p - to) / d1);
                pk.ray.color = s.color;
                pk.ray.weight = std::clamp(1.0f - (m + 1.5f) / 7.5f, 0.15f, 1.0f);
                picks.push_back(pk);
            }
            if (picks.size() > MaxRays)
            {
                std::nth_element(picks.begin(), picks.begin() + MaxRays, picks.end(), [](const Pick& a, const Pick& b) { return a.mag < b.mag; });
                picks.resize(MaxRays);
            }
            for (const auto& pk : picks)
                m_jumpRays.push_back(pk.ray);
        }
        // DEV: ELITE_DEV_JUMP_FX_CAPTURE=<dir> - frames at fixed times after the jump (the effect's review)
        if (const char* dir = std::getenv("ELITE_DEV_JUMP_FX_CAPTURE"); dir && *dir)
            for (const double dt : {0.10, 0.30, 0.60, 0.90, 1.20, 1.50, 1.80, 2.05, 2.30, 2.60, 3.00, 6.00})
            {
                char name[64];
                std::snprintf(name, sizeof name, "/jfx_%04d.png", static_cast<int>(dt * 1000.0 + 0.5));
                core::dev::frameCaptureRequests().push_back({now + dt, std::string(dir) + name, false});
            }
        std::cerr << "[JumpFx] fling (t=" << now << "): " << m_jumpRays.size() << " star rays, observer moved " << glm::length(to - from) << " ly" << std::endl;
    }
    if (motion.jumpExitSerial != m_jumpSeenExitSerial)
    {
        m_jumpSeenExitSerial = motion.jumpExitSerial;
        const auto* snap = m_client->celestialSnapshot();
        std::string body = motion.jumpExitBodyName;
        glm::dvec3 star(0.0);
        if (snap && snap->systemId == motion.systemId)
            for (const auto& b : snap->bodies)
            {
                if (b.id == motion.jumpExitBodyName && !b.name.empty())
                    body = b.name;
                if (b.type == world::celestial::BodyType::Star && b.parentId.empty())
                    star = b.worldMeters;
            }
        const glm::dvec3 p = (world::coordinates::fullMeters(it->second.transform.worldPosition) - star) / AuM;
        char b[256];
        addJumpTerminalLine("ПРЫЖОК ЗАВЕРШЁН: система " + motion.jumpTargetName);
        const bool atStar = motion.jumpExitAltitudeM > 0.5 * AuM;
        if (atStar)
            addJumpTerminalLine("ВЫХОД: открытый космос, " + distanceText(motion.jumpExitAltitudeM) + " от звезды");
        else
            addJumpTerminalLine("ВЫХОД: орбита " + body + ", высота " + distanceText(motion.jumpExitAltitudeM));
        std::snprintf(b, sizeof b, "КООРДИНАТЫ: %+.4f  %+.4f  %+.4f а.е. от звезды", p.x, p.y, p.z);
        addJumpTerminalLine(b);
        const double box = jr::ExitFarOffSigmas * motion.jumpExitSigmaM;
        if (motion.jumpExitDeviationM > box)
            addJumpTerminalLine("ВНИМАНИЕ: корабль снесло - " + distanceText(motion.jumpExitDeviationM) + " от расчётной точки выхода (норма до " +
                                    distanceText(box) + ")",
                                true);
        else
            addJumpTerminalLine("ОТКЛОНЕНИЕ: " + distanceText(motion.jumpExitDeviationM) + " от расчётной точки (в пределах расчётных " +
                                distanceText(box) + ")");
    }

    // ---- the frame of the effect
    const float px = static_cast<float>(std::max(1, context().viewport().height)) / 1080.0f;
    const double tj = now - m_jumpFxStart;
    if (motion.jumpState == 1 || motion.jumpState == 2)
    {
        const double frac = motion.jumpState == 2 ? 1.0
                            : motion.jumpChargeDurationS > 0.0 ? std::clamp(motion.jumpChargeElapsedS / motion.jumpChargeDurationS, 0.0, 1.0)
                                                               : 1.0;
        const ChargeLook c = chargeLook(frac, motion.jumpState == 2);
        m_jumpScreen.shakePx = shakeAt(now, c.shakePx * px);
        m_jumpScreen.zoom = c.zoom;
        m_jumpScreen.bulge = c.bulge;
        audio::AudioEngine::instance().setJumpHum(static_cast<float>(0.12 + 0.88 * std::pow(frac, 1.5)));
        char b[160];
        if (motion.jumpState == 1)
            std::snprintf(b, sizeof b, "ЗАРЯД ЯДРА %s %3d%%  %s / %s", bar(frac, 16).c_str(), static_cast<int>(frac * 100.0),
                          minutes(motion.jumpChargeElapsedS).c_str(), minutes(motion.jumpChargeDurationS).c_str());
        else
            std::snprintf(b, sizeof b, "ЗАРЯД ЯДРА %s 100%%  ГОТОВ - ждём чистого пространства", bar(1.0, 16).c_str());
        m_jumpTerminalLive = b;
    }
    else if (motion.jumpState == 3)
        m_jumpTerminalLive = "ВЫХОД: определяем положение...";

    if (tj >= 0.0 && tj < FxEndS)
    {
        const ChargeLook peak = chargeLook(1.0, false);
        if (tj < FlingS)
        {
            const float e = easeOutCubic(tj / FlingS);
            m_jumpScreen.shakePx = shakeAt(now, peak.shakePx * px * (1.0f - e));
            m_jumpScreen.zoom = mixf(peak.zoom, 1.0f, e);
            m_jumpScreen.bulge = mixf(peak.bulge, -0.06f, e);
            m_jumpScreen.vignette = 0.35f * e;
        }
        else if (tj < ReturnStartS)
        {
            const float e = static_cast<float>((tj - FlingS) / (ReturnStartS - FlingS));
            m_jumpScreen.shakePx = glm::vec2(0.0f);
            m_jumpScreen.zoom = 1.0f;
            m_jumpScreen.bulge = mixf(-0.06f, -0.05f, e);
            m_jumpScreen.vignette = 0.35f;
        }
        else
        {
            // damped swings of the lens about neutral: ~2.5 periods, the last ones barely seen
            const double u = (tj - ReturnStartS) / ReturnS;
            constexpr double Tau = 6.283185307179586;
            const double swing = -0.05 * std::exp(-4.0 * u) * std::cos(Tau * 2.5 * u);
            m_jumpScreen.shakePx = glm::vec2(0.0f);
            m_jumpScreen.zoom = 1.0f;
            m_jumpScreen.bulge = static_cast<float>(swing) * (1.0f - smooth01((u - 0.9) / 0.1));
            m_jumpScreen.vignette = 0.35f * (1.0f - smooth01(u / 0.4));
        }
        // the old sky is gone at once; the new one comes up as the rays collapse into it
        m_sceneRenderer.setStarfieldBrightness(smooth01((tj - (RaysStartS + 0.6 * RaysS)) / (0.4 * RaysS)));
        if (tj >= RaysStartS && tj < RaysStartS + RaysS)
            m_jumpRays01 = static_cast<float>((tj - RaysStartS) / RaysS);
        audio::AudioEngine::instance().setJumpHum(static_cast<float>(std::max(0.0, 1.0 - tj / 1.2)));
    }
}

void SpaceState::renderJumpStarRays(const Viewport& vp)
{
    if (m_jumpRays01 < 0.0f || m_jumpRays.empty() || !m_activeMainCamera || vp.width <= 0 || vp.height <= 0)
        return;
    const glm::mat3 view = glm::mat3(m_activeMainCamera->viewMatrix());
    const glm::mat4 proj = m_activeMainCamera->projectionMatrix();
    const float W = static_cast<float>(vp.width);
    const float H = static_cast<float>(vp.height);
    const auto toScreen = [&](const glm::vec3& dir, glm::vec2& out) {
        const glm::vec4 clip = proj * glm::vec4(view * dir, 0.0f);
        if (clip.w <= 1.0e-4f)
            return false;
        out = glm::vec2((clip.x / clip.w * 0.5f + 0.5f) * W, (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * H);
        return out.x > -0.5f * W && out.x < 1.5f * W && out.y > -0.5f * H && out.y < 1.5f * H;
    };
    // the ray's head runs from the old point to the new one (0..0.55), its tail follows (0.45..1).
    // The path is a curve that first leaves the screen centre (author 2026-11-09: straight rays
    // converging on the centre read as a jump BACKWARD): every star starts streaming outward, as
    // when rushing forward, and then bends into its place in the new sky.
    const double u = m_jumpRays01;
    const float head = smooth01(u / 0.55);
    const float tail = smooth01((u - 0.45) / 0.55);
    const float fade = 1.0f - smooth01((u - 0.85) / 0.15);
    const float px = H / 1080.0f;
    m_jumpRayBatch.begin(vp.width, vp.height);
    for (const auto& r : m_jumpRays)
    {
        glm::vec2 a, b;
        if (!toScreen(r.from, a) || !toScreen(r.to, b))
            continue;
        const glm::vec2 centre(0.5f * W, 0.5f * H);
        glm::vec2 out = a - centre;
        out = glm::length(out) > 1.0f ? glm::normalize(out) : glm::vec2(1.0f, 0.0f);
        const glm::vec2 ctrl = a + out * (0.6f * glm::length(b - a) + 0.10f * H);
        const auto curve = [&](float s) {
            const float q = 1.0f - s;
            return q * q * a + 2.0f * q * s * ctrl + s * s * b;
        };
        const glm::vec2 h = curve(head);
        const glm::vec4 c(glm::mix(r.color, glm::vec3(0.75f, 0.88f, 1.0f), 0.35f), 1.0f);
        const float w = (0.8f + 1.8f * r.weight) * px;
        // a streak brighter toward its head
        constexpr int Pieces = 8;
        for (int k = 0; k < Pieces; ++k)
        {
            const float s0 = static_cast<float>(k) / Pieces;
            const float s1 = static_cast<float>(k + 1) / Pieces;
            glm::vec4 ck = c;
            ck.a = (0.15f + 0.75f * s1) * r.weight * fade;
            m_jumpRayBatch.line(curve(tail + (head - tail) * s0), curve(tail + (head - tail) * s1), w, ck);
        }
        // the head: the star itself
        glm::vec4 hc = c;
        hc.a = std::min(1.0f, 0.4f + r.weight) * fade;
        const float hs = w * 1.6f;
        m_jumpRayBatch.filledTriangle(h + glm::vec2(-hs, -hs), h + glm::vec2(hs, -hs), h + glm::vec2(hs, hs), hc);
        m_jumpRayBatch.filledTriangle(h + glm::vec2(-hs, -hs), h + glm::vec2(hs, hs), h + glm::vec2(-hs, hs), hc);
    }
    m_jumpRayBatch.flush();
}

void SpaceState::applyJumpScreenEffect()
{
    m_jumpScreenEffect.apply(m_jumpScreen);
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

    // DEV diagnosis for 5 s after a jump: what the client draws against (celestial snapshot, the
    // player's position against its bodies)
    const double now = glfwGetTime();
    static double lastDiag = -1.0;
    static const bool diag = std::getenv("ELITE_DEV_JUMP_DIAG") != nullptr;
    const double since = now - m_jumpFxStart;
    if (diag && since >= 0.0 && since < 6.0 && now - lastDiag >= 1.0)
    {
        lastDiag = now;
        const auto snap = m_client->celestialSnapshot();
        const glm::dvec3 p = motion.travelFrame.valid ? motion.travelFrame.localToWorldPosition(motion.localPositionMeters)
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
