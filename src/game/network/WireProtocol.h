#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/network/ClientMessage.h"
#include "src/game/network/MapSnapshotMessage.h"
#include "src/game/network/SessionMessage.h"
#include "src/game/network/TimeSyncMessage.h"

namespace game::network::wire
{

/*
    Portable process/network wire foundation.

    The in-memory protocol structs are deliberately NOT memcpy'd to the wire:
    they contain std::string/std::vector/std::variant and their object layout is
    compiler/ABI dependent. Every scalar below is encoded explicitly in network
    byte order. IEEE-754 float/double bit patterns are transported as uint32/64.

    Stage M8A covers the connection/control plane. Stage M8B keeps the large
    data-plane schema out of this framing file: SimulationSnapshot/MapResponse
    field order lives in WireDataSchema.h, while WireDataCodec.h turns one
    complete logical object into one payload byte buffer. TcpTransport may then
    treat that payload as opaque bytes.
*/
inline constexpr std::uint32_t WireMagic = 0x454C4954u; // "ELIT"
inline constexpr std::uint16_t WireProtocolVersion = 4u;
inline constexpr std::uint32_t MaxWirePayloadBytes = 16u * 1024u * 1024u;
inline constexpr std::uint32_t MaxWireStringBytes = 1024u * 1024u;
inline constexpr std::size_t WireHeaderBytes = 20u;

enum class WireMessageKind : std::uint16_t
{
    SessionWelcome = 1,
    ClientMessage = 2,
    MapRequest = 3,
    TimeSyncRequest = 4,
    TimeSyncResponse = 5,

    // Data-plane ids. Payload field order is defined separately in
    // WireDataSchema.h so framing stays independent of world growth.
    SimulationSnapshot = 6,
    MapResponse = 7
};

struct WireFrame
{
    WireMessageKind kind = WireMessageKind::ClientMessage;
    std::uint64_t sequence = 0;
    std::vector<std::uint8_t> payload;
};

class WireWriter
{
public:
    const std::vector<std::uint8_t>& bytes() const noexcept
    {
        return m_bytes;
    }

    std::vector<std::uint8_t> take()
    {
        return std::move(m_bytes);
    }

    void u8(std::uint8_t value)
    {
        m_bytes.push_back(value);
    }

    void boolean(bool value)
    {
        u8(value ? 1u : 0u);
    }

    void u16(std::uint16_t value)
    {
        m_bytes.push_back(static_cast<std::uint8_t>((value >> 8u) & 0xFFu));
        m_bytes.push_back(static_cast<std::uint8_t>(value & 0xFFu));
    }

    void u32(std::uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
            m_bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }

    void u64(std::uint64_t value)
    {
        for (int shift = 56; shift >= 0; shift -= 8)
            m_bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xFFu));
    }

    void i32(std::int32_t value)
    {
        const std::uint32_t raw = value >= 0
            ? static_cast<std::uint32_t>(value)
            : std::numeric_limits<std::uint32_t>::max()
                - static_cast<std::uint32_t>(-(static_cast<std::int64_t>(value) + 1));
        u32(raw);
    }

    void i64(std::int64_t value)
    {
        const std::uint64_t raw = value >= 0
            ? static_cast<std::uint64_t>(value)
            : std::numeric_limits<std::uint64_t>::max()
                - static_cast<std::uint64_t>(-(value + 1));
        u64(raw);
    }

    void f32(float value)
    {
        static_assert(sizeof(float) == sizeof(std::uint32_t) &&
                      std::numeric_limits<float>::is_iec559,
            "wire protocol requires 32-bit IEEE-754 float storage");
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u32(bits);
    }

    void f64(double value)
    {
        static_assert(sizeof(double) == sizeof(std::uint64_t) &&
                      std::numeric_limits<double>::is_iec559,
            "wire protocol requires 64-bit IEEE-754 double storage");
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        u64(bits);
    }

    bool string(const std::string& value)
    {
        if (value.size() > MaxWireStringBytes ||
            value.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        u32(static_cast<std::uint32_t>(value.size()));
        m_bytes.insert(m_bytes.end(), value.begin(), value.end());
        return true;
    }

    void dvec3(const glm::dvec3& value)
    {
        f64(value.x);
        f64(value.y);
        f64(value.z);
    }

private:
    std::vector<std::uint8_t> m_bytes;
};

