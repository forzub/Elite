#pragma once

class GameState
{
public:
    virtual ~GameState() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    virtual void handleInput() = 0;
    virtual void update(float dt) = 0;
    virtual void render() = 0;
};
