#include "src/game/ship/view/PlayerShipView.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/game/ship/core/ShipCore.h"
#include "src/game/equipment/ShipEquipment.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/render/bitmap/TextureLoader.h"

bool PlayerShipView::g_debugLogNextFrame = false;




void PlayerShipView::init(
        StateContext& context,
        const ShipDescriptor* desc,
        const ShipTransform& transform
    )
    {

        const Viewport& vp = context.viewport();
        float aspect = (float)vp.width / (float)vp.height;

        m_shipDesc = desc;

        m_cameras[ShipCameraMode::Cockpit] = Camera();
        m_cameras[ShipCameraMode::Rear]    = Camera();
        m_cameras[ShipCameraMode::Drone]   = Camera();


        // Устанавливаем проекционные матрицы для каждой камеры
        // Предполагаем, что у Camera есть метод setPerspective
        float fov = 70.0f; // или другое значение из настроек
        float nearPlane = 0.1f;
        float farPlane = 500000.0f;
        
        m_cameras[ShipCameraMode::Cockpit].setPerspective(fov, nearPlane, farPlane);
        m_cameras[ShipCameraMode::Cockpit].setAspect(aspect);
        
        m_cameras[ShipCameraMode::Rear].setPerspective(fov, nearPlane, farPlane);
        m_cameras[ShipCameraMode::Rear].setAspect(aspect);
        
        m_cameras[ShipCameraMode::Drone].setPerspective(fov, nearPlane, farPlane);
        m_cameras[ShipCameraMode::Drone].setAspect(aspect);

        mainCamera = ShipCameraMode::Cockpit;

        // camera.setAspect(context.aspect());
        // camera.setPosition(transform.position);

        


        if (desc && desc->cockpit)
        {
            m_hasCockpit = true;
            m_cockpitGeometry = desc->cockpit->geometry;

            m_cockpitBaseTex =
                TextureLoader::load2D(desc->cockpit->baseTexturePath, false);

            m_cockpitGlassTex =
                TextureLoader::load2D(desc->cockpit->glassTexturePath, false);

            cockpitPass.setGeometry(m_cockpitGeometry);
            cockpitBitmapPass.setBaseTexture(m_cockpitBaseTex);
            cockpitBitmapPass.setGlassTexture(m_cockpitGlassTex);
        }

        if (desc && !desc->hud.edgeBoundary.contour.empty())
        {
            hudEdgeMapper.setBoundary(
                desc->hud.edgeBoundary.contour,
                vp.width,
                vp.height
            );
        }

        cockpitBitmapPass.init();
        cockpitPass.init();
        m_worldLabelRenderer.init(context);
    }






void PlayerShipView::updateCockpitState(ShipRole role,
    const ShipDescriptor* desc,
    const ShipTransform& transform,
    const game::ShipEquipment& equipment)
{
    if (role != ShipRole::Player || !desc)
        return;

    // ===== SPEED BAR =====

    float maxSpeed = desc->physics.maxCombatSpeed ;
    float currentSpeed = transform.forwardVelocity;
    float speed01 = glm::clamp(currentSpeed / maxSpeed, 0.0f, 1.0f);

    auto& barOv  = m_cockpitState.overrides["speed_bar_fill"];

    barOv .visible = true;
    barOv .overrideFill = true;
    barOv .fill01 = speed01;


    // ========= вращение как спидометр ============
    float speedneedle01 = transform.forwardVelocity /
                desc->physics.maxCombatSpeed;

    speedneedle01 = glm::clamp(speedneedle01, 0.0f, 1.0f);

    auto& needle = m_cockpitState.overrides["speed_needle"];
    needle.visible = true;
    needle.overrideRotation = true;
    needle.rotationDeg = -90.0f + speedneedle01 * 180.0f;

}







void PlayerShipView::updateBoundary(int width, int height)
{
    // Предполагаем, что у вас есть доступ к ShipDescriptor
    // Если нет - нужно передавать его при обновлении
    if (!m_shipDesc) return; // нужно добавить поле m_shipDesc
    
    hudEdgeMapper.setBoundary(
        m_shipDesc->hud.edgeBoundary.contour,
        width,
        height
    );
}







//                                                                 ###                 ##       ##
//                                                                  ##                          ##
//  ######    ####    #####              ####     ####     ####     ##  ##  ######    ###      #####
//   ##  ##  ##  ##   ##  ##            ##  ##   ##  ##   ##  ##    ## ##    ##  ##    ##       ##
//   ##      ######   ##  ##            ##       ##  ##   ##        ####     ##  ##    ##       ##
//   ##      ##       ##  ##            ##  ##   ##  ##   ##  ##    ## ##    #####     ##       ## ##
//  ####      #####   ##  ##             ####     ####     ####     ##  ##   ##       ####       ###
//                                                                          ####

void PlayerShipView::renderCockpit()
{
    cockpitBitmapPass.renderBase();
    cockpitBitmapPass.setGlassIntensity(1.0f);
    cockpitBitmapPass.renderGlass();

    cockpitPass.render(m_cockpitState);
}


