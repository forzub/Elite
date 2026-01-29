#include "StateStack.h"
#include "GameState.h"

#include "core/StateContext.h"
#include "core/Log.h"


#include <iostream>

StateStack::StateStack(StateContext& context)
    : m_context(context)
{
}

StateContext& StateStack::context()
{
    return m_context;
}

// --------------------------------------------------

void StateStack::push(std::unique_ptr<GameState> state)
{
    LOG("[StateStack] push");
    m_pending.push_back({ ActionType::Push, std::move(state) });
}

// --------------------------------------------------

void StateStack::pop()
{
    LOG("[StateStack] popState");
    m_pending.push_back({ ActionType::Pop, nullptr });
}

// --------------------------------------------------

void StateStack::clear()
{
    
    m_pending.push_back({ ActionType::Clear, nullptr });
}

// --------------------------------------------------

void StateStack::applyPendingChanges()
{

    for (auto& action : m_pending)
    {
        switch (action.type)
        {
        case ActionType::Push:
            
            m_stack.push_back(std::move(action.state));
            m_stack.back()->onEnter();
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

// --------------------------------------------------

GameState* StateStack::current() const
{
    return m_stack.empty() ? nullptr : m_stack.back().get();
}

// --------------------------------------------------

bool StateStack::empty() const
{
    return m_stack.empty();
}

// --------------------------------------------------

void StateStack::renderAll()
{
    for (auto& state : m_stack)
    state->render();
}

// --------------------------------------------------

void StateStack::submitRenderData()
{
    if (!m_stack.empty())
    m_stack.back()->renderUI();
}

// --------------------------------------------------

GameState* StateStack::current()
{
    return m_stack.empty() ? nullptr : m_stack.back().get();
}


// --------------------------------------------------

GameState* StateStack::previous()
{
    if (m_stack.size() < 2)
        return nullptr;

    return m_stack[m_stack.size() - 2].get();
}



