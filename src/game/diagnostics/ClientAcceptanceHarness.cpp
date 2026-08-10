#include "src/game/diagnostics/ClientAcceptanceHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "src/game/client/GameClient.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/host/LocalGameSession.h"
#include "src/game/session/IGameSession.h"
#include "src/game/ship/controller/PlayerInputMapper.h"
#include "src/game/simulation/ShipSnapshot.h"
#include "src/world/celestial/DetailMapTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace game::diagnostics
{
namespace
{
constexpr double FrameSeconds = 0.02;
constexpr int SynchronizationFrameLimit = 400;
constexpr int WaitFrameLimit = 400;

class AcceptanceFailure final : public std::runtime_error
{
public:
    explicit AcceptanceFailure(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw AcceptanceFailure(message);
}

void pass(const char* name)
{
    std::cerr << "[PASS] client-acceptance " << name << '\n';
}

bool finiteVec(const glm::dvec3& value)
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool finiteVec(const glm::vec3& value)
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z);
}

bool finiteMatrix(const glm::mat4& value)
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            if (!std::isfinite(value[column][row]))
                return false;
        }
    }

    return true;
}

void requireOrientationBasis(
    const ShipTransform& transform,
    const std::string& label
)
{
    require(finiteMatrix(transform.orientation), label + ": orientation contains non-finite values");

    const glm::vec3 forward = transform.forward();
    const glm::vec3 right = transform.right();
    const glm::vec3 up = transform.up();

    require(finiteVec(forward) && finiteVec(right) && finiteVec(up), label + ": basis contains non-finite values");
    require(std::abs(glm::length(forward) - 1.0f) < 1.0e-4f, label + ": forward is not normalized");
    require(std::abs(glm::length(right) - 1.0f) < 1.0e-4f, label + ": right is not normalized");
    require(std::abs(glm::length(up) - 1.0f) < 1.0e-4f, label + ": up is not normalized");

    require(std::abs(glm::dot(forward, right)) < 1.0e-4f, label + ": forward/right are not orthogonal");
    require(std::abs(glm::dot(forward, up)) < 1.0e-4f, label + ": forward/up are not orthogonal");
    require(std::abs(glm::dot(right, up)) < 1.0e-4f, label + ": right/up are not orthogonal");

    const glm::vec3 reconstructedRight = glm::normalize(glm::cross(forward, up));
    require(glm::length(reconstructedRight - right) < 1.0e-4f, label + ": handedness changed");
}

class SyntheticKeyState final : public IPlayerInputKeyState
{
public:
    void clear()
    {
        m_pressed.clear();
    }

    void press(int key)
    {
        m_pressed.insert(key);
    }

    bool isKeyPressed(int key) const override
    {
        return m_pressed.find(key) != m_pressed.end();
    }

private:
    std::unordered_set<int> m_pressed;
};

ShipControlState mapKeys(
    PlayerInputMapper& mapper,
    SyntheticKeyState& keys
)
{
    ShipControlState control;
    mapper.updateFromKeyState(control, keys);
    return control;
}

const ShipSnapshot& findServerShip(
    const game::debug::IDebugSessionControl& debug,
    EntityId id
)
{
    const auto& ships = debug.snapshot().ships;
    const auto it = std::find_if(
        ships.begin(),
        ships.end(),
        [id](const ShipSnapshot& ship)
        {
            return ship.id == id;
        }
    );

    require(it != ships.end(), "authoritative player ship disappeared from server snapshot");
    return *it;
}

const ClientShipState& findClientShip(
    const GameClient& client,
    EntityId id
)
{
    const auto& ships = client.world().ships();
    const auto it = ships.find(id.value);
    require(it != ships.end(), "player ship disappeared from ClientWorldState");
    return it->second;
}

