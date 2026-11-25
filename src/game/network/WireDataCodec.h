#pragma once

#include <algorithm>
#include <iostream>
#include <cstdint>
#include <vector>

#include "src/game/network/WireDataSchema.h"

namespace game::network::wire
{

/*
    Top-level data-plane codec.

    One complete logical object is serialized to one raw byte buffer first.
    Compression, framing and TCP operate on that buffer later and do not know
    how many entities or fields it contains.
*/
inline constexpr std::uint16_t SimulationSnapshotWireSchemaVersion = 76u;   // 76: ShipSnapshot radarContacts removed (the old RadarModule pipeline, off since the RadarScanReport); 75: ShipTransform's dead mirrors removed (targetSpeed, forwardVelocity, localVelocity, relativeVelocityMetersPerSecond); 74: motion capsuleDeltaVLeftMps (the cockpit-capsule's reaction mass); 73: radar scan observerWorldVelocityMps (own acceleration since the scan); 72: session.constructionOverrides (SRV-1); 71: radar track boundingRadiusMeters (the target's real size, TrajectoryConflict); 70: city occupiedBerths (the berth dispatcher); 69: ship landingGear; 68: ship padService (hub city pads); 48: motion.jFieldLevelMps2; 49: hub security (motion.security*, city promotedAirSlots); 50: session.trade (TRADE_DESIGN §7.5); 51: session.trade deal rules + spendableCr (trade step 3); 52: session.trade cargo[].dutyShare (trade step 4); 53: session.trade board / contracts / dayNow (trade step 5); 54: session.wallet garnish (debt, withheld, protected); 55: session.wallet income tax (trade step 7); 56: session.trade market[] regime (state price, cap, note; step 8); 57: cargo[] legalityHere + blackBidCrPerT (step 9); 58: session.wallet licences / offers / pilot categories (step 11); 59: session.trade insurance (step 13); 60: ship refit productOverrides (per-instance configuration); 61: session controlledEntityId + controlEpoch (ship change); 62: wallet vacancies + the job (employment service); 63: wallet sos / activeSosId / sarLicence (rescue); 64: wallet botCandidates / bots (step 15); 65: wallet buildOffers / buildProjects (step 16); 66: wallet siteOffers / mines (step 16 part 2); 67: mine stockT / sellLocal / here (part 3)
inline constexpr std::uint16_t MapResponseWireSchemaVersion = 2u;

inline bool encodeSimulationSnapshot(
    const SimulationSnapshot& value,
    std::vector<std::uint8_t>& outPayload)
{
    WireWriter writer;
    writer.u16(SimulationSnapshotWireSchemaVersion);

    if (!binary::encodeValue(writer, value))
    {
        std::cerr << "[Wire] snapshot: a field failed to encode (a string over the limit or a bad value)" << std::endl;
        return false;
    }

    outPayload = writer.take();
    if (outPayload.size() > MaxWirePayloadBytes)
    {
        // what makes it big: each part encoded alone
        const auto sizeOf = [](const auto& part) {
            WireWriter w;
            (void)binary::encodeValue(w, part);
            return w.take().size();
        };
        {
            // the heaviest objects (type, graph share)
            std::vector<std::pair<std::size_t, std::size_t>> sizes;
            for (std::size_t i = 0; i < value.objects.size(); ++i)
                sizes.emplace_back(sizeOf(value.objects[i]), i);
            std::sort(sizes.rbegin(), sizes.rend());
            for (std::size_t k = 0; k < sizes.size() && k < 6; ++k)
            {
                const auto& o = value.objects[sizes[k].second];
                std::cerr << "[Wire]   object " << o.id.value << " type " << static_cast<int>(o.type) << " '" << o.displayName
                          << "': " << sizes[k].first << " B, graph " << sizeOf(o.graph) << " B (modules "
                          << o.graph.modules.size() << " = " << sizeOf(o.graph.modules) << " B, links "
                          << o.graph.structuralLinks.size() << " = " << sizeOf(o.graph.structuralLinks) << " B, debug hit "
                          << o.graph.debugHitVolumes.size() << " = " << sizeOf(o.graph.debugHitVolumes) << " B, assembly "
                          << sizeOf(o.graph.assemblyModules) << " B)" << std::endl;
            }
            std::size_t over100k = 0, total100k = 0;
            for (const auto& s : sizes)
                if (s.first > 100000)
                {
                    ++over100k;
                    total100k += s.first;
                }
            std::cerr << "[Wire]   objects over 100 KB: " << over100k << ", " << total100k << " B together" << std::endl;
        }
        std::size_t biggestHub = 0, biggestHubIndex = 0;
        for (std::size_t i = 0; i < value.hubs.size(); ++i)
        {
            const std::size_t s = sizeOf(value.hubs[i]);
            if (s > biggestHub)
            {
                biggestHub = s;
                biggestHubIndex = i;
            }
        }
        std::cerr << "[Wire] snapshot: " << outPayload.size() << " bytes over the " << MaxWirePayloadBytes
                  << " limit - ships " << value.ships.size() << " = " << sizeOf(value.ships) << " B, objects "
                  << value.objects.size() << " = " << sizeOf(value.objects) << " B, hubs " << value.hubs.size() << " = "
                  << sizeOf(value.hubs) << " B (biggest #" << biggestHubIndex << " " << biggestHub << " B), signals "
                  << sizeOf(value.signals) << " B, session " << sizeOf(value.session) << " B" << std::endl;
    }
    return outPayload.size() <= MaxWirePayloadBytes;
}

inline bool decodeSimulationSnapshot(
    const std::vector<std::uint8_t>& payload,
    SimulationSnapshot& outValue)
{
    WireReader reader(payload);
    std::uint16_t schemaVersion = 0;

    if (!reader.u16(schemaVersion) ||
        schemaVersion != SimulationSnapshotWireSchemaVersion ||
        !binary::decodeValue(reader, outValue))
    {
        return false;
    }

    return finishDecode(reader);
}

inline bool encodeMapResponse(
    const MapResponse& value,
    std::vector<std::uint8_t>& outPayload)
{
    WireWriter writer;
    writer.u16(MapResponseWireSchemaVersion);

    if (!binary::encodeValue(writer, value))
        return false;

    outPayload = writer.take();
    return outPayload.size() <= MaxWirePayloadBytes;
}

inline bool decodeMapResponse(
    const std::vector<std::uint8_t>& payload,
    MapResponse& outValue)
{
    WireReader reader(payload);
    std::uint16_t schemaVersion = 0;

    if (!reader.u16(schemaVersion) ||
        schemaVersion != MapResponseWireSchemaVersion ||
        !binary::decodeValue(reader, outValue))
    {
        return false;
    }

    return finishDecode(reader);
}

} // namespace game::network::wire
