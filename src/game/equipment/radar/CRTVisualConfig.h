#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "IRadarVisualConfig.h"


struct CRTVisualConfig : public IRadarVisualConfig
{

    glm::vec4 m_bgColor        = {0.0f, 0.15f, 0.0f, 0.7f};
    glm::vec4 m_axisColor      = {0.0f, 1.0f, 0.0f, 0.8f};

    // --- Sweep ---
    glm::vec4 m_sweepColor     = {0.0f, 1.0f, 0.0f, 0.1f};
    float     m_sweepLineWidth = 2.0f;
    float     m_trailFadePower = 1.5f;

    // --- Contacts ---
    glm::vec4 m_contactColor   = {0.0f, 1.0f, 0.0f, 0.80f};
    float     m_contactHalfWidth = 5.0f;
    float     m_contactBoxSize   = 4.0f;

    // --- Layout tuning ---
    float     m_radiusScale      = 0.85f;   // уменьшение радиуса для меток
    float     m_verticalScale    = 0.3f;    // масштаб высоты
    // текстуры
    const char* backgroundTexturePath;
    const char* overlayTexturePath;
};