void synchronize(game::host::LocalGameSession& session)
{
    session.beginSynchronization();

    for (int frame = 0; frame < SynchronizationFrameLimit; ++frame)
    {
        if (session.state() == game::session::GameSessionState::Ready)
            return;

        if (session.state() == game::session::GameSessionState::Failed)
            break;

        session.updateSynchronization(FrameSeconds);
    }

    throw AcceptanceFailure(
        std::string("local session did not synchronize: ") + session.error()
    );
}

void runFrame(
    game::host::LocalGameSession& session,
    const ShipControlState& control
)
{
    GameClient& client = session.client();

    // Mirror the production SpaceState frame order:
    // synchronize/prepare -> sample input -> advance server -> client update.
    client.prepareGameplayFrame(FrameSeconds);
    client.submitInput(control);
    session.advance(FrameSeconds);
    client.update(
        static_cast<float>(FrameSeconds),
        static_cast<float>(session.fixedStepSeconds()),
        FrameSeconds
    );
}

void runFrames(
    game::host::LocalGameSession& session,
    const ShipControlState& control,
    int frameCount
)
{
    for (int i = 0; i < frameCount; ++i)
        runFrame(session, control);
}

template <typename Predicate>
void waitFor(
    game::host::LocalGameSession& session,
    Predicate predicate,
    const std::string& failureMessage
)
{
    ShipControlState neutral;

    for (int frame = 0; frame < WaitFrameLimit; ++frame)
    {
        if (predicate())
            return;

        runFrame(session, neutral);
    }

    throw AcceptanceFailure(failureMessage);
}

void testInputMapping()
{
    PlayerInputMapper mapper;
    SyntheticKeyState keys;

    keys.press(GLFW_KEY_W);
    keys.press(GLFW_KEY_D);
    keys.press(GLFW_KEY_Q);
    auto control = mapKeys(mapper, keys);
    require(control.pitchInput == -1.0f, "W no longer maps to negative pitch");
    require(control.rollInput == 1.0f, "D no longer maps to positive roll");
    require(control.yawInput == 1.0f, "Q no longer maps to positive yaw");

    keys.clear();
    keys.press(GLFW_KEY_KP_8);
    keys.press(GLFW_KEY_KP_6);
    keys.press(GLFW_KEY_KP_9);
    control = mapKeys(mapper, keys);
    require(control.forwardInput == 1.0f, "KP8 no longer maps to forward manoeuvre thrust");
    require(control.strafeInput == 1.0f, "KP6 no longer maps to positive strafe");
    require(control.liftInput == 1.0f, "KP9 no longer maps to positive lift");

    keys.clear();
    keys.press(GLFW_KEY_EQUAL);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == 1.0f, "= no longer increases target speed");

    keys.clear();
    keys.press(GLFW_KEY_MINUS);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == -1.0f, "- no longer decreases target speed");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    keys.press(GLFW_KEY_Q);
    control = mapKeys(mapper, keys);
    require(control.yawInput == 0.0f, "Ctrl+Q leaked into yaw instead of command chord handling");

    keys.clear();
    keys.press(GLFW_KEY_J);
    keys.press(GLFW_KEY_W);
    keys.press(GLFW_KEY_Q);
    keys.press(GLFW_KEY_KP_8);
    control = mapKeys(mapper, keys);
    require(control.cruiseActive, "J no longer enables cruise control state");
    require(control.pitchInput == 0.0f && control.yawInput == 0.0f, "cruise no longer suppresses rotation input");
    require(control.forwardInput == 0.0f, "cruise no longer suppresses manoeuvre thrust");

    pass("INPUT MAPPING");
}

