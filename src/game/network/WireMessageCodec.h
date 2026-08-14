#pragma once

#include <cstdint>
#include <vector>

#include "src/game/network/WireCompression.h"
#include "src/game/network/WireDataCodec.h"
#include "src/game/network/WireProtocol.h"

namespace game::network::wire
{

/*
    Typed message <-> WireFrame payload bridge.

    This layer knows which logical protocol object belongs to each frame kind.
    It does NOT know how bytes are transported. TcpWireStream only sees a
    WireMessageKind plus an opaque payload byte buffer.

    Data-plane objects are serialized completely first, then passed through the
    schema-blind compressor envelope. Control-plane messages stay uncompressed
    because they are tiny and latency-sensitive.
*/
inline bool encodeMessagePayload(
    const SessionWelcome& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor&)
{
    outKind = WireMessageKind::SessionWelcome;
    return encodeSessionWelcome(value, outPayload);
}

inline bool encodeMessagePayload(
    const ClientMessage& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor&)
{
    outKind = WireMessageKind::ClientMessage;
    return encodeClientMessage(value, outPayload);
}

inline bool encodeMessagePayload(
    const MapRequest& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor&)
{
    outKind = WireMessageKind::MapRequest;
    return encodeMapRequest(value, outPayload);
}

inline bool encodeMessagePayload(
    const TimeSyncRequest& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor&)
{
    outKind = WireMessageKind::TimeSyncRequest;
    return encodeTimeSyncRequest(value, outPayload);
}

inline bool encodeMessagePayload(
    const TimeSyncResponse& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor&)
{
    outKind = WireMessageKind::TimeSyncResponse;
    return encodeTimeSyncResponse(value, outPayload);
}

inline bool encodeCompressedDataPlanePayload(
    const std::vector<std::uint8_t>& rawPayload,
    const IWireCompressor& compressor,
    std::vector<std::uint8_t>& outPayload)
{
    CompressedWirePayload compressed;
    return compressWirePayload(rawPayload, compressor, compressed) &&
        encodeCompressedWirePayloadEnvelope(compressed, outPayload);
}

inline bool decodeCompressedDataPlanePayload(
    const std::vector<std::uint8_t>& payload,
    const IWireCompressor& compressor,
    std::vector<std::uint8_t>& outRawPayload)
{
    CompressedWirePayload compressed;
    return decodeCompressedWirePayloadEnvelope(payload, compressed) &&
        decompressWirePayload(compressed, compressor, outRawPayload);
}

inline bool encodeMessagePayload(
    const SimulationSnapshot& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor& compressor)
{
    std::vector<std::uint8_t> rawPayload;
    if (!encodeSimulationSnapshot(value, rawPayload))
        return false;

    outKind = WireMessageKind::SimulationSnapshot;
    return encodeCompressedDataPlanePayload(
        rawPayload,
        compressor,
        outPayload
    );
}

inline bool encodeMessagePayload(
    const MapResponse& value,
    WireMessageKind& outKind,
    std::vector<std::uint8_t>& outPayload,
    const IWireCompressor& compressor)
{
    std::vector<std::uint8_t> rawPayload;
    if (!encodeMapResponse(value, rawPayload))
        return false;

    outKind = WireMessageKind::MapResponse;
    return encodeCompressedDataPlanePayload(
        rawPayload,
        compressor,
        outPayload
    );
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    SessionWelcome& outValue,
    const IWireCompressor&)
{
    return frame.kind == WireMessageKind::SessionWelcome &&
        decodeSessionWelcome(frame.payload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    ClientMessage& outValue,
    const IWireCompressor&)
{
    return frame.kind == WireMessageKind::ClientMessage &&
        decodeClientMessage(frame.payload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    MapRequest& outValue,
    const IWireCompressor&)
{
    return frame.kind == WireMessageKind::MapRequest &&
        decodeMapRequest(frame.payload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    TimeSyncRequest& outValue,
    const IWireCompressor&)
{
    return frame.kind == WireMessageKind::TimeSyncRequest &&
        decodeTimeSyncRequest(frame.payload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    TimeSyncResponse& outValue,
    const IWireCompressor&)
{
    return frame.kind == WireMessageKind::TimeSyncResponse &&
        decodeTimeSyncResponse(frame.payload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    SimulationSnapshot& outValue,
    const IWireCompressor& compressor)
{
    if (frame.kind != WireMessageKind::SimulationSnapshot)
        return false;

    std::vector<std::uint8_t> rawPayload;
    return decodeCompressedDataPlanePayload(
               frame.payload,
               compressor,
               rawPayload
           ) &&
        decodeSimulationSnapshot(rawPayload, outValue);
}

inline bool decodeMessagePayload(
    const WireFrame& frame,
    MapResponse& outValue,
    const IWireCompressor& compressor)
{
    if (frame.kind != WireMessageKind::MapResponse)
        return false;

    std::vector<std::uint8_t> rawPayload;
    return decodeCompressedDataPlanePayload(
               frame.payload,
               compressor,
               rawPayload
           ) &&
        decodeMapResponse(rawPayload, outValue);
}

} // namespace game::network::wire
