// GameSimulation: hub navigation frames, spawn reference frames, celestial body resolution, ship reference-frame placement / update, navigation gravity and dynamic navigation context.

#include "src/game/simulation/GameSimulationInternal.h"



const game::navigation::HubNavigationFrame*
GameSimulation::hubNavigationFrame(
    const std::string& hubId
) const
{
    auto it =
        m_hubNavigationFrames.find(hubId);

    if (it == m_hubNavigationFrames.end())
        return nullptr;

    return &it->second;
}


void GameSimulation::rebuildHubNavigationFrames(double frameDeltaSeconds)
{
    // Keep the previous kinematic epoch long enough to derive frame
    // acceleration. The current production motion path still consumes the
    // legacy HubNavigationFrame fields; acceleration is shadow state only in
    // this migration stage.
    const auto previousFrames = m_hubNavigationFrames;
    m_hubNavigationFrames.clear();

    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (hub.systemId != m_activeCelestialSystemId)
            continue;
        game::navigation::HubNavigationFrame frame;

        frame.systemId = hub.systemId;
        frame.hubId = hub.id;
        frame.parentBodyId = hub.parentBodyId;

        frame.originMeters =
            world::coordinates::fullMeters(
                hub.worldPosition
            );

        glm::dvec3 parentMeters {0.0};

        auto parentIt =
            m_celestialBodyPositionsAu.find(hub.parentBodyId);

        if (parentIt != m_celestialBodyPositionsAu.end())
        {
            parentMeters =
                parentIt->second *
                world::celestial::MetersPerAu;
        }

        const glm::dvec3 radial =
            safeNormalizeD(
                frame.originMeters - parentMeters,
                glm::dvec3(0.0, 1.0, 0.0)
            );

        // Скорость хаба.
        auto velIt =
            m_hubVelocityMetersPerSecond.find(hubId);

        if (velIt != m_hubVelocityMetersPerSecond.end())
        {
            frame.velocityMetersPerSecond =
                velIt->second;
        }
        else
        {
            frame.velocityMetersPerSecond =
                world::orbits::computeOrbitVelocityMetersPerSecond(
                    hub.motion,
                    m_orbitalUniverseTimeSeconds
                );
        }







        /*
            frame.velocityMetersPerSecond — полная мировая скорость хаба:

                parent planet world velocity
                +
                hub orbital velocity relative to planet.

            Для построения orbital frame нужна только скорость
            хаба относительно родительской планеты.

            Иначе движение планеты вокруг звезды поворачивает
            prograde/normal и Hub Map получает другую плоскость орбиты,
            чем Planet Details.
        */
        glm::dvec3 parentVelocityMetersPerSecond {
            0.0
        };

        bool parentVelocityResolved =
            false;

        const auto parentVelocityIt =
            m_celestialBodyVelocitiesMetersPerSecond.find(
                hub.parentBodyId
            );

        if (parentVelocityIt !=
            m_celestialBodyVelocitiesMetersPerSecond.end())
        {
            parentVelocityMetersPerSecond =
                parentVelocityIt->second;

            parentVelocityResolved =
                true;
        }

        glm::dvec3 relativeOrbitalVelocityMetersPerSecond;

        if (parentVelocityResolved)
        {
            relativeOrbitalVelocityMetersPerSecond =
                frame.velocityMetersPerSecond -
                parentVelocityMetersPerSecond;
        }
        else
        {
            /*
                При стартовой инициализации velocity cache теоретически
                может быть ещё не заполнен. Аналитическая локальная
                орбитальная скорость остаётся корректным fallback.
            */
            relativeOrbitalVelocityMetersPerSecond =
                world::orbits::
                    computeOrbitVelocityMetersPerSecond(
                        hub.motion,
                        m_orbitalUniverseTimeSeconds
                    );
        }

        glm::dvec3 prograde =
            safeNormalizeD(
                relativeOrbitalVelocityMetersPerSecond,
                glm::dvec3(1.0, 0.0, 0.0)
            );

            /*
                Убираем возможную радиальную составляющую.
                Для круговой орбиты она практически нулевая,
                но frame должен оставаться ортогональным.
            */
            prograde =
                safeNormalizeD(
                    prograde -
                        radial *
                        glm::dot(
                            prograde,
                            radial
                        ),

                    glm::dvec3(
                        1.0,
                        0.0,
                        0.0
                    )
                );









        const glm::dvec3 normal =
            safeNormalizeD(
                glm::cross(prograde, radial),
                glm::dvec3(0.0, 0.0, 1.0)
            );

        // Пересобираем prograde через normal/radial,
        // чтобы оси были ортогональными.
        prograde =
            safeNormalizeD(
                glm::cross(radial, normal),
                prograde
            );

        frame.radialAxis = radial;
        frame.progradeAxis = prograde;
        frame.normalAxis = normal;

        const double orbitRadiusMeters =
            glm::length(frame.originMeters - parentMeters);

        const glm::dvec3 tangentialVelocityMetersPerSecond =
            relativeOrbitalVelocityMetersPerSecond -
            radial *
                glm::dot(
                    relativeOrbitalVelocityMetersPerSecond,
                    radial
                );

        const double angularSpeedRadPerSecond =
            orbitRadiusMeters > 1.0
                ? glm::length(tangentialVelocityMetersPerSecond) /
                    orbitRadiusMeters
                : 0.0;

        /*
            Basis convention is X=prograde, Y=radial, Z=normal with
            normal = cross(prograde, radial). Therefore the orbital frame
            rotates around -normal for positive prograde motion.
        */
        frame.angularVelocityWorldRadPerSecond =
            -normal * angularSpeedRadPerSecond;

        const auto previousFrameIt = previousFrames.find(hubId);
        if (std::abs(frameDeltaSeconds) > 1.0e-9 &&
            previousFrameIt != previousFrames.end() &&
            previousFrameIt->second.valid &&
            previousFrameIt->second.systemId == frame.systemId)
        {
            frame.accelerationMetersPerSecond2 =
                (frame.velocityMetersPerSecond -
                 previousFrameIt->second.velocityMetersPerSecond) /
                frameDeltaSeconds;

            frame.angularAccelerationWorldRadPerSecond2 =
                (frame.angularVelocityWorldRadPerSecond -
                 previousFrameIt->second.angularVelocityWorldRadPerSecond) /
                frameDeltaSeconds;
        }
        else if (!m_gravityBodies.empty())
        {
            // First-epoch fallback: for an orbital hub the physical frame
            // acceleration is gravity-driven. Subsequent ticks use the actual
            // derivative of the authoritative frame velocity above.
            frame.accelerationMetersPerSecond2 =
                game::navigation::GravityFieldSystem::sample(
                    frame.originMeters,
                    m_gravityBodies
                ).accelerationMps2;
        }

        // Пока prime ищем как первый модуль.
        // Позже лучше сохранить явно из initial_world_state.json.
        if (!hub.modules.empty())
        {
            const EntityId primeObjectId =
                hub.modules.front();

            auto objIt =
                m_staticObjects.find(primeObjectId);

            if (objIt != m_staticObjects.end())
                frame.primeModuleId = objIt->second.hubModuleId;
        }

        frame.valid = true;





        hub.orientation =
            game::navigation::hubVisualOrientation(
                frame.progradeAxis,
                frame.radialAxis,
                frame.normalAxis
            );









        m_hubNavigationFrames[hubId] = frame;
    }
}








