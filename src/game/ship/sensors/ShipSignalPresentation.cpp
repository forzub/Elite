#include "ShipSignalPresentation.h"

#include "game/signals/WorldLabelSystem.h"
#include "game/signals/WorldSignalWaves.h"




WorldLabel& ShipSignalPresentation::getOrCreateLabel(
    const SignalReceptionResult& result)
{
    const WorldSignal* key = result.source;

    auto it = m_labels.find(key);
    if (it != m_labels.end())
        return it->second;

    WorldLabel label;
    label.visual.presence = SignalPresence::Absent;
    label.visual.visibility = 0.0f;
    label.visual.presenceTimer = 0.0f;
    label.visual.noisePhase = 0.0f;

    auto [newIt, _] = m_labels.emplace(key, std::move(label));
    return newIt->second;
}





void ShipSignalPresentation::update(
    float dt,
    const std::vector<SignalReceptionResult>& signalResults)
{
    for (const SignalReceptionResult& result : signalResults)
    {
        WorldLabel& label = getOrCreateLabel(result);

        // --- DATA ---
        label.data.worldPos           = result.sourceWorldPos;
        label.data.semanticState      = result.semanticState;
        label.data.signalToNoiseRatio = result.signalToNoiseRatio;
        label.data.receivedPower      = result.receivedPower;
        label.data.stability          = result.stability;
        label.data.displayClass       = result.source->displayClass;

        if (result.semanticState == SignalSemanticState::Decoded ||
            label.data.displayClass == SignalDisplayClass::Global)
        {
            label.data.displayName  = result.source->label;
            label.data.distance     = result.distance;
            label.data.hasDistance  = true;
            label.visual.presence   = SignalPresence::Present;
        }
        else
        {
            label.data.displayName = "undefined";
            label.data.hasDistance = false;
        }

        // --- VISUAL INERTIA ---
        WorldLabelSystem::updateVisualState(
            dt,
            result.semanticState,
            result.signalToNoiseRatio,
            label.visual
        );

        bool allowWaveSpawn =
            ((label.data.semanticState == SignalSemanticState::Noise) &&
             (label.data.displayClass != SignalDisplayClass::Global) &&
             (label.visual.presence != SignalPresence::FadingOut) &&
             (label.visual.presence != SignalPresence::Absent));

        if (label.data.semanticState == SignalSemanticState::Decoded &&
            label.data.displayClass == SignalDisplayClass::Other)
        {
            allowWaveSpawn = true;
        }

        updateWorldSignalWaves(label.visual, dt, allowWaveSpawn);
    }
}



std::unordered_map<const WorldSignal*, WorldLabel>&
ShipSignalPresentation::labels()
{
    return m_labels;
}

const std::unordered_map<const WorldSignal*, WorldLabel>&
ShipSignalPresentation::labels() const
{
    return m_labels;
}
