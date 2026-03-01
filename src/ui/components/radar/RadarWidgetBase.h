#pragma once
#include <vector>
#include <glm/glm.hpp>
#include "src/scene/EntityID.h"
#include "src/ui/components/UIComponent.h"
#include "src/game/equipment/radar/IRadarVisualConfig.h"
#include "src/game/equipment/radar/IRadarEffectsConfig.h"



struct RadarContactView
{
    EntityId id;
    glm::vec3 Position;
};






class RadarWidgetBase : public UIComponent
{
public:
     ~RadarWidgetBase();

    virtual void init() {};
    void setPlayerTransform(
        const glm::vec3& pos,
        const glm::mat4& orientation);

    void setContacts(
        const std::vector<RadarContactView>& contacts);

    void update(float dt) override;
    void render(
        const Viewport& vp,
        float parentX,
        float parentY,
        float parentW,
        float parentH
    ) override;

    void configureFromDesc(
        float sweepSpeed,
        float displayRange);

    virtual void applyVisualConfig(
            const IRadarVisualConfig& cfg);
    virtual void applyEffectsConfig(
            const game::IRadarEffectsConfig& effects);
        
        

protected:
    virtual void renderRadarContent(
        float px,
        float py,
        float pw,
        float ph) = 0;
    virtual void renderBackground() = 0;
    virtual void renderOverlay()    = 0;

    virtual void renderContacts();
    virtual void renderSweep();

    const glm::vec3& playerPosition() const
    {
        return m_playerPos;
    }

    const glm::mat4& playerOrientation() const
    {
        return m_playerOrientation;
    }

protected:
    float m_centerX{};
    float m_centerY{};
    float m_radius{};

    glm::vec3 m_playerPos{};
    glm::mat4 m_playerOrientation{1.0f};

    std::vector<RadarContactView> m_contacts;

    float m_sweepAngle = 0.0f;
    float m_sweepSpeed = 30.0f;
    float m_displayRange = 200.0f;


    bool                        isPrinting = false;
};