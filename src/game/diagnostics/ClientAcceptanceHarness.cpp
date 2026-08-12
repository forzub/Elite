#include "src/game/diagnostics/ClientAcceptanceHarness.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "src/core/Application.h"
#include "src/game/client/GameClient.h"
#include "src/game/navigation/CoordinateDisplayService.h"
#include "src/game/presentation/ClientHudPresentation.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/presentation/StarSystemLabelPresentation.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/game/ui/SystemMapUiCommandRouter.h"
#include "src/game/debug/IDebugSessionControl.h"
#include "src/game/diagnostics/HubMotionLab.h"
#include "src/game/host/LocalGameSession.h"
#include "src/game/session/IGameSession.h"
#include "src/game/ship/controller/PlayerInputMapper.h"
#include "src/game/simulation/ShipSnapshot.h"
#include "src/render/starfield/GalaxyStarfieldRenderer.h"
#include "src/world/celestial/DetailMapTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/ui/components/UIContainer.h"
#include "src/ui/components/UIText.h"

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
    SyntheticKeyState& keys,
    game::navigation::LocalFlightControlLaw currentLocalControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian
)
{
    ShipControlState control;
    mapper.updateFromKeyState(
        control,
        keys,
        currentLocalControlLaw
    );
    return control;
}

void testGameUiToggleContract()
{
    GameUiController ui;

    require(!ui.isOpen(), "game UI controller no longer starts closed");

    auto checkLatch = [&](auto consume, const char* name)
    {
        require((ui.*consume)(true), std::string("first ") + name + " press is no longer consumed");
        require(!(ui.*consume)(true), std::string("held ") + name + " now retriggers instead of latching");
        require(!(ui.*consume)(false), std::string(name) + " release unexpectedly triggers an action");
        require((ui.*consume)(true), std::string(name) + " latch did not reset after release");
        (ui.*consume)(false);
    };

    checkLatch(&GameUiController::consumeF9Press, "F9");
    checkLatch(&GameUiController::consumeF10Press, "F10");
    checkLatch(&GameUiController::consumeF11Press, "F11");
    checkLatch(&GameUiController::consumeF12Press, "F12");

    require(
        ui.navigationAction(true) == GameUiNavigationAction::OpenOrSwitch,
        "closed map UI now treats a function key as close instead of open"
    );

    require(ui.open(GameUiMode::SystemMap), "SystemMap UI did not open through direct-open contract");
    require(ui.isMode(GameUiMode::SystemMap), "SystemMap UI mode was not recorded as open");
    require(!ui.open(GameUiMode::SystemMap), "opening the same SystemMap mode retriggered state change");
    require(
        ui.navigationAction(true) == GameUiNavigationAction::Close,
        "same F9-F12 level no longer closes an already-visible map"
    );
    require(
        ui.navigationAction(false) == GameUiNavigationAction::OpenOrSwitch,
        "different F9-F12 level now closes instead of switching maps"
    );
    require(ui.close(), "SystemMap UI did not close through close contract");
    require(!ui.isOpen(), "SystemMap UI remained open after close");

    pass("F9-F12 SAME-LEVEL CLOSE + CROSS-LEVEL SWITCH");
}

void testCoordinateDisplayHotkeyContract()
{
    auto& display = game::navigation::CoordinateDisplayService::instance();
    const auto saved = display.format();

    display.setFormat(game::navigation::CoordinateDisplayFormat::Hierarchical);
    require(std::string(display.formatName()) == "STRAIGHT THERE", "hierarchical coordinate display name changed");
    require(
        display.formatLine("G1 0/0/0").find("[STRAIGHT THERE]") == 0,
        "hierarchical coordinate display no longer reaches visible formatted line"
    );

    display.cycle();
    require(display.format() == game::navigation::CoordinateDisplayFormat::Axis, "first coordinate-format cycle no longer selects Axis");
    require(std::string(display.formatName()) == "THREE AXES", "Axis coordinate display name changed");

    display.cycle();
    require(display.format() == game::navigation::CoordinateDisplayFormat::PackedBase32, "second coordinate-format cycle no longer selects PackedBase32");
    require(std::string(display.formatName()) == "VERY SECRET CODE", "PackedBase32 coordinate display name changed");

    display.cycle();
    require(display.format() == game::navigation::CoordinateDisplayFormat::Hierarchical, "coordinate-format cycle no longer wraps to Hierarchical");

    display.setFormat(saved);
    pass("CTRL+F11 + COORDINATE DISPLAY FORMAT");
}

