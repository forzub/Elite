#include <glm/glm.hpp>
#include <cstdlib>
#include <iostream>

#include "game/equipment/signalNode/processing/SignalReceiver.h"
 

#include "render/VisualTuning.h"
#include "world/Planet.h"
#include "world/InterferenceSource.h"


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
    const glm::vec3& receiverPos,
    const ReceiverModule& receiver,
    const std::vector<WorldSignal>& worldSignals,
    const std::vector<Planet>& planets,
    const std::vector<InterferenceSource>& interferenceSources,
    std::vector<SignalReceptionResult>& outResults,
    const Ship* ownerShip
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
        float distance = glm::distance(receiverPos, signal.position);

        // printf(
        //     "[RX] signal from %s dist=%.1f power=%.1f\n",
        //     signal.label.c_str(),
        //     distance,
        //     signal.power
        // );

        // Жёсткая отсечка по максимальной дальности
        if (distance > signal.maxRange)
            continue;

        // === БЛОК 2. Базовое затухание сигнала ===
        // Простая модель: обратный квадрат расстояния
        float attenuation = 1.0f / (distance * distance + 1.0f);
        float receivedPower = signal.power * attenuation;

        // === БЛОК 3. Проверка экранирования планетами ===
        float occlusionFactor = 1.0f;

        for (const Planet& planet : planets)
        {
            if (isOccludedByPlanet(signal.position, receiverPos, planet))
            {
                occlusionFactor = 0.0f;
                break;
            }
        }

        receivedPower *= occlusionFactor;

        // === БЛОК. Активные и пассивные помехи ===
        float interferencePower = 0.0f;
        for (const InterferenceSource& src : interferenceSources)
        {
            // --- активные помехи можно отключить
            if (src.type == InterferenceType::Active && !src.enabled)
                continue;

            float d = glm::distance(receiverPos, src.position);
            if (d > src.radius)
                continue;

            // затухание помех — как у сигнала
            interferencePower += src.power / (d * d + 1.0f);
        }

        // TODO: учёт jammer-объектов как физических источников

        // === БЛОК 5. Шум приёмника ===
        // float noiseFloor = receiverNoiseFloor;

        float effectiveNoiseFloor =
        receiver.isOperational()
        ? VisualTuning::receiverBaseNoise / receiver.sensitivity
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
        
        result.sourceWorldPos = signal.position;
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
    const glm::vec3& receiverPos
) const
{
    float dist = glm::length(sig.position - receiverPos);
    if (dist <= 0.0f)
        return sig.power;

    // простое затухание 1/r^2
    return sig.power / (dist * dist);
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
    glm::vec3 toPlanet = planet.position - signalPos;

    // Проекция на луч
    float projection = glm::dot(toPlanet, rayDir);

    // Планета "позади" источника или дальше приёмника
    if (projection < 0.0f || projection > rayLength)
        return false;

    // Кратчайшее расстояние от центра планеты до луча
    glm::vec3 closestPoint = signalPos + rayDir * projection;
    float distanceToPlanet = glm::distance(closestPoint, planet.position);

    // Если луч проходит через сферу — экранирование
    return distanceToPlanet < planet.radius;
}