#include "src/game/traffic/DockServiceTraffic.h"
#include "src/game/catalog/ships/ShipCargoPassports.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>

#include <glm/gtc/quaternion.hpp>

namespace game::traffic::dock
{
namespace
{

namespace hex = game::docking::hex;
using game::docking::traffic::DockTrafficPhase;

constexpr double Infinity = std::numeric_limits<double>::infinity();

// ---- deterministic randomness (splitmix64) --------------------------------
struct Rng
{
    std::uint64_t state;
    std::uint64_t next()
    {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    double uniform() { return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0); }
};

double smoothstep01(double x)
{
    x = std::clamp(x, 0.0, 1.0);
    return x * x * (3.0 - 2.0 * x);
}

glm::dmat3 slerpBasis(const glm::dmat3& a, const glm::dmat3& b, double w)
{
    const glm::dquat qa = glm::quat_cast(a);
    glm::dquat qb = glm::quat_cast(b);
    if (glm::dot(qa, qb) < 0.0) qb = -qb;
    return glm::mat3_cast(glm::slerp(qa, qb, std::clamp(w, 0.0, 1.0)));
}

// ---- 1D trapezoid profile -------------------------------------------------
struct Profile
{
    double length = 0.0, v0 = 0.0, v1 = 0.0, a = 1.0, vp = 0.0;
    double t1 = 0.0, t2 = 0.0, total = 0.0;

