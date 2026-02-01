#pragma once

struct IEquipmentModule
{
    virtual ~IEquipmentModule() = default;
    virtual void initFromDesc(const void* desc) = 0;
};
