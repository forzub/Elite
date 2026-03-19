#pragma once
#include <vector>

#include "src/game/ship/core/ShipCore.h"

using game::ship::core::ShipCore;

#include "src/scene/EntityID.h"
#include "src/world/types/ObjectType.h"
#include "src/game/items/cryptocard/CryptoCard.h"
#include "src/world/ITransmitterSource.h"
#include "src/world/types/RadarContactInput.h"
#include "src/game/network/ClientShipCommand.h"

#include "src/game/ship/damage/ShipDamageHandler.h"

class Ship : public ITransmitterSource
{
public:
    Ship() = default; 

    Ship(const Ship&) = delete;
    Ship& operator=(const Ship&) = delete;


    void init(
        ShipRole                role,
        const ShipDescriptor&   descriptor,
        glm::vec3               position,
        const ShipInitData&     initData
    );

    void update(
        float dt,
        const WorldParams& world,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    );

    void setControlState(const ShipControlState& newControl);

 

    bool addItem(Item* item);
    bool removeItem(Item* item);

    // === безопасный доступ ===
    const ShipCore& core() const { return m_core; }
    ShipCore& core() { return m_core; }

    EntityId id() const { return m_id; }
    void setId(EntityId id) { m_id = id; }

    void applyControl();
    void updatePhysics(float dt, const WorldParams& world);
    void updateSignals(
        float dt,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    );
    void updatePerception(float dt, const std::vector<world::RadarContactInput>& radarInputs);
    void setTypeId(ObjectType id) { m_typeId = id; }
    ObjectType typeId() const { return m_typeId; }

    std::optional<WorldSignal> emitSignal() const override;

    void applyCommand(const ClientShipCommand& cmd);

private:
    ShipCore            m_core;
    ShipControlState    m_control;
    EntityId            m_id;
    ObjectType          m_typeId;

    void updateControlIntent();
};