    // minCoast: keeps a coast phase this long (room for a flip).
    static Profile make(double length, double v0, double v1, double a, double vmax, double minCoast = 0.0)
    {
        Profile p;
        p.length = std::max(length, 1.0e-6);
        p.v0 = v0;
        p.v1 = v1;
        p.a = std::max(a, 1.0e-6);
        double vp = std::sqrt(std::max(0.0, (2.0 * p.a * p.length + v0 * v0 + v1 * v1) * 0.5));
        if (minCoast > 0.0)
        {
            // L = (vp^2 - v0^2)/2a + vp*tc + (vp^2 - v1^2)/2a
            const double A = 1.0 / p.a, B = minCoast, C = -(p.length + (v0 * v0 + v1 * v1) / (2.0 * p.a));
            vp = std::min(vp, (-B + std::sqrt(B * B - 4.0 * A * C)) / (2.0 * A));
        }
        vp = std::min(vp, vmax);
        vp = std::max(vp, std::max(v0, v1));
        double d1 = (vp * vp - v0 * v0) / (2.0 * p.a);
        double d3 = (vp * vp - v1 * v1) / (2.0 * p.a);
        if (d1 + d3 > p.length)
        {
            // Inconsistent request (cannot brake in the distance): raise a.
            p.a = (2.0 * vp * vp - v0 * v0 - v1 * v1) / (2.0 * p.length);
            d1 = (vp * vp - v0 * v0) / (2.0 * p.a);
            d3 = (vp * vp - v1 * v1) / (2.0 * p.a);
        }
        p.vp = vp;
        p.t1 = (vp - v0) / p.a;
        const double d2 = std::max(0.0, p.length - d1 - d3);
        p.t2 = p.t1 + (vp > 1.0e-9 ? d2 / vp : 0.0);
        p.total = p.t2 + (vp - v1) / p.a;
        return p;
    }
    double s(double t) const
    {
        t = std::clamp(t, 0.0, total);
        if (t <= t1) return v0 * t + 0.5 * a * t * t;
        const double s1 = v0 * t1 + 0.5 * a * t1 * t1;
        if (t <= t2) return s1 + vp * (t - t1);
        const double s2 = s1 + vp * (t2 - t1);
        const double u = t - t2;
        return std::min(length, s2 + vp * u - 0.5 * a * u * u);
    }
};

// ---- segments ---------------------------------------------------------------
enum class SegmentKind : std::uint8_t
{
    Hold,       // fixed pose (FIXED frame)
    Rotate,     // fixed position, basis slerp
    BurnLine,   // Newtonian: burn - coast - flip - brake along a line
    RcsLine,    // translate along a line, constant basis
    Curve,      // quadratic Bezier, nose along the tangent
    Axial,      // along the axis with roll sync to the dock spin
    AxialOut,   // departure along the axis, roll decay, burn/flip/brake
    RotHold,    // ROTATING frame fixed pose
    RotMove     // ROTATING frame move (station animation or corridor)
};

struct Segment
{
    SegmentKind kind = SegmentKind::Hold;
    double t0 = 0.0;
    double duration = Infinity;
    glm::dvec3 p0 {0.0}, p1 {0.0}, control {0.0};
    glm::dmat3 b0 {1.0}, b1 {1.0};
    glm::dvec3 upHint {0.0, 1.0, 0.0};
    Profile profile;
    double flipSeconds = 0.0;
    bool smoothTime = false; // RotMove: smoothstep(time) instead of profile
    // RotMove on its profile: a quadratic Bezier through `control` (arcTable
    // filled) and / or the hull turning b0 -> b1 between these fractions of
    // the path (the drift onto a pad: sliding in while the nose swings round).
    bool curved = false;
    bool turnBasis = false;
    double turnStart = 0.0, turnEnd = 1.0;
    // Axial roll sync / AxialOut roll decay.
    double rollStart = 0.0, phaseStart = 0.0, rollOffset = 0.0, syncLength = 1.0, rollRate = 0.0, rollDecay = 10.0;
    std::vector<double> arcTable; // Curve: cumulative length per u sample
};

glm::dvec3 bezier(const Segment& s, double u)
{
    const double v = 1.0 - u;
    return v * v * s.p0 + 2.0 * v * u * s.control + u * u * s.p1;
}

glm::dvec3 bezierTangent(const Segment& s, double u)
{
    return 2.0 * (1.0 - u) * (s.control - s.p0) + 2.0 * u * (s.p1 - s.control);
}

double bezierU(const Segment& s, double dist)
{
    const auto& t = s.arcTable;
    const std::size_t n = t.size() - 1;
    const auto it = std::lower_bound(t.begin(), t.end(), dist);
    if (it == t.begin()) return 0.0;
    if (it == t.end()) return 1.0;
    const std::size_t i = static_cast<std::size_t>(it - t.begin());
    const double f = (dist - t[i - 1]) / std::max(t[i] - t[i - 1], 1.0e-9);
    return (static_cast<double>(i - 1) + f) / static_cast<double>(n);
}

glm::dvec3 upFromRoll(double roll) { return {std::cos(roll), std::sin(roll), 0.0}; }

struct SegmentPose
{
    glm::dvec3 position {0.0};
    glm::dmat3 basis {1.0};
};

void fillArcTable(Segment& s)
{
    s.arcTable.assign(1, 0.0);
    glm::dvec3 prev = s.p0;
    for (int i = 1; i <= 64; ++i)
    {
        const glm::dvec3 p = bezier(s, i / 64.0);
        s.arcTable.push_back(s.arcTable.back() + glm::length(p - prev));
        prev = p;
    }
}

// Lively docking (author 2026-11-07: "влеты в станцию бодрее ... кобра вполне может дрифтом
// заходить на площадку"): the portal is crossed at EntrySpeed, the corridor is flown without
// crawling, and inside the chamber the ship drifts onto its pad - one arc from the axis out to
// above the pad while the hull swings nose-to-exit, braking all the way - then a short touchdown.
constexpr double EntrySpeedMps = 12.0;
constexpr double ChamberGateSpeedMps = 8.0;
constexpr double DriftBrakeMps2 = 1.0;
constexpr double TouchdownSeconds = 6.0;
constexpr double DepartureOutSpeedMps = 12.0;

// ROTATING frame: from the chamber gate (on the axis, moving +Z at v0) to above pad k.
Segment driftToPad(const hex::HexDockGeometry& g, int padIndex, const glm::dvec3& gate, const glm::dvec3& above, double v0)
{
    Segment drift;
    drift.kind = SegmentKind::RotMove;
    drift.curved = true;
    drift.turnBasis = true;
    drift.turnStart = 0.15;
    drift.turnEnd = 0.85;
    drift.p0 = gate;
    drift.p1 = above;
    // Keep going along the axis first, then slide out to the wall.
    drift.control = {0.0, 0.0, above.z + 0.25 * (above.z - gate.z)};
    fillArcTable(drift);
    drift.b0 = hex::shipBasis({0.0, 0.0, 1.0}, {0.0, 1.0, 0.0});
    drift.b1 = hex::padLandedBasis(padIndex);
    (void)g;
    drift.profile = Profile::make(drift.arcTable.back(), v0, 0.0, DriftBrakeMps2, std::max(v0, 1.0));
    drift.duration = drift.profile.total;
    return drift;
}

// Evaluate one segment at local time tau, FIXED frame.
SegmentPose evaluate(const Segment& s, double tau, double phase)
{
    SegmentPose out;
    const glm::dmat3 spin = hex::rotationAboutZ(phase);
    switch (s.kind)
    {
        case SegmentKind::Hold:
            out.position = s.p0;
            out.basis = s.b0;
            break;
        case SegmentKind::Rotate:
            out.position = s.p0;
            out.basis = slerpBasis(s.b0, s.b1, smoothstep01(tau / s.duration));
            break;
        case SegmentKind::RcsLine:
        {
            const glm::dvec3 d = s.p1 - s.p0;
            const double len = glm::length(d);
            out.position = s.p0 + (len > 0.0 ? d / len : d) * s.profile.s(tau);
            out.basis = s.b0;
            break;
        }
        case SegmentKind::BurnLine:
        {
            const glm::dvec3 d = s.p1 - s.p0;
            const glm::dvec3 dir = glm::normalize(d);
            out.position = s.p0 + dir * s.profile.s(tau);
            const glm::dmat3 noseFwd = hex::shipBasis(dir, s.upHint);
            const bool brakes = s.profile.vp - s.profile.v1 > 1.0e-6;
            if (!brakes)
            {
                out.basis = noseFwd;
                break;
            }
            const double flipEnd = s.profile.t2;
            const double flipStart = flipEnd - s.flipSeconds;
            if (tau <= flipStart)
                out.basis = noseFwd;
            else
            {
                // Yaw 180 deg about the ship's up axis.
                const double w = smoothstep01((tau - flipStart) / std::max(s.flipSeconds, 1.0e-6));
                const glm::dvec3 up = noseFwd[1];
                const glm::dvec3 side = glm::cross(up, dir);
                const double a = w * hex::Pi;
                const glm::dvec3 nose = dir * std::cos(a) + side * std::sin(a);
                out.basis = hex::shipBasis(nose, up);
            }
            break;
        }
        case SegmentKind::Curve:
        {
            const double u = bezierU(s, s.profile.s(tau));
            out.position = bezier(s, u);
            out.basis = hex::shipBasis(bezierTangent(s, u), s.upHint);
            break;
        }
        case SegmentKind::Axial:
        {
            const double dist = s.profile.s(tau);
            out.position = s.p0 + glm::dvec3(0.0, 0.0, 1.0) * dist;
            const double w = smoothstep01(dist / s.syncLength);
            const double roll = s.rollStart + w * (s.rollOffset + (phase - s.phaseStart));
            out.basis = hex::shipBasis(glm::dvec3(0.0, 0.0, 1.0), upFromRoll(roll));
            break;
        }
        case SegmentKind::AxialOut:
        {
            const double dist = s.profile.s(tau);
            out.position = s.p0 + glm::dvec3(0.0, 0.0, -1.0) * dist;
            const double tr = std::min(tau, s.rollDecay);
            const double roll = s.rollStart + s.rollRate * (tr - tr * tr / (2.0 * s.rollDecay));
            const glm::dvec3 up = upFromRoll(roll);
            const glm::dvec3 dir {0.0, 0.0, -1.0};
            const double flipStart = s.profile.t2 - s.flipSeconds;
            if (tau <= flipStart)
                out.basis = hex::shipBasis(dir, up);
            else
            {
                const double w = smoothstep01((tau - flipStart) / std::max(s.flipSeconds, 1.0e-6));
                const glm::dvec3 side = glm::cross(up, dir);
                const double a = w * hex::Pi;
                out.basis = hex::shipBasis(dir * std::cos(a) + side * std::sin(a), up);
            }
            break;
        }
        case SegmentKind::RotHold:
            out.position = spin * s.p0;
            out.basis = spin * s.b0;
            break;
        case SegmentKind::RotMove:
        {
            double w;
            if (s.smoothTime)
                w = smoothstep01(tau / s.duration);
            else
                w = s.profile.s(tau) / std::max(s.profile.length, 1.0e-9);
            w = std::clamp(w, 0.0, 1.0);
            const glm::dvec3 local = s.curved && s.arcTable.size() > 1
                ? bezier(s, bezierU(s, w * s.arcTable.back()))
                : s.p0 + (s.p1 - s.p0) * w;
            glm::dmat3 basis = s.b0;
            if (s.turnBasis)
                basis = slerpBasis(s.b0, s.b1, smoothstep01(std::clamp((w - s.turnStart) / std::max(s.turnEnd - s.turnStart, 1.0e-6), 0.0, 1.0)));
            else if (s.smoothTime)
                basis = slerpBasis(s.b0, s.b1, w);
            out.position = spin * local;
            out.basis = spin * basis;
            break;
        }
    }
    return out;
}

double rollOf(const glm::dvec3& up) { return std::atan2(up.y, up.x); }

// Gate (queue waiting point) per slot, FIXED frame.
struct GateSlot
{
    glm::dvec3 position;
    double azimuth;        // preferred hold azimuth
    double departureAzimuth;
    bool shuttle;
    int ring = 0;          // unique dispersal / far point per slot
};

std::vector<GateSlot> gateSlots(const hex::HexDockGeometry& g)
{
    std::vector<GateSlot> slots;
    // Earth side (-Y) for shuttles, staggered in depth so inbound vertical
    // legs never pass another waiting shuttle.
    for (int i = 0; i < 3; ++i)
    {
        const double a = 1.5 * hex::Pi; // 270 deg
        slots.push_back({{0.0, -g.gateRadius, g.gateZ - 1200.0 * i}, a, (240.0 + 60.0 * (i % 2)) * hex::Pi / 180.0, true, i});
    }
    // Busy-traffic gates on the other hold azimuths, two rings.
    for (int ring = 0; ring < 2; ++ring)
        for (int k : {0, 1, 2, 3, 5})
        {
            const double a = hex::holdAzimuthRad(k);
            const double r = g.gateRadius + 1000.0 * ring;
            slots.push_back({{r * std::cos(a), r * std::sin(a), g.gateZ - 1500.0 * ring}, a, a + hex::Pi / 6.0, false, ring});
        }
    return slots;
}

const char* EarthToHubCargo[] = {"food", "replacement parts", "electronics", "manufactured goods",
                                 "medical supplies", "specialist equipment"};
const char* HubToEarthCargo[] = {"orbital production", "processed material", "station waste / recycling",
                                 "returned equipment", "samples", "high-value cargo"};

} // namespace

// ---------------------------------------------------------------------------

const TrafficShipClass& trafficShipClass(TrafficShipKind kind)
{
    // Mass and cargo capacity: the authoritative cargo passports (no traffic-only numbers).
    using catalog::ships::CobrettIN1Cargo;
    using catalog::ships::Ocs4Cargo;
    // Orbital cargo shuttle: see src/game/catalog/ships/OrbitalCargoShuttlePassports.h
    static const TrafficShipClass shuttle {
        TrafficShipKind::CargoShuttle, "OCS-4 \"Lastochka\" orbital cargo shuttle",
        56.0, 18.0, 14.0, 0.5 * std::sqrt(56.0 * 56.0 + 18.0 * 18.0 + 14.0 * 14.0),
        Ocs4Cargo.dryMassKg, 1000.0 * Ocs4Cargo.cargoTonnes,
        4.0, 200.0, 25.0, 1.2, 18.0, 900.0, game::docking::DockSizeClass::Medium,
        1000.0 * (catalog::ships::grossCargoTonnes(Ocs4Cargo) - Ocs4Cargo.cargoTonnes)};
    static const TrafficShipClass cobrett {
        TrafficShipKind::Cobrett, "Cobrett-I FRN-U N1",
        22.2, 26.0, 5.0, 0.5 * std::sqrt(22.2 * 22.2 + 26.0 * 26.0 + 5.0 * 5.0),
        CobrettIN1Cargo.dryMassKg, 1000.0 * CobrettIN1Cargo.cargoTonnes,
        12.0, 300.0, 30.0, 2.0, 10.0, 240.0, game::docking::DockSizeClass::Medium,
        1000.0 * (catalog::ships::grossCargoTonnes(CobrettIN1Cargo) - CobrettIN1Cargo.cargoTonnes)};
    return kind == TrafficShipKind::CargoShuttle ? shuttle : cobrett;
}

game::docking::DockingEnvelope dockingEnvelope(const TrafficShipClass& c)
{
    game::docking::DockingEnvelope e;
    e.widthMeters = c.width;
    e.heightMeters = c.height;
    e.lengthMeters = c.length;
    e.sweptWidthMeters = c.width + 4.0;
    e.sweptHeightMeters = c.height + 4.0;
    e.landingFootprintWidthMeters = c.width * 0.9;
    e.landingFootprintLengthMeters = c.length * 0.9;
    e.landingGearClearanceMeters = 1.0;
    e.massKg = c.dryMassKg + c.cargoCapacityKg + c.containerTareKg;
    return e;
}

const char* actorPhaseName(ActorPhase phase) noexcept
{
    switch (phase)
    {
        case ActorPhase::Inbound: return "inbound";
        case ActorPhase::WaitGate: return "wait-gate";
        case ActorPhase::ToHold: return "to-hold";
        case ActorPhase::AtHold: return "at-hold";
        case ActorPhase::Approach: return "approach";
        case ActorPhase::Corridor: return "corridor";
        case ActorPhase::StationTransfer: return "station-transfer";
        case ActorPhase::Descend: return "descend";
        case ActorPhase::Docked: return "docked";
        case ActorPhase::DepartureWait: return "departure-wait";
        case ActorPhase::Lift: return "lift";
        case ActorPhase::ToChamberGate: return "to-chamber-gate";
        case ActorPhase::CorridorOut: return "corridor-out";
        case ActorPhase::AxialOut: return "axial-out";
        case ActorPhase::Disperse: return "disperse";
        case ActorPhase::Outbound: return "outbound";
        case ActorPhase::FarLeg: return "far-leg";
        case ActorPhase::FarWait: return "far-wait";
        case ActorPhase::Gone: return "gone";
    }
    return "?";
}

struct DockServiceTraffic::Impl
{
    struct Actor
    {
        std::uint64_t id = 0;
        TrafficShipKind kind = TrafficShipKind::Cobrett;
        std::string callsign;
        ActorPhase phase = ActorPhase::Gone;
        double phaseSince = 0.0;
        std::deque<Segment> segments;
        Segment current;
        int gateSlot = -1;
        int padIndex = -1;
        CargoManifest cargo;
        glm::dvec3 farPoint {0.0};
        bool reportedClear = false;
        double serviceUntil = 0.0;
        double queuedSince = 0.0;
        int loops = 0;
        bool external = false; // pilot ship under station control
        std::uint64_t flightSerial = 0; // persistent flight identity (not parsed back from the callsign)
    };