void GameSimulation::prepareReferenceFramesForSpawn()
{
    // ------------------------------------------------------------
    // Подготовка орбитальных хабов и reference frames ДО спауна
    // игрока в frame.
    //
    // Важно:
    // это НЕ полный simulation update.
    // Тут нет AI, physics, snapshot, debug logs.
    // Мы только приводим хабы/станции/reference frame
    // в корректное стартовое состояние.
    // ------------------------------------------------------------

    for (auto& [hubId, hub] : m_orbitalHubs)
    {
        if (hub.systemId != m_activeCelestialSystemId ||
            !hub.motion.enabled)
        {
            continue;
        }

        if (!hub.parentBodyId.empty())
        {
            auto parentIt =
                m_celestialBodyPositionsAu.find(
                    hub.parentBodyId
                );

            if (parentIt != m_celestialBodyPositionsAu.end())
            {
                hub.motion.centerMeters =
                    parentIt->second *
                    world::celestial::MetersPerAu;
            }
        }

        const glm::dvec3 hubPosMeters =
            world::orbits::computeOrbitPositionMeters(
                hub.motion,
                m_orbitalUniverseTimeSeconds
            );

        const glm::dvec3 localOrbitVelocityMetersPerSecond =
            world::orbits::computeOrbitVelocityMetersPerSecond(
                hub.motion,
                m_orbitalUniverseTimeSeconds
            );

        glm::dvec3 parentVelocityMetersPerSecond {0.0};

        auto parentVelocityIt =
            m_celestialBodyVelocitiesMetersPerSecond.find(
                hub.parentBodyId
            );

        if (parentVelocityIt !=
            m_celestialBodyVelocitiesMetersPerSecond.end())
        {
            parentVelocityMetersPerSecond =
                parentVelocityIt->second;
        }

        const glm::dvec3 hubVelocityMetersPerSecond =
            parentVelocityMetersPerSecond +
            localOrbitVelocityMetersPerSecond;

        m_hubVelocityMetersPerSecond[hubId] =
            hubVelocityMetersPerSecond;

        hub.worldPosition =
            world::coordinates::makeWorldPositionFromMeters(
                hubPosMeters
            );

        hub.orientation =
            glm::mat4(1.0f);
    }

    rebuildHubNavigationFrames(0.0);

    /*
        The Stage 3E live AI probe is a real visible NPC, not a presentation
        ghost. Give it a canonical hub-local production frame before the first
        authoritative update. This is also required by accelerated-universe
        diagnostics: every visible ship must be able to seed the alternate
        trajectory branch from one coherent production epoch.
    */
    if constexpr (game::diagnostics::ActivationCadenceLabEnabled)
    {
        if (m_activationCadenceLabShipId.value != 0)
        {
            game::navigation::ReferenceFrame labFrame;
            labFrame.type =
                game::navigation::ReferenceFrameType::OrbitalHub;
            labFrame.systemId = m_activeCelestialSystemId;
            labFrame.hubId =
                game::diagnostics::ActivationCadenceLabHubId;
            labFrame.localOffsetMeters = glm::dvec3(
                game::diagnostics::ActivationCadenceLabLocalOffsetMeters,
                0.0,
                0.0
            );

            if (!placeShipInReferenceFrame(
                    m_activationCadenceLabShipId,
                    labFrame))
            {
                std::cerr
                    << "[ActivationCadenceLab] failed to place live AI probe "
                    << "in hub reference frame\n";
            }
        }
    }

    // Обновляем объекты, прикреплённые к хабам,
    // чтобы станция уже была в правильной мировой позиции
    // до первого snapshot.
    for (auto& [id, obj] : m_staticObjects)
    {
        if (obj.systemId != m_activeCelestialSystemId ||
            !obj.attachedToHub)
            continue;

        auto hubIt =
            m_orbitalHubs.find(obj.hubId);

        if (hubIt == m_orbitalHubs.end())
            continue;

        const auto& hub =
            hubIt->second;

        const glm::dvec3 hubMeters =
            world::coordinates::fullMeters(
                hub.worldPosition
            );

        const auto* hubFrame =
            hubNavigationFrame(obj.hubId);

        const bool movingGapBoundary =
            game::diagnostics::
                isNavigationRuntimeLabMovingGapBoundary(
                    obj.displayName
                );
        const glm::dvec3 localLinearVelocityVisualMps =
            movingGapBoundary
                ? game::diagnostics::
                    NavigationRuntimeLabMovingGapVelocityVisualMps
                : glm::dvec3(0.0);
        const glm::dvec3 effectiveLocalOffsetMeters =
            obj.hubLocalOffsetMeters +
            localLinearVelocityVisualMps *
                m_serverTimelineClock.timeSeconds();

        const glm::dvec3 rotatedOffset =
            obj.inheritHubOrientation && hubFrame && hubFrame->valid
                ? game::navigation::hubVisualLocalToWorldVector(
                    hubFrame->progradeAxis,
                    hubFrame->radialAxis,
                    hubFrame->normalAxis,
                    effectiveLocalOffsetMeters
                )
                : effectiveLocalOffsetMeters;

        obj.setWorldPositionMeters(
            hubMeters + rotatedOffset
        );

        glm::dvec3 objectVelocityMetersPerSecond {0.0};
        const auto hubVelocityIt =
            m_hubVelocityMetersPerSecond.find(obj.hubId);
        if (hubVelocityIt != m_hubVelocityMetersPerSecond.end())
            objectVelocityMetersPerSecond = hubVelocityIt->second;

        if (hubFrame && hubFrame->valid)
        {
            objectVelocityMetersPerSecond +=
                glm::cross(
                    hubFrame->angularVelocityWorldRadPerSecond,
                    rotatedOffset
                );
            if (movingGapBoundary)
            {
                objectVelocityMetersPerSecond +=
                    game::navigation::hubVisualLocalToWorldVector(
                        hubFrame->progradeAxis,
                        hubFrame->radialAxis,
                        hubFrame->normalAxis,
                        localLinearVelocityVisualMps
                    );
            }
        }
        else if (movingGapBoundary)
        {
            objectVelocityMetersPerSecond +=
                localLinearVelocityVisualMps;
        }
        obj.linearVelocity = glm::vec3(objectVelocityMetersPerSecond);

        if (hubFrame && hubFrame->valid)
        {
            const glm::dvec3 currentLocalRotationDeg =
                obj.hubLocalRotationDeg +
                obj.hubLocalAngularVelocityDegPerSecond *
                    m_orbitalUniverseTimeSeconds;
            const glm::dvec3 objectAngularVelocityWorldRadPerSecond =
                hubFrame->angularVelocityWorldRadPerSecond +
                game::navigation::hubAttachedAngularVelocityWorld(
                    hubFrame->progradeAxis,
                    hubFrame->radialAxis,
                    hubFrame->normalAxis,
                    currentLocalRotationDeg,
                    obj.hubLocalAngularVelocityDegPerSecond
                );
            obj.angularVelocity =
                glm::vec3(objectAngularVelocityWorldRadPerSecond);
        }

        if (obj.inheritHubOrientation)
        {
            if (hubFrame && hubFrame->valid)
            {
                const glm::dvec3 currentLocalRotationDeg =
                    obj.hubLocalRotationDeg +
                    obj.hubLocalAngularVelocityDegPerSecond *
                        m_orbitalUniverseTimeSeconds;
                obj.orientation =
                    game::navigation::hubAttachedVisualOrientation(
                        hubFrame->progradeAxis,
                        hubFrame->radialAxis,
                        hubFrame->normalAxis,
                        currentLocalRotationDeg
                    );
            }
        }
    }

    rebuildNavigationGravityContext();
}






