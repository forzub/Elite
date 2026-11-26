#include "src/game/network/WireCompressionLz4.h"
#include "src/game/network/TcpTransport.h"

#include <type_traits>
#include <queue>
#include <iostream>
#include <typeinfo>
#include <chrono>
#include <utility>
#include <vector>

#include "src/game/network/TcpWireStream.h"
#include "src/game/network/WireMessageCodec.h"

namespace game::network
{
namespace
{

template<typename T>
bool popQueue(std::queue<T>& queue, T& outValue)
{
    if (queue.empty())
        return false;

    outValue = std::move(queue.front());
    queue.pop();
    return true;
}

// what one TCP session's snapshots cost (the replication audit 2026-11-26: measure before reshaping the ship row):
// every 10 s the snapshots sent, ships carried, raw (encoded) and wire (LZ4) bytes per second, and the row parts of
// the controlled ship and of an average other ship
struct SnapshotTrafficMeter
{
    using Clock = std::chrono::steady_clock;
    Clock::time_point windowStart = Clock::now();
    std::uint64_t snapshots = 0, ships = 0, rawBytes = 0, wireBytes = 0;

    template <class V>
    static std::size_t sizeOf(const V& v)
    {
        wire::WireWriter w;
        (void)wire::binary::encodeValue(w, v);
        return w.take().size();
    }

    void record(const SimulationSnapshot& snapshot, const std::vector<std::uint8_t>& payload)
    {
        wire::CompressedWirePayload envelope;
        const bool parsed = wire::decodeCompressedWirePayloadEnvelope(payload, envelope);
        ++snapshots;
        ships += snapshot.ships.size();
        rawBytes += parsed ? envelope.uncompressedBytes : payload.size();
        wireBytes += payload.size();
        const double seconds = std::chrono::duration<double>(Clock::now() - windowStart).count();
        if (seconds < 10.0)
            return;
        std::cerr << "[NetBytes] session snapshots " << snapshots / seconds << "/s, ships/snapshot "
                  << (snapshots ? static_cast<double>(ships) / snapshots : 0.0) << ", raw "
                  << rawBytes / seconds / 1024.0 << " KiB/s, wire " << wireBytes / seconds / 1024.0 << " KiB/s (x"
                  << (wireBytes ? static_cast<double>(rawBytes) / wireBytes : 0.0) << ")";
        // the row parts, from this snapshot
        const ShipSnapshot* own = nullptr;
        std::size_t others = 0, otherRow = 0, otherMotion = 0, otherTelemetry = 0;
        for (const auto& ship : snapshot.ships)
        {
            if (ship.id.value == snapshot.session.controlledEntityId)
                own = &ship;
            else
            {
                ++others;
                otherRow += sizeOf(ship);
                otherMotion += sizeOf(ship.transform.motion);
                otherTelemetry += sizeOf(ship.transform.motion.engineTelemetry);
            }
        }
        if (own)
            std::cerr << "; own row " << sizeOf(*own) << " B (motion " << sizeOf(own->transform.motion) << ", engine telemetry "
                      << sizeOf(own->transform.motion.engineTelemetry) << ", referenceFrame " << sizeOf(own->referenceFrame)
                      << ", refit " << sizeOf(own->refit) << ", core " << sizeOf(own->shipCoreStatus) << ")";
        if (others)
            std::cerr << "; other rows avg " << otherRow / others << " B (motion " << otherMotion / others
                      << ", engine telemetry " << otherTelemetry / others << ")";
        std::cerr << std::endl;
        *this = {};
    }
};

template<typename T>
bool sendTyped(
    wire::TcpWireStream& stream,
    const T& value,
    const wire::IWireCompressor& compressor,
    SnapshotTrafficMeter* meter = nullptr)
{
    wire::WireMessageKind kind = wire::WireMessageKind::ClientMessage;
    std::vector<std::uint8_t> payload;
    if (!wire::encodeMessagePayload(value, kind, payload, compressor))
    {
        // which message could not be encoded (2026-11-23: the dedicated server dropped every client right after the
        // bootstrap with only "failed to encode/send TCP protocol message")
        std::cerr << "[Wire] ENCODE FAILED: " << typeid(T).name() << " kind=" << static_cast<int>(kind)
                  << " payload_bytes=" << payload.size() << std::endl;
        return false;
    }
    const std::size_t bytes = payload.size();
    if constexpr (std::is_same_v<T, SimulationSnapshot>)
        if (meter)
            meter->record(value, payload);
    if (!stream.send(kind, std::move(payload)))
    {
        std::cerr << "[Wire] SEND FAILED: " << typeid(T).name() << " kind=" << static_cast<int>(kind) << " bytes=" << bytes
                  << " stream=" << stream.lastError() << std::endl;
        return false;
    }
    return true;
}

} // namespace

struct TcpClientTransport::Impl
{
    void fail(std::string message)
    {
        if (error.empty())
            error = std::move(message);
        stream.close();
    }

