#include "RadarModule.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game {

void RadarModule::init(const RadarDesc& desc)
{
    m_desc = desc;
    setEnabled(true);
    m_priority = game::equipment::PowerPriority::Comfort;

    std::cout << "[INFO] radar installed\n";
    std::cout << "  |- maxRange: " << m_desc.maxRange << "\n";
    std::cout << "  |- power   : " << m_desc.powerConsumption << "\n";
    

}



// ================================






// ================================

double RadarModule::getRequestedPower() const
{
    if (!isOperational())
        return 0.0;

    return m_desc.powerConsumption;
}





// ================================

void RadarModule::setAvailablePower(double power)
{
    m_availablePower = power;
}


double RadarModule::getAvailablePower() const 
{
    return m_availablePower;
}



// ================================

double RadarModule::getHeatGeneration() const
{
    if (!isOperational())
        return 0.0;

    return m_availablePower * m_desc.heatGeneration;
}





// ================================

bool RadarModule::hasLock() const
{
    return m_hasLock;
}



// ================================

bool RadarModule::detectTarget(
    double distance,
    double targetRCS
) const
{
    if (!isOperational())
        return false;

    if (distance > m_desc.maxRange)
        return false;

    double effectiveRange =
        m_desc.maxRange * std::sqrt(targetRCS);

    if (effectiveRange > m_desc.maxRange)
        effectiveRange = m_desc.maxRange;

    return distance <= effectiveRange;
}




// ================================

}