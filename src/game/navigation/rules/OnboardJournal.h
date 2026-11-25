#pragma once

// The onboard computer's JOURNAL (author 2026-11-24): what the ship met, recorded by the computer itself - hubs and
// inhabited zones (coordinates + description), safe / dangerous places, signals known and unknown, anomalous behaviour of
// space, the exits into sumps, wrecks. Every map shows these marks as their own layer. Strong radiation degrades the
// records (the coordinates blur, then the fields become unreadable, then the record is lost).
//
// The journal lives in the computer: a cockpit-capsule carries it away from a lost hull (memory cockpit-capsule-rescue);
// a capsule with no hub / beacon around flies to the nearest settlement it KNOWS from here. It is kept and written on
// the CLIENT (the player's knowledge; author «всё, что можно хранить на клиенте - хранить и делать на клиенте»).
//
// Positions are kept relative to an anchor body (a planet / moon / the star) when the place moves with one (an orbital
// hub), so an old record still points at the right place; otherwise system-barycentric.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::journal
{

enum class Kind : std::uint8_t
{
    Hub = 0,             // a hub (its id, name, role, docks)
    InhabitedZone = 1,   // the comms reach of an inhabited place (rescue services hear an SOS here)
    Safe = 2,            // a known safe place (a parking orbit, a clear passage)
    Danger = 3,          // a dangerous place (an impact, a dense swarm, radiation)
    Signal = 4,          // a signal heard (known: its source named; unknown: only where / when)
    Anomaly = 5,         // anomalous behaviour of space (a current, a distortion)
    SumpExit = 6,        // an exit into a sump (JumpSumps)
    Wreck = 7,           // a wreck (one's own lost hull too)
    Beacon = 8           // a capsule beacon
};

inline const char* kindTag(Kind k)
{
    switch (k)
    {
    case Kind::Hub: return "hub";
    case Kind::InhabitedZone: return "zone";
    case Kind::Safe: return "safe";
    case Kind::Danger: return "danger";
    case Kind::Signal: return "signal";
    case Kind::Anomaly: return "anomaly";
    case Kind::SumpExit: return "sump";
    case Kind::Wreck: return "wreck";
    case Kind::Beacon: return "beacon";
    }
    return "?";
}

inline const char* kindRu(Kind k)
{
    switch (k)
    {
    case Kind::Hub: return "хаб";
    case Kind::InhabitedZone: return "обитаемая зона";
    case Kind::Safe: return "безопасное место";
    case Kind::Danger: return "опасное место";
    case Kind::Signal: return "сигнал";
    case Kind::Anomaly: return "аномалия";
    case Kind::SumpExit: return "выход в отстойник";
    case Kind::Wreck: return "обломки";
    case Kind::Beacon: return "маяк";
    }
    return "?";
}

struct Entry
{
    Kind kind = Kind::Hub;
    std::string key;            // the identity (hub id, beacon id, "<system>:<cell>" for a place): a newer record replaces
    int systemId = -1;
    std::string anchorBodyId;   // empty: system-barycentric
    glm::dvec3 positionM {0.0}; // relative to the anchor body's centre (or the barycentre)
    double errorM = 0.0;        // the position's uncertainty (grows under radiation)
    double radiusM = 0.0;       // the place's reach (a zone's comms radius, a danger's extent; 0 a point)
    std::string label;          // a name ("Высокий", "SOS 3.2 MHz")
    std::string detail;         // a description ("доки: МЧС, ремонт; площадки 12")
    double recordedAtS = 0.0;   // the universe time of the record
    float reliability01 = 1.0f; // 1 clean; lower: degraded (radiation); 0 unreadable
    bool hasRescueDock = false; // a hub with a МЧС dock
};

// put a record in: the same key -> the fresher one wins (a degraded fresh record still beats an older clean one: the
// computer writes over what it saw again); returns true when the journal changed
inline bool store(std::vector<Entry>& journal, const Entry& e)
{
    for (auto& j : journal)
        if (j.kind == e.kind && j.key == e.key)
        {
            if (e.recordedAtS < j.recordedAtS)
                return false;
            // a place that moves about its anchor (an orbiting hub, a Lagrange beacon) is the same place: only a move
            // beyond 2 % of its distance (or a changed description) is a new record
            const double moveTolerance = std::max(1.0, 0.02 * glm::length(e.positionM));
            const bool changed = j.label != e.label || j.detail != e.detail || j.anchorBodyId != e.anchorBodyId ||
                                 glm::length(j.positionM - e.positionM) > moveTolerance || j.reliability01 != e.reliability01 ||
                                 j.hasRescueDock != e.hasRescueDock || std::abs(j.radiusM - e.radiusM) > 1.0;
            // a re-sighting refreshes the time; only a change in content counts as a change of the journal
            j = e;
            return changed;
        }
    journal.push_back(e);
    return true;
}

// Radiation (author: «информация может деградировать из-за сильной радиации»). The computer's memory is hardened: a
// dose RATE below RadiationHardeningGyPerHour does nothing (the caller passes only the dose taken above that rate). Each
// such gray takes ~2 % of the reliability of every record (a spread kept deterministic by the key), the coordinates blur
// by (1 - reliability) x 5 % of the distance to the anchor, a record under 0.25 loses its description, under 0.05 it is
// gone.
inline constexpr double RadiationHardeningGyPerHour = 1.0;
inline constexpr double ReliabilityLossPerGy = 0.02;

// the part of a dose that counts: the dose of a step of `seconds` minus what the hardening takes at its rate
inline double damagingDoseGy(double doseGy, double seconds)
{
    return std::max(0.0, doseGy - RadiationHardeningGyPerHour * seconds / 3600.0);
}

inline std::uint32_t keyHash(const std::string& s)
{
    std::uint32_t h = 2166136261u;
    for (const char c : s)
        h = (h ^ static_cast<std::uint8_t>(c)) * 16777619u;
    return h;
}

// returns the number of records lost
inline int degrade(std::vector<Entry>& journal, double doseGy)
{
    if (doseGy <= 0.0)
        return 0;
    int lost = 0;
    for (auto& e : journal)
    {
        const double spread = 0.5 + (keyHash(e.key) % 1000u) / 1000.0;   // 0.5 .. 1.5
        const double loss = doseGy * ReliabilityLossPerGy * spread;
        e.reliability01 = static_cast<float>(std::max(0.0, e.reliability01 - loss));
        e.errorM = std::max(e.errorM, (1.0 - e.reliability01) * 0.05 * glm::length(e.positionM));
        if (e.reliability01 < 0.25f)
            e.detail.clear();
    }
    const auto before = journal.size();
    journal.erase(std::remove_if(journal.begin(), journal.end(), [](const Entry& e) { return e.reliability01 < 0.05f; }),
                  journal.end());
    lost = static_cast<int>(before - journal.size());
    return lost;
}

// ---- text on the wire / in the save: one record a line, fields '|'-separated ('|' and newlines removed from text)
inline std::string clean(std::string s)
{
    for (char& c : s)
        if (c == '|' || c == '\n' || c == '\r')
            c = ' ';
    return s;
}

inline std::string format(const std::vector<Entry>& journal)
{
    std::ostringstream o;
    o.precision(17);
    for (const auto& e : journal)
        o << static_cast<int>(e.kind) << '|' << clean(e.key) << '|' << e.systemId << '|' << clean(e.anchorBodyId) << '|'
          << e.positionM.x << '|' << e.positionM.y << '|' << e.positionM.z << '|' << e.errorM << '|' << e.radiusM << '|'
          << clean(e.label) << '|' << clean(e.detail) << '|' << e.recordedAtS << '|' << e.reliability01 << '|'
          << (e.hasRescueDock ? 1 : 0) << '\n';
    return o.str();
}

inline std::vector<Entry> parse(const std::string& text)
{
    std::vector<Entry> out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line))
    {
        std::vector<std::string> f;
        std::string cur;
        for (const char c : line)
            if (c == '|')
            {
                f.push_back(cur);
                cur.clear();
            }
            else
                cur += c;
        f.push_back(cur);
        if (f.size() != 14)
            continue;
        try
        {
            Entry e;
            e.kind = static_cast<Kind>(std::stoi(f[0]));
            e.key = f[1];
            e.systemId = std::stoi(f[2]);
            e.anchorBodyId = f[3];
            e.positionM = glm::dvec3(std::stod(f[4]), std::stod(f[5]), std::stod(f[6]));
            e.errorM = std::stod(f[7]);
            e.radiusM = std::stod(f[8]);
            e.label = f[9];
            e.detail = f[10];
            e.recordedAtS = std::stod(f[11]);
            e.reliability01 = std::stof(f[12]);
            e.hasRescueDock = f[13] == "1";
            out.push_back(std::move(e));
        }
        catch (...)
        {
        }
    }
    return out;
}

} // namespace game::navigation::journal
