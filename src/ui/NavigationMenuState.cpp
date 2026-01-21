#include "NavigationMenuState.h"
#include "core/StateStack.h"

#include <iostream>

NavigationMenuState::NavigationMenuState(StateStack& states)
    : GameState(states)
    , m_shouldExit(false)
{
}

void NavigationMenuState::handleInput()
{
    std::cout << "\n=== NAVIGATION MENU ===\n";
    std::cout << "1 - Back to Main Menu\n";
    std::cout << "Choose: ";

    int choice = 0;
    std::cin >> choice;

    if (choice == 1)
    {
        m_shouldExit = true;
    }
}

void NavigationMenuState::update(float)
{
    if (m_shouldExit)
    {
        m_stack.pop();
    }
}

void NavigationMenuState::render()
{
    // Пока ничего
}
