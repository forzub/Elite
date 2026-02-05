#pragma once

struct QuantitySlot
{
    int capacity = 0;
    int value    = 0;

    bool canAdd(int amount) const
    {
        return value + amount <= capacity;
    }

    bool canRemove(int amount) const
    {
        return value - amount >= 0;
    }

    bool add(int amount)
    {
        if (!canAdd(amount)) return false;
        value += amount;
        return true;
    }

    bool remove(int amount)
    {
        if (!canRemove(amount)) return false;
        value -= amount;
        return true;
    }

    int free() const
    {
        return capacity - value;
    }
};