    TrafficConfig config;
    std::function<double(double)> phaseAt;
    const hex::HexDockGeometry& g = hex::hexDockGeometry();
    game::docking::traffic::DockTrafficController dispatcher;
    std::vector<GateSlot> gates;
    std::vector<std::uint64_t> gateOwner;
    std::map<std::uint64_t, Actor> actors;
    std::vector<TrafficLogLine> log;
    Rng rng;
    std::uint64_t nextId;
    double now = 0.0;
    double nextShuttleAt = 30.0;
    int shuttleSerial = 1700;
    Stats stats;
    bool initialised = false;

    Impl(TrafficConfig c, std::function<double(double)> phase, std::uint64_t firstId)
        : config(std::move(c)),
          phaseAt(std::move(phase)),
          dispatcher(hex::hexDockFacilityDescriptor(c.dockFacilityId, c.systemId, c.dockFrameId)),
          gates(gateSlots(hex::hexDockGeometry())),
          gateOwner(gates.size(), 0),
          rng{c.seed},
          nextId(firstId)
    {
    }

    void say(std::uint64_t id, const std::string& text)
    {
        std::ostringstream line;
        const auto it = actors.find(id);
        line << (it != actors.end() ? it->second.callsign : std::to_string(id)) << " " << text;
        log.push_back({now, line.str()});
    }

    void setPhase(Actor& a, ActorPhase p)
    {
        a.phase = p;
        a.phaseSince = now;
    }

    // ---- segment builders (each starts at the end of the previous one) ----
    SegmentPose endPose(const Actor& a) const
    {
        const Segment& last = a.segments.empty() ? a.current : a.segments.back();
        const double end = last.t0 + last.duration;
        return evaluate(last, std::isfinite(last.duration) ? last.duration : 0.0, phaseAt(std::isfinite(end) ? end : now));
    }
    double endTime(const Actor& a) const
    {
        const Segment& last = a.segments.empty() ? a.current : a.segments.back();
        return std::isfinite(last.duration) ? last.t0 + last.duration : now;
    }
    void push(Actor& a, Segment s)
    {
        s.t0 = endTime(a);
        a.segments.push_back(std::move(s));
    }
    void startNow(Actor& a)
    {
        // Cut the trailing infinite hold (current or queued) at `now` so the
        // new plan starts right after it, never behind it.
        Segment& last = a.segments.empty() ? a.current : a.segments.back();
        if (!std::isfinite(last.duration))
            last.duration = std::max(0.0, now - last.t0);
    }

