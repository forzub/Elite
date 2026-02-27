#pragma once

namespace game::ship::core
{

class ReactorSystem
{
public:
    ReactorSystem(double maxOutputMW);
    ReactorSystem() = default;

    void setThrottle(double value);        // 0..1
    double throttle() const;

    double maxOutput() const;              // конструктивная
    double currentOutput() const;          // с учетом throttle и повреждений

    void applyStructuralDamage(double amount); // 0..1
    double structuralIntegrity() const;

    void update(double dt);

    double heatGeneration() const;         // сколько тепла генерирует сейчас

private:
    double m_maxOutput = 0.0;
    double m_throttle = 0.0;
    double m_structuralIntegrity = 1.0;
    double m_instability = 0.0;
};

}