void testBootAndIdle(
    game::host::LocalGameSession& session,
    game::debug::IDebugSessionControl& debug,
    EntityId playerId
)
{
    GameClient& client = session.client();
    require(client.readyForGameplay(), "client is not ready after synchronization");
    require(client.hasSessionSnapshot(), "client has no session snapshot after synchronization");

    const auto& serverPlayer = findServerShip(debug, playerId);
    const auto& clientPlayer = findClientShip(client, playerId);

    require(serverPlayer.role == ShipRole::Player, "authoritative player role changed");
    require(clientPlayer.role == ShipRole::Player, "client player role changed");
    require(serverPlayer.transform.motion.systemId == client.playerNavigation().currentSystemId, "player system membership differs from client navigation state");
    require(serverPlayer.transform.motion.mode == game::navigation::MotionMode::HubTactical, "initial player is no longer in HubTactical runtime mode");
    require(serverPlayer.referenceFrame.valid, "initial player reference frame is invalid");
    require(!serverPlayer.referenceFrame.hubId.empty(), "initial player has no hub reference frame");
    require(finiteVec(serverPlayer.transform.motion.localPositionMeters), "server player local position is non-finite");
    require(finiteVec(clientPlayer.renderTransform.motion.localPositionMeters), "client player render position is non-finite");
    requireOrientationBasis(serverPlayer.transform, "server player boot orientation");
    requireOrientationBasis(clientPlayer.renderTransform, "client player boot render orientation");

    const glm::dvec3 startLocal = serverPlayer.transform.motion.localPositionMeters;
    const glm::mat4 startOrientation = serverPlayer.transform.orientation;

    ShipControlState neutral;
    runFrames(session, neutral, 50);

    const auto& after = findServerShip(debug, playerId);
    require(glm::length(after.transform.motion.localPositionMeters - startLocal) < 0.05, "idle player drifted in hub-local space");

    double orientationDelta = 0.0;
    for (int column = 0; column < 3; ++column)
        orientationDelta += glm::length(glm::vec3(after.transform.orientation[column] - startOrientation[column]));
    require(orientationDelta < 1.0e-4, "idle player orientation drifted");

    pass("BOOT + IDLE STABILITY");
}

void testFastUniverseRoundTrip(
    game::host::LocalGameSession& session,
    game::debug::IDebugSessionControl& debug,
    EntityId playerId
)
{
    GameClient& client = session.client();
    PlayerInputMapper mapper;
    SyntheticKeyState keys;

    const auto beforeRevision = client.sessionSnapshot().universeTimelineRevision;
    const auto before = findServerShip(debug, playerId);
    const glm::dvec3 beforeLocal = before.transform.motion.localPositionMeters;
    const glm::mat4 beforeOrientation = before.transform.orientation;

    constexpr double TestScale = 200.0;
    debug.setUniverseTimeSimulation(true, TestScale);

    waitFor(
        session,
        [&]()
        {
            return
                client.hasSessionSnapshot() &&
                client.sessionSnapshot().universeTimeSimulation &&
                client.sessionSnapshot().universeTimelineRevision > beforeRevision;
        },
        "client did not enter accelerated universe-time branch"
    );

    const auto acceleratedRevision = client.sessionSnapshot().universeTimelineRevision;
    require(std::abs(client.sessionSnapshot().universeTimeScale - TestScale) < 1.0e-9, "client received wrong accelerated universe-time scale");

    // Deliberately "touch the controls" while gameplay is frozen. Those
    // commands must be acknowledged/discarded by the diagnostic branch and
    // must not fire after returning to normal gameplay.
    keys.press(GLFW_KEY_KP_8);
    keys.press(GLFW_KEY_Q);
    const ShipControlState attemptedControl = mapKeys(mapper, keys);
    runFrames(session, attemptedControl, 30);

    debug.setUniverseTimeSimulation(false, TestScale);

    waitFor(
        session,
        [&]()
        {
            return
                client.hasSessionSnapshot() &&
                !client.sessionSnapshot().universeTimeSimulation &&
                client.sessionSnapshot().universeTimelineRevision > acceleratedRevision;
        },
        "client did not return from accelerated universe-time branch"
    );

    ShipControlState neutral;
    runFrames(session, neutral, 20);

    const auto& restored = findServerShip(debug, playerId);
    require(glm::length(restored.transform.motion.localPositionMeters - beforeLocal) < 0.05, "controls leaked from accelerated branch into restored gameplay position");

    double orientationDelta = 0.0;
    for (int column = 0; column < 3; ++column)
        orientationDelta += glm::length(glm::vec3(restored.transform.orientation[column] - beforeOrientation[column]));
    require(orientationDelta < 1.0e-4, "controls leaked from accelerated branch into restored gameplay orientation");

    pass("FAST TIME ROUND-TRIP");
}

