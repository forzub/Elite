#pragma once

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
inline constexpr std::uint16_t SimulationSnapshotWireSchemaVersion = 54u;   // 48: motion.jFieldLevelMps2; 49: hub security (motion.security*, city promotedAirSlots); 50: session.trade (TRADE_DESIGN §7.5); 51: session.trade deal rules + spendableCr (trade step 3); 52: session.trade cargo[].dutyShare (trade step 4); 53: session.trade board / contracts / dayNow (trade step 5); 54: session.wallet garnish (debt, withheld, protected)
inline constexpr std::uint16_t MapResponseWireSchemaVersion = 2u;

inline bool encodeSimulationSnapshot(
    const SimulationSnapshot& value,
    std::vector<std::uint8_t>& outPayload)
{
    WireWriter writer;
    writer.u16(SimulationSnapshotWireSchemaVersion);

    if (!binary::encodeValue(writer, value))
        return false;

    outPayload = writer.take();
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
