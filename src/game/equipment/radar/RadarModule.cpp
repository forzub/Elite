#include "RadarModule.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>



namespace game {

void RadarModule::init(const RadarDesc& desc)
{
    m_desc = desc;
    setEnabled(true);

    std::cout << "[INFO] radar installed\n";
    std::cout << "  |- maxRange: " << m_desc.maxRange << "\n";
    std::cout << "  |- power   : " << m_desc.powerConsumption << "\n";

}



// ================================

void RadarModule::update(
    double dt,
    const std::vector<world::RadarContactInput>& worldObjects,
    const glm::vec3& myWorldPos,
    const glm::mat4& myOrientation
)
{
    m_detected.clear();

    if (!isOperational())
        return;

    if (m_availablePower < m_desc.powerConsumption)
        return;

    m_scanTimer += dt;

    // if (m_desc.scanInterval > 0.0 &&
    //     m_scanTimer < m_desc.scanInterval)
    //     return;

    m_scanTimer = 0.0;

    for (const auto& obj : worldObjects)
    {
        glm::vec3 delta = obj.worldPosition - myWorldPos;

        double distance = glm::length(delta);

        if (distance > m_desc.maxRange)
            continue;

        double effectiveRange =
            m_desc.maxRange * std::sqrt(obj.radarCrossSection);

        if (distance > effectiveRange)
            continue;

        // перевод в локальные координаты
        glm::vec3 local =
            glm::vec3(glm::inverse(myOrientation) * glm::vec4(delta, 1.0f));

        m_detected.push_back({
            obj.id,
            distance,
            local
        });
    }
}





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






// ================================

double RadarModule::getHeatGeneration() const
{
    if (!isOperational())
        return 0.0;

    return m_desc.heatGeneration;
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
const std::vector<RadarContact>& RadarModule::getContacts() const
{
    return m_detected;
}

}