bool GameSimulation::resolveCelestialBodyMeters(
    int systemId,
    const std::string& bodyId,
    glm::dvec3& outCenterMeters,
    double& outRadiusMeters
) const
{
    if (systemId < 0 || systemId != m_activeCelestialSystemId)
        return false;

    auto it = m_celestialBodyPositionsAu.find(bodyId);

    if (it == m_celestialBodyPositionsAu.end())
        return false;

    outCenterMeters =
        it->second * world::celestial::MetersPerAu;

    const auto gravityIt =
        m_celestialBodyGravityParameters.find(bodyId);

    outRadiusMeters =
        gravityIt != m_celestialBodyGravityParameters.end()
        ? gravityIt->second.radiusMeters
        : 0.0;

    return true;
}








bool GameSimulation::resolveCelestialBodyVelocityMetersPerSecond(
    int systemId,
    const std::string& bodyId,
    glm::dvec3& outVelocityMetersPerSecond
) const
{
    if (systemId < 0 || systemId != m_activeCelestialSystemId)
    {
        outVelocityMetersPerSecond = glm::dvec3(0.0);
        return false;
    }

    const auto it =
        m_celestialBodyVelocitiesMetersPerSecond.find(
            bodyId
        );

    if (it ==
        m_celestialBodyVelocitiesMetersPerSecond.end())
    {
        outVelocityMetersPerSecond =
            glm::dvec3(0.0);

        return false;
    }

    outVelocityMetersPerSecond =
        it->second;

    return true;
}