//                                       ###              ###
//                                        ##               ##
//  ######    ####    #####               ##      ####     ##
//   ##  ##  ##  ##   ##  ##              ##         ##    #####
//   ##      ######   ##  ##              ##      #####    ##  ##
//   ##      ##       ##  ##              ##     ##  ##    ##  ##
//  ####      #####   ##  ##             ####     #####   ######


void PlayerShipView::renderWorldLabels(
    const std::vector<WorldLabel>& labels,
    const glm::vec3& shipPosition,
    const glm::mat4& viewMatrix,
    const glm::mat4& projectionMatrix,
    const Viewport& vp
)
{
   

    glm::vec2 screenCenter(
        vp.width * 0.5f,
        vp.height * 0.5f
    );

    for (const auto& label : labels)
    {
        glm::vec2 projectedPos;
        
        
        bool projected = projectToScreen(
            label.data.worldPos,
            viewMatrix,
            projectionMatrix,
            vp.width,
            vp.height,
            projectedPos
        );



        glm::vec3 toTarget =
            glm::normalize(label.data.worldPos - shipPosition);


        glm::vec4 viewDir4 =
            viewMatrix * glm::vec4(toTarget, 0.0f);

        glm::vec2 dir2D(viewDir4.x, -viewDir4.y);




        if (glm::length(dir2D) < 1e-4f)
            continue;

        dir2D = glm::normalize(dir2D);

        // --- создаём копию для рендера ---
        WorldLabel renderLabel = label;

        renderLabel.edgeDir = dir2D;

        bool insideHud = false;

        if (projected)
            insideHud = hudEdgeMapper.isInsideBoundary(projectedPos);

        if (projected && insideHud)
        {
            renderLabel.onScreen  = true;
            renderLabel.screenPos = projectedPos;
        }
        else
        {
            glm::vec2 edgePos;

            if (hudEdgeMapper.projectDirection(
                    screenCenter,
                    dir2D,
                    edgePos))
            {
                renderLabel.onScreen  = false;
                renderLabel.screenPos = edgePos;
            }
        }

        m_worldLabelRenderer.renderHUD(renderLabel);
    }
  
}

//                                      ###                                    ###
//                                       ##                                     ##
//  ######    ####    #####              ##       ####    ##  ##   #####        ##    ####    ######   ##  ##
//   ##  ##  ##  ##   ##  ##             #####   ##  ##   ##  ##   ##  ##    #####       ##    ##  ##  ##  ##
//   ##      ######   ##  ##             ##  ##  ##  ##   ##  ##   ##  ##   ##  ##    #####    ##      ##  ##
//   ##      ##       ##  ##             ##  ##  ##  ##   ##  ##   ##  ##   ##  ##   ##  ##    ##       #####
//  ####      #####   ##  ##            ######    ####     ######  ##  ##    ######   #####   ####         ##
//                                                                                                     #####

void PlayerShipView::renderHudBoundary()
{
     const auto& contour = hudEdgeMapper.boundaryPx();
        if (contour.size() >= 2)
        {
            glDisable(GL_TEXTURE_2D);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glColor4f(0.2f, 0.8f, 1.0f, 0.25f); // голубой, ~25% прозрачности

            glBegin(GL_LINE_LOOP);
            for (const auto& p : contour)
            {
                glVertex2f(p.x, p.y);
            }
            glEnd();
        }
}





void PlayerShipView::update(
        float dt,
        ShipRole role,
        const glm::vec3& position,
        const glm::mat4& orientation
)
{
    if (role != ShipRole::Player)
        return;

    ShipTransform tempTransform;
    tempTransform.position = position;
    tempTransform.orientation = orientation;


    cameraController.updateMode(
        ShipCameraMode::Cockpit,
        dt,
        tempTransform,
        m_cameras[ShipCameraMode::Cockpit]
    );

    cameraController.updateMode(
        ShipCameraMode::Rear,
        dt,
        tempTransform,
        m_cameras[ShipCameraMode::Rear]
    );

    cameraController.updateMode(
        ShipCameraMode::Drone,
        dt,
        tempTransform,
        m_cameras[ShipCameraMode::Drone]
    );


}





void PlayerShipView::updateCockpitStateFromSnapshot(
    float forwardVelocity,
    float targetSpeed,
    bool cruiseActive,
    const std::vector<WorldLabel>& labels
)
{
    m_cockpitState.forwardVelocity = forwardVelocity;
    m_cockpitState.targetSpeed     = targetSpeed;
    m_cockpitState.cruiseActive    = cruiseActive;

    m_worldLabels = labels;
}


void PlayerShipView::resize(int width, int height)
{
    float aspect = (float)width / (float)height;
    
    // Просто обновляем aspect ratio у всех камер
    // Не нужно переустанавливать перспективу, только aspect
    for (auto& [mode, camera] : m_cameras)
    {
        camera.setAspect(aspect);
    }
}

