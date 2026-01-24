#pragma once

#include "core/GameState.h"

#include <vector>
#include <cstddef>

// Параметры, приходящие извне (из SpaceState)
struct ConfirmExitOptions
{
    bool canSave = false;
    bool canLoad = false;
};

// Возможные действия
enum class ExitAction
{
    Continue,
    Save,
    Load,
    ExitToMenu,
    Count
};

class ConfirmExitState : public GameState
{
public:
    ConfirmExitState(StateStack& states, const ConfirmExitOptions& options);
    ~ConfirmExitState();

    bool isModal() const override { return true; }
    void renderUI() override;

    void handleInput() override;
    void update(float dt) override;
    void render() override;

    bool wantsConfirmExit() const override { return false; }
    bool onGlobalEscape() override;

private:
    void activateSelected();
    bool isActionEnabled(ExitAction action) const;

    // Навигация
    void selectNext();
    void selectPrevious();

private:
    ConfirmExitOptions m_options;

    std::vector<ExitAction> m_actions;
    std::size_t m_selectedIndex = 0;

    // edge-detection
    bool m_leftPressed  = false;
    bool m_rightPressed = false;
    bool m_enterPressed = false;
    bool m_escPressed   = false;

};


