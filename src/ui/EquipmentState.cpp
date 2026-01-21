#include "EquipmentState.h"
#include "core/StateStack.h"

#include <iostream>

EquipmentState::EquipmentState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
{
}

void EquipmentState::handleInput()
{
    std::cout << "\n=== Equipment State MENU ===\n";
    std::cout << "1 - Back to Main Menu\n";
    std::cout << "Choose: ";

    int choice = 0;
    std::cin >> choice;

    if (choice == 1)
    {
        m_shouldExit = true;
    }
}

void EquipmentState::update(float)
{
    if (m_shouldExit)
    {
        m_stack.pop();
    }
}

void EquipmentState::render()
{
    // Пока ничего
}
