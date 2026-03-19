#include "SignalTransmitterModule.h"
#include <iostream>

double SignalTransmitterModule::getRequestedPower() const
{
   
    return m_desc.powerConsumption;
}

void SignalTransmitterModule::setAvailablePower(double power)
{
    m_availablePower = power;
    if (m_availablePower > 0.0){
        setEnabled(true);
    }

    
}

double SignalTransmitterModule::getHeatGeneration() const
{
    if (!isOperational())
        return 0.0;

    return m_desc.heatGeneration;
}