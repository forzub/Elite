#pragma once

namespace game::ship::core
{

class ThermalSystem
{
public:
    ThermalSystem(double thermalMass, double maxSafeTemp);
    ThermalSystem() = default;

    void addHeat(double amount);
    void setCoolingRate(double rate);   // базовое охлаждение

    void update(double dt);

    double temperature() const;
    double maxSafeTemperature() const;

    double integrity() const; // структурная целостность

private:
    double m_temperature = 0.0;
    double m_thermalMass = 1.0;         // инерция нагрева
    double m_coolingRate = 0.0;

    double m_maxSafeTemperature = 100.0;
    double m_structuralIntegrity = 1.0;
};

}