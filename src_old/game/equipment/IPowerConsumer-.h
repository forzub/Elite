#pragma once

namespace game::equipment
{

class IPowerConsumer
{
public:
    virtual ~IPowerConsumer() = default;

    virtual double requestedPower() const = 0;
    virtual void setAvailablePower(double power) = 0;

    virtual bool isPowered() const = 0;
};

}