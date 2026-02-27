#pragma once

namespace game::equipment {

class IHeatSource
{
public:
    virtual ~IHeatSource() = default;

    virtual double getHeatGeneration() const = 0;
};

}