#include "src/game/network/TcpWireStream.h"

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif
#include <asio.hpp>

#include <array>
#include <deque>
#include <limits>
#include <queue>
#include <utility>

namespace game::network::wire
{
namespace
{
using Tcp = asio::ip::tcp;

bool isWouldBlock(const asio::error_code& error)
{
    return error == asio::error::would_block ||
        error == asio::error::try_again;
}

struct PendingWrite
{
    std::vector<std::uint8_t> bytes;
    std::size_t offset = 0u;
};

void configureConnectedSocket(Tcp::socket& socket, asio::error_code& error)
{
    socket.set_option(Tcp::no_delay(true), error);
    if (error)
        return;

    socket.non_blocking(true, error);
}

} // namespace

struct TcpWireStream::Impl
{
    Impl()
        : io(std::make_shared<asio::io_context>())
        , socket(*io)
    {
    }

    Impl(
        std::shared_ptr<asio::io_context> sharedIo,
        Tcp::socket acceptedSocket
    )
        : io(std::move(sharedIo))
        , socket(std::move(acceptedSocket))
    {
        asio::error_code error;
        configureConnectedSocket(socket, error);
        if (error)
        {
            fail("failed to configure accepted TCP socket: " + error.message());
            return;
        }
        isConnected = true;
    }

