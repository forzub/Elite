#pragma once

#include "DamageEvent.h"
#include "HitVolume.h"

namespace game::damage
{

struct DamageResult
{
    const DamageEvent* event = nullptr;
    const HitVolume* volume = nullptr;
};

}