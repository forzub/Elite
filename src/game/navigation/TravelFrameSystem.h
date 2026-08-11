#pragma once

#include <string>

#include "src/game/navigation/DynamicMotionState.h"

namespace game::navigation
{

// Owns the relationship between a ship's kinematic travel frame and an
// external reference frame (a hub today, another travel/corridor frame later).
// Matching copies kinematics but never aliases ownership: the ship keeps its
// own frame identity and can detach without changing position/velocity.
class TravelFrameSystem
{
public:
    static bool matchToReference(
        DynamicMotionState& motion,
        const KinematicFrame& reference,
        const std::string& ownedFrameId,
        const std::string& referenceFrameId
    )
    {
        if (!reference.valid || reference.systemId < 0 || ownedFrameId.empty())
            return false;

        motion.travelFrame = reference;
        motion.travelFrame.frameId = ownedFrameId;
        motion.travelFrame.valid = true;
        motion.systemId = reference.systemId;
        motion.matchedToReferenceFrame = true;
        motion.matchedReferenceFrameId = referenceFrameId;
        return true;
    }

    static bool refreshMatchedReference(
        DynamicMotionState& motion,
        const KinematicFrame& reference,
        const std::string& referenceFrameId
    )
    {
        if (!motion.matchedToReferenceFrame ||
            motion.matchedReferenceFrameId != referenceFrameId ||
            !motion.travelFrame.valid ||
            motion.travelFrame.frameId.empty() ||
            !reference.valid ||
            reference.systemId != motion.systemId)
        {
            return false;
        }

        const std::string ownedFrameId = motion.travelFrame.frameId;
        motion.travelFrame = reference;
        motion.travelFrame.frameId = ownedFrameId;
        motion.travelFrame.valid = true;
        return true;
    }

    static void detach(DynamicMotionState& motion)
    {
        // Detaching changes only the relationship/ownership state. The frame's
        // instantaneous position, velocity, acceleration and basis are kept
        // exactly as they were at the separation epoch.
        motion.matchedToReferenceFrame = false;
        motion.matchedReferenceFrameId.clear();
    }
};

} // namespace game::navigation
