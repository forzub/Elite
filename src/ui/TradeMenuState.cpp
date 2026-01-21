#include "TradeMenuState.h"
#include "core/StateStack.h"

#include <iostream>

TradeMenuState::TradeMenuState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
{
}

void TradeMenuState::handleInput()
{
    std::cout << "\n=== TRADE MENU ===\n";
    std::cout << "1 - Back to Main Menu\n";
    std::cout << "Choose: ";

    int choice = 0;
    std::cin >> choice;

    if (choice == 1)
    {
        m_shouldExit = true;
    }
}

void TradeMenuState::update(float)
{
    if (m_shouldExit)
    {
        m_stack.pop();
    }
}

void TradeMenuState::render()
{
    // Пока ничего
}
