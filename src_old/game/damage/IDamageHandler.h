#pragma once

#include "DamageResult.h"

namespace game::damage
{

class IDamageHandler
{
public:

    virtual void handleDamage(const DamageResult& result) = 0;

    virtual ~IDamageHandler() = default;
};

}