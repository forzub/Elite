#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/game/network/WireProtocol.h"

namespace game::network::wire
{

/*
    Compression is intentionally a byte-to-byte stage.

    A compressor must never know how many ships, modules, map objects or other
    logical fields are inside a payload. WireDataCodec serializes one logical
    message first; compression only sees the resulting byte buffer.
*/
enum class WireCompressionMethod : std::uint8_t
{
    None = 0
};

inline constexpr std::size_t WireCompressionEnvelopeBytes = 5u;

struct CompressedWirePayload
{
    WireCompressionMethod method = WireCompressionMethod::None;
    std::uint32_t uncompressedBytes = 0;
    std::vector<std::uint8_t> bytes;
};

class IWireCompressor
{
public:
    virtual ~IWireCompressor() = default;

    virtual WireCompressionMethod method() const noexcept = 0;

    virtual bool compress(
        const std::vector<std::uint8_t>& input,
        std::vector<std::uint8_t>& output) const = 0;

    virtual bool decompress(
        const std::vector<std::uint8_t>& input,
        std::size_t expectedUncompressedBytes,
        std::vector<std::uint8_t>& output) const = 0;
};

class NoWireCompression final : public IWireCompressor
{
public:
    WireCompressionMethod method() const noexcept override
    {
        return WireCompressionMethod::None;
    }

    bool compress(
        const std::vector<std::uint8_t>& input,
        std::vector<std::uint8_t>& output) const override
    {
        if (input.size() > MaxWirePayloadBytes)
            return false;
        output = input;
        return true;
    }

    bool decompress(
        const std::vector<std::uint8_t>& input,
        std::size_t expectedUncompressedBytes,
        std::vector<std::uint8_t>& output) const override
    {
        if (expectedUncompressedBytes > MaxWirePayloadBytes ||
            input.size() != expectedUncompressedBytes)
        {
            return false;
        }

        output = input;
        return true;
    }
};


inline bool encodeCompressedWirePayloadEnvelope(
    const CompressedWirePayload& payload,
    std::vector<std::uint8_t>& outBytes)
{
    if (payload.method != WireCompressionMethod::None ||
        payload.uncompressedBytes > MaxWirePayloadBytes ||
        payload.bytes.size() > MaxWirePayloadBytes ||
        payload.bytes.size() + WireCompressionEnvelopeBytes > MaxWirePayloadBytes)
    {
        return false;
    }

    WireWriter writer;
    writer.u8(static_cast<std::uint8_t>(payload.method));
    writer.u32(payload.uncompressedBytes);
    outBytes = writer.take();
    outBytes.insert(outBytes.end(), payload.bytes.begin(), payload.bytes.end());
    return true;
}

inline bool decodeCompressedWirePayloadEnvelope(
    const std::vector<std::uint8_t>& bytes,
    CompressedWirePayload& outPayload)
{
    if (bytes.size() < WireCompressionEnvelopeBytes ||
        bytes.size() > MaxWirePayloadBytes)
    {
        return false;
    }

    WireReader reader(bytes.data(), WireCompressionEnvelopeBytes);
    std::uint8_t method = 0;
    std::uint32_t uncompressedBytes = 0;
    if (!reader.u8(method) ||
        !reader.u32(uncompressedBytes) ||
        !reader.empty() ||
        method > static_cast<std::uint8_t>(WireCompressionMethod::None) ||
        uncompressedBytes > MaxWirePayloadBytes)
    {
        return false;
    }

    outPayload = {};
    outPayload.method = static_cast<WireCompressionMethod>(method);
    outPayload.uncompressedBytes = uncompressedBytes;
    outPayload.bytes.assign(
        bytes.begin() + static_cast<std::ptrdiff_t>(WireCompressionEnvelopeBytes),
        bytes.end()
    );
    return true;
}

inline bool compressWirePayload(
    const std::vector<std::uint8_t>& rawBytes,
    const IWireCompressor& compressor,
    CompressedWirePayload& outPayload)
{
    if (rawBytes.size() > MaxWirePayloadBytes)
        return false;

    outPayload = {};
    outPayload.method = compressor.method();
    outPayload.uncompressedBytes =
        static_cast<std::uint32_t>(rawBytes.size());

    return compressor.compress(rawBytes, outPayload.bytes) &&
        outPayload.bytes.size() <= MaxWirePayloadBytes;
}

inline bool decompressWirePayload(
    const CompressedWirePayload& payload,
    const IWireCompressor& compressor,
    std::vector<std::uint8_t>& outRawBytes)
{
    if (payload.method != compressor.method() ||
        payload.uncompressedBytes > MaxWirePayloadBytes ||
        payload.bytes.size() > MaxWirePayloadBytes)
    {
        return false;
    }

    return compressor.decompress(
        payload.bytes,
        payload.uncompressedBytes,
        outRawBytes
    ) && outRawBytes.size() == payload.uncompressedBytes;
}

} // namespace game::network::wire
