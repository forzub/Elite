#pragma once

#include "src/game/debug/DebugSessionMessage.h"
#include "src/game/simulation/SimulationSnapshot.h"

namespace game::debug
{
/*
    Server-side endpoint of the local debug/control channel.

    Debug pages are application-side tools, but their commands may mutate
    authoritative state. They therefore cross the same ownership boundary as
    gameplay messages instead of reaching into ServerRuntime/GameServer memory.
    The local implementation is thread-safe because the application/debug-tool
    endpoint and ServerRuntime now execute on different OS threads.
*/
class IServerDebugChannel
{
public:
    virtual ~IServerDebugChannel() = default;

    virtual bool receiveCommand(DebugCommand& outCommand) = 0;
    virtual void publishSnapshot(const SimulationSnapshot& snapshot) = 0;
    virtual void publishState(const DebugSessionState& state) = 0;
};
}
