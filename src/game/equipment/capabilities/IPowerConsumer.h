#pragma once

namespace game::equipment {

class IPowerConsumer
{
public:
    virtual ~IPowerConsumer() = default;

    virtual double getRequestedPower() const = 0;
    virtual void   setAvailablePower(double power) = 0;
};

}