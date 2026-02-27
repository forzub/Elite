#pragma once
#include <unordered_map>
#include "RadarWidgetBase.h"
#include "render/bitmap/TextureLoader.h"
#include "src/game/equipment/radar/IRadarVisualConfig.h"


struct CRTContact
{
    RadarContactView data;
    float lastHitTime = -1000.0f;
    glm::vec3 latchedLocal = glm::vec3(0.0f);
};


class RadarCRTWidget : public RadarWidgetBase
{
public:
    void init(const char* bgPath, const char* overlayPath);
    void update(float dt) override;
    void applyVisualConfig(const IRadarVisualConfig& base) override;

protected:
   
    void renderBackground() override;
    void renderOverlay() override;
    void renderContacts() override;
    void renderSweep() override;

private:
    unsigned int                m_bg = 0;
    unsigned int                m_overlay = 0;

    float                       m_time = 0.0f;
    float                       m_scanlinePhase = 0.0f;
    float                       m_sweepWidthDeg = 3.0f;   // толщина луча
    float                       m_decayRate = 0.8f;   // скорость затухания
    std::unordered_map<EntityId, CRTContact> m_crtContacts;

    float                       m_perspective = 0.30f;
    float                       m_markerHalfWidth = 5.0f;
    float                       m_markerThickness = 2.0f;
     float                       m_sweepThickness = 2.0f;

    float                       m_trailLengthDeg = 35.0f;   // длина хвоста
    int                         m_trailSteps = 30;          // сколько линий
 


    // ================= VISUAL CONFIG =================

    // --- Background ---
    glm::vec4 m_bgColor        = {0.0f, 0.15f, 0.0f, 0.0f};
    glm::vec4 m_axisColor      = {0.0f, 1.0f, 0.0f, 0.0f};

    // --- Sweep ---
    glm::vec4 m_sweepColor     = {0.0f, 1.0f, 0.0f, 0.0f};
    float     m_sweepLineWidth = 2.0f;
    float     m_trailFadePower = 1.5f;

    // --- Contacts ---
    glm::vec4 m_contactColor   = {0.0f, 1.0f, 0.0f, 0.0f};
    float     m_contactHalfWidth = 5.0f;
    float     m_contactBoxSize   = 4.0f;

    // --- Layout tuning ---
    float     m_radiusScale      = 0.85f;   // уменьшение радиуса для меток
    float     m_verticalScale    = 0.3f;    // масштаб высоты

   
    
    
};