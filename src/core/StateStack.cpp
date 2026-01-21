#include "StateStack.h"
#include "GameState.h"

StateStack::StateStack()
{
}

void StateStack::push(std::unique_ptr<GameState> state)
{
    m_pending.push_back({ ActionType::Push, std::move(state) });
}

void StateStack::pop()
{
    m_pending.push_back({ ActionType::Pop, nullptr });
}

void StateStack::clear()
{
    m_pending.push_back({ ActionType::Clear, nullptr });
}

void StateStack::applyPendingChanges()
{
    for (auto& action : m_pending)
    {
        switch (action.type)
        {
        case ActionType::Push:
            action.state->onEnter();
            m_stack.push_back(std::move(action.state));
            break;

        case ActionType::Pop:
            if (!m_stack.empty())
            {
                m_stack.back()->onExit();
                m_stack.pop_back();
            }
            break;

        case ActionType::Clear:
            while (!m_stack.empty())
            {
                m_stack.back()->onExit();
                m_stack.pop_back();
            }
            break;
        }
    }

    m_pending.clear();
}

GameState* StateStack::current()
{
    if (m_stack.empty())
        return nullptr;

    return m_stack.back().get();
}

bool StateStack::empty() const
{
    return m_stack.empty();
}
