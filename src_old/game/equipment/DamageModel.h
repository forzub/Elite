#pragma once
#include "Integrity.h"

namespace game
{

class DamageModel
{
public:
    void applyStructuralDamage(double amount);
    void applyFunctionalDamage(double amount);

    void addHeat(double amount);
    void cool(double amount);

    void update(double dt);

    const Integrity& getIntegrity() const;

private:
    Integrity m_integrity;
};

}
