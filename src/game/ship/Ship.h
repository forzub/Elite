#pragma once
#include <vector>

#include "src/game/ship/core/ShipCore.h"
#include "src/scene/EntityID.h"
#include "ShipTypeId.h"




class Ship
{
public:

    void init(
        ShipRole role,
        const ShipDescriptor& descriptor,
        glm::vec3 position,
        const ShipInitData& initData
    );

    void update(
        float dt,
        const WorldParams& world,
        const std::vector<WorldSignal>& worldSignals,
        const std::vector<Planet>& planets,
        const std::vector<InterferenceSource>& interferenceSources
    );

    void setControlState(const ShipControlState& newControl);

    bool installCryptoCard(CryptoCard* card);
    bool removeCryptoCard(CryptoCard* card);

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
    void updatePerception(float dt);
    void setTypeId(ShipTypeId id) { m_typeId = id; }
    ShipTypeId typeId() const { return m_typeId; }

private:

    ShipCore            m_core;
    ShipControlState    m_control;
    EntityId            m_id;
    ShipTypeId          m_typeId;

    void updateControlIntent();
};


