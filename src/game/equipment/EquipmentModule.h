// #pragma once

// #include "Integrity.h"

// namespace game
// {

// class EquipmentModule
// {
// public:
//     virtual ~EquipmentModule() = default;

//     virtual void update(double dt)
//     {
//         // базовая деградация при перегреве
//         if (m_integrity.thermal > 0.8)
//             m_integrity.degradeFunction(dt * 0.05);
//     }

//     const Integrity& getIntegrity() const
//     {
//         return m_integrity;
//     }

//     bool isOperational() const
//     {
//         return m_integrity.isOperational();
//     }

// protected:
//     Integrity m_integrity;
// };

// }

#pragma once

#include "Integrity.h"

namespace game
{

class EquipmentModule
{
public:
    virtual ~EquipmentModule() = default;

    virtual void update(double dt)
    {
        // пока пусто
    }

    void setEnabled(bool value) { m_enabled = value; }
    bool isEnabled() const { return m_enabled; }

    bool isOperational() const
    {
        return m_enabled && m_integrity.isOperational();
    }

    Integrity& integrity() { return m_integrity; }
    const Integrity& integrity() const { return m_integrity; }

protected:
    bool        m_enabled = false;
    Integrity   m_integrity;


};

}