void testOrientationAndMovement(
    game::host::LocalGameSession& session,
    game::debug::IDebugSessionControl& debug,
    EntityId playerId
)
{
    PlayerInputMapper mapper;
    SyntheticKeyState keys;

    const auto& beforeTurn = findServerShip(debug, playerId);
    const glm::vec3 initialForward = beforeTurn.transform.forward();

    keys.press(GLFW_KEY_Q);
    const ShipControlState yawControl = mapKeys(mapper, keys);
    runFrames(session, yawControl, 25);

    keys.clear();
    ShipControlState neutral = mapKeys(mapper, keys);
    runFrames(session, neutral, 50);

    const auto& afterTurn = findServerShip(debug, playerId);
    requireOrientationBasis(afterTurn.transform, "server player after yaw");
    require(glm::length(afterTurn.transform.forward() - initialForward) > 0.02f, "yaw input did not change authoritative forward vector");

    const glm::vec3 forwardBeforeThrust = afterTurn.transform.forward();
    const glm::dvec3 localBeforeThrust = afterTurn.transform.motion.localPositionMeters;

    keys.press(GLFW_KEY_KP_8);
    const ShipControlState forwardControl = mapKeys(mapper, keys);
    runFrames(session, forwardControl, 75);

    keys.clear();
    neutral = mapKeys(mapper, keys);
    runFrames(session, neutral, 20);

    const auto& afterThrust = findServerShip(debug, playerId);
    const glm::dvec3 displacement =
        afterThrust.transform.motion.localPositionMeters - localBeforeThrust;

    require(glm::length(displacement) > 0.25, "forward manoeuvre input did not move authoritative player");
    require(afterThrust.transform.motion.forwardSpeedMps > 0.0, "forward manoeuvre input did not produce forward speed");
    require(afterThrust.acknowledgedControlTick > 0, "server never acknowledged client fixed-step controls");

    const auto& frame = afterThrust.referenceFrame;
    require(frame.valid, "player reference frame became invalid during movement");

    const glm::dvec3 localForward {
        glm::dot(glm::dvec3(forwardBeforeThrust), frame.progradeAxis),
        glm::dot(glm::dvec3(forwardBeforeThrust), frame.radialAxis),
        glm::dot(glm::dvec3(forwardBeforeThrust), frame.normalAxis)
    };

    require(glm::length(localForward) > 0.9, "could not project ship forward into hub-local basis");
    require(glm::dot(displacement, glm::normalize(localForward)) > 0.0, "ship moved opposite to its forward vector after yaw");

    const double targetSpeedBeforeThrottle =
        afterThrust.transform.motion.targetForwardSpeedMps;

    keys.press(GLFW_KEY_EQUAL);
    const ShipControlState throttleUpControl = mapKeys(mapper, keys);
    runFrames(session, throttleUpControl, 25);

    const auto& afterThrottleUp = findServerShip(debug, playerId);
    require(
        afterThrottleUp.transform.motion.targetForwardSpeedMps >
            targetSpeedBeforeThrottle + 1.0,
        "target-speed control did not increase authoritative engine setpoint"
    );

    const double raisedTargetSpeed =
        afterThrottleUp.transform.motion.targetForwardSpeedMps;

    keys.clear();
    keys.press(GLFW_KEY_MINUS);
    const ShipControlState throttleDownControl = mapKeys(mapper, keys);
    runFrames(session, throttleDownControl, 25);

    const auto& afterThrottleDown = findServerShip(debug, playerId);
    require(
        afterThrottleDown.transform.motion.targetForwardSpeedMps <
            raisedTargetSpeed - 1.0,
        "target-speed control did not decrease authoritative engine setpoint"
    );

    keys.clear();
    neutral = mapKeys(mapper, keys);
    runFrames(session, neutral, 10);

    const auto& clientPlayer = findClientShip(session.client(), playerId);
    require(finiteVec(clientPlayer.transform.motion.localPositionMeters), "client predicted player position became non-finite");
    require(finiteVec(clientPlayer.renderTransform.motion.localPositionMeters), "client rendered player position became non-finite");
    requireOrientationBasis(clientPlayer.renderTransform, "client player render orientation after flight");

    pass("ORIENTATION + PLAYER FLIGHT + ENGINE CONTROL");
}

