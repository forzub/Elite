#pragma once
#include <string>

namespace game::equipment
{

// Добавляем enum (можно прямо здесь, без отдельного файла)
enum class PowerPriority
{
    Critical,
    Combat,
    Mobility,
    Support,
    Comfort
};

class IPowerConsumer
{
public:
    virtual ~IPowerConsumer() = default;

    virtual double getRequestedPower() const = 0;
    virtual void   setAvailablePower(double power) = 0;
    
    // НОВЫЕ МЕТОДЫ - делаем их pure virtual, но с реализацией по умолчанию нельзя
    virtual PowerPriority getPriority() const { return PowerPriority::Support; }
    virtual std::string getLabel() const { return "Unknown"; }
    virtual double getAvailablePower() const { return 0.0; }
    
};

}