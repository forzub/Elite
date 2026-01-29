#include "EliteCobraMk1.h"

const ShipDescriptor& getEliteCobraMk1()
{
    static ShipDescriptor desc;

    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        desc.name = "Elite Cobra Mk I";

        // -------------------------
        // Physics
        // -------------------------
        desc.physics = {
            90.0f, 60.0f, 120.0f,   // angular limits
            1000.0f, 9.0f,          // angular accel/damp
            500.0f, 10000.0f, 5.0f, // speeds
            0.0f,                   // auto-level
            20.0f, 6.0f, 50.0f      // strafe
        };

        // -------------------------
        // HUD profile (ВОТ ТУТ)
        // -------------------------
        desc.hud.name = "Elite Mk1 Cockpit";

        desc.hud.edgeBoundary.contour = {
            {0.10f, 0.12f},
            {0.90f, 0.12f},
            {0.96f, 0.50f},
            {0.90f, 0.88f},
            {0.10f, 0.88f},
            {0.04f, 0.50f}
        };
    }

    return desc;
}
