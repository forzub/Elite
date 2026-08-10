#pragma once

#include <algorithm>
#include <string>

#include <nlohmann/json.hpp>

#include "src/debug/DebugSettings.h"

namespace game::debug_control
{
using json = nlohmann::json;

inline json vec4ToJson(const glm::vec4& value)
{
    return json::array({value.x, value.y, value.z, value.w});
}

inline glm::vec4 jsonToVec4(const json& value, const glm::vec4& fallback)
{
    if (!value.is_array() || value.size() < 4)
        return fallback;

    return glm::vec4(
        value[0].get<float>(),
        value[1].get<float>(),
        value[2].get<float>(),
        value[3].get<float>()
    );
}

inline json encodeRenderSettings(const ::debug::DebugRenderSettings& dbg)
{
    json payload;

    payload["drawMeshes"] = dbg.drawMeshes;
    payload["renderCockpit"] = dbg.renderCockpit;
    payload["renderShipUi"] = dbg.renderShipUi;
    payload["renderStarfield"] = dbg.renderStarfield;
    payload["renderRearCamera"] = dbg.renderRearCamera;
    payload["showStarLabels"] = dbg.showStarLabels;
    payload["showAllStarLabels"] = dbg.showAllStarLabels;

    payload["renderPlayerShip"] = dbg.renderPlayerShip;
    payload["renderNpcShips"] = dbg.renderNpcShips;
    payload["renderTrafficShips"] = dbg.renderTrafficShips;
    payload["renderRealShips"] = dbg.renderRealShips;
    payload["renderVisualShips"] = dbg.renderVisualShips;
    payload["renderHubs"] = dbg.renderHubs;
    payload["renderLargeObjects"] = dbg.renderLargeObjects;
    payload["renderCelestialBodies"] = dbg.renderCelestialBodies;
    payload["renderSystemMapObjects"] = dbg.renderSystemMapObjects;

    payload["postProcessEnabled"] = dbg.postProcessEnabled;
    payload["postBloomThreshold"] = dbg.postBloomThreshold;
    payload["postBloomKnee"] = dbg.postBloomKnee;
    payload["postBloomIntensity"] = dbg.postBloomIntensity;
    payload["postSoftening"] = dbg.postSoftening;
    payload["postSaturation"] = dbg.postSaturation;
    payload["postContrast"] = dbg.postContrast;
    payload["postVignette"] = dbg.postVignette;
    payload["postGrain"] = dbg.postGrain;
    payload["postHaze"] = dbg.postHaze;

    payload["cloudSpeedMultiplier"] = dbg.cloudSpeedMultiplier;
    payload["debugControlAutoUpdates"] = dbg.debugControlAutoUpdates;
    payload["systemMapLiveRefreshSec"] = dbg.systemMapLiveRefreshSec;
    payload["sceneMode"] = dbg.sceneMode;
    payload["showConstellationHover"] = dbg.showConstellationHover;
    payload["constellationHoverRadiusPx"] = dbg.constellationHoverRadiusPx;

    payload["drawAxes"] = dbg.drawAxes;
    payload["drawWorldAxes"] = dbg.drawWorldAxes;
    payload["drawObjectAxes"] = dbg.drawObjectAxes;
    payload["drawModulePivots"] = dbg.drawModulePivots;
    payload["drawHitVolumes"] = dbg.drawHitVolumes;
    payload["hitVolumeFilterMode"] = dbg.hitVolumeFilterMode;
    payload["publishObjectOrientation"] = dbg.publishObjectOrientation;
    payload["hitVolumesOverlay"] = dbg.hitVolumesOverlay;
    payload["hideMeshesWhenDrawingHitVolumes"] = dbg.hideMeshesWhenDrawingHitVolumes;

    payload["worldAxisLength"] = dbg.worldAxisLength;
    payload["shipAxisLength"] = dbg.shipAxisLength;
    payload["objectAxisLength"] = dbg.objectAxisLength;
    payload["moduleAxisLength"] = dbg.moduleAxisLength;
    payload["moduleCrossSize"] = dbg.moduleCrossSize;
    payload["rotAxisLength"] = dbg.rotAxisLength;

    payload["axesOverlay"] = dbg.axesOverlay;
    payload["crossesOverlay"] = dbg.crossesOverlay;
    payload["linesOverlay"] = dbg.linesOverlay;

    payload["axisXColor"] = vec4ToJson(dbg.axisXColor);
    payload["axisYColor"] = vec4ToJson(dbg.axisYColor);
    payload["axisZColor"] = vec4ToJson(dbg.axisZColor);
    payload["moduleOriginColor"] = vec4ToJson(dbg.moduleOriginColor);
    payload["modulePivotColor"] = vec4ToJson(dbg.modulePivotColor);
    payload["rotationAxisColor"] = vec4ToJson(dbg.rotationAxisColor);

    return payload;
}

inline void applyRenderSettings(
    ::debug::DebugRenderSettings& dbg,
    const json& payload
)
{
    dbg.drawMeshes = payload.value("drawMeshes", dbg.drawMeshes);
    dbg.renderCockpit = payload.value("renderCockpit", dbg.renderCockpit);
    dbg.renderShipUi = payload.value("renderShipUi", dbg.renderShipUi);
    dbg.renderStarfield = payload.value("renderStarfield", dbg.renderStarfield);
    dbg.renderRearCamera = payload.value("renderRearCamera", dbg.renderRearCamera);
    dbg.showStarLabels = payload.value("showStarLabels", dbg.showStarLabels);
    dbg.showAllStarLabels = payload.value("showAllStarLabels", dbg.showAllStarLabels);

    dbg.renderPlayerShip = payload.value("renderPlayerShip", dbg.renderPlayerShip);
    dbg.renderNpcShips = payload.value("renderNpcShips", dbg.renderNpcShips);
    dbg.renderTrafficShips = payload.value("renderTrafficShips", dbg.renderTrafficShips);
    dbg.renderRealShips = payload.value("renderRealShips", dbg.renderRealShips);
    dbg.renderVisualShips = payload.value("renderVisualShips", dbg.renderVisualShips);
    dbg.renderHubs = payload.value("renderHubs", dbg.renderHubs);
    dbg.renderLargeObjects = payload.value("renderLargeObjects", dbg.renderLargeObjects);
    dbg.renderCelestialBodies = payload.value("renderCelestialBodies", dbg.renderCelestialBodies);
    dbg.renderSystemMapObjects = payload.value("renderSystemMapObjects", dbg.renderSystemMapObjects);

    dbg.postProcessEnabled = payload.value("postProcessEnabled", dbg.postProcessEnabled);
    dbg.postBloomThreshold = payload.value("postBloomThreshold", dbg.postBloomThreshold);
    dbg.postBloomKnee = payload.value("postBloomKnee", dbg.postBloomKnee);
    dbg.postBloomIntensity = payload.value("postBloomIntensity", dbg.postBloomIntensity);
    dbg.postSoftening = payload.value("postSoftening", dbg.postSoftening);
    dbg.postSaturation = payload.value("postSaturation", dbg.postSaturation);
    dbg.postContrast = payload.value("postContrast", dbg.postContrast);
    dbg.postVignette = payload.value("postVignette", dbg.postVignette);
    dbg.postGrain = payload.value("postGrain", dbg.postGrain);
    dbg.postHaze = payload.value("postHaze", dbg.postHaze);
    dbg.cloudSpeedMultiplier = payload.value("cloudSpeedMultiplier", dbg.cloudSpeedMultiplier);

    dbg.postBloomThreshold = std::clamp(dbg.postBloomThreshold, 0.0f, 2.0f);
    dbg.postBloomKnee = std::clamp(dbg.postBloomKnee, 0.001f, 1.0f);
    dbg.postBloomIntensity = std::clamp(dbg.postBloomIntensity, 0.0f, 2.0f);
    dbg.postSoftening = std::clamp(dbg.postSoftening, 0.0f, 1.0f);
    dbg.postSaturation = std::clamp(dbg.postSaturation, 0.0f, 2.0f);
    dbg.postContrast = std::clamp(dbg.postContrast, 0.5f, 2.0f);
    dbg.postVignette = std::clamp(dbg.postVignette, 0.0f, 1.0f);
    dbg.postGrain = std::clamp(dbg.postGrain, 0.0f, 0.10f);
    dbg.postHaze = std::clamp(dbg.postHaze, 0.0f, 1.0f);
    dbg.cloudSpeedMultiplier = std::clamp(dbg.cloudSpeedMultiplier, 0.0f, 100000.0f);

    dbg.debugControlAutoUpdates = payload.value(
        "debugControlAutoUpdates",
        dbg.debugControlAutoUpdates
    );
    dbg.systemMapLiveRefreshSec = payload.value(
        "systemMapLiveRefreshSec",
        dbg.systemMapLiveRefreshSec
    );
    dbg.systemMapLiveRefreshSec = std::clamp(
        dbg.systemMapLiveRefreshSec,
        0.02f,
        5.0f
    );

    dbg.sceneMode = payload.value("sceneMode", dbg.sceneMode);
    if (dbg.sceneMode != "game" && dbg.sceneMode != "promo1")
        dbg.sceneMode = "game";

    dbg.showConstellationHover = payload.value(
        "showConstellationHover",
        dbg.showConstellationHover
    );
    dbg.constellationHoverRadiusPx = payload.value(
        "constellationHoverRadiusPx",
        dbg.constellationHoverRadiusPx
    );

    dbg.drawAxes = payload.value("drawAxes", dbg.drawAxes);
    dbg.drawWorldAxes = payload.value("drawWorldAxes", dbg.drawWorldAxes);
    dbg.drawObjectAxes = payload.value("drawObjectAxes", dbg.drawObjectAxes);
    dbg.drawModulePivots = payload.value("drawModulePivots", dbg.drawModulePivots);
    dbg.drawHitVolumes = payload.value("drawHitVolumes", dbg.drawHitVolumes);
    dbg.hitVolumeFilterMode = payload.value("hitVolumeFilterMode", dbg.hitVolumeFilterMode);
    dbg.publishObjectOrientation = payload.value(
        "publishObjectOrientation",
        dbg.publishObjectOrientation
    );
    dbg.hitVolumesOverlay = payload.value("hitVolumesOverlay", dbg.hitVolumesOverlay);
    dbg.hideMeshesWhenDrawingHitVolumes = payload.value(
        "hideMeshesWhenDrawingHitVolumes",
        dbg.hideMeshesWhenDrawingHitVolumes
    );

    dbg.worldAxisLength = payload.value("worldAxisLength", dbg.worldAxisLength);
    dbg.shipAxisLength = payload.value("shipAxisLength", dbg.shipAxisLength);
    dbg.objectAxisLength = payload.value("objectAxisLength", dbg.objectAxisLength);
    dbg.moduleAxisLength = payload.value("moduleAxisLength", dbg.moduleAxisLength);
    dbg.moduleCrossSize = payload.value("moduleCrossSize", dbg.moduleCrossSize);
    dbg.rotAxisLength = payload.value("rotAxisLength", dbg.rotAxisLength);

    dbg.axesOverlay = payload.value("axesOverlay", dbg.axesOverlay);
    dbg.crossesOverlay = payload.value("crossesOverlay", dbg.crossesOverlay);
    dbg.linesOverlay = payload.value("linesOverlay", dbg.linesOverlay);

    if (payload.contains("axisXColor"))
        dbg.axisXColor = jsonToVec4(payload["axisXColor"], dbg.axisXColor);
    if (payload.contains("axisYColor"))
        dbg.axisYColor = jsonToVec4(payload["axisYColor"], dbg.axisYColor);
    if (payload.contains("axisZColor"))
        dbg.axisZColor = jsonToVec4(payload["axisZColor"], dbg.axisZColor);
    if (payload.contains("moduleOriginColor"))
        dbg.moduleOriginColor = jsonToVec4(payload["moduleOriginColor"], dbg.moduleOriginColor);
    if (payload.contains("modulePivotColor"))
        dbg.modulePivotColor = jsonToVec4(payload["modulePivotColor"], dbg.modulePivotColor);
    if (payload.contains("rotationAxisColor"))
        dbg.rotationAxisColor = jsonToVec4(payload["rotationAxisColor"], dbg.rotationAxisColor);
}

} // namespace game::debug_control