    void service()
    {
        stream.service();

        wire::WireFrame frame;
        while (stream.receive(frame))
        {
            switch (frame.kind)
            {
                case wire::WireMessageKind::SessionReject:
                {
                    SessionReject value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid SessionReject wire payload");
                        return;
                    }
                    rejects.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::SessionWelcome:
                {
                    SessionWelcome value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid SessionWelcome wire payload");
                        return;
                    }
                    welcomes.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::SimulationSnapshot:
                {
                    SimulationSnapshot value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid SimulationSnapshot wire payload");
                        return;
                    }
                    snapshots.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::MapResponse:
                {
                    MapResponse value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid MapResponse wire payload");
                        return;
                    }
                    mapResponses.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::TimeSyncResponse:
                {
                    TimeSyncResponse value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid TimeSyncResponse wire payload");
                        return;
                    }
                    timeSyncResponses.push(std::move(value));
                    break;
                }

                default:
                    fail("server sent a client-to-server wire message kind");
                    return;
            }
        }

        if (!stream.connected() && error.empty() && !stream.lastError().empty())
            error = stream.lastError();
    }

    template<typename T>
    void send(const T& value)
    {
        if (!sendTyped(stream, value, compressor))
        {
            fail(
                stream.lastError().empty()
                    ? "failed to encode/send TCP protocol message"
                    : stream.lastError()
            );
        }
    }

    void resetQueues()
    {
        rejects = std::queue<SessionReject>{};
        welcomes = std::queue<SessionWelcome>{};
        snapshots = std::queue<SimulationSnapshot>{};
        mapResponses = std::queue<MapResponse>{};
        timeSyncResponses = std::queue<TimeSyncResponse>{};
    }

    wire::TcpWireStream stream;
    wire::Lz4WireCompression compressor;   // the data plane compressed (was none at all)
    std::queue<SessionReject> rejects;
    std::queue<SessionWelcome> welcomes;
    std::queue<SimulationSnapshot> snapshots;
    std::queue<MapResponse> mapResponses;
    std::queue<TimeSyncResponse> timeSyncResponses;
    std::string error;
};

struct TcpServerTransport::Impl
{
    // audit SRV-5: an inbound queue's bound (the server drains every tick; a sane client sends tens a tick)
    static constexpr std::size_t MaxInboundQueued = 4096;
    explicit Impl(std::unique_ptr<wire::TcpWireStream> acceptedStream)
        : stream(std::move(acceptedStream))
    {
        if (!stream)
            error = "TcpServerTransport requires an accepted wire stream";
    }

    void fail(std::string message)
    {
        if (error.empty())
            error = std::move(message);
        if (stream)
            stream->close();
    }

