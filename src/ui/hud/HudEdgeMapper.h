#pragma once

#include <glm/glm.hpp>
#include <vector>

class HudEdgeMapper
{
public:
    // Устанавливаем границу HUD
    // normalizedBoundary — из ShipHudProfile
    void setBoundary(
        const std::vector<glm::vec2>& normalizedBoundary,
        int viewportW,
        int viewportH
    );

    // Проекция направления на границу
    // direction — нормализованный вектор от центра экрана
    bool projectDirection(
        const glm::vec2& screenCenter,
        const glm::vec2& direction,
        glm::vec2& outPoint
    ) const;

    // доступ на чтение
    const std::vector<glm::vec2>& boundaryPx() const
    {
        return m_boundaryPx;
    }

    bool isInsideBoundary(const glm::vec2& point) const;

private:
    // Та же граница, но УЖЕ в пикселях
    std::vector<glm::vec2> m_boundaryPx;
};