    void pushRotate(Actor& a, const glm::dmat3& to, double seconds)
    {
        const auto end = endPose(a);
        Segment s;
        s.kind = SegmentKind::Rotate;
        s.p0 = end.position;
        s.b0 = end.basis;
        s.b1 = to;
        s.duration = seconds;
        push(a, s);
    }
    void pushHold(Actor& a, const glm::dmat3* basis = nullptr)
    {
        const auto end = endPose(a);
        Segment s;
        s.kind = SegmentKind::Hold;
        s.p0 = end.position;
        s.b0 = basis ? *basis : end.basis;
        s.duration = Infinity;
        push(a, s);
    }
    void pushBurn(Actor& a, const glm::dvec3& to, double v0, double v1, double accel, double vmax, const glm::dvec3& upHint, bool rotateFirst)
    {
        const TrafficShipClass& c = trafficShipClass(a.kind);
        const auto end = endPose(a);
        const glm::dvec3 dir = glm::normalize(to - end.position);
        if (rotateFirst)
            pushRotate(a, hex::shipBasis(dir, upHint), c.flipSeconds);
        Segment s;
        s.kind = SegmentKind::BurnLine;
        s.p0 = end.position;
        s.p1 = to;
        s.upHint = upHint;
        s.flipSeconds = c.flipSeconds;
        const bool brakes = v1 < vmax;
        s.profile = Profile::make(glm::length(to - end.position), v0, v1, accel, vmax, brakes ? c.flipSeconds + 2.0 : 0.0);
        s.duration = s.profile.total;
        push(a, s);
    }

    glm::dmat3 holdBasis(int holdIndex) const
    {
        const glm::dvec3 hold = hex::holdPosition(g, holdIndex);
        const glm::dvec3 control {0.0, 0.0, g.axisJoinZ - 300.0};
        return hex::shipBasis(control - hold, {0.0, 1.0, 0.0});
    }

    // ---- actors -----------------------------------------------------------
    std::uint64_t spawn(TrafficShipKind kind, const std::string& callsign)
    {
        Actor a;
        a.id = nextId++;
        a.kind = kind;
        a.callsign = callsign;
        actors[a.id] = a;
        return a.id;
    }

    int takeGate(std::uint64_t id, bool shuttle)
    {
        for (std::size_t i = 0; i < gates.size(); ++i)
            if (gates[i].shuttle == shuttle && gateOwner[i] == 0)
            {
                gateOwner[i] = id;
                return static_cast<int>(i);
            }
        return -1;
    }
    void freeGate(Actor& a)
    {
        if (a.gateSlot >= 0 && gateOwner[static_cast<std::size_t>(a.gateSlot)] == a.id)
            gateOwner[static_cast<std::size_t>(a.gateSlot)] = 0;
        a.gateSlot = -1;
    }

    // The far end of a flight: the surface port or a connected hub of the network, fixed per
    // flight serial so the outbound leg of the same flight keeps its counterpart.
    std::string counterpartFor(std::uint64_t serial) const
    {
        const std::size_t n = config.connectedDestinations.size();
        const double u = static_cast<double>((serial * 2654435761ull) % 1000ull) / 1000.0;
        if (n == 0 || u < config.surfaceShare)
            return config.surfaceName;
        return config.connectedDestinations[static_cast<std::size_t>(serial % n)];
    }

    CargoManifest manifest(bool toHub, std::uint64_t serial, double when)
    {
        CargoManifest m;
        std::ostringstream id;
        id << config.flightPrefix << "-" << serial << (toHub ? "A" : "D");
        m.flightId = id.str();
        const auto& list = toHub ? EarthToHubCargo : HubToEarthCargo;
        m.cargoClass = list[rng.next() % 6];
        m.tonnes = std::round(trafficShipClass(TrafficShipKind::CargoShuttle).cargoCapacityKg / 1000.0 * (0.55 + 0.45 * rng.uniform()));
        const std::string other = counterpartFor(serial);
        m.origin = toHub ? other : config.hubDisplayName;
        m.destination = toHub ? config.hubDisplayName : other;
        m.category = other == config.surfaceName ? "cargo" : "service";
        m.scheduledSeconds = when;
        m.etaSeconds = toHub ? when : when + config.shuttleTransferSeconds;
        return m;
    }

    void materializeShuttle()
    {
        const int serial = shuttleSerial++;
        std::ostringstream cs;
        cs << config.shuttleCallsignPrefix << "-" << serial;
        const std::uint64_t id = spawn(TrafficShipKind::CargoShuttle, cs.str());
        Actor& a = actors[id];
        a.gateSlot = takeGate(id, true);
        if (a.gateSlot < 0)
        {
            // Every Earth-side gate taken: the flight is held at Earth (the
            // schedule slips), it never materialises on top of a waiting ship.
            say(id, "held at " + config.surfaceName + ": all inbound queue gates occupied");
            actors.erase(id);
            nextShuttleAt = now + 60.0;
            return;
        }
        a.flightSerial = static_cast<std::uint64_t>(serial);
        a.cargo = manifest(true, a.flightSerial, now);
        const TrafficShipClass& c = trafficShipClass(a.kind);
        const glm::dvec3 gate = gates[static_cast<std::size_t>(a.gateSlot)].position;
        const glm::dvec3 start = gate + config.earthDirectionFixed * config.materializeDistance;
        // Already coasting from the aggregated Earth leg, nose first.
        a.current = Segment {};
        a.current.kind = SegmentKind::Hold;
        a.current.t0 = now;
        a.current.duration = 0.0;
        a.current.p0 = start;
        a.current.b0 = hex::shipBasis(gate - start, {0.0, 0.0, -1.0});
        pushBurn(a, gate, c.cruiseSpeed, 0.0, c.mainAccel, c.cruiseSpeed, {0.0, 0.0, -1.0}, false);
        setPhase(a, ActorPhase::Inbound);
        ++stats.shuttleFlights;
        std::ostringstream text;
        text << "materialised " << static_cast<int>(config.materializeDistance / 1000.0) << " km out on the inbound side, "
             << a.cargo.flightId << " " << a.cargo.tonnes << " t " << a.cargo.cargoClass
             << " (after the aggregated " << static_cast<int>(config.shuttleTransferSeconds / 60.0) << "-min Earth leg), coasting "
             << c.cruiseSpeed << " m/s -> gate " << a.gateSlot;
        say(id, text.str());
    }

    void startCobrettFarWait(Actor& a, const glm::dvec3& at, double waitSeconds)
    {
        a.current = Segment {};
        a.current.kind = SegmentKind::Hold;
        a.current.t0 = now;
        a.current.duration = waitSeconds;
        a.current.p0 = at;
        a.current.b0 = hex::shipBasis(-at, {0.0, 1.0, 0.0});
        setPhase(a, ActorPhase::FarWait);
    }

    glm::dvec3 farPointFor(const Actor& a) const
    {
        const GateSlot& gate = gates[static_cast<std::size_t>(a.gateSlot)];
        const double az = gate.departureAzimuth;
        const double tilt = (50.0 - 15.0 * gate.ring) * hex::Pi / 180.0;
        const glm::dvec3 dir {std::sin(tilt) * std::cos(az), std::sin(tilt) * std::sin(az), -std::cos(tilt)};
        return dispersePoint(a) + dir * config.cobrettFarDistance;
    }
    glm::dvec3 dispersePoint(const Actor& a) const
    {
        // Departures leave the axis only beyond every queue gate (z -11 km):
        // their dispersal never crosses the gate -> hold arrivals.
        const GateSlot& gate = gates[static_cast<std::size_t>(a.gateSlot)];
        const double r = 1500.0 + 700.0 * gate.ring;
        return {r * std::cos(gate.departureAzimuth), r * std::sin(gate.departureAzimuth), -12000.0 - 800.0 * gate.ring};
    }