    void service()
    {
        if (!stream)
            return;

        stream->service();

        wire::WireFrame frame;
        while (stream->receive(frame))
        {
            switch (frame.kind)
            {
                case wire::WireMessageKind::SessionHello:
                {
                    SessionHello value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid SessionHello wire payload");
                        return;
                    }
                    if (sessionHellos.size() >= MaxInboundQueued)
                    {
                        fail("a client flooded the sessionHellos queue (over " + std::to_string(MaxInboundQueued) + ") - connection closed");
                        return;
                    }
                    sessionHellos.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::ClientMessage:
                {
                    ClientMessage value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid ClientMessage wire payload");
                        return;
                    }
                    if (clientMessages.size() >= MaxInboundQueued)
                    {
                        fail("a client flooded the clientMessages queue (over " + std::to_string(MaxInboundQueued) + ") - connection closed");
                        return;
                    }
                    clientMessages.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::MapRequest:
                {
                    MapRequest value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid MapRequest wire payload");
                        return;
                    }
                    if (mapRequests.size() >= MaxInboundQueued)
                    {
                        fail("a client flooded the mapRequests queue (over " + std::to_string(MaxInboundQueued) + ") - connection closed");
                        return;
                    }
                    mapRequests.push(std::move(value));
                    break;
                }

                case wire::WireMessageKind::TimeSyncRequest:
                {
                    TimeSyncRequest value;
                    if (!wire::decodeMessagePayload(frame, value, compressor))
                    {
                        fail("invalid TimeSyncRequest wire payload");
                        return;
                    }
                    if (timeSyncRequests.size() >= MaxInboundQueued)
                    {
                        fail("a client flooded the timeSyncRequests queue (over " + std::to_string(MaxInboundQueued) + ") - connection closed");
                        return;
                    }
                    timeSyncRequests.push(std::move(value));
                    break;
                }

                default:
                    fail("client sent a server-to-client wire message kind");
                    return;
            }
        }

        if (!stream->connected() && error.empty() && !stream->lastError().empty())
            error = stream->lastError();
    }

    SnapshotTrafficMeter snapshotMeter;

    template<typename T>
    void send(const T& value)
    {
        if (!stream || !sendTyped(*stream, value, compressor, &snapshotMeter))
        {
            // a snapshot that could not be ENCODED (the stream itself is fine: over the wire size limit, SRV-2): tell
            // the client why before closing - it showed only "TCP peer closed connection"
            if constexpr (std::is_same_v<T, SimulationSnapshot>)
            {
                if (stream && stream->lastError().empty())
                {
                    SessionReject reject;
                    reject.reason = SessionRejectReason::SnapshotTooLarge;
                    reject.retryable = false;
                    (void)sendTyped(*stream, reject, compressor);
                    fail("snapshot over the wire size limit - SNAPSHOT_TOO_LARGE sent to the client");
                    return;
                }
            }
            fail(
                stream && !stream->lastError().empty()
                    ? stream->lastError()
                    : "failed to encode/send TCP protocol message"
            );
        }
    }

    std::unique_ptr<wire::TcpWireStream> stream;
    wire::Lz4WireCompression compressor;   // the data plane compressed (was none at all)
    std::queue<SessionHello> sessionHellos;
    std::queue<ClientMessage> clientMessages;
    std::queue<MapRequest> mapRequests;
    std::queue<TimeSyncRequest> timeSyncRequests;
    std::string error;
};

TcpClientTransport::TcpClientTransport()
    : m_impl(std::make_unique<Impl>())
{
}

TcpClientTransport::~TcpClientTransport() = default;

bool TcpClientTransport::connect(
    const std::string& host,
    std::uint16_t port)
{
    m_impl->error.clear();
    m_impl->resetQueues();
    const bool ok = m_impl->stream.connect(host, port);
    if (!ok)
        m_impl->error = m_impl->stream.lastError();
    return ok;
}

void TcpClientTransport::service()
{
    m_impl->service();
}

void TcpClientTransport::disconnect()
{
    m_impl->stream.close();
}

bool TcpClientTransport::connected() const noexcept
{
    return m_impl->stream.connected();
}

const std::string& TcpClientTransport::lastError() const noexcept
{
    return m_impl->error.empty()
        ? m_impl->stream.lastError()
        : m_impl->error;
}

void TcpClientTransport::sendSessionHello(const SessionHello& hello)
{
    m_impl->send(hello);
}

bool TcpClientTransport::receiveSessionReject(SessionReject& outReject)
{
    m_impl->service();
    return popQueue(m_impl->rejects, outReject);
}

bool TcpClientTransport::receiveSessionWelcome(SessionWelcome& outWelcome)
{
    m_impl->service();
    return popQueue(m_impl->welcomes, outWelcome);
}