game::navigation::ResolvedFrameState GameSimulation::resolveReferenceFrame(
    const game::navigation::ReferenceFrame& frame
) const
{
    using namespace game::navigation;

    ResolvedFrameState result;

    if (frame.type == ReferenceFrameType::OrbitalHub ||
        frame.type == ReferenceFrameType::HubModule)
    {
        const auto* hubFrame =
            hubNavigationFrame(frame.hubId);

        if (!hubFrame || !hubFrame->valid)
            return result;

        if (frame.systemId >= 0 &&
            frame.systemId != hubFrame->systemId)
        {
            return result;
        }

        result.systemId = hubFrame->systemId;
        result.positionMeters =
            hubFrame->originMeters;

        result.velocityMetersPerSecond =
            hubFrame->velocityMetersPerSecond;

        result.orientation =
            game::navigation::hubVisualOrientation(
                hubFrame->progradeAxis,
                hubFrame->radialAxis,
                hubFrame->normalAxis
            );

        result.positionMeters =
            hubFrame->localToWorldPosition(
                frame.localOffsetMeters
            );

        result.velocityMetersPerSecond =
            hubFrame->localToWorldVelocity(
                frame.localOffsetMeters,
                glm::dvec3(0.0)
            );

        result.valid = true;
        return result;
    }

    return result;
}