    game::docking::traffic::DockingTrafficRequest requestFor(const Actor& a) const
    {
        game::docking::traffic::DockingTrafficRequest r;
        r.shipId = a.id;
        const TrafficShipClass& c = trafficShipClass(a.kind);
        r.shipClass = c.sizeClass;
        r.envelope = dockingEnvelope(c);
        r.insideControlledApproachHorizon = true;
        return r;
    }

    void initialise()
    {
        initialised = true;
        // Docked at start (mid-service), registered with the dispatcher by
        // the normal lifecycle so every right is consistent.
        int pad = 0;
        auto dockAtStart = [&](TrafficShipKind kind, const std::string& cs, double serviceLeft) {
            const std::uint64_t id = spawn(kind, cs);
            Actor& a = actors[id];
            if (kind == TrafficShipKind::CargoShuttle)
            {
                a.flightSerial = static_cast<std::uint64_t>(shuttleSerial++);
                a.cargo = manifest(true, a.flightSerial, now);
            }
            if (!dispatcher.registerDocked(requestFor(a), hex::padIdFor(pad), now))
            {
                actors.erase(id);
                return;
            }
            a.padIndex = pad++;
            const TrafficShipClass& c = trafficShipClass(kind);
            a.current = Segment {};
            a.current.kind = SegmentKind::RotHold;
            a.current.t0 = now;
            a.current.duration = Infinity;
            a.current.p0 = hex::padLandedCenter(g, a.padIndex, c.height * 0.5);
            a.current.b0 = hex::padLandedBasis(a.padIndex);
            a.serviceUntil = now + serviceLeft;
            setPhase(a, ActorPhase::Docked);
            say(id, std::string("docked at start on pad ") + hex::padIdFor(a.padIndex));
        };
        for (int i = 0; i < config.initialDockedShuttles; ++i)
        {
            std::ostringstream cs;
            cs << config.shuttleCallsignPrefix << "-" << shuttleSerial;
            dockAtStart(TrafficShipKind::CargoShuttle, cs.str(), 120.0 + 300.0 * i);
        }
        for (int i = 0; i < config.cobrettCount; ++i)
        {
            std::ostringstream cs;
            cs << config.cobrettCallsignPrefix << "-" << (11 + i);
            if (i < config.initialDockedCobretts)
            {
                dockAtStart(TrafficShipKind::Cobrett, cs.str(), 60.0 + 90.0 * i);
                continue;
            }
            const std::uint64_t id = spawn(TrafficShipKind::Cobrett, cs.str());
            Actor& a = actors[id];
            a.gateSlot = takeGate(id, false);
            if (a.gateSlot < 0)
            {
                actors.erase(id);
                continue;
            }
            startCobrettFarWait(a, farPointFor(a), 20.0 + 75.0 * i);
            say(id, "busy-traffic Cobrett waiting at its far point");
        }
        // Docked Cobretts need a gate slot for their next loop.
        for (auto& [id, a] : actors)
            if (a.kind == TrafficShipKind::Cobrett && a.gateSlot < 0)
                a.gateSlot = takeGate(id, false);
    }