bool TcpClientTransport::receiveSnapshot(SimulationSnapshot& outSnapshot)
{
    m_impl->service();
    return popQueue(m_impl->snapshots, outSnapshot);
}

void TcpClientTransport::sendClientMessage(const ClientMessage& msg)
{
    m_impl->send(msg);
}

void TcpClientTransport::sendMapRequest(const MapRequest& request)
{
    m_impl->send(request);
}

bool TcpClientTransport::receiveMapResponse(MapResponse& outResponse)
{
    m_impl->service();
    return popQueue(m_impl->mapResponses, outResponse);
}

void TcpClientTransport::sendTimeSyncRequest(const TimeSyncRequest& request)
{
    m_impl->send(request);
}

bool TcpClientTransport::receiveTimeSyncResponse(TimeSyncResponse& outResponse)
{
    m_impl->service();
    return popQueue(m_impl->timeSyncResponses, outResponse);
}

TcpServerTransport::TcpServerTransport(
    std::unique_ptr<wire::TcpWireStream> stream)
    : m_impl(std::make_unique<Impl>(std::move(stream)))
{
}

TcpServerTransport::~TcpServerTransport() = default;

void TcpServerTransport::update(float)
{
    m_impl->service();
}

bool TcpServerTransport::receiveSessionHello(SessionHello& outHello)
{
    m_impl->service();
    return popQueue(m_impl->sessionHellos, outHello);
}

bool TcpServerTransport::receiveClientMessage(ClientMessage& outMessage)
{
    m_impl->service();
    return popQueue(m_impl->clientMessages, outMessage);
}

bool TcpServerTransport::receiveMapRequest(MapRequest& outRequest)
{
    m_impl->service();
    return popQueue(m_impl->mapRequests, outRequest);
}

bool TcpServerTransport::receiveTimeSyncRequest(TimeSyncRequest& outRequest)
{
    m_impl->service();
    return popQueue(m_impl->timeSyncRequests, outRequest);
}

void TcpServerTransport::publishSessionRejectImmediately(
    const SessionReject& reject)
{
    m_impl->send(reject);
}

void TcpServerTransport::publishSessionWelcomeImmediately(
    const SessionWelcome& welcome)
{
    m_impl->send(welcome);
}

void TcpServerTransport::publishSnapshot(
    const SimulationSnapshot& snapshot)
{
    m_impl->send(snapshot);
}

void TcpServerTransport::publishSnapshotImmediately(
    const SimulationSnapshot& snapshot)
{
    m_impl->send(snapshot);
}

void TcpServerTransport::sendMapResponse(MapResponse response)
{
    m_impl->send(response);
}

void TcpServerTransport::sendTimeSyncResponse(TimeSyncResponse response)
{
    m_impl->send(response);
}

bool TcpServerTransport::connected() const noexcept
{
    return m_impl->stream && m_impl->stream->connected();
}

void TcpServerTransport::disconnect()
{
    if (m_impl->stream)
        m_impl->stream->close();
}

const std::string& TcpServerTransport::lastError() const noexcept
{
    if (!m_impl->error.empty())
        return m_impl->error;

    static const std::string Empty;
    return m_impl->stream ? m_impl->stream->lastError() : Empty;
}

TcpServerListener::TcpServerListener()
    : m_listener(std::make_unique<wire::TcpWireListener>())
{
}

TcpServerListener::~TcpServerListener() = default;

bool TcpServerListener::listen(
    const std::string& bindAddress,
    std::uint16_t port)
{
    return m_listener->listen(bindAddress, port);
}

std::unique_ptr<TcpServerTransport> TcpServerListener::acceptPending()
{
    auto stream = m_listener->acceptPending();
    if (!stream)
        return nullptr;

    return std::unique_ptr<TcpServerTransport>(
        new TcpServerTransport(std::move(stream))
    );
}

bool TcpServerListener::listening() const noexcept
{
    return m_listener->listening();
}

std::uint16_t TcpServerListener::localPort() const noexcept
{
    return m_listener->localPort();
}

void TcpServerListener::close()
{
    m_listener->close();
}

const std::string& TcpServerListener::lastError() const noexcept
{
    return m_listener->lastError();
}

} // namespace game::network
