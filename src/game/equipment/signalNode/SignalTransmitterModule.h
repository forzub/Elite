#pragma once

#include "src/game/equipment/EquipmentModule.h"
#include "SignalTransmitterDesc.h"
#include "src/world/types/SignalPayload.h"
#include "src/world/WorldSignal.h"
#include "src/game/equipment/capabilities/IHeatSource.h"
#include "src/game/equipment/capabilities/IPowerConsumer.h"

#include <optional>

class SignalTransmitterModule : 
    public game::EquipmentModule,
    public game::equipment::IPowerConsumer,
    public game::equipment::IHeatSource
{
public:

    void init(const SignalTransmitterDesc& desc)
    {
        m_desc          = desc;
        m_txPower       = desc.txPower;
        m_baseRange     = desc.baseRange;
        m_displayClass  = desc.displayClass;

        
    }

    void update(double dt) override
    {
        EquipmentModule::update(dt);

        if (!isEnabled())
            return;

        m_integrity.addHeat(0.02 * dt);
        m_integrity.cool(0.01 * dt);

        if (m_integrity.thermal >= 1.0)
            setEnabled(false);
    }

    // ───────────────
    // Управление сигналом
    // ───────────────

    void enableSignal()   { m_signalEnabled = true;  }
    void disableSignal()  { m_signalEnabled = false; }

    void setPayload(const SignalPayload& payload)
    {
        m_payload = payload;
    }

    // ───────────────
    // Генерация сигнала
    // ───────────────

    std::optional<WorldSignal> emit(
        const glm::vec3& position,
        EntityId owner
    ) const
    {
        if (!m_signalEnabled)
            return std::nullopt;

        if (!isOperational())
            return std::nullopt;

        if (m_payload.empty())
            return std::nullopt;

        WorldSignal sig;

        sig.type         = m_payload.type;
        sig.displayClass = m_displayClass;

        sig.address.actor   = m_payload.actor;
        sig.address.channel = m_payload.channel;

        sig.position = position;
        sig.power    = effectivePower();
        sig.maxRange = effectiveRange();

        sig.enabled  = true;
        sig.label    = m_payload.text();
        sig.owner    = owner;

        return sig;
    }

    double getRequestedPower() const override;
    void   setAvailablePower(double power) override;
    double getHeatGeneration() const override;

    float effectivePower() const
    {
        return m_txPower * m_integrity.functional;
    }
    
    float effectiveRange() const
    {
        return m_baseRange * m_integrity.functional;
    }

    game::equipment::PowerPriority getPriority() const override { 
        return m_priority; 
    }
    std::string getLabel() const override { return m_label; }

private:

    float                               m_txPower   = 0.0f;
    float                               m_baseRange = 0.0f;
    double                              m_availablePower = 0.0;

    SignalDisplayClass                  m_displayClass = SignalDisplayClass::Local;
    bool                                m_signalEnabled = false;
    SignalPayload                       m_payload;
    SignalTransmitterDesc               m_desc;

    std::string                     m_label = "transmitter";
    game::equipment::PowerPriority  m_priority = game::equipment::PowerPriority::Critical;

};