    // ---- per-actor state machine -----------------------------------------
    void step(Actor& a)
    {
        // Advance through finished segments.
        while (std::isfinite(a.current.duration) && now >= a.current.t0 + a.current.duration && !a.segments.empty())
        {
            a.current = a.segments.front();
            a.segments.pop_front();
        }
        const bool segmentsDone = a.segments.empty() && std::isfinite(a.current.duration) &&
                                  now >= a.current.t0 + a.current.duration;
        const TrafficShipClass& c = trafficShipClass(a.kind);
        const auto status = dispatcher.status(a.id);

        switch (a.phase)
        {
            case ActorPhase::Inbound:
                if (segmentsDone)
                {
                    const glm::dmat3 wait = hex::shipBasis(-endPose(a).position, {0.0, 1.0, 0.0});
                    pushRotate(a, wait, c.flipSeconds);
                    pushHold(a, &wait);
                    a.queuedSince = now;
                    const double az = gates[static_cast<std::size_t>(a.gateSlot)].azimuth;
                    dispatcher.requestDocking(requestFor(a), now, az);
                    setPhase(a, ActorPhase::WaitGate);
                    say(a.id, "at queue gate, docking requested");
                }
                break;
            case ActorPhase::WaitGate:
                if (status.phase == DockTrafficPhase::HoldAssigned)
                {
                    const int hold = hex::indexOfId(status.holdPointId);
                    startNow(a);
                    pushBurn(a, hex::holdPosition(g, hold), 0.0, 0.0, c.mainAccel * 0.4, 120.0, {0.0, 1.0, 0.0}, true);
                    pushRotate(a, holdBasis(hold), c.flipSeconds);
                    pushHold(a);
                    freeGate(a);
                    setPhase(a, ActorPhase::ToHold);
                    stats.maxQueueWaitSeconds = std::max(stats.maxQueueWaitSeconds, now - a.queuedSince);
                    say(a.id, "hold " + status.holdPointId + " assigned, leaving the queue gate");
                }
                break;
            case ActorPhase::ToHold:
                if (a.segments.empty() && !std::isfinite(a.current.duration))
                {
                    dispatcher.reportAtHold(a.id, now);
                    setPhase(a, ActorPhase::AtHold);
                }
                break;
            case ActorPhase::AtHold:
                if (status.phase == DockTrafficPhase::EntryCleared)
                {
                    a.padIndex = hex::indexOfId(status.padId);
                    startNow(a);
                    // Hold -> axis join: Bezier, tangent along +Z at the join.
                    Segment curve;
                    curve.kind = SegmentKind::Curve;
                    curve.p0 = endPose(a).position;
                    curve.p1 = {0.0, 0.0, g.axisJoinZ};
                    curve.control = {0.0, 0.0, g.axisJoinZ - 300.0};
                    curve.upHint = {0.0, 1.0, 0.0};
                    curve.arcTable.assign(1, 0.0);
                    glm::dvec3 prev = curve.p0;
                    for (int i = 1; i <= 64; ++i)
                    {
                        const glm::dvec3 p = bezier(curve, i / 64.0);
                        curve.arcTable.push_back(curve.arcTable.back() + glm::length(p - prev));
                        prev = p;
                    }
                    curve.profile = Profile::make(curve.arcTable.back(), 0.0, c.localSpeed * 0.8, c.localAccel, c.localSpeed);
                    curve.duration = curve.profile.total;
                    push(a, curve);
                    // Axis join -> entry face, roll synced to the slot.
                    Segment axial;
                    axial.kind = SegmentKind::Axial;
                    axial.p0 = curve.p1;
                    const double entrySpeed = EntrySpeedMps;
                    axial.profile = Profile::make(g.entryFaceZ - g.axisJoinZ, c.localSpeed * 0.8, entrySpeed, c.localAccel * 0.6, c.localSpeed * 0.8);
                    axial.duration = axial.profile.total;
                    axial.syncLength = 0.6 * (g.entryFaceZ - g.axisJoinZ);
                    const double t0 = endTime(a);
                    axial.phaseStart = phaseAt(t0);
                    axial.rollStart = hex::Pi * 0.5; // up = +Y at the join
                    const double target = axial.phaseStart + hex::Pi * 0.5;
                    axial.rollOffset = std::remainder(target - axial.rollStart, 2.0 * hex::Pi);
                    push(a, axial);
                    // Entry corridor, co-rotating (ROTATING frame).
                    Segment corridor;
                    corridor.kind = SegmentKind::RotMove;
                    corridor.p0 = {0.0, 0.0, g.entryFaceZ};
                    corridor.p1 = {0.0, 0.0, g.chamberFrontZ + c.length * 0.5 + 5.0}; // whole ship inside
                    corridor.b0 = hex::shipBasis({0.0, 0.0, 1.0}, {0.0, 1.0, 0.0});
                    corridor.profile = Profile::make(corridor.p1.z - g.entryFaceZ, entrySpeed, ChamberGateSpeedMps, 0.6, entrySpeed);
                    corridor.duration = corridor.profile.total;
                    push(a, corridor);
                    setPhase(a, ActorPhase::Approach);
                    say(a.id, "entry cleared " + status.holdPointId + " -> pad " + status.padId + ", leaving hold");
                }
                break;
            case ActorPhase::Approach:
                if (a.current.kind == SegmentKind::RotMove)
                    setPhase(a, ActorPhase::Corridor);
                break;
            case ActorPhase::Corridor:
                if (segmentsDone)
                {
                    dispatcher.reportInsideChamber(a.id, now);
                    // Station control: kinematic capture to above the pad,
                    // nose turned to the exit, then lowered and latched.
                    const glm::dvec3 landed = hex::padLandedCenter(g, a.padIndex, c.height * 0.5);
                    const glm::dvec3 above = landed - hex::wallNormal(a.padIndex) * 22.0;
                    Segment transfer = driftToPad(g, a.padIndex, {0.0, 0.0, g.chamberFrontZ + c.length * 0.5 + 5.0}, above,
                                                  ChamberGateSpeedMps);
                    push(a, transfer);
                    Segment descend;
                    descend.kind = SegmentKind::RotMove;
                    descend.smoothTime = true;
                    descend.p0 = above;
                    descend.p1 = landed;
                    descend.b0 = descend.b1 = transfer.b1;
                    descend.duration = TouchdownSeconds;
                    push(a, descend);
                    Segment docked;
                    docked.kind = SegmentKind::RotHold;
                    docked.p0 = landed;
                    docked.b0 = transfer.b1;
                    push(a, docked);
                    setPhase(a, ActorPhase::StationTransfer);
                    say(a.id, std::string("handoff at the chamber gate: station control to pad ") + hex::padIdFor(a.padIndex));
                }
                break;
            case ActorPhase::StationTransfer:
                if (a.current.kind == SegmentKind::RotHold)
                {
                    dispatcher.reportLanded(a.id, now);
                    a.serviceUntil = now + c.serviceSeconds;
                    setPhase(a, ActorPhase::Docked);
                    ++stats.arrivals;
                    if (a.kind == TrafficShipKind::CargoShuttle)
                        say(a.id, "latched; unloading " + std::to_string(static_cast<int>(a.cargo.tonnes)) + " t " + a.cargo.cargoClass);
                    else
                        say(a.id, "latched; turnaround service");
                }
                break;
            case ActorPhase::Docked:
                if (!a.external && now >= a.serviceUntil)
                {
                    if (a.kind == TrafficShipKind::CargoShuttle)
                    {
                        a.cargo = manifest(false, a.flightSerial, now);
                        say(a.id, "loaded " + std::to_string(static_cast<int>(a.cargo.tonnes)) + " t " + a.cargo.cargoClass +
                                      " for " + a.cargo.destination + ", departure requested");
                    }
                    else
                        say(a.id, "service done, departure requested");
                    dispatcher.requestDeparture(a.id, now);
                    setPhase(a, ActorPhase::DepartureWait);
                }
                break;
            case ActorPhase::DepartureWait:
                if (status.phase == DockTrafficPhase::DepartureCleared)
                {
                    startNow(a);
                    const glm::dvec3 landed = hex::padLandedCenter(g, a.padIndex, c.height * 0.5);
                    const glm::dvec3 above = landed - hex::wallNormal(a.padIndex) * 22.0;
                    Segment lift;
                    lift.kind = SegmentKind::RotMove;
                    lift.smoothTime = true;
                    lift.p0 = landed;
                    lift.p1 = above;
                    lift.b0 = lift.b1 = hex::padLandedBasis(a.padIndex);
                    lift.duration = TouchdownSeconds;
                    push(a, lift);
                    Segment toGate;
                    toGate.kind = SegmentKind::RotMove;
                    toGate.smoothTime = true;
                    toGate.p0 = above;
                    toGate.p1 = {0.0, 0.0, g.chamberFrontZ + c.length * 0.5 + 5.0};
                    toGate.b0 = lift.b1;
                    toGate.b1 = hex::shipBasis({0.0, 0.0, -1.0}, {0.0, 1.0, 0.0});
                    toGate.duration = 22.0;
                    push(a, toGate);
                    Segment out;
                    out.kind = SegmentKind::RotMove;
                    out.p0 = toGate.p1;
                    out.p1 = {0.0, 0.0, g.entryFaceZ};
                    out.b0 = toGate.b1;
                    out.profile = Profile::make(out.p0.z - out.p1.z, 0.0, DepartureOutSpeedMps, 1.0, DepartureOutSpeedMps);
                    out.duration = out.profile.total;
                    push(a, out);
                    // Axis: burn out, flip, brake to the dispersal stop. A pilot ship flies it too
                    // (author 2026-11-07: "пусть автоматика выставит корабль за пределы дока"): the
                    // station releases it at rest beyond every queue gate, not at the portal.
                    Segment axial;
                    axial.kind = SegmentKind::AxialOut;
                    axial.p0 = {0.0, 0.0, g.entryFaceZ};
                    axial.flipSeconds = c.flipSeconds;
                    axial.profile = Profile::make(g.entryFaceZ - g.departureTurnZ, DepartureOutSpeedMps, 0.0, c.localAccel, 80.0, c.flipSeconds + 2.0);
                    axial.duration = axial.profile.total;
                    const double t0 = endTime(a);
                    axial.rollStart = phaseAt(t0) + hex::Pi * 0.5;
                    axial.rollRate = g.spinRadPerSecond();
                    push(a, axial);
                    setPhase(a, ActorPhase::Lift);
                    a.reportedClear = false;
                    say(a.id, std::string("departure cleared, station lifts from pad ") + hex::padIdFor(a.padIndex));
                }
                break;
            case ActorPhase::Lift:
            case ActorPhase::ToChamberGate:
                if (a.current.kind == SegmentKind::RotMove && !a.current.smoothTime)
                {
                    dispatcher.reportLeftChamber(a.id, now);
                    setPhase(a, ActorPhase::CorridorOut);
                }
                else if (a.phase == ActorPhase::Lift && a.current.smoothTime && a.current.p0.z != a.current.p1.z &&
                         a.current.p1.x == 0.0 && a.current.p1.y == 0.0)
                    setPhase(a, ActorPhase::ToChamberGate);
                break;
            case ActorPhase::CorridorOut:
                if (a.current.kind == SegmentKind::AxialOut)
                    setPhase(a, ActorPhase::AxialOut);
                else if (a.external && segmentsDone)
                {
                    // (no axial segment queued - an old departure): released at the portal
                    say(a.id, "released to the pilot at the portal");
                    setPhase(a, ActorPhase::Gone);
                }
                break;
            case ActorPhase::AxialOut:
            {
                const SegmentPose p = evaluate(a.current, now - a.current.t0, phaseAt(now));
                if (!a.reportedClear && p.position.z <= g.departureClearZ)
                {
                    a.reportedClear = true;
                    dispatcher.reportClear(a.id, now);
                    ++stats.departures;
                    say(a.id, "clear of the axis lane, outer lane released");
                }
                if (segmentsDone && a.external)
                {
                    // the pilot ship: at rest beyond the queue gates - the pilot takes over
                    say(a.id, "released to the pilot beyond the dock lanes");
                    setPhase(a, ActorPhase::Gone);
                    break;
                }
                if (segmentsDone)
                {
                    // Into the own departure sector, away from the axis.
                    if (a.gateSlot < 0)
                        a.gateSlot = takeGate(a.id, a.kind == TrafficShipKind::CargoShuttle);
                    if (a.gateSlot < 0)
                        a.gateSlot = a.kind == TrafficShipKind::CargoShuttle ? 0 : 3;
                    pushBurn(a, dispersePoint(a), 0.0, 0.0, c.mainAccel * 0.3, 80.0, {0.0, 1.0, 0.0}, true);
                    pushHold(a);
                    setPhase(a, ActorPhase::Disperse);
                }
                break;
            }
            case ActorPhase::Disperse:
                if (a.segments.empty() && !std::isfinite(a.current.duration))
                {
                    startNow(a);
                    if (a.kind == TrafficShipKind::CargoShuttle)
                    {
                        // Earth leg: burn down to cruise and leave the local zone.
                        const glm::dvec3 from = endPose(a).position;
                        if (a.gateSlot >= 0 && gates[static_cast<std::size_t>(a.gateSlot)].shuttle)
                            freeGate(a);
                        pushBurn(a, from + config.earthDirectionFixed * config.materializeDistance, 0.0, c.cruiseSpeed,
                                 c.mainAccel, c.cruiseSpeed, {0.0, 0.0, -1.0}, true);
                        setPhase(a, ActorPhase::Outbound);
                        say(a.id, a.cargo.flightId + " outbound: burning for " + a.cargo.destination + " with " +
                                      std::to_string(static_cast<int>(a.cargo.tonnes)) + " t " + a.cargo.cargoClass);
                    }
                    else
                    {
                        a.farPoint = farPointFor(a);
                        pushBurn(a, a.farPoint, 0.0, 0.0, c.mainAccel, c.cruiseSpeed, {0.0, 1.0, 0.0}, true);
                        pushHold(a);
                        setPhase(a, ActorPhase::FarLeg);
                    }
                }
                break;
            case ActorPhase::Outbound:
                if (segmentsDone)
                {
                    say(a.id, "left the local zone (" + a.cargo.flightId + " continues aggregated, arrival " + a.cargo.destination + " in " +
                                  std::to_string(static_cast<int>(config.shuttleTransferSeconds / 60.0)) + " min)");
                    setPhase(a, ActorPhase::Gone);
                }
                break;
            case ActorPhase::FarLeg:
                if (a.segments.empty() && !std::isfinite(a.current.duration))
                {
                    a.current.duration = std::max(0.0, now - a.current.t0) + 20.0;
                    setPhase(a, ActorPhase::FarWait);
                    ++stats.cobrettLoops;
                    say(a.id, "turned around 20 km out");
                }
                break;
            case ActorPhase::FarWait:
                if (segmentsDone)
                {
                    if (a.gateSlot < 0)
                        a.gateSlot = takeGate(a.id, false);
                    const glm::dvec3 gate = gates[static_cast<std::size_t>(a.gateSlot)].position;
                    pushBurn(a, gate, 0.0, 0.0, c.mainAccel, c.cruiseSpeed, {0.0, 1.0, 0.0}, true);
                    setPhase(a, ActorPhase::Inbound);
                    say(a.id, "returning to the hub");
                }
                break;
            case ActorPhase::Gone:
                break;
        }
    }

