#pragma once

namespace game::diagnostics
{
// Boots the real local host/client stack without creating a window and runs
// player-like regression scenarios through production control/network paths.
int runClientAcceptanceSelfTest();
}