void testRemoteMotion(game::host::LocalGameSession& session)
{
    auto capture = [&]()
    {
        std::unordered_map<std::uint8_t, glm::dvec3> positions;

        for (const auto& [id, ship] : session.client().world().ships())
        {
            (void)id;
            if (ship.motionLabKind == HubMotionLabActorKind::None)
                continue;

            positions[static_cast<std::uint8_t>(ship.motionLabKind)] =
                ship.renderTransform.motion.localPositionMeters;
        }

        return positions;
    };

    const auto before = capture();
    require(before.size() == HubMotionLabActors.size(), "not all Hub Motion Lab NPCs reached ClientWorldState");

    ShipControlState neutral;
    runFrames(session, neutral, 75);

    const auto after = capture();
    require(after.size() == before.size(), "Hub Motion Lab NPC count changed during presentation test");

    for (const auto& [kind, beforePosition] : before)
    {
        const auto it = after.find(kind);
        require(it != after.end(), "Hub Motion Lab NPC disappeared during presentation test");
        require(finiteVec(it->second), "Hub Motion Lab rendered position became non-finite");
        require(glm::length(it->second - beforePosition) > 0.01, "Hub Motion Lab NPC render position did not advance");
    }

    const auto& telemetry = session.client().world().hubMotionLabPresentationSample();
    require(std::isfinite(telemetry.requestedRenderTimeSeconds), "presentation render time became non-finite");

    pass("REMOTE NPC PRESENTATION");
}

