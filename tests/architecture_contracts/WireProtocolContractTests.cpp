#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <variant>
#include <vector>

#include "src/game/network/WireProtocol.h"

namespace
{
using namespace game::network;
using namespace game::network::wire;

[[noreturn]] void fail(const std::string& message)
{
    std::cerr << "[FAIL] wire protocol contract: " << message << '\n';
    std::exit(1);
}

void require(bool condition, const std::string& message)
{
    if (!condition)
        fail(message);
}

bool nearlyEqual(double a, double b)
{
    return std::abs(a - b) <= 1.0e-12;
}

bool nearlyEqual(float a, float b)
{
    return std::abs(a - b) <= 1.0e-6f;
}


void testSignedScalarRoundTrip()
{
    WireWriter writer;
    writer.i32(std::numeric_limits<std::int32_t>::min());
    writer.i32(-1);
    writer.i32(std::numeric_limits<std::int32_t>::max());
    writer.i64(std::numeric_limits<std::int64_t>::min());
    writer.i64(-1);
    writer.i64(std::numeric_limits<std::int64_t>::max());

    WireReader reader(writer.bytes());
    std::int32_t i32min = 0;
    std::int32_t i32neg = 0;
    std::int32_t i32max = 0;
    std::int64_t i64min = 0;
    std::int64_t i64neg = 0;
    std::int64_t i64max = 0;
    require(reader.i32(i32min) && reader.i32(i32neg) && reader.i32(i32max) &&
            reader.i64(i64min) && reader.i64(i64neg) && reader.i64(i64max) &&
            reader.empty(),
        "signed scalar round-trip decode failed");
    require(i32min == std::numeric_limits<std::int32_t>::min(), "i32 min mismatch");
    require(i32neg == -1, "i32 -1 mismatch");
    require(i32max == std::numeric_limits<std::int32_t>::max(), "i32 max mismatch");
    require(i64min == std::numeric_limits<std::int64_t>::min(), "i64 min mismatch");
    require(i64neg == -1, "i64 -1 mismatch");
    require(i64max == std::numeric_limits<std::int64_t>::max(), "i64 max mismatch");
}

void testFrameByteOrderAndFragmentation()
{
    WireFrame frame;
    frame.kind = WireMessageKind::ClientMessage;
    frame.sequence = 0x0102030405060708ull;
    frame.payload = {0xAAu, 0xBBu, 0xCCu};

    const auto bytes = encodeFrame(frame);
    require(bytes.size() == WireHeaderBytes + frame.payload.size(),
        "encoded frame size mismatch");

    const auto version = WireProtocolVersion;
    const auto kind = static_cast<std::uint16_t>(frame.kind);
    const std::vector<std::uint8_t> expectedHeader = {
        0x45u, 0x4Cu, 0x49u, 0x54u, // ELIT
        static_cast<std::uint8_t>((version >> 8u) & 0xFFu),
        static_cast<std::uint8_t>(version & 0xFFu),
        static_cast<std::uint8_t>((kind >> 8u) & 0xFFu),
        static_cast<std::uint8_t>(kind & 0xFFu),
        0x00u, 0x00u, 0x00u, 0x03u,// payload bytes
        0x01u, 0x02u, 0x03u, 0x04u,
        0x05u, 0x06u, 0x07u, 0x08u
    };
    require(std::equal(expectedHeader.begin(), expectedHeader.end(), bytes.begin()),
        "wire header is not deterministic network byte order");

    WireFrameDecoder decoder;
    WireFrame decoded;
    for (const auto byte : bytes)
    {
        decoder.push(&byte, 1u);
        if (decoder.pop(decoded))
            break;
    }

    require(!decoder.failed(), "fragmented decoder entered failure state");
    require(decoded.kind == frame.kind, "fragmented frame kind mismatch");
    require(decoded.sequence == frame.sequence, "fragmented frame sequence mismatch");
    require(decoded.payload == frame.payload, "fragmented frame payload mismatch");
    require(decoder.bufferedBytes() == 0u, "fragmented decoder retained bytes");

    WireFrame second;
    second.kind = WireMessageKind::TimeSyncRequest;
    second.sequence = 99u;
    second.payload = {0x11u};
    const auto secondBytes = encodeFrame(second);

    std::vector<std::uint8_t> combined = bytes;
    combined.insert(combined.end(), secondBytes.begin(), secondBytes.end());

    WireFrameDecoder coalesced;
    coalesced.push(combined);
    WireFrame firstOut;
    WireFrame secondOut;
    require(coalesced.pop(firstOut), "first coalesced frame missing");
    require(coalesced.pop(secondOut), "second coalesced frame missing");
    require(firstOut.sequence == frame.sequence, "first coalesced sequence mismatch");
    require(secondOut.sequence == second.sequence, "second coalesced sequence mismatch");
}

void testFrameValidation()
{
    WireFrame frame;
    frame.kind = WireMessageKind::ClientMessage;
    frame.sequence = 1u;
    frame.payload = {0x01u};

    auto badMagic = encodeFrame(frame);
    badMagic[0] = 0u;
    WireFrameDecoder magicDecoder;
    magicDecoder.push(badMagic);
    WireFrame ignored;
    require(!magicDecoder.pop(ignored), "bad magic unexpectedly decoded");
    require(magicDecoder.failed(), "bad magic did not fail decoder");

    auto badVersion = encodeFrame(frame);
    const auto invalidVersion =
        static_cast<std::uint16_t>(WireProtocolVersion ^ 0x0001u);
    badVersion[4] = static_cast<std::uint8_t>(
        (invalidVersion >> 8u) & 0xFFu);
    badVersion[5] = static_cast<std::uint8_t>(invalidVersion & 0xFFu);
    WireFrameDecoder versionDecoder;
    versionDecoder.push(badVersion);
    require(!versionDecoder.pop(ignored), "bad version unexpectedly decoded");
    require(versionDecoder.failed(), "bad version did not fail decoder");

    std::vector<std::uint8_t> oversized(WireHeaderBytes, 0u);
    oversized[0] = 0x45u;
    oversized[1] = 0x4Cu;
    oversized[2] = 0x49u;
    oversized[3] = 0x54u;
    oversized[4] = static_cast<std::uint8_t>(
        (WireProtocolVersion >> 8u) & 0xFFu);
    oversized[5] = static_cast<std::uint8_t>(
        WireProtocolVersion & 0xFFu);
    const auto clientMessageKind =
        static_cast<std::uint16_t>(WireMessageKind::ClientMessage);
    oversized[6] = static_cast<std::uint8_t>(
        (clientMessageKind >> 8u) & 0xFFu);
    oversized[7] = static_cast<std::uint8_t>(
        clientMessageKind & 0xFFu);
    const std::uint32_t invalidSize = MaxWirePayloadBytes + 1u;
    oversized[8] = static_cast<std::uint8_t>((invalidSize >> 24u) & 0xFFu);
    oversized[9] = static_cast<std::uint8_t>((invalidSize >> 16u) & 0xFFu);
    oversized[10] = static_cast<std::uint8_t>((invalidSize >> 8u) & 0xFFu);
    oversized[11] = static_cast<std::uint8_t>(invalidSize & 0xFFu);

    WireFrameDecoder sizeDecoder;
    sizeDecoder.push(oversized);
    require(!sizeDecoder.pop(ignored), "oversized frame unexpectedly decoded");
    require(sizeDecoder.failed(), "oversized frame did not fail decoder");
}

void testSessionWelcomeRoundTrip()
{
    SessionWelcome welcome;
    welcome.sessionId.value = 0x1122334455667788ull;
    welcome.playerId = PlayerId{0x8877665544332211ull};
    welcome.controlledShipInstanceId = 0x1020304050607080ull;
    welcome.controlledEntityId = EntityId{42u};
    welcome.fixedStepSeconds = 0.02;
    welcome.starAtlasCatalog.schemaVersion = 7u;
    welcome.starAtlasCatalog.contentFingerprint = 0xABCDEF1020304050ull;

    std::vector<std::uint8_t> payload;
    require(encodeSessionWelcome(welcome, payload), "SessionWelcome encode failed");

    SessionWelcome decoded;
    require(decodeSessionWelcome(payload, decoded), "SessionWelcome decode failed");
    require(decoded.sessionId == welcome.sessionId, "SessionWelcome session id mismatch");
    require(decoded.playerId == welcome.playerId, "SessionWelcome PlayerId mismatch");
    require(decoded.controlledShipInstanceId == welcome.controlledShipInstanceId,
        "SessionWelcome ShipInstanceId mismatch");
    require(decoded.controlledEntityId == welcome.controlledEntityId,
        "SessionWelcome controlled entity mismatch");
    require(std::abs(decoded.fixedStepSeconds - welcome.fixedStepSeconds) < 1.0e-12,
        "SessionWelcome fixed step mismatch");
    require(decoded.starAtlasCatalog.schemaVersion == welcome.starAtlasCatalog.schemaVersion,
        "SessionWelcome schema version mismatch");
    require(decoded.starAtlasCatalog.contentFingerprint ==
            welcome.starAtlasCatalog.contentFingerprint,
        "SessionWelcome fingerprint mismatch");
}

void testClientMessageRoundTrip()
{
    ClientMessage controlMessage;
    controlMessage.clientTick = 1234u;
    ShipControlState control;
    control.cruiseActive = true;
    control.jumpActive = false;
    control.pitchInput = 0.25f;
    control.yawInput = -0.5f;
    control.rollInput = 0.75f;
    control.targetSpeedRate = 1.25f;
    control.assistedMaxSpeedCommand = true;
    control.strafeInput = -0.2f;
    control.liftInput = 0.3f;
    control.forwardInput = 0.9f;
    control.localControlLawCommandValid = true;
    control.requestedLocalControlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    control.velocityAlignmentCommand = game::navigation::VelocityAlignmentMode::BrakeToStop;
    control.controlTick = 777u;
    controlMessage.payload = control;

    std::vector<std::uint8_t> payload;
    require(encodeClientMessage(controlMessage, payload), "control message encode failed");

    ClientMessage decoded;
    require(decodeClientMessage(payload, decoded), "control message decode failed");
    require(decoded.clientTick == controlMessage.clientTick, "client tick mismatch");
    require(std::holds_alternative<ShipControlState>(decoded.payload),
        "control message variant changed");

    const auto& decodedControl = std::get<ShipControlState>(decoded.payload);
    require(decodedControl.cruiseActive == control.cruiseActive, "cruise flag mismatch");
    require(decodedControl.jumpActive == control.jumpActive, "jump flag mismatch");
    require(nearlyEqual(decodedControl.pitchInput, control.pitchInput), "pitch mismatch");
    require(nearlyEqual(decodedControl.yawInput, control.yawInput), "yaw mismatch");
    require(nearlyEqual(decodedControl.rollInput, control.rollInput), "roll mismatch");
    require(nearlyEqual(decodedControl.targetSpeedRate, control.targetSpeedRate),
        "target speed rate mismatch");
    require(decodedControl.assistedMaxSpeedCommand == control.assistedMaxSpeedCommand,
        "assisted max-speed command mismatch");
    require(nearlyEqual(decodedControl.strafeInput, control.strafeInput), "strafe mismatch");
    require(nearlyEqual(decodedControl.liftInput, control.liftInput), "lift mismatch");
    require(nearlyEqual(decodedControl.forwardInput, control.forwardInput), "forward mismatch");
    require(decodedControl.localControlLawCommandValid ==
            control.localControlLawCommandValid,
        "control law command-valid mismatch");
    require(decodedControl.requestedLocalControlLaw == control.requestedLocalControlLaw,
        "control law mismatch");
    require(decodedControl.velocityAlignmentCommand == control.velocityAlignmentCommand,
        "velocity alignment mismatch");
    require(decodedControl.controlTick == control.controlTick, "control tick mismatch");

    ClientMessage commandMessage;
    commandMessage.clientTick = 5678u;
    ClientShipCommand command;
    command.type = ClientShipCommand::DamageRadiator;
    command.index = 3;
    command.amount = 17.5;
    commandMessage.payload = command;

    require(encodeClientMessage(commandMessage, payload), "ship command encode failed");
    require(decodeClientMessage(payload, decoded), "ship command decode failed");
    require(std::holds_alternative<ClientShipCommand>(decoded.payload),
        "ship command variant changed");
    const auto& decodedCommand = std::get<ClientShipCommand>(decoded.payload);
    require(decodedCommand.type == command.type, "ship command type mismatch");
    require(decodedCommand.index == command.index, "ship command index mismatch");
    require(nearlyEqual(decodedCommand.amount, command.amount), "ship command amount mismatch");
}

void testTimeSyncRoundTrip()
{
    TimeSyncRequest request;
    request.sequence = 91u;
    request.clientSendTimeSeconds = 123.456;

    std::vector<std::uint8_t> payload;
    require(encodeTimeSyncRequest(request, payload), "time-sync request encode failed");
    TimeSyncRequest decodedRequest;
    require(decodeTimeSyncRequest(payload, decodedRequest), "time-sync request decode failed");
    require(decodedRequest.sequence == request.sequence, "time-sync request sequence mismatch");
    require(nearlyEqual(decodedRequest.clientSendTimeSeconds, request.clientSendTimeSeconds),
        "time-sync request time mismatch");

    TimeSyncResponse response;
    response.sequence = 92u;
    response.clientSendTimeSeconds = 456.789;
    response.serverReceiveTimeSeconds = 457.001;
    require(encodeTimeSyncResponse(response, payload), "time-sync response encode failed");
    TimeSyncResponse decodedResponse;
    require(decodeTimeSyncResponse(payload, decodedResponse), "time-sync response decode failed");
    require(decodedResponse.sequence == response.sequence, "time-sync response sequence mismatch");
    require(nearlyEqual(decodedResponse.clientSendTimeSeconds,
                        response.clientSendTimeSeconds),
        "time-sync response client time mismatch");
    require(nearlyEqual(decodedResponse.serverReceiveTimeSeconds,
                        response.serverReceiveTimeSeconds),
        "time-sync response server time mismatch");
}

void testMapRequestRoundTrip()
{
    std::vector<MapRequest> requests;
    requests.emplace_back(GalaxyMapRequest{1001u});
    requests.emplace_back(SystemMapRequest{1002u, 17});

    DetailMapRequest detail;
    detail.requestId = 1003u;
    detail.target.sceneKind = world::celestial::DetailSceneKind::SpatialVolume;
    detail.target.focusClass = world::celestial::DetailObjectClass::Ship;
    detail.target.systemId = 23;
    detail.target.systemPositionLy = glm::dvec3(1.5, -2.25, 3.75);
    detail.target.anchorId = "anchor";
    detail.target.focusId = "focus";
    detail.target.spatialCell.level = 6;
    detail.target.spatialCell.maximumLevel = 6;
    detail.target.spatialCell.x = -11;
    detail.target.spatialCell.y = 22;
    detail.target.spatialCell.z = 33;
    detail.target.spatialCell.centerAu = glm::dvec3(0.01, 0.02, 0.03);
    detail.target.spatialCell.edgeAu = 0.004;
    requests.emplace_back(detail);

    HubMapRequest hub;
    hub.requestId = 1004u;
    hub.systemId = 31;
    hub.hubId = "hub:alpha";
    requests.emplace_back(hub);

    for (std::size_t i = 0; i < requests.size(); ++i)
    {
        std::vector<std::uint8_t> payload;
        require(encodeMapRequest(requests[i], payload), "map request encode failed");
        MapRequest decoded;
        require(decodeMapRequest(payload, decoded), "map request decode failed");
        require(decoded.index() == requests[i].index(), "map request variant mismatch");
    }

    const auto& decodedDetailSource = std::get<DetailMapRequest>(requests[2]);
    std::vector<std::uint8_t> payload;
    require(encodeMapRequest(requests[2], payload), "detail request re-encode failed");
    MapRequest decodedDetailVariant;
    require(decodeMapRequest(payload, decodedDetailVariant), "detail request re-decode failed");
    const auto& decodedDetail = std::get<DetailMapRequest>(decodedDetailVariant);
    require(decodedDetail.requestId == decodedDetailSource.requestId,
        "detail request id mismatch");
    require(decodedDetail.target == decodedDetailSource.target,
        "detail target mismatch");

    require(encodeMapRequest(requests[3], payload), "hub request re-encode failed");
    MapRequest decodedHubVariant;
    require(decodeMapRequest(payload, decodedHubVariant), "hub request re-decode failed");
    const auto& decodedHub = std::get<HubMapRequest>(decodedHubVariant);
    const auto& sourceHub = std::get<HubMapRequest>(requests[3]);
    require(decodedHub.requestId == sourceHub.requestId, "hub request id mismatch");
    require(decodedHub.systemId == sourceHub.systemId, "hub request system mismatch");
    require(decodedHub.hubId == sourceHub.hubId, "hub request id string mismatch");
}

void testTrailingAndInvalidPayloadRejection()
{
    ClientMessage message;
    ShipControlState control;
    control.controlTick = 1u;
    message.payload = control;

    std::vector<std::uint8_t> payload;
    require(encodeClientMessage(message, payload), "baseline client message encode failed");
    payload.push_back(0xFFu);
    ClientMessage decoded;
    require(!decodeClientMessage(payload, decoded),
        "client message accepted trailing unknown bytes");

    std::vector<std::uint8_t> invalidVariant = {0,0,0,0,0,0,0,1, 0xFFu};
    require(!decodeClientMessage(invalidVariant, decoded),
        "client message accepted unknown variant tag");
}
}

int main()
{
    testSignedScalarRoundTrip();
    testFrameByteOrderAndFragmentation();
    testFrameValidation();
    testSessionWelcomeRoundTrip();
    testClientMessageRoundTrip();
    testTimeSyncRoundTrip();
    testMapRequestRoundTrip();
    testTrailingAndInvalidPayloadRejection();

    std::cout << "[PASS] portable wire framing + control-plane codec" << '\n';
    return 0;
}