void testConstellationOverlayContract()
{
    GalaxyStarfieldRenderer starfield;

    require(!starfield.constellationOverlayEnabled(), "constellation overlay no longer starts disabled");
    starfield.setConstellationOverlayEnabled(true);
    require(starfield.constellationOverlayEnabled(), "constellation overlay cannot be enabled");
    starfield.setConstellationOverlayEnabled(false);
    require(!starfield.constellationOverlayEnabled(), "constellation overlay cannot be disabled");

    pass("CTRL+F12 + CONSTELLATION OVERLAY STATE");
}

class SystemMapUiTargetSpy final : public game::ui::ISystemMapUiTarget
{
public:
    void reset()
    {
        actions.clear();
    }

    void selectSystemMapSystem(int systemId) override
    {
        actions.push_back("select:" + std::to_string(systemId));
    }

    void setSystemMapGalaxyMode() override
    {
        actions.push_back("galaxy");
    }

    void setSystemMapCurrentSystemMode() override
    {
        actions.push_back("current-system");
    }

    void setSystemMapHubMode() override
    {
        actions.push_back("hub");
    }

    void setSystemMapLoadedDetailMode() override
    {
        actions.push_back("loaded-detail");
    }

    void setSystemMapDetailMode() override
    {
        actions.push_back("selected-detail");
    }

    std::vector<std::string> actions;
};

void testSystemMapUiCommandContract()
{
    using game::ui::SystemMapUiCommandType;

    auto expect = [](
        const char* text,
        SystemMapUiCommandType expectedType,
        int expectedSystemId = -1
    )
    {
        const auto command = game::ui::parseSystemMapUiCommand(text);
        require(command.has_value(), std::string("map UI command no longer parses: ") + text);
        require(command->type == expectedType, std::string("map UI command changed meaning: ") + text);
        require(command->systemId == expectedSystemId, std::string("map UI command changed system id: ") + text);
    };

    expect("system_map_open_selected:42", SystemMapUiCommandType::OpenSelectedSystem, 42);
    expect("system_map_select:17", SystemMapUiCommandType::SelectSystem, 17);
    expect("system_map_galaxy", SystemMapUiCommandType::Galaxy);
    expect("system_map_current_system", SystemMapUiCommandType::CurrentSystem);
    expect("system_map_hub", SystemMapUiCommandType::Hub);
    expect("system_map_detail", SystemMapUiCommandType::LoadedDetail);
    expect("system_map_planet", SystemMapUiCommandType::SelectedDetail);
    expect("close_system_map", SystemMapUiCommandType::Close);

    require(!game::ui::parseSystemMapUiCommand("system_map_select:not-a-number"), "malformed map selection command is accepted");
    require(!game::ui::parseSystemMapUiCommand("system_map_open_selected:-1"), "negative map system id is accepted");
    require(!game::ui::parseSystemMapUiCommand("unrelated_command"), "unrelated UI command was captured by map router");

    pass("SYSTEM MAP UI COMMAND CONTRACT");
}

void testSystemMapUiCommandDispatchContract()
{
    SystemMapUiTargetSpy target;
    bool closed = false;

    auto dispatch = [&](const char* text)
    {
        target.reset();
        closed = false;

        const auto command = game::ui::parseSystemMapUiCommand(text);
        require(command.has_value(), std::string("dispatch input no longer parses: ") + text);

        game::ui::dispatchSystemMapUiCommand(
            *command,
            &target,
            [&]()
            {
                closed = true;
            }
        );
    };

    dispatch("system_map_open_selected:42");
    require(
        target.actions == std::vector<std::string>{"select:42", "current-system"},
        "open-selected no longer selects a system before entering System mode"
    );
    require(!closed, "open-selected unexpectedly closes map UI");

    dispatch("system_map_select:17");
    require(
        target.actions == std::vector<std::string>{"select:17"},
        "system selection command changed action"
    );

    dispatch("system_map_galaxy");
    require(target.actions == std::vector<std::string>{"galaxy"}, "Galaxy command changed action");

    dispatch("system_map_current_system");
    require(target.actions == std::vector<std::string>{"current-system"}, "current-system command changed action");

    dispatch("system_map_hub");
    require(target.actions == std::vector<std::string>{"hub"}, "Hub command changed action");

    dispatch("system_map_detail");
    require(target.actions == std::vector<std::string>{"loaded-detail"}, "loaded Details command changed action");

    dispatch("system_map_planet");
    require(target.actions == std::vector<std::string>{"selected-detail"}, "selected Details command changed action");

    dispatch("close_system_map");
    require(target.actions.empty(), "close command touched map state target");
    require(closed, "close command no longer closes map UI");

    pass("SYSTEM MAP UI COMMAND DISPATCH");
}

void runFrame(
    game::host::LocalGameSession& session,
    const ShipControlState& control
);

