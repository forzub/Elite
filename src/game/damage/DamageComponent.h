#pragma once

#include <vector>
#include <algorithm>
#include "DamageEvent.h"

namespace game::damage
{

class DamageComponent
{
public:
    // применить новое повреждение
    void apply(const DamageEvent& event);

    // обновление временных эффектов
    void update(float dt);

    // получить активные повреждения
    const std::vector<DamageEvent>& active() const;

    // есть ли активные повреждения
    bool hasAny() const;

    // очистить всё (например, при ремонте)
    void clear();

private:
    std::vector<DamageEvent> m_active;
};

}