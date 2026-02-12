#pragma once
#include "CockpitIndicators.h"
#include "ui/cockpit/CockpitGeometry.h"

class CockpitIndicatorRenderer {
public:
    static void buildIndicatorGeometry(
        const CockpitIndicatorLayout& layout,
        CockpitGeometry& geom
    );
};
