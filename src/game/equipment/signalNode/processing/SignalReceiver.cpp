#include <glm/glm.hpp>
#include <cstdlib>
#include <iostream>

#include "game/equipment/signalNode/processing/SignalReceiver.h"
 

#include "render/VisualTuning.h"
#include "world/Planet.h"
#include "world/InterferenceSource.h"
#include "src/scene/EntityID.h"

#include "src/world/coordinates/WorldPosition.h"


static float rand01()
{
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}





static bool isDirectionOnly(SignalType t)
{
    return (t == SignalType::SOSAntic ||
            t == SignalType::Unknown);
}




//                       ###              ##
//                        ##              ##
//  ##  ##   ######       ##    ####     #####    ####
//  ##  ##    ##  ##   #####       ##     ##     ##  ##
//  ##  ##    ##  ##  ##  ##    #####     ##     ######
//  ##  ##    #####   ##  ##   ##  ##     ## ##  ##
//   ######   ##       ######   #####      ###    #####
//           ####


void SignalReceiver::update(
    float dt,
    const world::coordinates::WorldPosition& receiverWorldPosition,
    const game::ReceiverModule& receiver,
    const std::vector<WorldSignal>& worldSignals,
    const std::vector<Planet>& planets,
    const std::vector<InterferenceSource>& interferenceSources,
    std::vector<SignalReceptionResult>& outResults,
    const EntityId ownerShip
)
{
       
    outResults.clear();

    // printf(
    //     "[RX] update: worldSignals=%zu receiver.enabled=%d pos=(%.1f %.1f %.1f)\n",
    //     worldSignals.size(),
    //     receiver.enabled,
    //     receiverPos.x, receiverPos.y, receiverPos.z
    // );

    // === БЛОК 1. Обход всех источников сигналов ===
    for (const WorldSignal& signal : worldSignals)
    {


        if (signal.owner == ownerShip)
            continue;

        if (!signal.enabled)
            continue;

        // --- Геометрия ---
        const glm::dvec3 signalDeltaD =
            world::coordinates::relativeMeters(
                signal.worldPosition,
                receiverWorldPosition
            );

        const float distance =
            static_cast<float>(glm::length(signalDeltaD));

        const glm::vec3 signalLocalToReceiver =
            glm::vec3(signalDeltaD);

      
            

        // Жёсткая отсечка по максимальной дальности
        if (distance > signal.maxRange)
            continue;

        // === БЛОК 2. Базовое затухание сигнала ===
        // Простая модель: обратный квадрат расстояния
        float attenuation = 1.0f / (distance * distance + 1.0f);
        float receivedPower = signal.power * attenuation;

        // === БЛОК 3. Проверка экранирования планетами ===
        float occlusionFactor = 1.0f;

        // TODO double-coordinates:
        // Planet пока хранит glm::vec3 position. Не смешиваем его с WorldSignal.
        // Когда Planet получит WorldPosition, вернём точную проверку occlusion.
        for (const Planet& planet : planets)
        {
            (void)planet;
        }

        receivedPower *= occlusionFactor;

        // === БЛОК. Активные и пассивные помехи ===
        float interferencePower = 0.0f;

        // TODO double-coordinates:
        // InterferenceSource пока хранит legacy glm::vec3.
        // Чтобы не смешивать координатные системы, временно не учитываем помехи.
        (void)interferenceSources;

        // TODO: учёт jammer-объектов как физических источников

        // === БЛОК 5. Шум приёмника ===
        // float noiseFloor = receiverNoiseFloor;

        float effectiveNoiseFloor =
        receiver.isOperational()
        ? VisualTuning::receiverBaseNoise / receiver.sensitivity()
        : VisualTuning::receiverBaseNoise * 10.0f;

        float totalNoise = effectiveNoiseFloor + interferencePower;


        // === БЛОК 6. SNR и устойчивость ===
        float snr = 0.0f;
        if (totalNoise > 0.0f)
            snr = receivedPower / totalNoise;

        float stability = glm::clamp(
            snr / VisualTuning::snrStableThreshold,
            0.0f,
            1.0f
        );



        // === БЛОК 7. Семантическое состояние ===
        SignalSemanticState semanticState = SignalSemanticState::None;

        if (receivedPower <= totalNoise)
        {
            semanticState = SignalSemanticState::None;
        }
        else if (snr < VisualTuning::snrDecodeThreshold)
        {
            semanticState = SignalSemanticState::Noise;
        }
        else
        {
            semanticState = SignalSemanticState::Decoded;
        }


        // === БЛОК 8. Формирование результата ===
        SignalReceptionResult result;
        result.source = &signal;
        result.owner = signal.owner;
        
        result.sourceWorldPosition = signal.worldPosition;

        // Legacy mirror: позиция источника относительно приёмника.
        // Для старого HUD это безопаснее, чем глобальный float.
        result.sourceWorldPos = signalLocalToReceiver;

        result.distance = distance;

        result.emittedPower = signal.power;
        result.receivedPower = receivedPower;

        result.noiseFloor = effectiveNoiseFloor;
        result.interferencePower = interferencePower;

        result.occlusionFactor = occlusionFactor;

        result.signalToNoiseRatio = snr;
        result.stability = stability;

        result.semanticState = semanticState;


        outResults.push_back(result);

     
    }
    
}








// ============================================================================================
//     detected
// ============================================================================================

const std::vector<DetectedSignal>& SignalReceiver::detected() const
{
    return m_detected;
}








// ============================================================================================
//     computeSignalStrength
// ============================================================================================
float SignalReceiver::computeSignalStrength(
    const WorldSignal& sig,
    const world::coordinates::WorldPosition& receiverWorldPosition
) const
{
    const glm::dvec3 deltaD =
        world::coordinates::relativeMeters(
            sig.worldPosition,
            receiverWorldPosition
        );

    const double dist = glm::length(deltaD);

    if (dist <= 0.0)
        return sig.power;

    return static_cast<float>(
        sig.power / (dist * dist)
    );
}








// ============================================================================================
//     isOccludedByPlanet
// ============================================================================================

bool SignalReceiver::isOccludedByPlanet(
    const glm::vec3& signalPos,
    const glm::vec3& receiverPos,
    const Planet& planet)
{
    // Направление луча
    glm::vec3 rayDir = receiverPos - signalPos;
    float rayLength = glm::length(rayDir);
    rayDir = glm::normalize(rayDir);

    // Вектор от источника сигнала к центру планеты
    const world::coordinates::WorldPosition signalWorld =
        world::coordinates::makeWorldPositionFromMeters(
            glm::dvec3(signalPos)
        );

    glm::vec3 toPlanet =
        world::coordinates::relativeMetersFloat(
            planet.worldPosition,
            signalWorld
        );

    // Проекция на луч
    float projection = glm::dot(toPlanet, rayDir);

    // Планета "позади" источника или дальше приёмника
    if (projection < 0.0f || projection > rayLength)
        return false;

    // Кратчайшее расстояние от центра планеты до луча
    glm::vec3 closestPoint = signalPos + rayDir * projection;
        const world::coordinates::WorldPosition closestPointWorld =
        world::coordinates::translated(
            signalWorld,
            glm::dvec3(closestPoint - signalPos)
        );

    float distanceToPlanet =
        glm::length(
            world::coordinates::relativeMetersFloat(
                planet.worldPosition,
                closestPointWorld
            )
        );

    // Если луч проходит через сферу — экранирование
    return distanceToPlanet < planet.radius;
}