    void advance(double t)
    {
        now = std::max(now, t);
        if (!initialised)
            initialise();
        while (now >= nextShuttleAt)
        {
            const double at = nextShuttleAt;
            nextShuttleAt += config.shuttleIntervalSeconds;
            const double saved = now;
            now = at;
            materializeShuttle();
            now = saved;
        }
        dispatcher.update(now);
        for (auto& [id, a] : actors)
            step(a);
        dispatcher.update(now);
        for (auto& [id, a] : actors)
            step(a);
        for (auto it = actors.begin(); it != actors.end();)
        {
            if (it->second.phase == ActorPhase::Gone)
            {
                freeGate(it->second);
                it = actors.erase(it);
            }
            else
                ++it;
        }
        for (const auto& e : dispatcher.drainEvents())
        {
            const auto it = actors.find(e.shipId);
            log.push_back({e.timeSeconds, std::string("[dispatcher] ") +
                                              (it != actors.end() ? it->second.callsign : std::to_string(e.shipId)) + " " + e.text});
        }
        const auto snap = dispatcher.snapshot();
        std::size_t holds = 0, pads = 0;
        for (const auto& [h, owner] : snap.holds) holds += owner != 0;
        for (const auto& [p, owner] : snap.pads) pads += owner != 0;
        stats.maxHoldsOccupied = std::max(stats.maxHoldsOccupied, holds);
        stats.maxPadsOccupied = std::max(stats.maxPadsOccupied, pads);
        stats.maxArrivalQueue = std::max(stats.maxArrivalQueue, snap.arrivalQueueLength);
    }

