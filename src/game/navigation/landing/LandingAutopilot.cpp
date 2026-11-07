#include "src/game/navigation/landing/LandingAutopilot.h"

#include <algorithm>
#include <cmath>

namespace game::navigation::landing
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
constexpr int TerrainWater = 100;
constexpr int TerrainLava = 9;
constexpr double CruiseHeightM = 2000.0;      // minimum transit height over the ground (more when fast)
constexpr double TransitAccelMps2 = 10.0;     // horizontal braking profile
constexpr double TransitSpeedCapMps = 1500.0;
constexpr double AuthorityMps2 = 40.0;        // the lander's J drive authority
constexpr double DescentStartM = 10.0;        // horizontal distance to start the descent
constexpr double HoverSeconds = 4.0;          // the hover over the spot before the vertical descent

void tangentBasis(const glm::dvec3& n, glm::dvec3& t1, glm::dvec3& t2)
{
    t1 = glm::normalize(glm::cross(n, std::abs(n.y) < 0.9 ? glm::dvec3(0, 1, 0) : glm::dvec3(1, 0, 0)));
    t2 = glm::cross(n, t1);
}

// The hull footprint at a body-fixed direction: centre + 8 points on a circle
// of the hull's reach. false = terrain unknown there (still loading).
bool evaluate(const glm::dvec3& dir, double radiusM, double footprintM, const TerrainProbe& probe, LandingSite& out)
{
    TerrainPoint centre;
    if (!probe(dir, centre))
        return false;
    glm::dvec3 t1, t2;
    tangentBasis(dir, t1, t2);
    double lo = centre.heightM, hi = centre.heightM;
    bool wet = centre.material == TerrainWater || centre.material == TerrainLava;
    for (int k = 0; k < 8; ++k)
    {
        const double a = k * Pi / 4.0;
        const glm::dvec3 p = glm::normalize(dir * radiusM + (t1 * std::cos(a) + t2 * std::sin(a)) * footprintM);
        TerrainPoint s;
        if (!probe(p, s))
            return false;
        lo = std::min(lo, s.heightM);
        hi = std::max(hi, s.heightM);
        wet = wet || s.material == TerrainWater || s.material == TerrainLava;
    }
    out.valid = true;
    out.bodyDirection = dir;
    out.heightM = hi;
    out.roughnessM = hi - lo;
    out.slopeDeg = std::max(centre.slopeDeg, std::atan((hi - lo) / (2.0 * footprintM)) * 180.0 / Pi);
    out.material = wet ? TerrainWater : centre.material;
    return true;
}

double footprintOf(const glm::dvec3& hull)
{
    return std::max(hull.x, hull.z) * 1.3 + 5.0;
}
} // namespace

LandingSite planWildSite(const glm::dvec3& startBodyDirection, double bodyRadiusM, const glm::dvec3& hullHalfExtentsM,
                         const LandingRequest& request, const TerrainProbe& probe, bool* searched)
{
    if (searched)
        *searched = false;
    const glm::dvec3 start = glm::normalize(startBodyDirection);
    const double footprint = footprintOf(hullHalfExtentsM);
    const double step = std::max(150.0, footprint * 2.0);
    glm::dvec3 t1, t2;
    tangentBasis(start, t1, t2);
    for (int ring = 0; ring * step <= request.searchRadiusM; ++ring)
    {
        const double r = ring * step;
        const int count = ring == 0 ? 1 : static_cast<int>(std::ceil(2.0 * Pi * r / step));
        for (int k = 0; k < count; ++k)
        {
            const double a = 2.0 * Pi * k / count;
            const glm::dvec3 dir = glm::normalize(start * bodyRadiusM + (t1 * std::cos(a) + t2 * std::sin(a)) * r);
            LandingSite c;
            if (!evaluate(dir, bodyRadiusM, footprint, probe, c))
            {
                if (ring == 0)
                    return LandingSite {};   // the field is not loaded yet: plan again later
                continue;
            }
            c.offsetM = r;
            if (c.material == TerrainWater)
                continue;
            if (c.slopeDeg <= request.maxSlopeDeg)
            {
                if (searched)
                    *searched = true;
                return c;                    // the nearest flat spot
            }
        }
    }
    if (searched)
        *searched = true;
    return LandingSite {};                   // nothing flat enough here: do not land
}

LandingSite checkPadSite(const glm::dvec3& padBodyDirection, double bodyRadiusM, const glm::dvec3& hullHalfExtentsM,
                         const TerrainProbe& probe)
{
    LandingSite site;
    if (!evaluate(glm::normalize(padBodyDirection), bodyRadiusM, footprintOf(hullHalfExtentsM), probe, site))
        return LandingSite {};
    return site;
}