void testMapDataPipeline(game::host::LocalGameSession& session)
{
    GameClient& client = session.client();
    const int currentSystemId = client.playerNavigation().currentSystemId;
    require(currentSystemId >= 0, "client has no current system for map acceptance test");

    client.requestGalaxyMapSnapshot(true);
    waitFor(
        session,
        [&]() { return client.galaxyMapSnapshot() != nullptr; },
        "Galaxy map request did not complete through client/server transport"
    );

    const auto* galaxy = client.galaxyMapSnapshot();
    require(galaxy && !galaxy->systems.empty(), "Galaxy map snapshot is empty");
    require(
        std::any_of(
            galaxy->systems.begin(),
            galaxy->systems.end(),
            [currentSystemId](const world::celestial::GalaxyMapSystem& system)
            {
                return system.id == currentSystemId;
            }
        ),
        "current system is missing from Galaxy map snapshot"
    );

    client.requestSystemMapSnapshot(currentSystemId, true);
    waitFor(
        session,
        [&]() { return client.systemMapSnapshot(currentSystemId) != nullptr; },
        "System map request did not complete through client/server transport"
    );

    const auto* system = client.systemMapSnapshot(currentSystemId);
    require(system && system->systemId == currentSystemId, "System map returned wrong system");
    require(!system->bodies.empty(), "System map has no celestial bodies");

    const auto hubIt = std::find_if(
        system->objects.begin(),
        system->objects.end(),
        [](const world::celestial::SystemMapObject& object)
        {
            return
                object.kind == world::celestial::SystemMapObjectKind::Hub &&
                !object.stableId.empty();
        }
    );
    require(hubIt != system->objects.end(), "System map contains no selectable hub");

    world::celestial::DetailTarget detailTarget;
    detailTarget.systemId = currentSystemId;
    detailTarget.systemPositionLy = system->systemPositionLy;
    detailTarget.focusClass = world::celestial::DetailObjectClass::Hub;
    detailTarget.focusId = hubIt->stableId;

    if (!hubIt->parentBodyId.empty())
    {
        detailTarget.sceneKind = world::celestial::DetailSceneKind::CelestialBody;
        detailTarget.anchorId = hubIt->parentBodyId;
    }
    else
    {
        detailTarget.sceneKind = world::celestial::DetailSceneKind::LocalObject;
        detailTarget.anchorId = hubIt->stableId;
    }

    require(detailTarget.valid(), "derived Details target for selected hub is invalid");

    client.requestDetailMapSnapshot(detailTarget, true);
    waitFor(
        session,
        [&]() { return client.detailMapSnapshot(detailTarget) != nullptr; },
        "Details map request did not complete through client/server transport"
    );

    const auto* detail = client.detailMapSnapshot(detailTarget);
    require(detail && detail->valid, "Details map snapshot is invalid");
    require(detail->systemId == currentSystemId, "Details map crossed system boundary");
    require(detail->detailTarget == detailTarget, "Details map lost semantic navigation target");

    client.requestHubMapSnapshot(currentSystemId, hubIt->stableId, true);
    waitFor(
        session,
        [&]()
        {
            return client.hubMapSnapshot(currentSystemId, hubIt->stableId) != nullptr;
        },
        "Hub map request did not complete through client/server transport"
    );

    const auto* hub = client.hubMapSnapshot(currentSystemId, hubIt->stableId);
    require(hub && hub->valid, "Hub map snapshot is invalid");
    require(hub->systemId == currentSystemId, "Hub map crossed system boundary");
    require(hub->hubId == hubIt->stableId, "Hub map returned wrong hub");

    const auto revision = client.sessionSnapshot().universeTimelineRevision;
    require(client.galaxyMapMetadata().universeTimelineRevision == revision, "Galaxy map timeline revision differs from gameplay");
    require(client.systemMapMetadata().universeTimelineRevision == revision, "System map timeline revision differs from gameplay");
    require(client.detailMapMetadata().universeTimelineRevision == revision, "Details map timeline revision differs from gameplay");
    require(client.hubMapMetadata().universeTimelineRevision == revision, "Hub map timeline revision differs from gameplay");

    pass("MAP DATA PIPELINE");
}

} // namespace

int runClientAcceptanceSelfTest()
{
    try
    {
        std::cerr << "[SELFTEST] client-acceptance stage=input-mapping\n";
        testInputMapping();

        std::cerr << "[SELFTEST] client-acceptance stage=construct-local-session\n";
        game::host::LocalGameSession session;
        synchronize(session);

        auto* debug = session.debugControl();
        require(debug != nullptr, "local session has no debug/control facade");

        const EntityId playerId = session.playerId();
        require(playerId.value != 0, "local session returned invalid player id");

        std::cerr << "[SELFTEST] client-acceptance stage=boot-idle\n";
        testBootAndIdle(session, *debug, playerId);

        std::cerr << "[SELFTEST] client-acceptance stage=fast-time\n";
        testFastUniverseRoundTrip(session, *debug, playerId);

        std::cerr << "[SELFTEST] client-acceptance stage=flight\n";
        testOrientationAndMovement(session, *debug, playerId);

        std::cerr << "[SELFTEST] client-acceptance stage=remote-motion\n";
        testRemoteMotion(session);

        std::cerr << "[SELFTEST] client-acceptance stage=maps\n";
        testMapDataPipeline(session);

        std::cerr << "[PASS] client-acceptance real local-session scenarios\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[FAIL] client-acceptance: " << e.what() << '\n';
        return 4;
    }
}

} // namespace game::diagnostics
