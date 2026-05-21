#pragma once

#include "WorldFrame.h"
#include "WorldPosition.h"
#include <functional>

namespace world::coordinates
{

class FrameManager
{
public:
    FrameManager() = default;

    const WorldFrame& frame() const { return m_frame; }

    // Принудительно установить Origin
    void setOrigin(const WorldPosition& origin)
    {
        m_frame.origin = origin;
    }

    // Проверить, не пора ли сдвинуть Origin
    bool rebaseIfNeeded(
        const WorldPosition& cameraWorldPos,
        double thresholdMeters = 1'000'000.0  // 1000 км
    )
    {
        double dist = distanceMeters(cameraWorldPos, m_frame.origin);
        
        if (dist < thresholdMeters)
            return false;
        
        // Запоминаем старый Origin
        WorldPosition oldOrigin = m_frame.origin;
        
        // Сдвигаем Origin к камере
        m_frame.origin = cameraWorldPos;
        
        // Вызываем callback, если он установлен
        if (onFrameChanged)
            onFrameChanged(oldOrigin, m_frame.origin);
        
        return true;
    }

    // Событие при смене Origin
    std::function<void(const WorldPosition& oldOrigin, const WorldPosition& newOrigin)> onFrameChanged;

private:
    WorldFrame m_frame;
};

} // namespace world::coordinates