bool GameSimulation::placeShipInReferenceFrame(
    EntityId shipId,
    const game::navigation::ReferenceFrame& frame
)
{
    auto it =
        m_ships.find(shipId);

    if (it == m_ships.end())
        return false;

    const auto resolved =
        resolveReferenceFrame(frame);

    if (!resolved.valid)
        return false;

    auto& tr =
        it->second->core().transform();

    tr.setWorldPositionMeters(
        resolved.positionMeters
    );



    const auto* hubFrame =
        hubNavigationFrame(frame.hubId);

    if (hubFrame && hubFrame->valid)
    {
        const glm::vec3 forward =
            glm::normalize(
                glm::vec3(
                    hubFrame->originMeters -
                    resolved.positionMeters
                )
            );

        tr.orientation =
            makePromoLookOrientation(
                forward,
                glm::vec3(hubFrame->radialAxis)
            );
    }





    const glm::vec3 worldVelocity =
        glm::vec3(resolved.velocityMetersPerSecond);




    tr.referenceVelocityMetersPerSecond =
        resolved.velocityMetersPerSecond;

    tr.motion.mode =
        game::navigation::MotionMode::HubTactical;

    tr.motion.systemId = resolved.systemId;

    tr.motion.hubId =
        frame.hubId;

    if (hubFrame && hubFrame->valid)
    {
        const std::string ownedTravelFrameId =
            "ship_travel_" + std::to_string(shipId.value);

        game::navigation::TravelFrameSystem::matchToReference(
            tr.motion,
            hubFrame->kinematicFrame(),
            ownedTravelFrameId,
            hubFrame->hubId
        );
    }

    tr.motion.referenceVelocityMps =
        resolved.velocityMetersPerSecond;

    // TEST SCENARIO:
    // игрок появляется уже с орбитальным вектором хаба.
    // В будущем сюда будет приходить результат навигации/прыжка:
    // speed + direction + error.
    tr.motion.worldVelocityMps =
        resolved.velocityMetersPerSecond;

    // tr.motion.pendingReferenceVelocityMatch = true;

    tr.motion.desiredRelativeVelocityMps =
        glm::dvec3(0.0);

    // Entering a reference frame is an authoritative kinematic reset.
    // World velocity above already includes the resolved frame velocity at
    // this local offset, therefore retaining an old local velocity here would
    // double-count stale pre-placement motion on the next fixed step.
    tr.motion.localVelocityMps =
        glm::dvec3(0.0);
    tr.motion.mainEngineAccelerationMps2 =
        glm::dvec3(0.0);
    tr.motion.manoeuvreAccelerationMps2 =
        glm::dvec3(0.0);
    tr.motion.engineAccelerationMps2 =
        glm::dvec3(0.0);
    tr.motion.desiredTacticalVelocityMps =
        glm::dvec3(0.0);
    tr.motion.velocityAlignmentMode =
        game::navigation::VelocityAlignmentMode::None;
    tr.motion.assistedTargetSpeedHold = false;
    tr.motion.assistedThrottleTrimWasActive = false;

    tr.motion.localPositionMeters =
        frame.localOffsetMeters;

    // ВАЖНО:
    // Игрок не наследует скорость хаба автоматически.
    // Хаб — ориентир, не родительская тележка.
    // TEST SPAWN:
    // игрок появляется уже на той же орбите, что и хаб.
    // Это НЕ автоматическое наследование в механике,
    // а только стартовое условие сценария.
    tr.motion.worldVelocityMps =
        resolved.velocityMetersPerSecond;

    tr.motion.gravityAccelerationMps2 =
        glm::dvec3(0.0);

    tr.motion.primaryGravityBodyId.clear();
    tr.motion.orbitalCorridorId.clear();
    tr.motion.orbitalCorridorState = 0;

    tr.motion.targetForwardSpeedMps = 0.0;
    tr.motion.forwardSpeedMps = 0.0;
    tr.motion.strafeSpeedMps = 0.0;
    tr.motion.liftSpeedMps = 0.0;







    tr.motion.lockedToFramePosition = false;

    tr.forwardVelocity = 0.0f;
    tr.targetSpeed = 0.0f;
    tr.localVelocity = glm::vec3(0.0f);




    m_shipReferenceBindings[shipId] = {
        frame,
        false
    };

    return true;
}


