#pragma once

#include "src/game/ship/core/ShipControlState.h"

/*
    Narrow key-state seam used by the runtime mapper and by the client
    acceptance harness. The mapper remains the single owner of keyboard ->
    ShipControlState semantics; tests can emulate a player without depending
    on a real GLFW window or synthesizing OS input events.
*/
class IPlayerInputKeyState
{
public:
    virtual ~IPlayerInputKeyState() = default;
    virtual bool isKeyPressed(int key) const = 0;
};

struct PlayerInputMapper
{
    void update(ShipControlState& control);

    void updateFromKeyState(
        ShipControlState& control,
        const IPlayerInputKeyState& keys
    );

private:
    enum class CtrlF10State
    {
        Idle,
        Pressed,
        ReleaseDebounce
    };

    static constexpr int kCtrlF10ReleaseDebounceSamples = 3;

    CtrlF10State m_ctrlF10State = CtrlF10State::Idle;
    int m_ctrlF10ReleaseSamples = 0;

    game::navigation::LocalFlightControlLaw m_requestedLocalControlLaw =
        game::navigation::LocalFlightControlLaw::Newtonian;
};
