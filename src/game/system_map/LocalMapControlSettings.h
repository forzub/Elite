#pragma once

namespace game::system_map
{

struct LocalMapControlSettings
{
    double rotateSensitivity = 0.008;
    double rotateSensitivityScale = 1.0;

    double zoomStep = 1.08;
    double minZoom = 0.15;
    double maxZoom = 16.0;
    double spatialVolumeMinimumZoom = 1.40;

    double clickMoveThresholdPx = 8.0;

    bool constrainPitch = false;
    double minimumPitchRad = 0.12;
    double maximumPitchRad = 1.20;

    double panLimitViewportFractionX = 0.55;
    double panLimitViewportFractionY = 0.45;

    double pivotPickMinimumRadiusPx = 18.0;
    double pivotPickMaximumRadiusPx = 140.0;
    double pivotPickMarginPx = 80.0;
    double pivotPriorityBiasPx = 18.0;
};

} // namespace game::system_map