class WireReader
{
public:
    WireReader(const std::uint8_t* data, std::size_t size)
        : m_data(data)
        , m_size(size)
    {
    }

    explicit WireReader(const std::vector<std::uint8_t>& bytes)
        : WireReader(bytes.data(), bytes.size())
    {
    }

    bool good() const noexcept { return m_good; }
    bool empty() const noexcept { return m_offset == m_size; }
    std::size_t remaining() const noexcept
    {
        return m_offset <= m_size ? m_size - m_offset : 0u;
    }

    bool u8(std::uint8_t& value)
    {
        if (!require(1u))
            return false;
        value = m_data[m_offset++];
        return true;
    }

    bool boolean(bool& value)
    {
        std::uint8_t raw = 0;
        if (!u8(raw) || raw > 1u)
        {
            m_good = false;
            return false;
        }
        value = raw != 0u;
        return true;
    }

    bool u16(std::uint16_t& value)
    {
        if (!require(2u))
            return false;
        value =
            (static_cast<std::uint16_t>(m_data[m_offset]) << 8u) |
            static_cast<std::uint16_t>(m_data[m_offset + 1u]);
        m_offset += 2u;
        return true;
    }

    bool u32(std::uint32_t& value)
    {
        if (!require(4u))
            return false;
        value = 0u;
        for (int i = 0; i < 4; ++i)
            value = (value << 8u) | m_data[m_offset++];
        return true;
    }

    bool u64(std::uint64_t& value)
    {
        if (!require(8u))
            return false;
        value = 0u;
        for (int i = 0; i < 8; ++i)
            value = (value << 8u) | m_data[m_offset++];
        return true;
    }

    bool i32(std::int32_t& value)
    {
        std::uint32_t raw = 0;
        if (!u32(raw))
            return false;

        if (raw <= static_cast<std::uint32_t>(
                std::numeric_limits<std::int32_t>::max()))
        {
            value = static_cast<std::int32_t>(raw);
        }
        else
        {
            const std::uint32_t magnitudeMinusOne =
                std::numeric_limits<std::uint32_t>::max() - raw;
            value = -1 - static_cast<std::int32_t>(magnitudeMinusOne);
        }
        return true;
    }

    bool i64(std::int64_t& value)
    {
        std::uint64_t raw = 0;
        if (!u64(raw))
            return false;

        if (raw <= static_cast<std::uint64_t>(
                std::numeric_limits<std::int64_t>::max()))
        {
            value = static_cast<std::int64_t>(raw);
        }
        else
        {
            const std::uint64_t magnitudeMinusOne =
                std::numeric_limits<std::uint64_t>::max() - raw;
            value = -1 - static_cast<std::int64_t>(magnitudeMinusOne);
        }
        return true;
    }

