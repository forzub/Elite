#include "ShipyardState.h"
#include "core/StateStack.h"

#include <iostream>

ShipyardState::ShipyardState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
{
}

void ShipyardState::handleInput()
{
    std::cout << "\n=== SHIPYARD MENU ===\n";
    std::cout << "1 - Back to Main Menu\n";
    std::cout << "Choose: ";

    int choice = 0;
    std::cin >> choice;

    if (choice == 1)
    {
        m_shouldExit = true;
    }
}

void ShipyardState::update(float)
{
    if (m_shouldExit)
    {
        m_stack.pop();
    }
}

void ShipyardState::render()
{
    // Пока ничего
}