void GameSimulation::updateShipReferenceFrames(double dt)
{
    for (auto& [shipId, binding] : m_shipReferenceBindings)
    {
        Ship* ship = getShip(shipId);
        if (!ship)
            continue;

        const auto resolved =
            resolveReferenceFrame(binding.frame);

        if (!resolved.valid)
            continue;

        auto& tr =
            ship->core().transform();

        // External reference membership is authoritative only while the
        // ship-owned travel frame is explicitly matched to it. A detached
        // travel frame (future J transit) must not be silently pulled back to
        // the hub's system/domain by a stale binding.
        if (tr.motion.matchedToReferenceFrame)
            tr.motion.systemId = resolved.systemId;

        // Обновляем только скорость системы отсчёта.
        // Позицию свободного корабля не телепортируем.
        glm::dvec3 referenceVelocityMetersPerSecond =
            resolved.velocityMetersPerSecond;

        if (tr.motion.mode ==
                game::navigation::MotionMode::HubTactical)
        {
            const auto* hubFrame =
                hubNavigationFrame(tr.motion.hubId);

            if (hubFrame && hubFrame->valid)
            {
                if (tr.motion.travelFrame.valid)
                {
                    game::navigation::TravelFrameSystem::refreshMatchedReference(
                        tr.motion,
                        hubFrame->kinematicFrame(),
                        hubFrame->hubId
                    );
                }
                else
                {
                    game::navigation::TravelFrameSystem::matchToReference(
                        tr.motion,
                        hubFrame->kinematicFrame(),
                        "ship_travel_" + std::to_string(shipId.value),
                        hubFrame->hubId
                    );
                }
            }

            if (tr.motion.travelFrame.valid)
            {
                referenceVelocityMetersPerSecond =
                    tr.motion.travelFrame.localToWorldVelocity(
                        tr.motion.localPositionMeters,
                        glm::dvec3(0.0)
                    );
            }
        }

        tr.referenceVelocityMetersPerSecond =
            referenceVelocityMetersPerSecond;

        tr.motion.referenceVelocityMps =
            referenceVelocityMetersPerSecond;

        // localPositionMeters is authoritative inside a matched travel frame.
        // Re-materialize the world pose at the current frame epoch before any
        // AI/navigation/world-space consumer runs. This follows frame motion
        // without changing the ship's local position or integrating flight.
        if (tr.motion.mode ==
                game::navigation::MotionMode::HubTactical &&
            tr.motion.travelFrame.valid)
        {
            tr.setWorldPositionMeters(
                tr.motion.travelFrame.localToWorldPosition(
                    tr.motion.localPositionMeters
                )
            );

            tr.motion.worldVelocityMps =
                tr.motion.travelFrame.localToWorldVelocity(
                    tr.motion.localPositionMeters,
                    tr.motion.localVelocityMps
                );
        }

if (tr.motion.pendingReferenceVelocityMatch)
{
    const double referenceSpeed =
        glm::length(
            resolved.velocityMetersPerSecond
        );

    // Ждём, пока reference frame получит полноценную мировую скорость.
    // Для хаба Земли это примерно 30 км/с.
    // На первом кадре она может быть только локальной скоростью орбиты.
    if (referenceSpeed > 10000.0)
    {
        tr.motion.worldVelocityMps =
            resolved.velocityMetersPerSecond;

        tr.motion.desiredRelativeVelocityMps =
            glm::dvec3(0.0);

        tr.motion.localVelocityMps =
            glm::dvec3(0.0);

        tr.motion.pendingReferenceVelocityMatch = false;
    }
}








        // Жёсткая позиционная привязка нужна только потом:
        // docked / landed / attached.
        if (binding.lockPositionToFrame)
        {
            tr.setWorldPositionMeters(
                resolved.positionMeters
            );
        }
    }
}








