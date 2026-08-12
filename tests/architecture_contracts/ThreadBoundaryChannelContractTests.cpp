#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <variant>

#include "src/game/debug/LocalDebugSessionControl.h"
#include "src/game/network/LocalLoopbackTransport.h"

namespace
{
void require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[FAIL] thread-boundary channel: " << message << '\n';
    std::exit(2);
}
}

int main()
{
    constexpr std::uint64_t MessageCount = 2000;

    LocalLoopbackTransport transport;
    std::atomic<bool> producerDone {false};
    std::atomic<std::uint64_t> receivedMessages {0};
    std::atomic<std::uint64_t> receivedMaps {0};

    std::thread producer([&]() {
        for (std::uint64_t i = 1; i <= MessageCount; ++i)
        {
            game::network::ClientMessage message;
            message.clientTick = i;
            transport.sendClientMessage(message);

            game::network::GalaxyMapRequest request;
            request.requestId = i;
            transport.sendMapRequest(request);
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::thread consumer([&]() {
        std::uint64_t expectedMessage = 1;
        std::uint64_t expectedMap = 1;

        while (!producerDone.load(std::memory_order_acquire) ||
               receivedMessages.load() < MessageCount ||
               receivedMaps.load() < MessageCount)
        {
            game::network::ClientMessage message;
            if (transport.receiveClientMessage(message))
            {
                require(message.clientTick == expectedMessage, "client-message FIFO order changed");
                ++expectedMessage;
                receivedMessages.fetch_add(1);
            }

            game::network::MapRequest request;
            if (transport.receiveMapRequest(request))
            {
                const auto* galaxy = std::get_if<game::network::GalaxyMapRequest>(&request);
                require(galaxy != nullptr, "map request variant changed across transport seam");
                require(galaxy->requestId == expectedMap, "map-request FIFO order changed");
                ++expectedMap;
                receivedMaps.fetch_add(1);
            }

            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    require(receivedMessages.load() == MessageCount, "lost client messages across threaded seam");
    require(receivedMaps.load() == MessageCount, "lost map requests across threaded seam");

    game::debug::LocalDebugSessionControl debug;
    std::atomic<bool> debugProducerDone {false};
    std::atomic<std::uint64_t> debugReceived {0};

    std::thread debugProducer([&]() {
        for (std::uint64_t i = 0; i < MessageCount; ++i)
            debug.destroyShipModule(EntityId{7}, "module");
        debugProducerDone.store(true, std::memory_order_release);
    });

    std::thread debugConsumer([&]() {
        while (!debugProducerDone.load(std::memory_order_acquire) ||
               debugReceived.load() < MessageCount)
        {
            game::debug::DebugCommand command;
            if (debug.receiveCommand(command))
            {
                require(
                    command.type == game::debug::DebugCommandType::DestroyShipModule,
                    "debug command changed across threaded seam"
                );
                debugReceived.fetch_add(1);
            }
            else
            {
                std::this_thread::yield();
            }
        }
    });

    debugProducer.join();
    debugConsumer.join();
    require(debugReceived.load() == MessageCount, "lost debug commands across threaded seam");

    std::thread statePublisher([&]() {
        for (std::uint64_t i = 1; i <= MessageCount; ++i)
        {
            game::debug::DebugSessionState state;
            state.universeTimeScale = static_cast<double>(i);
            debug.publishState(state);
        }
    });

    while (debug.stateRevision() < MessageCount)
    {
        (void)debug.universeTimeScale();
        std::this_thread::yield();
    }
    statePublisher.join();

    require(debug.stateRevision() == MessageCount, "debug state revisions were lost under contention");
    require(
        debug.universeTimeScale() == static_cast<double>(MessageCount),
        "debug state copy did not publish the final worker value"
    );

    std::cout << "[PASS] thread-safe local gameplay/debug channel contract\n";
    return 0;
}
