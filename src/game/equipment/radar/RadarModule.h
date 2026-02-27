#pragma once
#include <vector>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include "src/game/equipment/EquipmentModule.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include "src/game/equipment/capabilities/IHeatSource.h"
#include "src/world/types/RadarContactInput.h"

#include "RadarDesc.h"
#include "RadarContact.h"


namespace game {

class RadarModule :
    public EquipmentModule,
    public game::equipment::IPowerConsumer,
    public game::equipment::IHeatSource
{
public:
    void init(const RadarDesc& desc);

    void update(
        double dt,
        const std::vector<world::RadarContactInput>& worldObjects,
        const glm::vec3& myWorldPos,
        const glm::mat4& myOrientation
    );

    double getRequestedPower() const override;
    void   setAvailablePower(double power) override;

    double getHeatGeneration() const override;

    bool hasLock() const;

    bool detectTarget(double distance, double targetRCS) const;
    const RadarDesc& getDesc() const { return m_desc; }
    const std::vector<RadarContact>& getContacts() const;

private:
    RadarDesc                   m_desc;
    double                      m_availablePower = 0.0;
    double                      m_lockProgress = 0.0;
    bool                        m_hasLock = false;
    std::vector<RadarContact>   m_detected;
    double                      m_scanTimer = 0.0;
};

}