ShipSnapshot findServerShip(
    game::host::LocalGameSession& session,
    game::debug::IDebugSessionControl& debug,
    EntityId id
)
{
    // Debug snapshots now cross the server ownership boundary as copied values.
    // Request a fresh authoritative publication explicitly instead of relying
    // on a reference into GameServer's current m_lastSnapshot.
    const std::uint64_t previousRevision = debug.snapshotRevision();
    debug.refreshSnapshot();

    ShipControlState neutral;
    for (int frame = 0; frame < WaitFrameLimit; ++frame)
    {
        runFrame(session, neutral);
        if (debug.snapshotRevision() > previousRevision)
            break;
    }

    require(
        debug.snapshotRevision() > previousRevision,
        "authoritative debug snapshot refresh did not cross the server boundary"
    );

    const auto snapshot = debug.snapshot();
    const auto& ships = snapshot.ships;
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

void runFrameForDuration(
    game::host::LocalGameSession& session,
    const ShipControlState& control,
    double frameSeconds
)
{
    GameClient& client = session.client();

    // Mirror the production SpaceState frame order:
    // synchronize/prepare -> sample input -> advance server -> client update.
    client.prepareGameplayFrame(frameSeconds);
    client.submitInput(control);
    session.advance(frameSeconds);
    client.update(
        static_cast<float>(frameSeconds),
        static_cast<float>(session.fixedStepSeconds()),
        frameSeconds
    );
}

void runFrame(
    game::host::LocalGameSession& session,
    const ShipControlState& control
)
{
    runFrameForDuration(session, control, FrameSeconds);
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
    keys.press(GLFW_KEY_S);
    keys.press(GLFW_KEY_A);
    keys.press(GLFW_KEY_E);
    control = mapKeys(mapper, keys);
    require(control.pitchInput == 1.0f, "S no longer maps to positive pitch");
    require(control.rollInput == -1.0f, "A no longer maps to negative roll");
    require(control.yawInput == -1.0f, "E no longer maps to negative yaw");

    keys.clear();
    keys.press(GLFW_KEY_KP_8);
    keys.press(GLFW_KEY_KP_6);
    keys.press(GLFW_KEY_KP_9);
    control = mapKeys(mapper, keys);
    require(control.forwardInput == 1.0f, "KP8 no longer maps to forward manoeuvre thrust");
    require(control.strafeInput == 1.0f, "KP6 no longer maps to positive strafe");
    require(control.liftInput == 1.0f, "KP9 no longer maps to positive lift");

    keys.clear();
    keys.press(GLFW_KEY_KP_2);
    keys.press(GLFW_KEY_KP_4);
    keys.press(GLFW_KEY_KP_3);
    control = mapKeys(mapper, keys);
    require(control.forwardInput == -1.0f, "KP2 no longer maps to reverse manoeuvre thrust");
    require(control.strafeInput == -1.0f, "KP4 no longer maps to negative strafe");
    require(control.liftInput == -1.0f, "KP3 no longer maps to negative lift");

    keys.clear();
    keys.press(GLFW_KEY_EQUAL);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == 1.0f, "= no longer maps to positive longitudinal command");

    keys.clear();
    keys.press(GLFW_KEY_KP_ADD);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == 1.0f, "KP+ no longer maps to positive longitudinal command");

    keys.clear();
    keys.press(GLFW_KEY_MINUS);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == -1.0f, "- no longer maps to negative longitudinal command");

    keys.clear();
    keys.press(GLFW_KEY_KP_SUBTRACT);
    control = mapKeys(mapper, keys);
    require(control.targetSpeedRate == -1.0f, "KP- no longer maps to negative longitudinal command");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    keys.press(GLFW_KEY_Q);
    control = mapKeys(mapper, keys);
    require(control.yawInput == 0.0f, "Ctrl+Q leaked into yaw instead of command chord handling");

    keys.clear();
    keys.press(GLFW_KEY_RIGHT_CONTROL);
    keys.press(GLFW_KEY_E);
    control = mapKeys(mapper, keys);
    require(control.yawInput == 0.0f, "Right-Ctrl+E leaked into yaw instead of command chord handling");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    keys.press(GLFW_KEY_F10);
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "Ctrl+F10 switched on press instead of release");
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "held Ctrl+F10 retriggers local flight-law switching");

    // First apparent release is not enough: simulate one up sample followed by
    // a down sample and make sure the release debounce treats it as bounce.
    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "Ctrl+F10 release debounce committed too early");

    keys.press(GLFW_KEY_F10);
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "Ctrl+F10 bounce retriggered flight-law switching");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "Ctrl+F10 release debounce committed on first stable sample");
    control = mapKeys(mapper, keys);
    require(!control.localControlLawCommandValid, "Ctrl+F10 release debounce committed on second stable sample");
    control = mapKeys(mapper, keys);
    require(control.localControlLawCommandValid, "first debounced Ctrl+F10 release emitted no flight-law command");
    require(
        control.requestedLocalControlLaw == game::navigation::LocalFlightControlLaw::Assisted,
        "first Ctrl+F10 release no longer selects Assisted mode"
    );

    // Holding Ctrl is allowed between deliberate F10 presses. The second
    // switch must again occur only after the qualified F10 release.
    keys.press(GLFW_KEY_F10);
    control = mapKeys(
        mapper,
        keys,
        game::navigation::LocalFlightControlLaw::Assisted
    );
    require(!control.localControlLawCommandValid, "second Ctrl+F10 switched on press instead of release");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    (void)mapKeys(
        mapper,
        keys,
        game::navigation::LocalFlightControlLaw::Assisted
    );
    (void)mapKeys(
        mapper,
        keys,
        game::navigation::LocalFlightControlLaw::Assisted
    );
    control = mapKeys(
        mapper,
        keys,
        game::navigation::LocalFlightControlLaw::Assisted
    );
    require(control.localControlLawCommandValid, "second debounced Ctrl+F10 release emitted no command");
    require(
        control.requestedLocalControlLaw == game::navigation::LocalFlightControlLaw::Newtonian,
        "second Ctrl+F10 release no longer returns to Newtonian mode"
    );

    keys.clear();
    keys.press(GLFW_KEY_HOME);
    control = mapKeys(mapper, keys);
    require(
        control.velocityAlignmentCommand == game::navigation::VelocityAlignmentMode::ForwardToVelocity,
        "HOME no longer requests nose-to-velocity alignment"
    );

    keys.clear();
    keys.press(GLFW_KEY_INSERT);
    control = mapKeys(mapper, keys);
    require(
        control.velocityAlignmentCommand == game::navigation::VelocityAlignmentMode::BackwardToVelocity,
        "INSERT no longer requests tail-to-velocity alignment"
    );

    keys.clear();
    keys.press(GLFW_KEY_END);
    control = mapKeys(mapper, keys);
    require(
        control.velocityAlignmentCommand == game::navigation::VelocityAlignmentMode::BrakeToStop,
        "END no longer requests velocity autobrake"
    );

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

    const auto serverPlayer = findServerShip(session, debug, playerId);
    const auto& clientPlayer = findClientShip(client, playerId);

    require(serverPlayer.role == ShipRole::Player, "authoritative player role changed");
    require(clientPlayer.role == ShipRole::Player, "client player role changed");
    require(serverPlayer.transform.motion.systemId == client.playerNavigation().currentSystemId, "player system membership differs from client navigation state");
    require(serverPlayer.transform.motion.mode == game::navigation::MotionMode::HubTactical, "initial player is no longer in HubTactical runtime mode");
    require(serverPlayer.referenceFrame.valid, "initial player reference frame is invalid");
    require(!serverPlayer.referenceFrame.hubId.empty(), "initial player has no hub reference frame");
    require(serverPlayer.transform.motion.travelFrame.valid, "initial player has no owned travel frame");
    require(serverPlayer.transform.motion.matchedToReferenceFrame, "initial player travel frame is not matched to spawn hub");
    require(!serverPlayer.transform.motion.travelFrame.frameId.empty(), "initial player travel frame has no identity");
    require(
        serverPlayer.referenceFrame.frameId == serverPlayer.transform.motion.travelFrame.frameId,
        "snapshot reference frame is not sourced from the owned player travel frame"
    );
    require(finiteVec(serverPlayer.transform.motion.localPositionMeters), "server player local position is non-finite");
    require(finiteVec(clientPlayer.renderTransform.motion.localPositionMeters), "client player render position is non-finite");
    requireOrientationBasis(serverPlayer.transform, "server player boot orientation");
    requireOrientationBasis(clientPlayer.renderTransform, "client player boot render orientation");

    const glm::dvec3 startLocal = serverPlayer.transform.motion.localPositionMeters;
    const glm::mat4 startOrientation = serverPlayer.transform.orientation;

    ShipControlState neutral;
    runFrames(session, neutral, 50);

    const auto after = findServerShip(session, debug, playerId);
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
    const auto before = findServerShip(session, debug, playerId);
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

    const auto restored = findServerShip(session, debug, playerId);
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

    const auto beforeTurn = findServerShip(session, debug, playerId);
    const glm::vec3 initialForward = beforeTurn.transform.forward();

    keys.press(GLFW_KEY_Q);
    const ShipControlState yawControl = mapKeys(mapper, keys);
    runFrames(session, yawControl, 25);

    keys.clear();
    ShipControlState neutral = mapKeys(mapper, keys);
    runFrames(session, neutral, 50);

    const auto afterTurn = findServerShip(session, debug, playerId);
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

    const auto afterThrust = findServerShip(session, debug, playerId);
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

    const double speedBeforeMainThrust =
        glm::length(afterThrust.transform.motion.localVelocityMps);

    keys.press(GLFW_KEY_EQUAL);
    const ShipControlState newtonianThrustControl = mapKeys(mapper, keys);
    runFrames(session, newtonianThrustControl, 25);

    const auto afterNewtonianThrust = findServerShip(session, debug, playerId);
    require(
        afterNewtonianThrust.transform.motion.localControlLaw ==
            game::navigation::LocalFlightControlLaw::Newtonian,
        "player no longer starts local flight in Newtonian mode"
    );
    require(
        glm::length(afterNewtonianThrust.transform.motion.localVelocityMps) >
            speedBeforeMainThrust + 1.0,
        "Newtonian + did not increase authoritative VREL"
    );

    // Switch through the real Ctrl+F10 mapper path. The command is a single
    // fixed-step event. Server snapshots are intentionally published at a
    // lower cadence than the authoritative simulation, so never assume that
    // N render frames imply that the corresponding state is already visible
    // in m_lastSnapshot. Send the event once, then wait for an authoritative
    // publication that contains the new persistent motion law.
    keys.clear();
    (void)mapKeys(mapper, keys);
    keys.press(GLFW_KEY_LEFT_CONTROL);
    keys.press(GLFW_KEY_F10);
    ShipControlState assistedModeControl = mapKeys(mapper, keys);
    require(!assistedModeControl.localControlLawCommandValid,
            "Ctrl+F10 switched on press during real flight");

    keys.clear();
    keys.press(GLFW_KEY_LEFT_CONTROL);
    (void)mapKeys(mapper, keys);
    (void)mapKeys(mapper, keys);
    assistedModeControl = mapKeys(mapper, keys);
    require(assistedModeControl.localControlLawCommandValid,
            "debounced Ctrl+F10 release did not emit Assisted mode command during real flight");

    // Reproduce the production failure that used to make Ctrl+F10 feel
    // intermittent: sample the release on a render frame shorter than the
    // fixed client step, then overwrite the continuous sample with neutral
    // input before a fixed step occurs. The discrete mode command must remain
    // pending until a numbered fixed-step input actually carries it.
    ShipControlState postReleaseNeutral;
    const double fixedSeconds = session.fixedStepSeconds();
    runFrameForDuration(
        session,
        assistedModeControl,
        fixedSeconds * 0.25
    );
    runFrameForDuration(
        session,
        postReleaseNeutral,
        fixedSeconds * 0.25
    );
    runFrameForDuration(
        session,
        postReleaseNeutral,
        fixedSeconds * 0.75
    );

    waitFor(
        session,
        [&]()
        {
            return
                findServerShip(session, debug, playerId).transform.motion.localControlLaw ==
                game::navigation::LocalFlightControlLaw::Assisted;
        },
        "authoritative player did not publish Assisted local flight law"
    );

    const auto afterModeSwitch = findServerShip(session, debug, playerId);

    const double targetSpeedBeforeThrottle =
        afterModeSwitch.transform.motion.targetForwardSpeedMps;

    keys.clear();
    (void)mapKeys(mapper, keys);
    keys.press(GLFW_KEY_EQUAL);
    const ShipControlState throttleUpControl = mapKeys(mapper, keys);
    runFrames(session, throttleUpControl, 25);

    const auto afterThrottleUp = findServerShip(session, debug, playerId);
    require(
        afterThrottleUp.transform.motion.targetForwardSpeedMps >
            targetSpeedBeforeThrottle + 1.0,
        "Assisted target-speed control did not increase authoritative setpoint"
    );

    const double raisedTargetSpeed =
        afterThrottleUp.transform.motion.targetForwardSpeedMps;

    keys.clear();
    keys.press(GLFW_KEY_MINUS);
    const ShipControlState throttleDownControl = mapKeys(mapper, keys);
    runFrames(session, throttleDownControl, 25);

    const auto afterThrottleDown = findServerShip(session, debug, playerId);
    require(
        afterThrottleDown.transform.motion.targetForwardSpeedMps <
            raisedTargetSpeed - 1.0,
        "Assisted target-speed control did not decrease authoritative setpoint"
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

UIText* addHudText(UIContainer& root, const char* id)
{
    auto text = std::make_unique<UIText>();
    text->id = id;
    UIText* raw = text.get();
    root.addChild(std::move(text));
    return raw;
}

void testHudPresentationBindings(
    game::host::LocalGameSession& session,
    EntityId playerId
)
{
    const auto& ship = findClientShip(session.client(), playerId);
    const auto telemetry =
        game::presentation::buildPlayerHudTelemetry(ship);

    require(finiteVec(telemetry.globalMeters), "HUD global coordinates became non-finite");
    require(std::isfinite(telemetry.speedMps), "HUD speed became non-finite");
    require(!telemetry.cellLabel.empty(), "HUD cell label became empty");
    require(!telemetry.xLabel.empty(), "HUD X label became empty");
    require(!telemetry.yLabel.empty(), "HUD Y label became empty");
    require(!telemetry.zLabel.empty(), "HUD Z label became empty");
    require(!telemetry.speedLabel.empty(), "HUD speed label became empty");

    const auto flightInstrument =
        game::presentation::buildFlightVectorIndicatorPresentation(ship);

    require(flightInstrument.visible, "flight-vector cockpit instrument became hidden");
    require(std::isfinite(flightInstrument.speedMps), "flight-vector speed became non-finite");
    require(
        flightInstrument.speedFraction01 >= 0.0f &&
        flightInstrument.speedFraction01 <= 1.0f,
        "flight-vector speed fraction escaped normalized range"
    );
    require(!flightInstrument.speedText.empty(), "flight-vector speed text became empty");
    require(!flightInstrument.modeText.empty(), "flight-vector mode text became empty");
    require(!flightInstrument.fontPath.empty(), "flight-vector instrument lost its font profile");

    for (int c = 0; c < 3; ++c)
    {
        const glm::vec3 axis = flightInstrument.shipModelToIndicatorBasis[c];
        require(finiteVec(axis), "flight-vector hull basis became non-finite");
    }

    game::presentation::FlightInstrumentTextProfile localizedProfile;
    localizedProfile.displayUnitsPerMps = 3.6;
    localizedProfile.speedDecimals = 0;
    localizedProfile.speedUnitLabel = "km/h";
    localizedProfile.newtonianModeLabel = "INERCIAL";
    localizedProfile.assistedModeLabel = "ASISTIDO";

    const auto localizedInstrument =
        game::presentation::buildFlightVectorIndicatorPresentation(
            ship,
            localizedProfile
        );

    require(
        localizedInstrument.speedText.find("km/h") != std::string::npos,
        "flight-vector renderer contract hard-coded m/s instead of formatted text"
    );
    require(
        localizedInstrument.modeText == "INERCIAL" ||
        localizedInstrument.modeText == "ASISTIDO",
        "flight-vector mode label ignored the localization profile"
    );

    UIContainer root;
    UIText* cell = addHudText(root, "main_coord_cell");
    UIText* x = addHudText(root, "main_coord_x");
    UIText* y = addHudText(root, "main_coord_y");
    UIText* z = addHudText(root, "main_coord_z");
    UIText* speed = addHudText(root, "main_coord_v");

    require(
        game::presentation::applyPlayerHudTelemetry(root, telemetry),
        "HUD presenter no longer resolves production UIText bindings"
    );

    require(cell->label == telemetry.cellLabel, "HUD CELL binding displays stale/wrong data");
    require(x->label == telemetry.xLabel, "HUD X binding displays stale/wrong data");
    require(y->label == telemetry.yLabel, "HUD Y binding displays stale/wrong data");
    require(z->label == telemetry.zLabel, "HUD Z binding displays stale/wrong data");
    require(speed->label == telemetry.speedLabel, "HUD speed binding displays stale/wrong data");

    UIContainer brokenRoot;
    addHudText(brokenRoot, "main_coord_cell");
    addHudText(brokenRoot, "main_coord_x");
    addHudText(brokenRoot, "main_coord_y");
    addHudText(brokenRoot, "main_coord_z");
    require(
        !game::presentation::applyPlayerHudTelemetry(brokenRoot, telemetry),
        "HUD presenter stopped reporting a missing production binding"
    );

    pass("HUD TELEMETRY + FLIGHT-VECTOR INSTRUMENT PRESENTATION");
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
        [&]()
        {
            return
                client.galaxyMapRequestStatus() ==
                    game::client::ClientRequestStatus::Ready &&
                client.galaxyMapSnapshot() != nullptr;
        },
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

    for (const auto& candidate : galaxy->systems)
    {
        require(!candidate.name.empty(), "Galaxy map contains a game system with no display name");

        const std::string skyLabel =
            game::presentation::buildGameSystemSkyLabel(
                candidate.name,
                {},
                std::to_string(candidate.id),
                glm::length(candidate.positionLy)
            );

        require(
            skyLabel.rfind(candidate.name, 0) == 0,
            "game-system sky label no longer starts with the authored game-system name"
        );
        require(
            skyLabel.find(" ly") != std::string::npos,
            "game-system sky label lost its distance suffix"
        );
    }

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

    game::presentation::SystemMapPanelPresentationInput panelInput;
    panelInput.universeTimeSeconds = client.universeTimeSeconds();
    panelInput.universeDate = client.sessionSnapshot().universeDate;
    panelInput.universeTimeScale = client.sessionSnapshot().universeTimeScale;
    panelInput.mode = game::system_map::MapMode::System;
    panelInput.galaxy = galaxy;
    panelInput.system = system;
    panelInput.navigation = &client.playerNavigation();
    panelInput.currentSystemName = system->systemName;
    panelInput.selectedSystemId = currentSystemId;
    panelInput.selectedHubId = hubIt->stableId;
    panelInput.canOpenDetail = true;

    auto panel =
        game::presentation::buildSystemMapPanelPayload(panelInput);

    require(panel.at("mode") == "System", "map panel lost System mode");
    require(panel.at("currentSystemId") == currentSystemId, "map panel displays wrong current system");
    require(panel.at("currentSystemName") == system->systemName, "map panel displays wrong current system name");
    require(panel.at("selectedSystemId") == currentSystemId, "map panel displays wrong selected system");
    require(panel.at("selectedHubId") == hubIt->stableId, "map panel displays wrong selected hub");
    require(panel.at("canOpenDetail") == true, "map panel lost Details availability state");
    require(
        panel.at("systemsCount").get<std::size_t>() == galaxy->systems.size(),
        "map panel systems count differs from Galaxy snapshot"
    );
    require(
        panel.at("systems").size() == galaxy->systems.size(),
        "map panel systems list differs from Galaxy snapshot"
    );

    const auto selectedItem = std::find_if(
        panel.at("systems").begin(),
        panel.at("systems").end(),
        [currentSystemId](const nlohmann::json& item)
        {
            return item.at("id").get<int>() == currentSystemId;
        }
    );
    require(selectedItem != panel.at("systems").end(), "map panel omitted current system row");
    require(selectedItem->at("current") == true, "map panel current-system marker disappeared");
    require(selectedItem->at("selected") == true, "map panel selected-system marker disappeared");

    panelInput.mode = game::system_map::MapMode::Detail;
    panelInput.canOpenDetail = false;
    panelInput.canOpenHub = true;
    panel = game::presentation::buildSystemMapPanelPayload(panelInput);
    require(panel.at("mode") == "Detail", "map panel lost Details mode");
    require(panel.at("canOpenHub") == true, "map panel lost Hub availability state");

    panelInput.mode = game::system_map::MapMode::Hub;
    panelInput.canOpenHub = false;
    panel = game::presentation::buildSystemMapPanelPayload(panelInput);
    require(panel.at("mode") == "Hub", "map panel lost Hub mode");

    panelInput.mode = game::system_map::MapMode::Galaxy;
    panel = game::presentation::buildSystemMapPanelPayload(panelInput);
    require(panel.at("mode") == "Galaxy", "map panel lost Galaxy mode");

    pass("MAP DATA + PANEL PRESENTATION");
}

void testGalaxyNavigationFlightPresentation(
    game::host::LocalGameSession& session,
    game::debug::IDebugSessionControl& debug,
    EntityId playerId
)
{
    GameClient& client = session.client();

    client.requestGalaxyMapSnapshot(true);
    waitFor(
        session,
        [&]()
        {
            return
                client.galaxyMapRequestStatus() ==
                    game::client::ClientRequestStatus::Ready &&
                client.galaxyMapSnapshot() != nullptr;
        },
        "Galaxy map did not become available for navigation-flight acceptance"
    );

    // forceRefresh can replace the cached GalaxyMapSnapshot storage. Never
    // retain pointers into its systems vector until the requested refresh has
    // completed; otherwise the acceptance test itself introduces dangling
    // pointers and becomes allocator/timing dependent.
    const auto* galaxy = client.galaxyMapSnapshot();
    require(galaxy && galaxy->systems.size() > 1, "Galaxy navigation test has no destination systems");

    const auto beforeNavigation = client.playerNavigation();
    const auto beforeMarker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(
            *galaxy,
            beforeNavigation
        );

    require(beforeMarker.insideKnownSystem, "player marker is not anchored to the current known system before flight");

    const glm::dvec3 forward =
        glm::normalize(glm::dvec3(beforeNavigation.forward));

    const world::celestial::GalaxyMapSystem* target = nullptr;
    glm::dvec3 targetDirection {0.0};
    double bestAlignment = -2.0;

    for (const auto& candidate : galaxy->systems)
    {
        if (candidate.id == beforeNavigation.currentSystemId)
            continue;

        const glm::dvec3 delta =
            candidate.positionLy - beforeMarker.positionLy;
        const double distance = glm::length(delta);
        if (distance <= 0.000001)
            continue;

        const glm::dvec3 direction = delta / distance;
        const double alignment = glm::dot(forward, direction);

        if (alignment > bestAlignment)
        {
            bestAlignment = alignment;
            target = &candidate;
            targetDirection = direction;
        }
    }

    require(target != nullptr, "could not select a destination star for navigation-flight acceptance");
    require(bestAlignment > 0.0, "no game-system star lies in the player's current forward hemisphere");

    const ShipSnapshot beforeShip =
        findServerShip(session, debug, playerId);
    require(beforeShip.referenceFrame.valid, "navigation-flight test lost the player reference frame before thrust");

    PlayerInputMapper mapper;
    SyntheticKeyState keys;
    keys.press(GLFW_KEY_KP_8);
    const ShipControlState forwardControl = mapKeys(mapper, keys);
    runFrames(session, forwardControl, 150);

    keys.clear();
    const ShipControlState neutral = mapKeys(mapper, keys);
    runFrames(session, neutral, 15);

    const auto afterNavigation = client.playerNavigation();
    const auto afterMarker =
        game::presentation::resolveGalaxyPlayerMarkerPosition(
            *galaxy,
            afterNavigation
        );

    const ShipSnapshot afterShip =
        findServerShip(session, debug, playerId);
    require(afterShip.referenceFrame.valid, "navigation-flight test lost the player reference frame after thrust");

    const glm::dvec3 relativeFlightLocalMeters =
        afterShip.referenceFrame.localPositionMeters -
        beforeShip.referenceFrame.localPositionMeters;

    const glm::dvec3 relativeFlightWorldMeters =
        beforeShip.referenceFrame.localToWorldVector(
            relativeFlightLocalMeters
        );

    require(
        glm::length(relativeFlightWorldMeters) > 0.25,
        "real player thrust did not change hub-relative flight position"
    );
    require(
        glm::dot(relativeFlightWorldMeters, targetDirection) > 0.0,
        "real player thrust moved away from the game-system star nearest the forward sightline"
    );

    const glm::dvec3 markerDeltaLy =
        afterMarker.positionLy - beforeMarker.positionLy;

    require(
        glm::length(markerDeltaLy) > 0.0,
        "Galaxy map player marker did not follow real player movement"
    );
    const double afterDistanceLy =
        glm::length(target->positionLy - afterMarker.positionLy);

    game::presentation::SystemMapPanelPresentationInput panelInput;
    panelInput.mode = game::system_map::MapMode::Galaxy;
    panelInput.galaxy = galaxy;
    panelInput.navigation = &afterNavigation;
    panelInput.selectedSystemId = target->id;
    panelInput.currentSystemName = "navigation-flight";

    const auto panel =
        game::presentation::buildSystemMapPanelPayload(panelInput);

    const auto targetRow =
        std::find_if(
            panel.at("systems").begin(),
            panel.at("systems").end(),
            [&](const nlohmann::json& item)
            {
                return item.at("id").get<int>() == target->id;
            }
        );

    require(targetRow != panel.at("systems").end(), "selected navigation star disappeared from map panel");
    require(targetRow->at("selected") == true, "selected navigation star lost its selected marker");

    const double panelDistanceLy =
        targetRow->at("distanceFromPlayerLy").get<double>();

    require(
        std::abs(panelDistanceLy - afterDistanceLy) < 1.0e-9,
        "map panel distance is no longer measured from the actual player marker"
    );

    requireOrientationBasis(afterShip.transform, "server player after galaxy navigation flight");

    pass("GALAXY NAVIGATION + REAL PLAYER FLIGHT PRESENTATION");
}

} // namespace

int runClientAcceptanceSelfTest()
{
    try
    {
        std::cerr << "[SELFTEST] client-acceptance stage=input-mapping\n";
        testInputMapping();

        std::cerr << "[SELFTEST] client-acceptance stage=game-ui-toggle\n";
        testGameUiToggleContract();

        std::cerr << "[SELFTEST] client-acceptance stage=current-function-hotkeys\n";
        testCoordinateDisplayHotkeyContract();
        testConstellationOverlayContract();

        std::cerr << "[SELFTEST] client-acceptance stage=map-ui-command-contract\n";
        testSystemMapUiCommandContract();
        testSystemMapUiCommandDispatchContract();

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

        std::cerr << "[SELFTEST] client-acceptance stage=hud-bindings\n";
        testHudPresentationBindings(session, playerId);

        std::cerr << "[SELFTEST] client-acceptance stage=remote-motion\n";
        testRemoteMotion(session);

        std::cerr << "[SELFTEST] client-acceptance stage=maps\n";
        testMapDataPipeline(session);

        std::cerr << "[SELFTEST] client-acceptance stage=galaxy-navigation-flight\n";
        testGalaxyNavigationFlightPresentation(session, *debug, playerId);

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
