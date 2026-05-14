#pragma once
#include <unordered_map>
#include <memory>
#include "src/render/bitmap/TextureLoader.h"
#include "src/render/ShaderUtils.h"
#include "src/game/equipment/types/RadarVisualProfile.h"

#include "src/game/equipment/radar/IRadarVisualConfig.h"
#include "src/game/equipment/radar/IRadarEffectsConfig.h"

#include "src/game/equipment/radar/config/PPIVisualConfig.h"
#include "src/ui/components/radar/RadarWidgetBase.h"
#include "src/ui/components/radar/RadarRenderTarget.h"


// Контакт (общий для всех типов)
struct PPIContact
{
    RadarContactView data;
    float lastHitTime = -1000.0f;
    glm::vec3 latchedLocal = glm::vec3(0.0f);
};


// Абстрактный базовый класс для эффектов
class IRadarEffect {
public:
    virtual ~IRadarEffect() = default;
    
    // Обновление эффектов
    virtual void update(float dt) = 0;
    
    // Применение эффектов в шейдере
    virtual void applyToShader(unsigned int shaderProgram, float time,
                               float centerX, float centerY, 
                               float radiusX, float radiusY) = 0;
    
    // Получить название шейдера
    virtual const char* getShaderName() const = 0;
    virtual void loadConfig(std::shared_ptr<game::IRadarEffectsConfig> config) = 0;
    // Сброс всех глюков
    virtual void resetGlitches() = 0;
};

// ============================================================================
// ОСНОВНОЙ КЛАСС ВИДЖЕТА РАДАРА
// ============================================================================

class RadarPPIWidget : public RadarWidgetBase
{
public:
    // Инициализация с указанием типа
    void init(game::RadarVisualProfile type = game::RadarVisualProfile::CRT);
    
    // Обновление состояния
    void update(float dt) override;
    
    // Применение конфигурации
    void applyVisualConfig(const IRadarVisualConfig& base) override;
    void applyEffectsConfig(const game::IRadarEffectsConfig& effects) override;

    // Управление внешним видом
    void setVisualConfig(const PPIVisualConfig& config);
    const PPIVisualConfig& getVisualConfig() const { return m_visualConfig; }

    // Доступ к эффектам
    IRadarEffect* getEffect() { return m_effect.get(); }

    void enableFreezeTest(bool enable) { m_freezeTestEnabled = enable; }

protected:
    // Методы рендеринга
    void renderRadarContent(float px, float py, float pw, float ph) override;
    void renderBackground() override;
    void renderOverlay() override;
    void renderContacts() override;
    void renderSweep() override;
    void renderGraticule();

private:
    void initShader(const std::string& fragPath);
    void switchEffect(game::RadarVisualProfile type);

private:
    // Тип эффекта
    std::unique_ptr<IRadarEffect> m_effect;
    
    // Текстуры
    unsigned int m_bg = 0;
    unsigned int m_overlay = 0;

    // Время и состояние
    float m_time = 0.0f;
    float m_sweepWidthDeg = 3.0f;
    std::unordered_map<EntityId, PPIContact> m_ppiContacts;

    // Внешний вид
    PPIVisualConfig m_visualConfig;

    // Шейдер пост-обработки
    unsigned int m_postShader = 0;
    bool m_shaderEnabled = false;
    float m_shaderTime = 0.0f;


    RadarRenderTarget m_renderTarget;      // текущий кадр




    // ===== ЭКСПЕРИМЕНТАЛЬНЫЕ ПОЛЯ =====
    bool m_freezeTestEnabled = false;
    
    // Для экспериментов с freeze
    RadarRenderTarget m_freezeTestTarget;  // тестовый FBO
    bool m_freezeTestFirstFrame = true;
    RadarRenderTarget m_prevRenderTarget;  // предыдущий кадр (НОВЫЙ!)
    bool m_firstFrame = true;
};