void LandingAutopilot::start(const LandingRequest& request, const glm::dvec3& startBodyDirection)
{
    m_request = request;
    m_start = glm::normalize(startBodyDirection);
    m_site = LandingSite {};
    m_phase = LandingPhase::Planning;
    m_status = "planning";
    m_heading = glm::dvec3(0.0);
    m_replanSeconds = 0.0;
    m_searchPasses = 0;
    m_haveLastSiteTop = false;
    m_hoverLeftS = 0.0;
    m_transitCapMps = -1.0;
}

bool LandingAutopilot::planAhead(const glm::dvec3& startBodyDirection, double bodyRadiusM,
                                 const glm::dvec3& hullHalfExtentsM, const TerrainProbe& probe)
{
    if (m_site.valid)
        return true;
    m_start = glm::normalize(startBodyDirection);
    bool searched = false;
    m_site = m_request.mode == LandingMode::Pad
        ? checkPadSite(m_request.padBodyDirection, bodyRadiusM, hullHalfExtentsM, probe)
        : planWildSite(m_start, bodyRadiusM, hullHalfExtentsM, m_request, probe, &searched);
    if (m_site.valid)
    {
        m_phase = LandingPhase::Transit;
        m_status = "site chosen ahead";
    }
    else if (searched)
        m_status = "no flat site near the approach point";
    return m_site.valid;
}