void GameSimulation::rebuildNavigationGravityContext()
{
    m_gravityBodies.clear();
    m_orbitalCorridors.clear();

    if (m_activeCelestialSystemId < 0)
        return;

    /*
        Navigation gravity is derived from the active system context instead of
        one named body. Every body whose catalog supplied a radius and GM may
        participate in the field. A zero influence radius means "do not apply
        an artificial cutoff"; primary-body selection still chooses the
        strongest local acceleration.
    */
    for (const auto& [bodyId, parameters] :
         m_celestialBodyGravityParameters)
    {
        const auto positionIt =
            m_celestialBodyPositionsAu.find(bodyId);

        if (positionIt == m_celestialBodyPositionsAu.end() ||
            parameters.radiusMeters <= 0.0 ||
            parameters.gravitationalParameterM3s2 <= 0.0)
        {
            continue;
        }

        game::navigation::GravityBody body;
        body.id = bodyId;
        body.centerMeters =
            positionIt->second * world::celestial::MetersPerAu;
        body.radiusMeters = parameters.radiusMeters;
        body.gravitationalParameterM3s2 =
            parameters.gravitationalParameterM3s2;

        // Catalog atmosphere outer radius (generated body physics / observed
        // data); airless bodies carry their physical radius.
        body.atmosphereRadiusMeters =
            std::max(body.radiusMeters, parameters.atmosphereOuterRadiusMeters);
        body.influenceRadiusMeters = 0.0;

        m_gravityBodies.push_back(std::move(body));
    }

    /*
        Corridors are runtime navigation policy around authored orbital hubs.
        They are generated for the active system only; a ship from another
        system can therefore never be classified against a foreign hub.
    */
    for (const auto& [hubId, hub] : m_orbitalHubs)
    {
        if (hub.systemId != m_activeCelestialSystemId ||
            !hub.motion.enabled ||
            hub.parentBodyId.empty())
        {
            continue;
        }

        const auto bodyIt =
            std::find_if(
                m_gravityBodies.begin(),
                m_gravityBodies.end(),
                [&](const game::navigation::GravityBody& body)
                {
                    return body.id == hub.parentBodyId;
                }
            );

        if (bodyIt == m_gravityBodies.end())
            continue;

        game::navigation::OrbitalCorridor corridor;
        corridor.id = hub.id + "_corridor";
        corridor.parentBodyId = hub.parentBodyId;
        corridor.hubId = hub.id;
        corridor.targetAltitudeMeters = hub.motion.altitudeMeters;
        corridor.halfWidthMeters = 50000.0;
        corridor.dangerBelowMeters = 50000.0;
        corridor.escapeAboveMeters = 50000.0;

        m_orbitalCorridors.push_back(std::move(corridor));
    }
}


