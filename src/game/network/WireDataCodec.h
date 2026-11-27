#pragma once

#include <algorithm>
#include <iostream>
#include <cstdint>
#include <vector>

#include "src/game/network/WireDataSchema.h"
#include "src/game/network/WireSnapshotDiagnostics.h"

namespace game::network::wire
{

/*
    Top-level data-plane codec.

    One complete logical object is serialized to one raw byte buffer first.
    Compression, framing and TCP operate on that buffer later and do not know
    how many entities or fields it contains.
*/
inline constexpr std::uint16_t SimulationSnapshotWireSchemaVersion = 85u;   // 85: ship padNotice split from padService (the control state); 84: refit ownerActor removed, refit.owned per receiving player; 83: motion manoeuvreGasDepleted removed; 82: motion travelFrame / referenceVelocity / local position+velocity removed (the referenceFrame carries them); 81: ShipTransform input axes / targetSpeedRate / jumpActive and 24 motion diagnostics without a client reader removed; 80: ObjectSnapshot float legacy position mirror removed; 79: motion deckActive / deckAxisPointLocal / deckOmegaLocal (the client predicts a ship on a turning deck by the turn), attachedObjectId + attachedLocal* (a ship coupled to a pad drawn from its city); 78: ShipTransform referenceVelocityMetersPerSecond removed (motion.referenceVelocityMps is the owner); 77: ShipTransform's legacy float position mirror removed; 76: ShipSnapshot radarContacts removed (the old RadarModule pipeline, off since the RadarScanReport); 75: ShipTransform's dead mirrors removed (targetSpeed, forwardVelocity, localVelocity, relativeVelocityMetersPerSecond); 74: motion capsuleDeltaVLeftMps (the cockpit-capsule's reaction mass); 73: radar scan observerWorldVelocityMps (own acceleration since the scan); 72: session.constructionOverrides (SRV-1); 71: radar track boundingRadiusMeters (the target's real size, TrajectoryConflict); 70: city occupiedBerths (the berth dispatcher); 69: ship landingGear; 68: ship padService (hub city pads); 48: motion.jFieldLevelMps2; 49: hub security (motion.security*, city promotedAirSlots); 50: session.trade (TRADE_DESIGN §7.5); 51: session.trade deal rules + spendableCr (trade step 3); 52: session.trade cargo[].dutyShare (trade step 4); 53: session.trade board / contracts / dayNow (trade step 5); 54: session.wallet garnish (debt, withheld, protected); 55: session.wallet income tax (trade step 7); 56: session.trade market[] regime (state price, cap, note; step 8); 57: cargo[] legalityHere + blackBidCrPerT (step 9); 58: session.wallet licences / offers / pilot categories (step 11); 59: session.trade insurance (step 13); 60: ship refit productOverrides (per-instance configuration); 61: session controlledEntityId + controlEpoch (ship change); 62: wallet vacancies + the job (employment service); 63: wallet sos / activeSosId / sarLicence (rescue); 64: wallet botCandidates / bots (step 15); 65: wallet buildOffers / buildProjects (step 16); 66: wallet siteOffers / mines (step 16 part 2); 67: mine stockT / sellLocal / here (part 3)
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
        reportOversizeSnapshot(value, outPayload.size());   // instrumentation (WireSnapshotDiagnostics.h)
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
