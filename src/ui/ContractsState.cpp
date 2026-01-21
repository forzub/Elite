#include "ContractsState.h"
#include "core/StateStack.h"

#include <iostream>

ContractsState::ContractsState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
{
}

void ContractsState::handleInput()
{
    std::cout << "\n=== Contracts State MENU ===\n";
    std::cout << "1 - Back to Main Menu\n";
    std::cout << "Choose: ";

    int choice = 0;
    std::cin >> choice;

    if (choice == 1)
    {
        m_shouldExit = true;
    }
}

void ContractsState::update(float)
{
    if (m_shouldExit)
    {
        m_stack.pop();
    }
}

void ContractsState::render()
{
    // Пока ничего
}
