#pragma once

#include <unordered_map>
#include <glm/glm.hpp>

#include "src/render/Camera.h"
#include "src/game/ship/view/ShipCameraController.h"

#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipRole.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/core/ShipControlState.h"

#include "src/game/ship/ShipHudProfile.h"

#include "src/game/ship/cockpit/CockpitContours.h"
#include "src/game/ship/cockpit/ShipCockpitState.h"

#include "src/ui/hud/HudEdgeMapper.h"
#include "core/StateContext.h"

#include "render/RenderCockpitBitmapPass.h"
#include "render/RenderCockpitPass.h"
#include "render/HUD/WorldLabelRenderer.h"

#include "world/WorldSignal.h"

#include "render/HUD/WorldLabel.h"
#include "render/ScreenUtils.h"

#include "src/game/ship/view/ShipCameraMode.h"



struct ShipDescriptor;
struct ShipEquipment;

struct PlayerShipView
{
    // ───────── Camera ─────────
    ShipCameraController            cameraController;

    std::unordered_map<ShipCameraMode, Camera> m_cameras;
    ShipCameraMode mainCamera = ShipCameraMode::Cockpit;


    // ───────── HUD ─────────
    HudEdgeMapper                   hudEdgeMapper;
    ShipHudProfile                  hudProfile;
    RenderCockpitBitmapPass         cockpitBitmapPass;
    RenderCockpitPass               cockpitPass;
    WorldLabelRenderer              m_worldLabelRenderer;

    // ───────── Cockpit ─────────
    bool                            m_hasCockpit = false;
    CockpitGeometry                 m_cockpitGeometry;
    unsigned int                    m_cockpitBaseTex = 0;
    unsigned int                    m_cockpitGlassTex = 0;
    ShipCockpitState                m_cockpitState;
    std::vector<WorldLabel>         m_worldLabels;


    void updateCockpitState(
        ShipRole role,
        const ShipDescriptor* desc,
        const ShipTransform& transform,
        const ShipEquipment& equipment
    );

    void init(
        StateContext& context,
        const ShipDescriptor* desc,
        const ShipTransform& transform
    );

    void renderCockpit();

    void renderWorldLabels(
        const std::vector<WorldLabel>& labels,
        const glm::vec3& shipPosition,
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        const Viewport& vp
    );

    void renderHudBoundary();

    void update(
        float dt,
        ShipRole role,
        const glm::vec3& position,
        const glm::mat4& orientation
    );

    void updateCockpitStateFromSnapshot(
        float forwardVelocity,
        float targetSpeed,
        bool cruiseActive,
        const std::vector<WorldLabel>& labels
    );
    
    



    
//   ####     ####     ####     ####     #####    #####    ####    ######    #####
//      ##   ##  ##   ##  ##   ##  ##   ##       ##       ##  ##    ##  ##  ##
//   #####   ##       ##       ######    #####    #####   ##  ##    ##       #####
//  ##  ##   ##  ##   ##  ##   ##            ##       ##  ##  ##    ##           ##
//   #####    ####     ####     #####   ######   ######    ####    ####     ######


    const ShipCockpitState& getCockpitState() const
    {
        return m_cockpitState;
    }

    bool hasCockpit() const { return m_hasCockpit; }

    const CockpitGeometry& getCockpitGeometry() const
    {
        return m_cockpitGeometry;
    }

    unsigned int getCockpitBaseTex() const
    {
        return m_cockpitBaseTex;
    }

    unsigned int getCockpitGlassTex() const
    {
        return m_cockpitGlassTex;
    }


    Camera& mainCameraRef()
    {
        return m_cameras[mainCamera];
    }

    Camera& camera(ShipCameraMode mode)
    {
        return m_cameras[mode];
    }

    void setMainCamera(ShipCameraMode mode)
    {
        mainCamera = mode;
    }

    const std::vector<WorldLabel>& worldLabels() const
    {
        return m_worldLabels;
    }
};