    TrafficPose poseOf(const Actor& a, double t) const
    {
        TrafficPose out;
        if (a.phase == ActorPhase::Gone)
            return out;
        const Segment* seg = &a.current;
        if (std::isfinite(a.current.duration) && t >= a.current.t0 + a.current.duration)
            for (const auto& s : a.segments)
            {
                seg = &s;
                if (!std::isfinite(s.duration) || t < s.t0 + s.duration)
                    break;
            }
        auto at = [&](double time) {
            const double tau = std::clamp(time - seg->t0, 0.0, std::isfinite(seg->duration) ? seg->duration : 1.0e18);
            return evaluate(*seg, tau, phaseAt(time));
        };
        const SegmentPose p = at(t);
        const double h = 0.1;
        out.present = true;
        out.position = p.position;
        out.basis = p.basis;
        out.velocity = (at(t + h).position - at(t - h).position) / (2.0 * h);
        return out;
    }
};

DockServiceTraffic::DockServiceTraffic(TrafficConfig config, std::function<double(double)> dockPhaseRad, std::uint64_t firstShipId)
    : m_impl(std::make_unique<Impl>(std::move(config), std::move(dockPhaseRad), firstShipId))
{
}

DockServiceTraffic::~DockServiceTraffic() = default;

void DockServiceTraffic::advance(double nowSeconds)
{
    m_impl->advance(nowSeconds);
}

std::vector<TrafficActorView> DockServiceTraffic::actors() const
{
    std::vector<TrafficActorView> out;
    for (const auto& [id, a] : m_impl->actors)
        out.push_back({id, a.kind, a.callsign, a.phase, a.phaseSince, a.cargo, a.gateSlot});
    return out;
}

TrafficPose DockServiceTraffic::pose(std::uint64_t actorId, double nowSeconds) const
{
    const auto it = m_impl->actors.find(actorId);
    if (it == m_impl->actors.end())
        return {};
    return m_impl->poseOf(it->second, nowSeconds);
}

std::vector<glm::dvec3> DockServiceTraffic::plannedPath(std::uint64_t actorId, double nowSeconds,
                                                        std::size_t maxPoints) const
{
    std::vector<glm::dvec3> out;
    const auto it = m_impl->actors.find(actorId);
    if (it == m_impl->actors.end() || it->second.phase == ActorPhase::Gone || maxPoints < 2)
        return out;
    const auto& a = it->second;
    // Sample times: now, then the midpoint and end of every remaining finite
    // segment (corners and curved corridor legs both survive).
    std::vector<double> times {nowSeconds};
    const auto addSegment = [&](const Segment& s) {
        if (!std::isfinite(s.duration))
            return;
        const double end = s.t0 + s.duration;
        if (end <= nowSeconds)
            return;
        const double mid = std::max(nowSeconds, s.t0) + 0.5 * (end - std::max(nowSeconds, s.t0));
        times.push_back(mid);
        times.push_back(end);
    };
    addSegment(a.current);
    for (const auto& s : a.segments)
        addSegment(s);
    if (times.size() > maxPoints)
    {
        std::vector<double> thinned;
        const double stride = static_cast<double>(times.size() - 1) / static_cast<double>(maxPoints - 1);
        for (std::size_t i = 0; i < maxPoints; ++i)
            thinned.push_back(times[std::min(times.size() - 1, static_cast<std::size_t>(std::lround(i * stride)))]);
        times.swap(thinned);
    }
    for (const double t : times)
    {
        const auto p = m_impl->poseOf(a, t);
        if (p.present && (out.empty() || glm::length(p.position - out.back()) > 1.0))
            out.push_back(p.position);
    }
    if (out.size() < 2)
        out.clear();
    return out;
}

std::string DockServiceTraffic::destination(std::uint64_t actorId) const
{
    const auto it = m_impl->actors.find(actorId);
    if (it == m_impl->actors.end())
        return {};
    const auto& a = it->second;
    switch (a.phase)
    {
        case ActorPhase::Outbound:
            return a.cargo.destination.empty() ? m_impl->config.surfaceName : a.cargo.destination;
        case ActorPhase::FarLeg:
        case ActorPhase::FarWait:
            return "far point 20 km";
        case ActorPhase::Docked:
        case ActorPhase::DepartureWait:
            return "pad " + std::string(a.padIndex >= 0 ? hex::padIdFor(a.padIndex) : "?");
        case ActorPhase::Lift:
        case ActorPhase::ToChamberGate:
        case ActorPhase::CorridorOut:
        case ActorPhase::AxialOut:
        case ActorPhase::Disperse:
            return a.kind == TrafficShipKind::CargoShuttle ? (a.cargo.destination.empty() ? m_impl->config.surfaceName : a.cargo.destination)
                                                           : std::string("far point 20 km");
        case ActorPhase::Inbound:
        case ActorPhase::WaitGate:
        case ActorPhase::ToHold:
        case ActorPhase::AtHold:
            return "hub dock (queue)";
        default:
            return a.padIndex >= 0 ? "pad " + std::string(hex::padIdFor(a.padIndex)) : std::string("hub dock");
    }
}

double DockServiceTraffic::controlledZoneRadius() const
{
    double r = std::abs(m_impl->g.departureTurnZ);
    for (const auto& gate : m_impl->gates)
        r = std::max(r, glm::length(gate.position));
    return r;
}

const TrafficShipClass& DockServiceTraffic::actorClass(std::uint64_t actorId) const
{
    const auto it = m_impl->actors.find(actorId);
    return trafficShipClass(it == m_impl->actors.end() ? TrafficShipKind::Cobrett : it->second.kind);
}

bool DockServiceTraffic::beginStationCapture(
    std::uint64_t shipId, TrafficShipKind classKind, const TrafficPose& fixedPose, double nowSeconds)
{
    auto& impl = *m_impl;
    if (impl.actors.count(shipId) != 0)
        return false;
    const auto status = impl.dispatcher.status(shipId);
    if (status.phase != game::docking::traffic::DockTrafficPhase::EntryCleared)
        return false;
    impl.now = std::max(impl.now, nowSeconds);
    Impl::Actor a;
    a.id = shipId;
    a.kind = classKind;
    a.callsign = "PILOT-" + std::to_string(shipId);
    a.external = true;
    a.padIndex = hex::indexOfId(status.padId);
    a.serviceUntil = std::numeric_limits<double>::infinity();
    const TrafficShipClass& c = trafficShipClass(classKind);
    const auto& g = impl.g;
    // Current pose in the ROTATING frame, then align onto the slot axis.
    const glm::dmat3 toRot = glm::transpose(hex::rotationAboutZ(impl.phaseAt(impl.now)));
    a.current = Segment {};
    a.current.kind = SegmentKind::RotMove;
    a.current.turnBasis = true;
    a.current.turnStart = 0.0;
    a.current.turnEnd = 0.6;
    a.current.t0 = impl.now;
    a.current.p0 = toRot * fixedPose.position;
    a.current.p1 = {0.0, 0.0, g.entryFaceZ - c.length * 0.5 - 2.0};
    a.current.b0 = toRot * fixedPose.basis;
    a.current.b1 = hex::shipBasis({0.0, 0.0, 1.0}, {0.0, 1.0, 0.0});
    // The pilot's own closing speed is kept (no stop at the capture point).
    const double v0 = std::clamp(glm::dot(fixedPose.velocity, glm::dvec3(0.0, 0.0, 1.0)), 0.0, 25.0);
    a.current.profile = Profile::make(glm::length(a.current.p1 - a.current.p0), v0, EntrySpeedMps, 0.8, std::max({v0, EntrySpeedMps, 20.0}));
    a.current.duration = a.current.profile.total;
    Segment corridor;
    corridor.kind = SegmentKind::RotMove;
    corridor.p0 = a.current.p1;
    corridor.p1 = {0.0, 0.0, g.chamberFrontZ + c.length * 0.5 + 5.0};
    corridor.b0 = a.current.b1;
    corridor.profile = Profile::make(corridor.p1.z - corridor.p0.z, EntrySpeedMps, ChamberGateSpeedMps, 0.6, EntrySpeedMps);
    corridor.duration = corridor.profile.total;
    impl.actors[shipId] = a;
    Impl::Actor& stored = impl.actors[shipId];
    impl.push(stored, corridor);
    impl.setPhase(stored, ActorPhase::Corridor);
    impl.say(shipId, "station capture at the portal -> pad " + status.padId);
    return true;
}

bool DockServiceTraffic::requestExternalDeparture(std::uint64_t shipId, double nowSeconds)
{
    auto it = m_impl->actors.find(shipId);
    if (it == m_impl->actors.end() || !it->second.external || it->second.phase != ActorPhase::Docked)
        return false;
    if (!m_impl->dispatcher.requestDeparture(shipId, nowSeconds))
        return false;
    m_impl->now = std::max(m_impl->now, nowSeconds);
    m_impl->setPhase(it->second, ActorPhase::DepartureWait);
    m_impl->say(shipId, "departure requested by the pilot");
    return true;
}

bool DockServiceTraffic::isExternal(std::uint64_t shipId) const
{
    const auto it = m_impl->actors.find(shipId);
    return it != m_impl->actors.end() && it->second.external;
}

game::docking::traffic::DockTrafficController& DockServiceTraffic::dispatcher()
{
    return m_impl->dispatcher;
}

const game::docking::traffic::DockTrafficController& DockServiceTraffic::dispatcher() const
{
    return m_impl->dispatcher;
}

std::vector<TrafficLogLine> DockServiceTraffic::drainLog()
{
    std::vector<TrafficLogLine> out;
    out.swap(m_impl->log);
    std::stable_sort(out.begin(), out.end(), [](const TrafficLogLine& a, const TrafficLogLine& b) {
        return a.timeSeconds < b.timeSeconds;
    });
    return out;
}

const DockServiceTraffic::Stats& DockServiceTraffic::stats() const
{
    return m_impl->stats;
}

} // namespace game::traffic::dock
