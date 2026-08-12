#include <cmath>
#include <cstdlib>
#include <iostream>

#include "src/game/debug/DebugSessionMessage.h"
#include "src/game/debug/LocalDebugSessionControl.h"

namespace
{
void require(bool condition, const char* message)
{
    if (condition)
        return;

    std::cerr << "[FAIL] debug-session boundary: " << message << '\n';
    std::exit(2);
}
}

int main()
{
    game::debug::LocalDebugSessionControl channel;

    require(channel.snapshotRevision() == 0, "snapshot revision must start at zero");
    require(channel.stateRevision() == 0, "state revision must start at zero");

    SimulationSnapshot authoritative;
    authoritative.metadata.serverTick = 42;
    authoritative.metadata.serverTimeSeconds = 12.5;

    channel.publishSnapshot(authoritative);
    require(channel.snapshotRevision() == 1, "published debug snapshot did not advance revision");

    authoritative.metadata.serverTick = 99;
    const SimulationSnapshot copied = channel.snapshot();
    require(copied.metadata.serverTick == 42, "debug snapshot aliases authoritative source memory");
    require(
        std::abs(copied.metadata.serverTimeSeconds - 12.5) < 1.0e-12,
        "debug snapshot copy lost metadata"
    );

    game::debug::DebugSessionState state;
    state.fastUniverseTime = true;
    state.universeTimeSimulation = true;
    state.universeTimeScale = 250.0;
    state.configuredUniverseTimeScale = 1000.0;
    channel.publishState(state);

    require(channel.stateRevision() == 1, "published debug state did not advance revision");
    require(channel.fastUniverseTime(), "fast-universe state did not cross debug seam");
    require(channel.universeTimeSimulation(), "universe simulation state did not cross debug seam");
    require(std::abs(channel.universeTimeScale() - 250.0) < 1.0e-12, "active debug scale changed");
    require(
        std::abs(channel.configuredUniverseTimeScale() - 1000.0) < 1.0e-12,
        "configured debug scale changed"
    );

    channel.destroyShipModule(EntityId{7}, "reactor");

    game::debug::DebugCommand command;
    require(channel.receiveCommand(command), "queued structure command did not reach server endpoint");
    require(
        command.type == game::debug::DebugCommandType::DestroyShipModule,
        "structure command type changed across debug seam"
    );
    require(command.shipId.value == 7, "structure command entity id changed");
    require(command.itemId == "reactor", "structure command module id changed");

    channel.setUniverseTimeSimulation(true, 500.0);
    require(channel.receiveCommand(command), "queued universe-time command did not reach server endpoint");
    require(
        command.type == game::debug::DebugCommandType::SetUniverseTimeSimulation,
        "universe-time command type changed across debug seam"
    );
    require(command.enabled, "universe-time enabled flag changed");
    require(std::abs(command.timeScale - 500.0) < 1.0e-12, "universe-time scale changed");

    channel.refreshSnapshot();
    require(channel.receiveCommand(command), "snapshot refresh request did not reach server endpoint");
    require(
        command.type == game::debug::DebugCommandType::RefreshSnapshot,
        "snapshot refresh command type changed"
    );

    channel.refreshStructureSnapshot();
    require(
        channel.receiveCommand(command),
        "structure snapshot refresh request did not reach server endpoint"
    );
    require(
        command.type == game::debug::DebugCommandType::RefreshStructureSnapshot,
        "structure snapshot refresh command type changed"
    );

    require(!channel.receiveCommand(command), "debug command queue did not drain completely");

    std::cout << "[PASS] copied asynchronous debug/control channel contract\n";
    return 0;
}
