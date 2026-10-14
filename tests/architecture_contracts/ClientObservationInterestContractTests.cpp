#include <cstdlib>
#include <iostream>
#include <variant>
#include <vector>

#include "src/game/network/WireProtocol.h"

namespace
{
using namespace game::network;
using namespace game::network::wire;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << '\n';
        std::exit(1);
    }
}

void testObservationInterestRoundTrip()
{
    ClientMessage message;
    message.clientTick = 9001;

    ClientObservationInterest interest;
    interest.serial = 17;
    interest.entries = {
        {EntityId{101}, ClientObservationDetail::Kinematic},
        {EntityId{202}, ClientObservationDetail::Response}
    };
    message.payload = interest;

    std::vector<std::uint8_t> payload;
    require(
        encodeClientMessage(message, payload),
        "observation-interest message did not encode"
    );

    ClientMessage decoded;
    require(
        decodeClientMessage(payload, decoded),
        "observation-interest message did not decode"
    );
    require(
        std::holds_alternative<ClientObservationInterest>(decoded.payload),
        "observation-interest variant tag changed"
    );

    const auto& out =
        std::get<ClientObservationInterest>(decoded.payload);
    require(out.serial == 17, "observation serial changed");
    require(out.entries.size() == 2, "observation entry count changed");
    require(
        out.entries[0].targetId == EntityId{101} &&
        out.entries[0].detail == ClientObservationDetail::Kinematic,
        "Kinematic observation entry changed"
    );
    require(
        out.entries[1].targetId == EntityId{202} &&
        out.entries[1].detail == ClientObservationDetail::Response,
        "Response observation entry changed"
    );
}

void testObservationWireCannotExpressFullControl()
{
    // detail=2 would be the natural next enum value, but the protocol accepts
    // only Kinematic(0) and Response(1). The client therefore cannot encode an
    // authoritative FullControl request in this message family.
    WireWriter writer;
    writer.u64(1); // client tick
    writer.u8(2);  // ClientObservationInterest variant
    writer.u64(1); // observation serial
    writer.u16(1); // one entry
    writer.u32(44);
    writer.u8(2);  // invalid detail

    ClientMessage decoded;
    require(
        !decodeClientMessage(writer.bytes(), decoded),
        "wire accepted client-authored FullControl observation demand"
    );
}

void testObservationEntryCap()
{
    ClientMessage message;
    ClientObservationInterest interest;
    interest.serial = 2;
    interest.entries.resize(MaxClientObservationInterestEntries + 1);
    for (std::size_t i = 0; i < interest.entries.size(); ++i)
    {
        interest.entries[i].targetId =
            EntityId{static_cast<std::uint32_t>(i + 1)};
        interest.entries[i].detail =
            ClientObservationDetail::Kinematic;
    }
    message.payload = std::move(interest);

    std::vector<std::uint8_t> payload;
    require(
        !encodeClientMessage(message, payload),
        "wire accepted observation-interest list above hard cap"
    );
}

} // namespace

int main()
{
    testObservationInterestRoundTrip();
    testObservationWireCannotExpressFullControl();
    testObservationEntryCap();

    std::cout
        << "[PASS] client observation interest is bounded and cannot request FullControl\n";
    return 0;
}