void GameSimulation::updateDynamicNavigationContext(double dt)
{
    (void)dt;

    for (auto& [id, shipPtr] : m_ships)
    {
        if (!shipPtr)
            continue;

        auto& tr =
            shipPtr->core().transform();

        if (tr.motion.systemId != m_activeCelestialSystemId)
            continue;

        const glm::dvec3 positionMeters =
            world::coordinates::fullMeters(
                tr.worldPosition
            );

        const auto gravity =
            game::navigation::GravityFieldSystem::sample(
                positionMeters,
                m_gravityBodies
            );

        tr.motion.gravityAccelerationMps2 =
            gravity.accelerationMps2;






        if (m_diagnostics.settings.gravityCsv)
            debugLogGravitySample(*shipPtr);












        tr.motion.primaryGravityBodyId =
            gravity.primaryBodyId;

        tr.motion.primaryGravityDistanceMeters =
            gravity.primaryDistanceMeters;

        tr.motion.primaryGravityAltitudeMeters =
            gravity.primaryAltitudeMeters;

        tr.motion.primaryGravityAccelerationMps2 =
            gravity.primaryAccelerationMps2;

        tr.motion.orbitalCorridorId.clear();
        tr.motion.orbitalCorridorState = 0;

        tr.motion.orbitalAltitudeMeters = 0.0;
        tr.motion.orbitalAltitudeErrorMeters = 0.0;
        tr.motion.orbitalTargetSpeedMps = 0.0;
        tr.motion.orbitalTangentialSpeedMps = 0.0;
        tr.motion.orbitalRadialSpeedMps = 0.0;
        tr.motion.orbitalSpeedErrorMps = 0.0;

        for (const auto& body : m_gravityBodies)
        {
            if (body.id != gravity.primaryBodyId)
                continue;

            for (const auto& corridor : m_orbitalCorridors)
            {
                if (corridor.parentBodyId != body.id)
                    continue;

                const auto sample =
                    game::navigation::OrbitalCorridorSystem::classify(
                        positionMeters,
                        tr.motion.worldVelocityMps,
                        body,
                        corridor
                    );

                if (!sample.valid)
                    continue;

                tr.motion.orbitalCorridorId =
                    sample.corridorId;

                tr.motion.orbitalCorridorState =
                    static_cast<int>(sample.state);

                tr.motion.orbitalAltitudeMeters =
                    sample.altitudeMeters;

                tr.motion.orbitalAltitudeErrorMeters =
                    sample.altitudeErrorMeters;

                tr.motion.orbitalTargetSpeedMps =
                    sample.targetCircularSpeedMps;

                tr.motion.orbitalTangentialSpeedMps =
                    sample.tangentialSpeedMps;

                tr.motion.orbitalRadialSpeedMps =
                    sample.radialSpeedMps;

                tr.motion.orbitalSpeedErrorMps =
                    sample.speedErrorMps;

                break;
            }

            break;
        }
    }
}
