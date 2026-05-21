#include "src/game/ship/view/PlayerShipView.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/game/ship/core/ShipCore.h"
#include "src/game/equipment/ShipEquipment.h"
#include "src/game/ship/ShipDescriptor.h"

#include "src/render/bitmap/TextureLoader.h"

#include <algorithm>
#include <cmath>
#include <vector>

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
        float nearPlane = 1.0f;
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
    if (labels.empty())
        return;

    struct Candidate
    {
        WorldLabel label;
        float score = 0.0f;
    };

    constexpr size_t MAX_TEXT_LABELS = 24;
    constexpr size_t MAX_WAVE_LABELS = 18;
    constexpr size_t MAX_EDGE_LABELS = 24;

    std::vector<Candidate> textLabels;
    std::vector<Candidate> waveLabels;
    std::vector<Candidate> edgeLabels;

    textLabels.reserve(MAX_TEXT_LABELS);
    waveLabels.reserve(MAX_WAVE_LABELS);
    edgeLabels.reserve(MAX_EDGE_LABELS);

    const glm::vec2 screenCenter(
        vp.width * 0.5f,
        vp.height * 0.5f
    );

    for (const auto& label : labels)
    {
        if (label.visual.visibility < 0.02f)
            continue;

        const glm::vec3 toTargetWorld = label.data.worldPos - shipPosition;
        const float dist2 = glm::dot(toTargetWorld, toTargetWorld);

        if (dist2 < 0.0001f)
            continue;

        const glm::vec3 toTarget = glm::normalize(toTargetWorld);

        glm::vec4 viewDir4 =
            viewMatrix * glm::vec4(toTarget, 0.0f);

        // Если объект глубоко позади камеры, оставляем только edge marker.
        glm::vec2 dir2D(viewDir4.x, -viewDir4.y);

        if (glm::length(dir2D) < 1e-4f)
            continue;

        dir2D = glm::normalize(dir2D);

        glm::vec2 projectedPos;
        const bool projected = projectToScreen(
            label.data.worldPos,
            viewMatrix,
            projectionMatrix,
            vp.width,
            vp.height,
            projectedPos
        );

        WorldLabel renderLabel = label;
        renderLabel.edgeDir = dir2D;

        bool insideHud = false;

        if (projected)
            insideHud = hudEdgeMapper.isInsideBoundary(projectedPos);

        if (projected && insideHud)
        {
            renderLabel.onScreen = true;
            renderLabel.screenPos = projectedPos;
        }
        else
        {
            glm::vec2 edgePos;

            if (!hudEdgeMapper.projectDirection(
                    screenCenter,
                    dir2D,
                    edgePos))
            {
                continue;
            }

            renderLabel.onScreen = false;
            renderLabel.screenPos = edgePos;
        }

        // Приоритет: ближе + видимее = важнее.
        const float distanceScore = 1.0f / (1.0f + std::sqrt(dist2) * 0.001f);
        const float score =
            renderLabel.visual.visibility * 2.0f +
            distanceScore;

        const bool isText =
            renderLabel.data.semanticState == SignalSemanticState::Decoded ||
            renderLabel.data.displayClass == SignalDisplayClass::Global;

        const bool isWave =
            !isText &&
            (
                renderLabel.data.semanticState == SignalSemanticState::Noise ||
                renderLabel.data.displayClass == SignalDisplayClass::Other
            );

        Candidate candidate;
        candidate.label = renderLabel;
        candidate.score = score;

        if (!renderLabel.onScreen)
        {
            edgeLabels.push_back(std::move(candidate));
        }
        else if (isText)
        {
            textLabels.push_back(std::move(candidate));
        }
        else if (isWave)
        {
            waveLabels.push_back(std::move(candidate));
        }
    }

    auto byScoreDesc = [](const Candidate& a, const Candidate& b)
    {
        return a.score > b.score;
    };

    std::sort(textLabels.begin(), textLabels.end(), byScoreDesc);
    std::sort(waveLabels.begin(), waveLabels.end(), byScoreDesc);
    std::sort(edgeLabels.begin(), edgeLabels.end(), byScoreDesc);

    if (textLabels.size() > MAX_TEXT_LABELS)
        textLabels.resize(MAX_TEXT_LABELS);

    if (waveLabels.size() > MAX_WAVE_LABELS)
        waveLabels.resize(MAX_WAVE_LABELS);

    if (edgeLabels.size() > MAX_EDGE_LABELS)
        edgeLabels.resize(MAX_EDGE_LABELS);

    std::vector<WorldLabel> batchedLabels;

    batchedLabels.reserve(
        textLabels.size() +
        waveLabels.size() +
        edgeLabels.size()
    );

    for (const auto& item : textLabels)
        batchedLabels.push_back(item.label);

    for (const auto& item : waveLabels)
        batchedLabels.push_back(item.label);

    for (const auto& item : edgeLabels)
        batchedLabels.push_back(item.label);

    m_worldLabelRenderer.renderHUDBatch(batchedLabels);


        static int dbgCounter = 0;
        dbgCounter++;

        if (dbgCounter % 60 == 0)
        {
            std::cout
                << "[Labels] text=" << textLabels.size()
                << " wave=" << waveLabels.size()
                << " edge=" << edgeLabels.size()
                << "\n";
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
        const ShipTransform& transform,
        const std::vector<game::simulation::ObjectDetachedFragmentSnapshot>& detachedFragments
)
{
    if (role != ShipRole::Player)
        return;

    const ShipTransform& tempTransform = transform;


    cameraController.updateMode(
        ShipCameraMode::Cockpit,
        dt,
        m_shipDesc,
        tempTransform,
        m_cameras[ShipCameraMode::Cockpit],
        &detachedFragments
    );

    cameraController.updateMode(
        ShipCameraMode::Rear,
        dt,
        m_shipDesc,
        tempTransform,
        m_cameras[ShipCameraMode::Rear],
        &detachedFragments
    );

    cameraController.updateMode(
        ShipCameraMode::Drone,
        dt,
        m_shipDesc,
        tempTransform,
        m_cameras[ShipCameraMode::Drone],
        &detachedFragments
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