    bool f32(float& value)
    {
        std::uint32_t bits = 0;
        if (!u32(bits))
            return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool f64(double& value)
    {
        std::uint64_t bits = 0;
        if (!u64(bits))
            return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }

    bool string(std::string& value)
    {
        std::uint32_t size = 0;
        if (!u32(size) || size > MaxWireStringBytes || !require(size))
        {
            m_good = false;
            return false;
        }

        value.assign(
            reinterpret_cast<const char*>(m_data + m_offset),
            static_cast<std::size_t>(size)
        );
        m_offset += size;
        return true;
    }

    bool dvec3(glm::dvec3& value)
    {
        return f64(value.x) && f64(value.y) && f64(value.z);
    }

private:
    bool require(std::size_t count)
    {
        if (!m_good || count > remaining())
        {
            m_good = false;
            return false;
        }
        return true;
    }

    const std::uint8_t* m_data = nullptr;
    std::size_t m_size = 0u;
    std::size_t m_offset = 0u;
    bool m_good = true;
};

inline std::vector<std::uint8_t> encodeFrame(const WireFrame& frame)
{
    if (frame.payload.size() > MaxWirePayloadBytes ||
        frame.payload.size() > std::numeric_limits<std::uint32_t>::max())
    {
        return {};
    }

    WireWriter writer;
    writer.u32(WireMagic);
    writer.u16(WireProtocolVersion);
    writer.u16(static_cast<std::uint16_t>(frame.kind));
    writer.u32(static_cast<std::uint32_t>(frame.payload.size()));
    writer.u64(frame.sequence);

    auto bytes = writer.take();
    bytes.insert(bytes.end(), frame.payload.begin(), frame.payload.end());
    return bytes;
}

class WireFrameDecoder
{
public:
    void push(const std::uint8_t* data, std::size_t size)
    {
        if (m_failed || data == nullptr || size == 0u)
            return;
        m_buffer.insert(m_buffer.end(), data, data + size);
    }

    void push(const std::vector<std::uint8_t>& bytes)
    {
        push(bytes.data(), bytes.size());
    }

    bool pop(WireFrame& outFrame)
    {
        if (m_failed || m_buffer.size() < WireHeaderBytes)
            return false;

        WireReader header(m_buffer.data(), WireHeaderBytes);
        std::uint32_t magic = 0;
        std::uint16_t version = 0;
        std::uint16_t kind = 0;
        std::uint32_t payloadSize = 0;
        std::uint64_t sequence = 0;

        if (!header.u32(magic) ||
            !header.u16(version) ||
            !header.u16(kind) ||
            !header.u32(payloadSize) ||
            !header.u64(sequence) ||
            !header.empty())
        {
            fail("malformed wire header");
            return false;
        }

        if (magic != WireMagic)
        {
            fail("wire magic mismatch");
            return false;
        }

        if (version != WireProtocolVersion)
        {
            fail("wire protocol version mismatch");
            return false;
        }

        if (payloadSize > MaxWirePayloadBytes)
        {
            fail("wire payload exceeds protocol limit");
            return false;
        }

        const std::size_t totalSize =
            WireHeaderBytes + static_cast<std::size_t>(payloadSize);
        if (m_buffer.size() < totalSize)
            return false;

        outFrame.kind = static_cast<WireMessageKind>(kind);
        outFrame.sequence = sequence;
        outFrame.payload.assign(
            m_buffer.begin() + static_cast<std::ptrdiff_t>(WireHeaderBytes),
            m_buffer.begin() + static_cast<std::ptrdiff_t>(totalSize)
        );

        m_buffer.erase(
            m_buffer.begin(),
            m_buffer.begin() + static_cast<std::ptrdiff_t>(totalSize)
        );
        return true;
    }

    bool failed() const noexcept { return m_failed; }
    const std::string& error() const noexcept { return m_error; }
    std::size_t bufferedBytes() const noexcept { return m_buffer.size(); }

private:
    void fail(std::string message)
    {
        m_failed = true;
        m_error = std::move(message);
    }

