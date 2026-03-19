#include "ReceiverModule.h"

namespace game {

void ReceiverModule::init(const ReceiverDesc& desc)
{
    m_desc = desc;
    setEnabled(true);

}

void ReceiverModule::update(double dt)
{
    if (!isOperational())
        return;
}

double ReceiverModule::getRequestedPower() const
{

    return m_desc.powerConsumption;
}

void ReceiverModule::setAvailablePower(double power)
{
    m_availablePower = power;
    if(m_availablePower > 0) setEnabled(true);
}

double ReceiverModule::getHeatGeneration() const
{
    if (!isOperational())
        return 0.0;

    return m_desc.heatGeneration;
}

}