LandingCommand LandingAutopilot::step(const LandingInput& in, const TerrainProbe& probe)
{
    LandingCommand cmd;
    const glm::dvec3 rel = in.shipPositionM - in.bodyCentreM;
    const double rLen = std::max(1.0, glm::length(rel));
    const glm::dvec3 up = rel / rLen;
    // velocity against the body centre (orbit) - and against the site itself once
    // it is chosen: the site turns with the body (the Moon's surface moves ~4 m/s),
    // so "over the spot and still" is measured against the spot
    glm::dvec3 w = in.shipVelocityMps - in.bodyVelocityMps;
    const double agl = rLen - in.bodyRadiusM - in.groundUnderShipM;
    cmd.heightAboveGroundM = agl;
    cmd.upWorld = up;

    m_replanSeconds -= in.dt;
    if ((m_phase == LandingPhase::Planning || m_phase == LandingPhase::Orbit) && m_replanSeconds <= 0.0)
    {
        // under the ship now (the ground track while orbiting), wider each pass
        m_replanSeconds = 3.0;
        const glm::dvec3 under = glm::transpose(in.worldFromBody) * up;
        LandingRequest wider = m_request;
        wider.searchRadiusM = m_request.searchRadiusM * (1.0 + 0.5 * std::min(m_searchPasses, 6));
        bool searched = false;
        m_site = m_request.mode == LandingMode::Pad
            ? checkPadSite(m_request.padBodyDirection, in.bodyRadiusM, in.hullHalfExtentsM, probe)
            : planWildSite(m_phase == LandingPhase::Planning ? m_start : under, in.bodyRadiusM, in.hullHalfExtentsM,
                           wider, probe, &searched);
        if (m_site.valid)
        {
            m_phase = LandingPhase::Transit;
            m_status = "site";
        }
        else if (searched)
        {
            ++m_searchPasses;
            m_phase = LandingPhase::Orbit;
            m_status = "no flat site: orbiting, searching the ground track";
        }
        else
            m_status = "waiting for the terrain";
    }

    glm::dvec3 wanted = glm::dvec3(0.0);   // hover while the terrain loads
    if (m_phase == LandingPhase::Orbit && in.bodyMuM3s2 > 0.0)
    {
        // a circular orbit at least 10 km over the ground: keep the horizontal
        // direction, circular speed, climb / sink to the height
        const glm::dvec3 wh = w - up * glm::dot(w, up);
        const glm::dvec3 along = glm::length(wh) > 1.0
            ? glm::normalize(wh)
            : glm::normalize(glm::cross(up, std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0)));
        const double orbitR = in.bodyRadiusM + in.groundUnderShipM + std::max(10000.0, agl);
        const double vCirc = std::sqrt(in.bodyMuM3s2 / rLen);
        const double vz = std::clamp((orbitR - rLen) / 10.0, -100.0, 100.0);
        wanted = along * vCirc + up * vz;
    }

    // braking profiles inside the real authority: half of what is left after gravity
    const double authority = in.authorityMps2 > 0.0 ? in.authorityMps2 : AuthorityMps2;
    const double gSurface = in.bodyMuM3s2 > 0.0 && rLen > 0.0 ? in.bodyMuM3s2 / (rLen * rLen) : 0.0;
    const double net = std::max(0.3, authority - gSurface);
    const double transitAccel = std::min(TransitAccelMps2, 0.5 * net);
    const double descentBrake = std::min(15.0, 0.5 * net);

    glm::dvec3 toSiteH(0.0);
    double dh = 0.0;
    if (m_site.valid && (m_phase == LandingPhase::Transit || m_phase == LandingPhase::Descent))
    {
        const glm::dvec3 siteDir = glm::normalize(in.worldFromBody * m_site.bodyDirection);
            const double halfHeight = in.touchdownHalfHeightM > 0.0 ? in.touchdownHalfHeightM : in.hullHalfExtentsM.y;
        const glm::dvec3 siteTop = in.bodyCentreM + siteDir * (in.bodyRadiusM + m_site.heightM + halfHeight + 0.5);
        if (m_haveLastSiteTop && in.dt > 0.0)
            w = in.shipVelocityMps - (siteTop - m_lastSiteTop) / in.dt;
        m_lastSiteTop = siteTop;
        m_haveLastSiteTop = true;
        const glm::dvec3 toSite = siteTop - in.shipPositionM;
        toSiteH = toSite - up * glm::dot(toSite, up);
        dh = glm::length(toSiteH);
        const double aboveSite = -glm::dot(toSite, up);   // hull bottom over the site's touchdown height
        if (m_phase == LandingPhase::Transit)
        {
            // the faster, the higher over the relief (10 s of flight): a short hop low,
            // a long one high
            const glm::dvec3 whNow = w - up * glm::dot(w, up);
            // entered fast (a glide hand-over at ~450 m/s): never faster than that - the approach
            // only slows down (live q4: it re-accelerated 403 -> 500 m/s and braked again)
            if (m_transitCapMps < 0.0)
                m_transitCapMps = glm::length(whNow) > 100.0 ? glm::length(whNow) : TransitSpeedCapMps;
            const double vh = std::min({m_transitCapMps, std::sqrt(2.0 * transitAccel * dh), dh / 4.0});
            // a glide path (~8.5 deg) toward the site: the ship comes down ALONG the surface and
            // reaches the site low and slow; far away the cruise height over the relief holds
            const double cruise = std::min(std::max(CruiseHeightM, glm::length(whNow) * 10.0), std::max(30.0, 0.15 * dh));
            const double vz = std::clamp((cruise - agl) / 5.0, -150.0, 150.0);
            wanted = (dh > 1.0e-3 ? toSiteH / dh * vh : glm::dvec3(0.0)) + up * vz;
            if (dh < DescentStartM && glm::length(whNow) < 2.0)
            {
                m_phase = LandingPhase::Descent;
                m_status = "descent";
                m_hoverLeftS = HoverSeconds;
            }
        }
        if (m_phase == LandingPhase::Descent)
        {
            // over the site: the lower of the site height and the ground under the hull
            const double h = std::max(0.0, std::min(aboveSite, agl));
            double down = std::max(1.5, std::min({400.0, std::sqrt(2.0 * descentBrake * h), h / 4.0}));
            glm::dvec3 hold = toSiteH / 2.0;
            if (glm::length(hold) > 20.0)
                hold *= 20.0 / glm::length(hold);
            // the last 50 m only straight down over the spot: hover while still off it
            if (h < 50.0 && dh > 3.0)
                down = 0.0;
            // arrived over the spot: a hover first, then straight down (author 2026-11-07: "в самом
            // финале - зависание над площадкой и вертикальная посадка")
            if (m_hoverLeftS > 0.0)
            {
                m_hoverLeftS -= in.dt;
                down = 0.0;
                m_status = "hover";
            }
            else if (m_status == "hover")
                m_status = "descent";
            wanted = hold - up * down;
        }
    }

    glm::dvec3 dv = wanted - w;
    cmd.velocityErrorMps = dv;
    const double maxDv = authority * std::max(in.dt, 1.0e-6);
    if (glm::length(dv) > maxDv)
        dv *= maxDv / glm::length(dv);
    cmd.deltaVelocityMps = dv;
    cmd.accelerationMps2 = in.dt > 0.0 ? glm::length(dv) / in.dt : 0.0;
    cmd.distanceToSiteM = dh;

    // level attitude: nose along the way to the site while travelling (or along the
    // orbit), else held
    glm::dvec3 heading = dh > 50.0 && m_phase == LandingPhase::Transit ? toSiteH
        : m_phase == LandingPhase::Orbit ? (w - up * glm::dot(w, up))
        : in.shipForward - up * glm::dot(in.shipForward, up);
    if (glm::length(heading) < 1.0e-6)
        heading = glm::length(m_heading) > 0.5
            ? m_heading
            : glm::cross(up, std::abs(up.z) < 0.9 ? glm::dvec3(0, 0, 1) : glm::dvec3(1, 0, 0));
    m_heading = glm::normalize(heading - up * glm::dot(heading, up));
    cmd.noseWorld = m_heading;
    return cmd;
}
} // namespace game::navigation::landing
