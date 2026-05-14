#include "CockpitIndicatorRenderer.h"

static void drawRect(
    CockpitGeometry& geom,
    glm::vec2 pos01,
    glm::vec2 size01,
    glm::vec4 color
) {
    CockpitPolygon poly;
    poly.fillType = CockpitFillType::Solid;
    poly.colorA = color;

    poly.contour01 = {
        pos01,
        {pos01.x + size01.x, pos01.y},
        {pos01.x + size01.x, pos01.y + size01.y},
        {pos01.x, pos01.y + size01.y}
    };

    geom.drawList.push_back({
        .type = CockpitDrawType::Fill,
        .polygon = poly
    });
}


static void drawBarIndicator(
    const CockpitIndicatorLayout& l,
    CockpitGeometry& geom
) {
    // фон
    drawRect(geom, l.pos01, l.size01, {0.15f, 0.15f, 0.15f, 1.0f});

    // рамка
    CockpitStroke stroke;
    stroke.color = {0.6f, 0.6f, 0.6f, 1.0f};
    stroke.thickness = 1.0f;

    stroke.path01 = {
        l.pos01,
        {l.pos01.x + l.size01.x, l.pos01.y},
        {l.pos01.x + l.size01.x, l.pos01.y + l.size01.y},
        {l.pos01.x, l.pos01.y + l.size01.y},
        l.pos01
    };

    geom.drawList.push_back({
        .type = CockpitDrawType::Stroke,
        .stroke = stroke
    });
}


static void drawSegmentArray(
    const CockpitIndicatorLayout& l,
    CockpitGeometry& geom
) {
    if (l.segments <= 0) return;

    float gap = 0.005f;
    float segW = (l.size01.x - gap * (l.segments - 1)) / l.segments;

    for (int i = 0; i < l.segments; ++i) {
        glm::vec2 p = {
            l.pos01.x + i * (segW + gap),
            l.pos01.y
        };

        drawRect(
            geom,
            p,
            {segW, l.size01.y},
            {0.12f, 0.12f, 0.12f, 1.0f}
        );
    }
}


void CockpitIndicatorRenderer::buildIndicatorGeometry(
    const CockpitIndicatorLayout& layout,
    CockpitGeometry& geom
) {
    switch (layout.visualType) {
        case IndicatorVisualType::Bar:
            drawBarIndicator(layout, geom);
            break;

        case IndicatorVisualType::SegmentArray:
            drawSegmentArray(layout, geom);
            break;

        default:
            break;
    }
}