    void fail(std::string message)
    {
        if (lastError.empty())
            lastError = std::move(message);

        isConnected = false;
        asio::error_code ignored;
        socket.shutdown(Tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
    }

    bool openClient(
        const std::string& host,
        std::uint16_t port)
    {
        close();
        lastError.clear();
        decoder = WireFrameDecoder{};
        incoming = std::queue<WireFrame>{};
        outgoing.clear();
        queuedBytes = 0u;
        inboundSequence = 0u;
        outboundSequence = 0u;

        asio::error_code error;
        Tcp::resolver resolver(*io);
        const auto endpoints = resolver.resolve(
            host,
            std::to_string(port),
            error
        );
        if (error)
        {
            fail("TCP resolve failed: " + error.message());
            return false;
        }

        socket = Tcp::socket(*io);
        asio::connect(socket, endpoints, error);
        if (error)
        {
            fail("TCP connect failed: " + error.message());
            return false;
        }

        configureConnectedSocket(socket, error);
        if (error)
        {
            fail("failed to configure TCP socket: " + error.message());
            return false;
        }

        isConnected = true;
        return true;
    }

    void close()
    {
        isConnected = false;
        asio::error_code ignored;
        socket.shutdown(Tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
        outgoing.clear();
        queuedBytes = 0u;
    }

    bool enqueue(
        WireMessageKind kind,
        std::vector<std::uint8_t> payload)
    {
        if (!isConnected)
            return false;

        if (outboundSequence == std::numeric_limits<std::uint64_t>::max())
        {
            fail("TCP wire outbound sequence exhausted");
            return false;
        }

        WireFrame frame;
        frame.kind = kind;
        frame.sequence = ++outboundSequence;
        frame.payload = std::move(payload);

        auto bytes = encodeFrame(frame);
        if (bytes.empty())
        {
            fail("failed to encode outbound wire frame");
            return false;
        }

        if (bytes.size() > MaxTcpQueuedWireBytes ||
            queuedBytes > MaxTcpQueuedWireBytes - bytes.size())
        {
            fail("TCP wire write queue exceeded safety limit");
            return false;
        }

        queuedBytes += bytes.size();
        outgoing.push_back(PendingWrite{std::move(bytes), 0u});
        pumpWrites();
        return isConnected;
    }

    void pumpWrites()
    {
        while (isConnected && !outgoing.empty())
        {
            PendingWrite& pending = outgoing.front();
            const std::size_t remaining =
                pending.bytes.size() - pending.offset;

            asio::error_code error;
            const std::size_t sent = socket.write_some(
                asio::buffer(
                    pending.bytes.data() + pending.offset,
                    remaining
                ),
                error
            );

            if (error)
            {
                if (isWouldBlock(error))
                    return;

                fail("TCP write failed: " + error.message());
                return;
            }

            if (sent == 0u)
                return;

            pending.offset += sent;
            queuedBytes -= sent;

            if (pending.offset == pending.bytes.size())
                outgoing.pop_front();
        }
    }

    void pumpReads()
    {
        std::array<std::uint8_t, 64u * 1024u> buffer {};

        while (isConnected)
        {
            asio::error_code error;
            const std::size_t received = socket.read_some(
                asio::buffer(buffer),
                error
            );

            if (error)
            {
                if (isWouldBlock(error))
                    return;

                if (error == asio::error::eof)
                    fail("TCP peer closed connection");
                else
                    fail("TCP read failed: " + error.message());
                return;
            }

            if (received == 0u)
                return;

            decoder.push(buffer.data(), received);

            WireFrame frame;
            while (decoder.pop(frame))
            {
                if (inboundSequence == std::numeric_limits<std::uint64_t>::max())
                {
                    fail("TCP wire inbound sequence exhausted");
                    return;
                }

                const std::uint64_t expected = inboundSequence + 1u;
                if (frame.sequence != expected)
                {
                    fail("TCP wire frame sequence discontinuity");
                    return;
                }

                inboundSequence = frame.sequence;
                incoming.push(std::move(frame));
            }

            if (decoder.failed())
            {
                fail("TCP wire decoder failed: " + decoder.error());
                return;
            }
        }
    }

    void service()
    {
        if (!isConnected)
            return;

        pumpWrites();
        pumpReads();
        pumpWrites();
    }

    std::shared_ptr<asio::io_context> io;
    Tcp::socket socket;
    WireFrameDecoder decoder;
    std::queue<WireFrame> incoming;
    std::deque<PendingWrite> outgoing;
    std::size_t queuedBytes = 0u;
    std::uint64_t inboundSequence = 0u;
    std::uint64_t outboundSequence = 0u;
    bool isConnected = false;
    std::string lastError;
};

struct TcpWireListener::Impl
{
    Impl()
        : io(std::make_shared<asio::io_context>())
        , acceptor(*io)
    {
    }

    void fail(std::string message)
    {
        lastError = std::move(message);
        close();
    }

    void close()
    {
        asio::error_code ignored;
        acceptor.close(ignored);
        port = 0u;
    }

    std::shared_ptr<asio::io_context> io;
    Tcp::acceptor acceptor;
    std::uint16_t port = 0u;
    std::string lastError;
};

TcpWireStream::TcpWireStream()
    : m_impl(std::make_unique<Impl>())
{
}

TcpWireStream::TcpWireStream(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl))
{
}

TcpWireStream::~TcpWireStream() = default;
TcpWireStream::TcpWireStream(TcpWireStream&&) noexcept = default;
TcpWireStream& TcpWireStream::operator=(TcpWireStream&&) noexcept = default;

bool TcpWireStream::connect(
    const std::string& host,
    std::uint16_t port)
{
    return m_impl->openClient(host, port);
}

void TcpWireStream::service()
{
    m_impl->service();
}

bool TcpWireStream::send(
    WireMessageKind kind,
    std::vector<std::uint8_t> payload)
{
    return m_impl->enqueue(kind, std::move(payload));
}

bool TcpWireStream::receive(WireFrame& outFrame)
{
    if (m_impl->incoming.empty())
        return false;

    outFrame = std::move(m_impl->incoming.front());
    m_impl->incoming.pop();
    return true;
}

bool TcpWireStream::connected() const noexcept
{
    return m_impl->isConnected;
}

void TcpWireStream::close()
{
    m_impl->close();
}

const std::string& TcpWireStream::lastError() const noexcept
{
    return m_impl->lastError;
}

std::size_t TcpWireStream::queuedWriteBytes() const noexcept
{
    return m_impl->queuedBytes;
}

std::uint64_t TcpWireStream::lastInboundSequence() const noexcept
{
    return m_impl->inboundSequence;
}

std::uint64_t TcpWireStream::lastOutboundSequence() const noexcept
{
    return m_impl->outboundSequence;
}

TcpWireListener::TcpWireListener()
    : m_impl(std::make_unique<Impl>())
{
}

TcpWireListener::~TcpWireListener() = default;
TcpWireListener::TcpWireListener(TcpWireListener&&) noexcept = default;
TcpWireListener& TcpWireListener::operator=(TcpWireListener&&) noexcept = default;

bool TcpWireListener::listen(
    const std::string& bindAddress,
    std::uint16_t requestedPort)
{
    m_impl->close();
    m_impl->lastError.clear();

    asio::error_code error;
    const auto address = asio::ip::make_address(bindAddress, error);
    if (error)
    {
        m_impl->fail("TCP bind address is invalid: " + error.message());
        return false;
    }

    const Tcp::endpoint endpoint(address, requestedPort);
    m_impl->acceptor.open(endpoint.protocol(), error);
    if (error)
    {
        m_impl->fail("TCP listener open failed: " + error.message());
        return false;
    }

    m_impl->acceptor.set_option(
        asio::socket_base::reuse_address(true),
        error
    );
    if (error)
    {
        m_impl->fail("TCP listener option failed: " + error.message());
        return false;
    }

    m_impl->acceptor.bind(endpoint, error);
    if (error)
    {
        m_impl->fail("TCP listener bind failed: " + error.message());
        return false;
    }

    m_impl->acceptor.listen(
        asio::socket_base::max_listen_connections,
        error
    );
    if (error)
    {
        m_impl->fail("TCP listener listen failed: " + error.message());
        return false;
    }

    m_impl->acceptor.non_blocking(true, error);
    if (error)
    {
        m_impl->fail("TCP listener non-blocking mode failed: " + error.message());
        return false;
    }

    const auto local = m_impl->acceptor.local_endpoint(error);
    if (error)
    {
        m_impl->fail("TCP listener endpoint query failed: " + error.message());
        return false;
    }

    m_impl->port = local.port();
    return true;
}

std::unique_ptr<TcpWireStream> TcpWireListener::acceptPending()
{
    if (!m_impl->acceptor.is_open())
        return nullptr;

    Tcp::socket socket(*m_impl->io);
    asio::error_code error;
    m_impl->acceptor.accept(socket, error);

    if (error)
    {
        if (isWouldBlock(error))
            return nullptr;

        m_impl->lastError = "TCP accept failed: " + error.message();
        return nullptr;
    }

    auto streamImpl = std::make_unique<TcpWireStream::Impl>(
        m_impl->io,
        std::move(socket)
    );
    auto stream = std::unique_ptr<TcpWireStream>(
        new TcpWireStream(std::move(streamImpl))
    );

    if (!stream->connected())
    {
        m_impl->lastError = stream->lastError();
        return nullptr;
    }

    return stream;
}

bool TcpWireListener::listening() const noexcept
{
    return m_impl->acceptor.is_open();
}

std::uint16_t TcpWireListener::localPort() const noexcept
{
    return m_impl->port;
}

void TcpWireListener::close()
{
    m_impl->close();
}

const std::string& TcpWireListener::lastError() const noexcept
{
    return m_impl->lastError;
}

} // namespace game::network::wire
