#pragma once

#include "src/world/coordinates/WorldFrame.h"
#include "src/world/coordinates/WorldPosition.h"

namespace world::coordinates
{

class FrameManager
{
public:
    FrameManager() = default;

    const WorldFrame& frame() const
    {
        return m_frame;
    }

    const WorldPosition& origin() const
    {
        return m_frame.origin;
    }

    void setOrigin(const WorldPosition& origin)
    {
        m_frame.origin = origin;
    }

    // Пока делаем максимально безопасно:
    // render-frame всегда привязан к камере / игроку.
    //
    // ВАЖНО:
    // здесь НЕ двигаем объекты;
    // здесь НЕ обновляем legacy position;
    // здесь НЕ вызываем callbacks.
    //
    // WorldFrame — это только система отсчёта для рендера.
    bool updateFromCamera(const WorldPosition& cameraWorldPosition)
    {
        const bool changed =
            distanceMeters(cameraWorldPosition, m_frame.origin) > 0.001;

        m_frame.origin = cameraWorldPosition;
        return changed;
    }

private:
    WorldFrame m_frame;
};

} // namespace world::coordinates