    std::vector<std::uint8_t> m_buffer;
    bool m_failed = false;
    std::string m_error;
};

inline bool finishDecode(WireReader& reader)
{
    return reader.good() && reader.empty();
}

inline bool encodeSessionWelcome(
    const SessionWelcome& value,
    std::vector<std::uint8_t>& outPayload
)
{
    WireWriter writer;
    writer.u64(value.sessionId.value);
    writer.u64(value.playerId.value);
    writer.u64(value.controlledShipInstanceId);
    writer.u32(value.controlledEntityId.value);
    writer.f64(value.fixedStepSeconds);
    writer.u32(value.starAtlasCatalog.schemaVersion);
    writer.u64(value.starAtlasCatalog.contentFingerprint);
    outPayload = writer.take();
    return true;
}

inline bool decodeSessionWelcome(
    const std::vector<std::uint8_t>& payload,
    SessionWelcome& outValue
)
{
    WireReader reader(payload);
    std::uint32_t controlled = 0;
    if (!reader.u64(outValue.sessionId.value) ||
        !reader.u64(outValue.playerId.value) ||
        !reader.u64(outValue.controlledShipInstanceId) ||
        !reader.u32(controlled) ||
        !reader.f64(outValue.fixedStepSeconds) ||
        !reader.u32(outValue.starAtlasCatalog.schemaVersion) ||
        !reader.u64(outValue.starAtlasCatalog.contentFingerprint) ||
        !finishDecode(reader))
    {
        return false;
    }

    outValue.controlledEntityId = EntityId{controlled};
    return true;
}

inline bool encodeShipControlState(
    WireWriter& writer,
    const ShipControlState& value
)
{
    writer.boolean(value.cruiseActive);
    writer.boolean(value.jumpActive);
    writer.f32(value.pitchInput);
    writer.f32(value.yawInput);
    writer.f32(value.rollInput);
    writer.f32(value.targetSpeedRate);
    writer.boolean(value.assistedMaxSpeedCommand);
    writer.f32(value.strafeInput);
    writer.f32(value.liftInput);
    writer.f32(value.forwardInput);
    writer.boolean(value.localControlLawCommandValid);
    writer.u8(static_cast<std::uint8_t>(value.requestedLocalControlLaw));
    writer.u8(static_cast<std::uint8_t>(value.velocityAlignmentCommand));
    writer.u64(value.controlTick);
    return true;
}

inline bool decodeShipControlState(
    WireReader& reader,
    ShipControlState& outValue
)
{
    std::uint8_t controlLaw = 0;
    std::uint8_t alignmentMode = 0;

    if (!reader.boolean(outValue.cruiseActive) ||
        !reader.boolean(outValue.jumpActive) ||
        !reader.f32(outValue.pitchInput) ||
        !reader.f32(outValue.yawInput) ||
        !reader.f32(outValue.rollInput) ||
        !reader.f32(outValue.targetSpeedRate) ||
        !reader.boolean(outValue.assistedMaxSpeedCommand) ||
        !reader.f32(outValue.strafeInput) ||
        !reader.f32(outValue.liftInput) ||
        !reader.f32(outValue.forwardInput) ||
        !reader.boolean(outValue.localControlLawCommandValid) ||
        !reader.u8(controlLaw) ||
        !reader.u8(alignmentMode) ||
        !reader.u64(outValue.controlTick))
    {
        return false;
    }

    if (controlLaw > static_cast<std::uint8_t>(
            game::navigation::LocalFlightControlLaw::Assisted) ||
        alignmentMode > static_cast<std::uint8_t>(
            game::navigation::VelocityAlignmentMode::BrakeToStop))
    {
        return false;
    }

    outValue.requestedLocalControlLaw =
        static_cast<game::navigation::LocalFlightControlLaw>(controlLaw);
    outValue.velocityAlignmentCommand =
        static_cast<game::navigation::VelocityAlignmentMode>(alignmentMode);
    return true;
}

inline bool encodeClientShipCommand(
    WireWriter& writer,
    const ClientShipCommand& value
)
{
    writer.u8(static_cast<std::uint8_t>(value.type));
    writer.i32(value.index);
    writer.f64(value.amount);
    return true;
}

inline bool decodeClientShipCommand(
    WireReader& reader,
    ClientShipCommand& outValue
)
{
    std::uint8_t type = 0;
    std::int32_t index = 0;
    if (!reader.u8(type) ||
        !reader.i32(index) ||
        !reader.f64(outValue.amount))
    {
        return false;
    }

    if (type > static_cast<std::uint8_t>(
            ClientShipCommand::StartBestRepairJob))
    {
        return false;
    }

    outValue.type = static_cast<ClientShipCommand::Type>(type);
    outValue.index = index;
    return true;
}

inline bool encodeClientMessage(
    const ClientMessage& value,
    std::vector<std::uint8_t>& outPayload
)
{
    WireWriter writer;
    writer.u64(value.clientTick);

    if (std::holds_alternative<ShipControlState>(value.payload))
    {
        writer.u8(0u);
        encodeShipControlState(
            writer,
            std::get<ShipControlState>(value.payload)
        );
    }
    else if (std::holds_alternative<ClientShipCommand>(value.payload))
    {
        writer.u8(1u);
        encodeClientShipCommand(
            writer,
            std::get<ClientShipCommand>(value.payload)
        );
    }
    else
    {
        return false;
    }

    outPayload = writer.take();
    return outPayload.size() <= MaxWirePayloadBytes;
}

inline bool decodeClientMessage(
    const std::vector<std::uint8_t>& payload,
    ClientMessage& outValue
)
{
    WireReader reader(payload);
    std::uint8_t variantTag = 0;
    if (!reader.u64(outValue.clientTick) || !reader.u8(variantTag))
        return false;

    if (variantTag == 0u)
    {
        ShipControlState control;
        if (!decodeShipControlState(reader, control))
            return false;
        outValue.payload = control;
    }
    else if (variantTag == 1u)
    {
        ClientShipCommand command;
        if (!decodeClientShipCommand(reader, command))
            return false;
        outValue.payload = command;
    }
    else
    {
        return false;
    }

    return finishDecode(reader);
}

inline bool encodeTimeSyncRequest(
    const TimeSyncRequest& value,
    std::vector<std::uint8_t>& outPayload
)
{
    WireWriter writer;
    writer.u64(value.sequence);
    writer.f64(value.clientSendTimeSeconds);
    outPayload = writer.take();
    return true;
}

inline bool decodeTimeSyncRequest(
    const std::vector<std::uint8_t>& payload,
    TimeSyncRequest& outValue
)
{
    WireReader reader(payload);
    return reader.u64(outValue.sequence) &&
        reader.f64(outValue.clientSendTimeSeconds) &&
        finishDecode(reader);
}

inline bool encodeTimeSyncResponse(
    const TimeSyncResponse& value,
    std::vector<std::uint8_t>& outPayload
)
{
    WireWriter writer;
    writer.u64(value.sequence);
    writer.f64(value.clientSendTimeSeconds);
    writer.f64(value.serverReceiveTimeSeconds);
    outPayload = writer.take();
    return true;
}

inline bool decodeTimeSyncResponse(
    const std::vector<std::uint8_t>& payload,
    TimeSyncResponse& outValue
)
{
    WireReader reader(payload);
    return reader.u64(outValue.sequence) &&
        reader.f64(outValue.clientSendTimeSeconds) &&
        reader.f64(outValue.serverReceiveTimeSeconds) &&
        finishDecode(reader);
}

inline bool encodeDetailTarget(
    WireWriter& writer,
    const world::celestial::DetailTarget& value
)
{
    writer.u8(static_cast<std::uint8_t>(value.sceneKind));
    writer.u8(static_cast<std::uint8_t>(value.focusClass));
    writer.i32(value.systemId);
    writer.dvec3(value.systemPositionLy);
    if (!writer.string(value.anchorId) || !writer.string(value.focusId))
        return false;

    writer.i32(value.spatialCell.level);
    writer.i32(value.spatialCell.maximumLevel);
    writer.i64(value.spatialCell.x);
    writer.i64(value.spatialCell.y);
    writer.i64(value.spatialCell.z);
    writer.dvec3(value.spatialCell.centerAu);
    writer.f64(value.spatialCell.edgeAu);
    return true;
}

inline bool decodeDetailTarget(
    WireReader& reader,
    world::celestial::DetailTarget& outValue
)
{
    std::uint8_t sceneKind = 0;
    std::uint8_t focusClass = 0;
    std::int32_t systemId = -1;
    std::int32_t level = -1;
    std::int32_t maximumLevel = -1;

    if (!reader.u8(sceneKind) ||
        !reader.u8(focusClass) ||
        !reader.i32(systemId) ||
        !reader.dvec3(outValue.systemPositionLy) ||
        !reader.string(outValue.anchorId) ||
        !reader.string(outValue.focusId) ||
        !reader.i32(level) ||
        !reader.i32(maximumLevel) ||
        !reader.i64(outValue.spatialCell.x) ||
        !reader.i64(outValue.spatialCell.y) ||
        !reader.i64(outValue.spatialCell.z) ||
        !reader.dvec3(outValue.spatialCell.centerAu) ||
        !reader.f64(outValue.spatialCell.edgeAu))
    {
        return false;
    }

    if (sceneKind > static_cast<std::uint8_t>(
            world::celestial::DetailSceneKind::LocalObject) ||
        focusClass > static_cast<std::uint8_t>(
            world::celestial::DetailObjectClass::Hub))
    {
        return false;
    }

    outValue.sceneKind =
        static_cast<world::celestial::DetailSceneKind>(sceneKind);
    outValue.focusClass =
        static_cast<world::celestial::DetailObjectClass>(focusClass);
    outValue.systemId = systemId;
    outValue.spatialCell.level = level;
    outValue.spatialCell.maximumLevel = maximumLevel;
    return true;
}

inline bool encodeMapRequest(
    const MapRequest& value,
    std::vector<std::uint8_t>& outPayload
)
{
    WireWriter writer;

    if (const auto* request = std::get_if<GalaxyMapRequest>(&value))
    {
        writer.u8(0u);
        writer.u64(request->requestId);
    }
    else if (const auto* request = std::get_if<SystemMapRequest>(&value))
    {
        writer.u8(1u);
        writer.u64(request->requestId);
        writer.i32(request->systemId);
    }
    else if (const auto* request = std::get_if<DetailMapRequest>(&value))
    {
        writer.u8(2u);
        writer.u64(request->requestId);
        if (!encodeDetailTarget(writer, request->target))
            return false;
    }
    else if (const auto* request = std::get_if<HubMapRequest>(&value))
    {
        writer.u8(3u);
        writer.u64(request->requestId);
        writer.i32(request->systemId);
        if (!writer.string(request->hubId))
            return false;
    }
    else
    {
        return false;
    }

    outPayload = writer.take();
    return outPayload.size() <= MaxWirePayloadBytes;
}

inline bool decodeMapRequest(
    const std::vector<std::uint8_t>& payload,
    MapRequest& outValue
)
{
    WireReader reader(payload);
    std::uint8_t variantTag = 0;
    if (!reader.u8(variantTag))
        return false;

    switch (variantTag)
    {
        case 0u:
        {
            GalaxyMapRequest request;
            if (!reader.u64(request.requestId))
                return false;
            outValue = std::move(request);
            break;
        }

        case 1u:
        {
            SystemMapRequest request;
            std::int32_t systemId = -1;
            if (!reader.u64(request.requestId) || !reader.i32(systemId))
                return false;
            request.systemId = systemId;
            outValue = std::move(request);
            break;
        }

        case 2u:
        {
            DetailMapRequest request;
            if (!reader.u64(request.requestId) ||
                !decodeDetailTarget(reader, request.target))
            {
                return false;
            }
            outValue = std::move(request);
            break;
        }

        case 3u:
        {
            HubMapRequest request;
            std::int32_t systemId = -1;
            if (!reader.u64(request.requestId) ||
                !reader.i32(systemId) ||
                !reader.string(request.hubId))
            {
                return false;
            }
            request.systemId = systemId;
            outValue = std::move(request);
            break;
        }

        default:
            return false;
    }

    return finishDecode(reader);
}

} // namespace game::network::wire
