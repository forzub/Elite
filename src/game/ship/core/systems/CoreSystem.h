#pragma once


#include "src/game/equipment/capabilities/IPowerConsumer.h"
#include "src/game/equipment/capabilities/IHeatSource.h"



namespace game::ship::core
{


class CoreSystem :   public game::equipment::IPowerConsumer,
                            public game::equipment::IHeatSource
{
public:

    enum class Mode
    {
        Normal,
        Emergency,
        Damaged
    };

public:
    explicit CoreSystem(double nominalPowerMW, std::string label);

    void update(double dt);

    void setMode(Mode mode);

    // ---- IPowerConsumer ----
    double getRequestedPower() const override;
    void setAvailablePower(double powerMW) override;
    double  getAvailablePower() const override;

    // ---- IHeatSource ----
    double getHeatGeneration() const override;
    std::string getLabel() const override { return m_label; }

private:
    double m_nominalPowerMW;     // Номинальная мощность
    double m_allocatedPowerMW;   // Выделенная мощность
    double m_heatGeneratedMW;    // Генерируемое тепло

    std::string m_label;

    Mode m_mode = Mode::Normal;
};

}