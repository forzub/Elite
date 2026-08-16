#include "src/game/network/TcpTransport.h"

#include <queue>
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

template<typename T>
bool sendTyped(
    wire::TcpWireStream& stream,
    const T& value,
    const wire::IWireCompressor& compressor)
{
    wire::WireMessageKind kind = wire::WireMessageKind::ClientMessage;
    std::vector<std::uint8_t> payload;
    return wire::encodeMessagePayload(
               value,
               kind,
               payload,
               compressor
           ) &&
        stream.send(kind, std::move(payload));
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
    wire::NoWireCompression compressor;
    std::queue<SessionReject> rejects;
    std::queue<SessionWelcome> welcomes;
    std::queue<SimulationSnapshot> snapshots;
    std::queue<MapResponse> mapResponses;
    std::queue<TimeSyncResponse> timeSyncResponses;
    std::string error;
};

struct TcpServerTransport::Impl
{
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

    template<typename T>
    void send(const T& value)
    {
        if (!stream || !sendTyped(*stream, value, compressor))
        {
            fail(
                stream && !stream->lastError().empty()
                    ? stream->lastError()
                    : "failed to encode/send TCP protocol message"
            );
        }
    }

    std::unique_ptr<wire::TcpWireStream> stream;
    wire::NoWireCompression compressor;
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
