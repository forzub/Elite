#include "EliteCobraMk1.h"
#include "game/ship/cockpit/CockpitContours.h"



std::vector<glm::vec2> calculateSlotRect(
    int index,
    int maxSlots,
    glm::vec2 areaMin,
    glm::vec2 areaMax
)
{
    float padding = 0.005f;

    float width = areaMax.x - areaMin.x;
    float height = areaMax.y - areaMin.y;

    float slotWidth = (width - padding * (maxSlots + 1)) / maxSlots;
    float slotHeight = height - 2.0f * padding;

    float x0 = areaMin.x + padding + index * (slotWidth + padding);
    float y0 = areaMin.y + padding;

    float x1 = x0 + slotWidth;
    float y1 = y0 + slotHeight;

    return {
        {x0, y0},
        {x1, y0},
        {x1, y1},
        {x0, y1}
    };
}






CockpitGeometry EliteCobraMk1::createCockpitGeometry(const ShipDescriptor& desc)
{
    CockpitGeometry geo;

    glm::vec4 colorA = {0.02f, 0.02f, 0.04f, 1.0f};
    glm::vec4 colorB = {0.04f, 0.04f, 0.09f, 1.0f};
    glm::vec2 gradFrom = {0.0f, 0.0f};
    glm::vec2 gradTo = {1.0f, 1.0f};
    
    float defStroke = 0.003f;
    glm::vec4 colorS = {0.31f, 0.31f, 0.36f, 1.0f};

    // Автоматически сгенерированный код для CockpitDraw
// Screen resolution: 2560x1440
// Generated: 11.02.2026, 19:00:35

// ============================================

    CockpitDrawCommand SHLD_PAN;
    SHLD_PAN.type = CockpitDrawType::Fill;
    SHLD_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.015625f, 0.961806f},
            {0.180605f, 0.961806f},
            {0.180605f, 0.979167f},
            {0.015625f, 0.979167f},
            {0.015625f, 0.961806f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SHLD_PAN);

    CockpitDrawCommand SHLD_EDGE;
    SHLD_EDGE.type = CockpitDrawType::Stroke;
    SHLD_EDGE.stroke = {
        {
            {0.015625f, 0.961806f},
            {0.180605f, 0.961806f},
            {0.180605f, 0.979167f},
            {0.015625f, 0.979167f},
            {0.015625f, 0.961806f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SHLD_EDGE);

    CockpitDrawCommand SPEED_PAN;
    SPEED_PAN.type = CockpitDrawType::Fill;
    SPEED_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.191406f, 0.913194f},
            {0.308594f, 0.913194f},
            {0.308594f, 0.930556f},
            {0.191406f, 0.930556f},
            {0.191406f, 0.913194f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SPEED_PAN);

    CockpitDrawCommand SPEED_EDGE;
    SPEED_EDGE.type = CockpitDrawType::Stroke;
    SPEED_EDGE.stroke = {
        {
            {0.191406f, 0.913194f},
            {0.308594f, 0.913194f},
            {0.308594f, 0.930556f},
            {0.191406f, 0.930556f},
            {0.191406f, 0.913194f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SPEED_EDGE);

    CockpitDrawCommand LASER_PAN;
    LASER_PAN.type = CockpitDrawType::Fill;
    LASER_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.191406f, 0.864583f},
            {0.308594f, 0.864583f},
            {0.308594f, 0.881944f},
            {0.191406f, 0.881944f},
            {0.191406f, 0.864583f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(LASER_PAN);

    CockpitDrawCommand LASER_EDGE;
    LASER_EDGE.type = CockpitDrawType::Stroke;
    LASER_EDGE.stroke = {
        {
            {0.191406f, 0.864583f},
            {0.308594f, 0.864583f},
            {0.308594f, 0.881944f},
            {0.191406f, 0.881944f},
            {0.191406f, 0.864583f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(LASER_EDGE);

    CockpitDrawCommand FUIL_PAN;
    FUIL_PAN.type = CockpitDrawType::Fill;
    FUIL_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.015625f, 0.913194f},
            {0.180605f, 0.913194f},
            {0.180605f, 0.930556f},
            {0.015625f, 0.930556f},
            {0.015625f, 0.913194f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(FUIL_PAN);

    CockpitDrawCommand FUIL_EDGE;
    FUIL_EDGE.type = CockpitDrawType::Stroke;
    FUIL_EDGE.stroke = {
        {
            {0.015625f, 0.913194f},
            {0.180605f, 0.913194f},
            {0.180605f, 0.930556f},
            {0.015625f, 0.930556f},
            {0.015625f, 0.913194f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(FUIL_EDGE);

    CockpitDrawCommand RAD_PAN;
    RAD_PAN.type = CockpitDrawType::Fill;
    RAD_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.015625f, 0.864583f},
            {0.180605f, 0.864583f},
            {0.180605f, 0.881944f},
            {0.015625f, 0.881944f},
            {0.015625f, 0.864583f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(RAD_PAN);

    CockpitDrawCommand RAD_EDGE;
    RAD_EDGE.type = CockpitDrawType::Stroke;
    RAD_EDGE.stroke = {
        {
            {0.015625f, 0.864583f},
            {0.180605f, 0.864583f},
            {0.180605f, 0.881944f},
            {0.015625f, 0.881944f},
            {0.015625f, 0.864583f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(RAD_EDGE);

    CockpitDrawCommand MIS_PAN;
    MIS_PAN.type = CockpitDrawType::Fill;
    MIS_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.191406f, 0.944444f},
            {0.308594f, 0.944444f},
            {0.308594f, 0.979167f},
            {0.191406f, 0.979167f},
            {0.191406f, 0.944444f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MIS_PAN);

    CockpitDrawCommand MIS_EDGE;
    MIS_EDGE.type = CockpitDrawType::Stroke;
    MIS_EDGE.stroke = {
        {
            {0.191406f, 0.944444f},
            {0.308594f, 0.944444f},
            {0.308594f, 0.979167f},
            {0.191406f, 0.979167f},
            {0.191406f, 0.944444f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MIS_EDGE);

    CockpitDrawCommand MSLOT1_PAN;
    MSLOT1_PAN.type = CockpitDrawType::Fill;
    MSLOT1_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.194531f, 0.950000f},
            {0.208203f, 0.950000f},
            {0.208203f, 0.974306f},
            {0.194531f, 0.974306f},
            {0.194531f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MSLOT1_PAN);

    CockpitDrawCommand MSLOT1_EDGE;
    MSLOT1_EDGE.type = CockpitDrawType::Stroke;
    MSLOT1_EDGE.stroke = {
        {
            {0.194531f, 0.950000f},
            {0.208203f, 0.950000f},
            {0.208203f, 0.974306f},
            {0.194531f, 0.974306f},
            {0.194531f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MSLOT1_EDGE);

    CockpitDrawCommand MSLOT2_PAN;
    MSLOT2_PAN.type = CockpitDrawType::Fill;
    MSLOT2_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.211328f, 0.950000f},
            {0.225000f, 0.950000f},
            {0.225000f, 0.974306f},
            {0.211328f, 0.974306f},
            {0.211328f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MSLOT2_PAN);

    CockpitDrawCommand MSLOT2_EDGE;
    MSLOT2_EDGE.type = CockpitDrawType::Stroke;
    MSLOT2_EDGE.stroke = {
        {
            {0.211328f, 0.950000f},
            {0.225000f, 0.950000f},
            {0.225000f, 0.974306f},
            {0.211328f, 0.974306f},
            {0.211328f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MSLOT2_EDGE);

    CockpitDrawCommand MSLOT3_PAN;
    MSLOT3_PAN.type = CockpitDrawType::Fill;
    MSLOT3_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.228125f, 0.950000f},
            {0.241797f, 0.950000f},
            {0.241797f, 0.974306f},
            {0.228125f, 0.974306f},
            {0.228125f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MSLOT3_PAN);

    CockpitDrawCommand MSLOT3_EDGE;
    MSLOT3_EDGE.type = CockpitDrawType::Stroke;
    MSLOT3_EDGE.stroke = {
        {
            {0.228125f, 0.950000f},
            {0.241797f, 0.950000f},
            {0.241797f, 0.974306f},
            {0.228125f, 0.974306f},
            {0.228125f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MSLOT3_EDGE);

    CockpitDrawCommand MSLOT4_PAN;
    MSLOT4_PAN.type = CockpitDrawType::Fill;
    MSLOT4_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.244922f, 0.950000f},
            {0.258594f, 0.950000f},
            {0.258594f, 0.974306f},
            {0.244922f, 0.974306f},
            {0.244922f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MSLOT4_PAN);

    CockpitDrawCommand MSLOT4_EDGE;
    MSLOT4_EDGE.type = CockpitDrawType::Stroke;
    MSLOT4_EDGE.stroke = {
        {
            {0.244922f, 0.950000f},
            {0.258594f, 0.950000f},
            {0.258594f, 0.974306f},
            {0.244922f, 0.974306f},
            {0.244922f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MSLOT4_EDGE);

    CockpitDrawCommand MLAB_PAN;
    MLAB_PAN.type = CockpitDrawType::Fill;
    MLAB_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.261719f, 0.950000f},
            {0.305469f, 0.950000f},
            {0.305469f, 0.974306f},
            {0.261719f, 0.974306f},
            {0.261719f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MLAB_PAN);

    CockpitDrawCommand MLAB_EDGE;
    MLAB_EDGE.type = CockpitDrawType::Stroke;
    MLAB_EDGE.stroke = {
        {
            {0.261719f, 0.950000f},
            {0.305469f, 0.950000f},
            {0.305469f, 0.974306f},
            {0.261719f, 0.974306f},
            {0.261719f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MLAB_EDGE);

    CockpitDrawCommand DECRYPT_PAN;
    DECRYPT_PAN.type = CockpitDrawType::Fill;
    DECRYPT_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.722652f, 0.895833f},
            {0.978578f, 0.895833f},
            {0.978578f, 0.930562f},
            {0.722652f, 0.930562f},
            {0.722652f, 0.895833f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(DECRYPT_PAN);

    CockpitDrawCommand DECRYPT_EDGE;
    DECRYPT_EDGE.type = CockpitDrawType::Stroke;
    DECRYPT_EDGE.stroke = {
        {
            {0.722652f, 0.895833f},
            {0.978578f, 0.895833f},
            {0.978578f, 0.930562f},
            {0.722652f, 0.930562f},
            {0.722652f, 0.895833f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(DECRYPT_EDGE);

    CockpitDrawCommand SIGNAL_PAN;
    SIGNAL_PAN.type = CockpitDrawType::Fill;
    SIGNAL_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.722652f, 0.944444f},
            {0.977824f, 0.944444f},
            {0.977824f, 0.979174f},
            {0.722652f, 0.979174f},
            {0.722652f, 0.944444f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIGNAL_PAN);

    CockpitDrawCommand SIGNAL_EDGE;
    SIGNAL_EDGE.type = CockpitDrawType::Stroke;
    SIGNAL_EDGE.stroke = {
        {
            {0.722652f, 0.944444f},
            {0.977824f, 0.944444f},
            {0.977824f, 0.979174f},
            {0.722652f, 0.979174f},
            {0.722652f, 0.944444f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIGNAL_EDGE);

    CockpitDrawCommand SIG1_PAN;
    SIG1_PAN.type = CockpitDrawType::Fill;
    SIG1_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.725664f, 0.950000f},
            {0.773086f, 0.950000f},
            {0.773086f, 0.974313f},
            {0.725664f, 0.974313f},
            {0.725664f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIG1_PAN);

    CockpitDrawCommand SIG1_EDGE;
    SIG1_EDGE.type = CockpitDrawType::Stroke;
    SIG1_EDGE.stroke = {
        {
            {0.725664f, 0.950000f},
            {0.773086f, 0.950000f},
            {0.773086f, 0.974313f},
            {0.725664f, 0.974313f},
            {0.725664f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIG1_EDGE);

    CockpitDrawCommand SIG2_PAN;
    SIG2_PAN.type = CockpitDrawType::Fill;
    SIG2_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.776098f, 0.950000f},
            {0.823516f, 0.950000f},
            {0.823516f, 0.974313f},
            {0.776098f, 0.974313f},
            {0.776098f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIG2_PAN);

    CockpitDrawCommand SIG2_EDGE;
    SIG2_EDGE.type = CockpitDrawType::Stroke;
    SIG2_EDGE.stroke = {
        {
            {0.776098f, 0.950000f},
            {0.823516f, 0.950000f},
            {0.823516f, 0.974313f},
            {0.776098f, 0.974313f},
            {0.776098f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIG2_EDGE);

    CockpitDrawCommand SIG3_PAN;
    SIG3_PAN.type = CockpitDrawType::Fill;
    SIG3_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.826527f, 0.950000f},
            {0.873949f, 0.950000f},
            {0.873949f, 0.974313f},
            {0.826527f, 0.974313f},
            {0.826527f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIG3_PAN);

    CockpitDrawCommand SIG3_EDGE;
    SIG3_EDGE.type = CockpitDrawType::Stroke;
    SIG3_EDGE.stroke = {
        {
            {0.826527f, 0.950000f},
            {0.873949f, 0.950000f},
            {0.873949f, 0.974313f},
            {0.826527f, 0.974313f},
            {0.826527f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIG3_EDGE);

    CockpitDrawCommand SIG4_PAN;
    SIG4_PAN.type = CockpitDrawType::Fill;
    SIG4_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.876961f, 0.950000f},
            {0.924383f, 0.950000f},
            {0.924383f, 0.974313f},
            {0.876961f, 0.974313f},
            {0.876961f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIG4_PAN);

    CockpitDrawCommand SIG4_EDGE;
    SIG4_EDGE.type = CockpitDrawType::Stroke;
    SIG4_EDGE.stroke = {
        {
            {0.876961f, 0.950000f},
            {0.924383f, 0.950000f},
            {0.924383f, 0.974313f},
            {0.876961f, 0.974313f},
            {0.876961f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIG4_EDGE);

    CockpitDrawCommand SIG5_PAN;
    SIG5_PAN.type = CockpitDrawType::Fill;
    SIG5_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.927395f, 0.950000f},
            {0.974816f, 0.950000f},
            {0.974816f, 0.974313f},
            {0.927395f, 0.974313f},
            {0.927395f, 0.950000f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(SIG5_PAN);

    CockpitDrawCommand SIG5_EDGE;
    SIG5_EDGE.type = CockpitDrawType::Stroke;
    SIG5_EDGE.stroke = {
        {
            {0.927395f, 0.950000f},
            {0.974816f, 0.950000f},
            {0.974816f, 0.974313f},
            {0.927395f, 0.974313f},
            {0.927395f, 0.950000f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(SIG5_EDGE);

    CockpitDrawCommand MISILLE_PAN;
    MISILLE_PAN.type = CockpitDrawType::Fill;
    MISILLE_PAN.polygon = {
        // Направление: CCW (гарантировано)
        {
            {0.323027f, 0.970347f},
            {0.319395f, 0.970993f},
            {0.319395f, 0.953319f},
            {0.323027f, 0.954125f},
            {0.323980f, 0.943549f},
            {0.330926f, 0.943549f},
            {0.336512f, 0.954285f},
            {0.365750f, 0.954896f},
            {0.369994f, 0.955604f},
            {0.374148f, 0.957271f},
            {0.377418f, 0.959688f},
            {0.380504f, 0.962778f},
            {0.377347f, 0.965819f},
            {0.374012f, 0.968167f},
            {0.369841f, 0.969681f},
            {0.365613f, 0.970590f},
            {0.336465f, 0.970347f},
            {0.330879f, 0.980757f},
            {0.323801f, 0.980757f},
            {0.323027f, 0.970347f},
            {0.323027f, 0.970347f},
        },                 
        CockpitFillType::LinearGradient,   // тип заливки
        colorA,        // colorA
        colorB,        // colorB
        gradFrom,                      // gradFrom
        gradTo                       // gradTo
    };
    geo.drawList.push_back(MISILLE_PAN);

    CockpitDrawCommand MISILLE_EDGE;
    MISILLE_EDGE.type = CockpitDrawType::Stroke;
    MISILLE_EDGE.stroke = {
        {
            {0.323027f, 0.970347f},
            {0.319395f, 0.970993f},
            {0.319395f, 0.953319f},
            {0.323027f, 0.954125f},
            {0.323980f, 0.943549f},
            {0.330926f, 0.943549f},
            {0.336512f, 0.954285f},
            {0.365750f, 0.954896f},
            {0.369994f, 0.955604f},
            {0.374148f, 0.957271f},
            {0.377418f, 0.959688f},
            {0.380504f, 0.962778f},
            {0.377347f, 0.965819f},
            {0.374012f, 0.968167f},
            {0.369841f, 0.969681f},
            {0.365613f, 0.970590f},
            {0.336465f, 0.970347f},
            {0.330879f, 0.980757f},
            {0.323801f, 0.980757f},
            {0.323027f, 0.970347f},
            {0.323027f, 0.970347f},
        },                 
        defStroke,
        colorS
    };
    geo.drawList.push_back(MISILLE_EDGE);



    // // ---------------------------------
    // // собственно - индикатор - скорость
    // // ---------------------------------

    // CockpitDrawCommand SPEEDVAR_PAN;
    // SPEEDVAR_PAN.type = CockpitDrawType::Fill;
    // SPEEDVAR_PAN.id   = "speed_bar_fill";
    // SPEEDVAR_PAN.polygon = {
    //     {
    //         {0.193750f, 0.917465f},
    //         {0.305798f, 0.917465f},
    //         {0.305798f, 0.926882f},
    //         {0.193750f, 0.926882f},
    //     },                 
    //     CockpitFillType::Solid,   // тип заливки
    //     {0.9f, 0.1f, 0.1f, 1.0f},       // colorA
    //     colorB,                         // colorB
    //     gradFrom,                       // gradFrom
    //     gradTo                          // gradTo
    // };
    // geo.drawList.push_back(SPEEDVAR_PAN);



    // // ---------------------------------
    // // собственно - индикатор - скорость - стрелочный
    // // ---------------------------------
    // CockpitPolygon needle;

    // std::vector<glm::vec2> local = {
    //     {0.0f, 0.0f},   // pivot (основание)
    //     {0.04f, 0.0f},
    //     {0.02f, -0.08f}
    // };

    // float cx = 0.5f;
    // float cy = 0.88f;

    // for (auto& p : local)
    // {
    //     needle.contour01.push_back({
    //         cx + p.x,
    //         cy + p.y
    //     });
    // }


    // needle.colorA = {1.0f, 0.0f, 0.0f, 1.0f};
    // needle.fillType = CockpitFillType::Solid;

    // CockpitDrawCommand cmd;
    // cmd.type = CockpitDrawType::Fill;
    // cmd.polygon = needle;
    // cmd.id = "speed_needle";
    // cmd.pivot01 = needle.contour01[0];
    // cmd.hasPivot = true;

    // geo.drawList.push_back(cmd);



    //  =============== слоты дешефратора ==========
   



    // int maxDecryptors = desc.systems.decryptorSlots;

    // glm::vec2 areaMin = {0.70f, 0.80f};
    // glm::vec2 areaMax = {0.95f, 0.90f};

    // float padding = 0.005f;

    // float totalWidth  = areaMax.x - areaMin.x;
    // float totalHeight = areaMax.y - areaMin.y;

    // if (maxDecryptors <= 0)
    //     return geo;

    // float slotWidth =
    //     (totalWidth - padding * (maxDecryptors + 1)) / maxDecryptors;

    // float slotHeight = totalHeight - 2.0f * padding;

    // for (int i = 0; i < maxDecryptors; ++i)
    // {
    //     float x0 = areaMin.x + padding + i * (slotWidth + padding);
    //     float y0 = areaMin.y + padding;

    //     float x1 = x0 + slotWidth;
    //     float y1 = y0 + slotHeight;

    //     // ───────── внешний модуль ─────────
    //     CockpitPolygon modulePoly;
    //     modulePoly.contour01 = {
    //         {x0, y0},
    //         {x1, y0},
    //         {x1, y1},
    //         {x0, y1}
    //     };

    //     modulePoly.colorA = {0.15f, 0.15f, 0.2f, 1.0f};
    //     modulePoly.fillType = CockpitFillType::Solid;

    //     CockpitDrawCommand moduleCmd;
    //     moduleCmd.type = CockpitDrawType::Fill;
    //     moduleCmd.polygon = modulePoly;
    //     moduleCmd.id = "decryptor_module_" + std::to_string(i);

    //     geo.drawList.push_back(moduleCmd);

    //     // ───────── внутренние слоты ─────────
    //     const int maxInnerSlots = 16;

    //     float innerPadding = 0.002f;

    //     float innerWidth =
    //         (slotWidth - innerPadding * (maxInnerSlots + 1)) / maxInnerSlots;

    //     float innerHeight = slotHeight * 0.4f;

    //     for (int j = 0; j < maxInnerSlots; ++j)
    //     {
    //         float sx0 = x0 + innerPadding + j * (innerWidth + innerPadding);
    //         float sy0 = y0 + slotHeight * 0.3f;

    //         float sx1 = sx0 + innerWidth;
    //         float sy1 = sy0 + innerHeight;

    //         CockpitPolygon innerPoly;
    //         innerPoly.contour01 = {
    //             {sx0, sy0},
    //             {sx1, sy0},
    //             {sx1, sy1},
    //             {sx0, sy1}
    //         };

    //         innerPoly.colorA = {0.08f, 0.08f, 0.1f, 1.0f};
    //         innerPoly.fillType = CockpitFillType::Solid;

    //         CockpitDrawCommand innerCmd;
    //         innerCmd.type = CockpitDrawType::Fill;
    //         innerCmd.polygon = innerPoly;
    //         innerCmd.id =
    //             "decryptor_" + std::to_string(i) +
    //             "_slot_" + std::to_string(j);

    //         geo.drawList.push_back(innerCmd);
    //     }
    // }

   



    return geo;
}




// CockpitLayout EliteCobraMk1::cockpitLayout(const ShipDescriptor& desc) {
//     CockpitLayout layout;

//     // SPEED
//     layout.indicators.push_back({
//         .id = "speed",
//         .visualType = IndicatorVisualType::Bar,
//         .pos01 = {0.06f, 0.08f},
//         .size01 = {0.18f, 0.025f}
//     });

//     // FUEL
//     layout.indicators.push_back({
//         .id = "fuel",
//         .visualType = IndicatorVisualType::Bar,
//         .pos01 = {0.06f, 0.045f},
//         .size01 = {0.18f, 0.025f}
//     });

//     // SHIELD
//     layout.indicators.push_back({
//         .id = "shield",
//         .visualType = IndicatorVisualType::Bar,
//         .pos01 = {0.06f, 0.01f},
//         .size01 = {0.18f, 0.025f}
//     });

//     // DECRYPTOR (массив сегментов)
//     layout.indicators.push_back({
//         .id = "decryptor",
//         .visualType = IndicatorVisualType::SegmentArray,
//         .pos01 = {0.70f, 0.05f},
//         .size01 = {0.25f, 0.035f},
//         .segments = desc.systems.decryptorSlots
//     });

//     return layout;
// }

