#pragma once

namespace game
{

class IHudDataProvider
{
public:
    virtual ~IHudDataProvider() = default;

    virtual double getHudValue() const = 0;
};

}
