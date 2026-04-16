#pragma once

#include "render/Camera.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/game/ship/view/ShipCameraMode.h"
#include "src/game/ship/ShipDescriptor.h"
#include "game/debug/AttachmentEditorData.h"

class ShipCameraController
{
public:
    void setAttachmentOverrides(const ShipAttachmentOverrideMap* overrides)
    {
        m_attachmentOverrides = overrides;
    }

    void updateMode(
        ShipCameraMode mode,
        float dt,
        const ShipDescriptor* desc,
        const ShipTransform& transform,
        Camera& camera
    );

private:
    const ShipAttachmentOverrideMap* m_attachmentOverrides = nullptr;
};