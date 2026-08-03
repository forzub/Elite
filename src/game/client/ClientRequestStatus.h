#pragma once

namespace game::client
{
enum class ClientRequestStatus
{
    Idle,
    Pending,
    Ready,
    TimedOut,
    Failed,
    Cancelled
};
}
