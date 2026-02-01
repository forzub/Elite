#pragma once

#include <unordered_map>
#include <vector>

#include "world/WorldSignal.h"
#include "src/world/types/SignalReceptionResult.h"

#include "render/HUD/WorldLabel.h"




class ShipSignalPresentation
{
public:
    // визуальное представление сигналов
    // std::unordered_map<const WorldSignal*, WorldLabel> labels;
    const std::unordered_map<const WorldSignal*, WorldLabel>& labels() const;
    std::unordered_map<const WorldSignal*, WorldLabel>& labels();

    void update(
        float dt,
        const std::vector<SignalReceptionResult>& signalResults
    );

private:
    WorldLabel& getOrCreateLabel(const SignalReceptionResult& result);
    std::unordered_map<const WorldSignal*, WorldLabel> m_labels;
};
