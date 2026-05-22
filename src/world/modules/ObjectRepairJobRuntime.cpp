#include "ObjectRepairJobRuntime.h"

#include <algorithm>
#include <iostream>

#include <glm/gtx/norm.hpp>

#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/world/navigation/ObstacleAvoidance.h"
#include "src/world/navigation/SmallCraftNavigation.h"

#include "src/world/navigation/NavigationAgentProfile.h"
#include "src/world/navigation/NavigationContactAdapters.h"
#include "src/world/navigation/TacticalCollisionMonitor.h"
#include "src/world/navigation/ObstaclePathPlanner.h"

#include "src/world/coordinates/WorldPosition.h"

namespace world::modules
{

void ObjectRepairJobRuntime::clear()
{
    m_jobs.clear();
}

bool ObjectRepairJobRuntime::hasJob(const std::string& moduleId) const
{
    for (const auto& job : m_jobs)
    {
        if (job.moduleId == moduleId &&
            job.state != ObjectRepairJobState::Done &&
            job.state != ObjectRepairJobState::Failed)
        {
            return true;
        }
    }

    return false;
}

glm::vec3 ObjectRepairJobRuntime::moveTowards(
    const glm::vec3& current,
    const glm::vec3& target,
    float maxDistance
)
{
    const glm::vec3 delta = target - current;
    const float dist2 = glm::length2(delta);

    if (dist2 <= 0.000001f)
        return target;

    const float dist = std::sqrt(dist2);

    if (dist <= maxDistance)
        return target;

    return current + delta / dist * maxDistance;
}




static glm::mat4 slerpOrientationMatrix(
    const glm::mat4& a,
    const glm::mat4& b,
    float t
)
{
    t = glm::clamp(t, 0.0f, 1.0f);

    glm::quat qa = glm::quat_cast(a);
    glm::quat qb = glm::quat_cast(b);

    glm::quat q = glm::slerp(qa, qb, t);
    q = glm::normalize(q);

    return glm::toMat4(q);
}

static float smooth01(float t)
{
    t = glm::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}











static glm::vec3 safeNormalizeOr(
    const glm::vec3& v,
    const glm::vec3& fallback
)
{
    if (glm::length2(v) < 0.000001f)
        return fallback;

    return glm::normalize(v);
}







static glm::vec3 computeRepairOutsideNormal(
    const std::string& moduleId,
    const glm::vec3& homeCenterWorld,
    const glm::vec3& ownerCenter
)
{
    glm::vec3 normal =
        safeNormalizeOr(
            homeCenterWorld - ownerCenter,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    if (moduleId.find("_top_") != std::string::npos ||
        moduleId.find("top") != std::string::npos)
    {
        normal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    return normal;
}




static glm::vec3 moveTowardsPoint(
    const glm::vec3& current,
    const glm::vec3& target,
    float maxDistance
)
{
    const glm::vec3 delta = target - current;
    const float dist2 = glm::length2(delta);

    if (dist2 <= 0.000001f)
        return target;

    const float dist = std::sqrt(dist2);

    if (dist <= maxDistance)
        return target;

    return current + delta / dist * maxDistance;
}




static float computeAllowedSpeedForDistance(
    float distance,
    float acceleration
)
{
    if (distance <= 0.0f)
        return 0.0f;

    if (acceleration <= 0.001f)
        return 0.0f;

    return std::sqrt(2.0f * acceleration * distance);
}





static glm::mat4 buildFragmentLocalModel(
    const ObjectDetachedFragmentRuntimeState& fragment
)
{
    return glm::translate(glm::mat4(1.0f), fragment.position) *
           fragment.orientation;
}

static glm::vec3 getFragmentCenterLocal(
    const ObjectDetachedFragmentRuntimeState& fragment
)
{
    return glm::vec3(
        glm::inverse(fragment.homeLocalModel) *
        glm::vec4(fragment.homeCenterLocal, 1.0f)
    );
}

static glm::vec3 getFragmentCenterWorld(
    const ObjectDetachedFragmentRuntimeState& fragment
)
{
    return glm::vec3(
        buildFragmentLocalModel(fragment) *
        glm::vec4(getFragmentCenterLocal(fragment), 1.0f)
    );
}

static glm::vec3 getFragmentCenterWorldForPose(
    const ObjectDetachedFragmentRuntimeState& fragment,
    const glm::vec3& position,
    const glm::mat4& orientation
)
{
    const glm::mat4 model =
        glm::translate(glm::mat4(1.0f), position) *
        orientation;

    return glm::vec3(
        model *
        glm::vec4(getFragmentCenterLocal(fragment), 1.0f)
    );
}

static void moveFragmentCenterToWorld(
    ObjectDetachedFragmentRuntimeState& fragment,
    const glm::vec3& desiredCenterWorld
)
{
    const glm::vec3 currentCenterWorld =
        getFragmentCenterWorld(fragment);

    const glm::vec3 newPosition =
        fragment.position + (desiredCenterWorld - currentCenterWorld);
    
    // Fragment runtime is owner-local.
    // Global WorldPosition is produced only in snapshots.
    fragment.position = newPosition;
}






static void setDronePositionLocal(
    ObjectRepairJobRuntimeState& job,
    const glm::vec3& position
)
{
    job.dronePosition = position;
}

static void moveDronePositionLocal(
    ObjectRepairJobRuntimeState& job,
    const glm::vec3& nextPosition,
    float dt
)
{
    const glm::vec3 oldPosition =
        job.dronePosition;

    job.dronePosition = nextPosition;

    if (dt > 0.000001f)
    {
        job.droneVelocity =
            (job.dronePosition - oldPosition) / dt;
    }
    else
    {
        job.droneVelocity = glm::vec3(0.0f);
    }
}




static void debugEvaluateRepairDroneTacticalRisk(
    const ObjectRepairJobRuntimeState& job,
    const std::vector<world::navigation::NavigationObstacle>& obstacles
)
{
    const auto profile =
        world::navigation::makeRepairDroneNavigationProfile();

    const auto contacts =
        world::navigation::makeContactsFromObstacles(
            obstacles,
            world::navigation::NavigationContactKind::DetachedFragment
        );

    const auto result =
        world::navigation::evaluateTacticalCollisionRisk(
            job.dronePosition,
            job.droneVelocity,
            profile,
            contacts
        );

    if (result.level == world::navigation::TacticalRiskLevel::Clear)
        return;

    if (result.level == world::navigation::TacticalRiskLevel::Emergency)
{
    (void)0;
}
}




static world::navigation::ObstaclePathPlannerParams makeRepairDronePathParams(
    bool towing
)
{
    world::navigation::ObstaclePathPlannerParams p;

    p.clearance = towing ? 6.0f : 3.0f;
    p.maxBypassDistance = towing ? 120.0f : 80.0f;
    p.radialSamples = towing ? 24 : 18;
    p.corridorRadius = towing ? 160.0f : 120.0f;
    p.maxConsideredObstacles = 32;
    p.endpointIgnoreRadius = towing ? 4.0f : 2.0f;
    p.allowTwoPointBypass = true;

    return p;
}




static std::vector<world::navigation::SmallCraftWaypoint>
buildCaptureApproachArcPath(
    const glm::vec3& dronePosition,
    const glm::vec3& fragmentCenterWorld,
    const glm::vec3& outsideNormal,
    float approachDistance,
    float orbitRadius
)
{
    std::vector<world::navigation::SmallCraftWaypoint> path;

    const glm::vec3 latchPoint =
        fragmentCenterWorld + outsideNormal * approachDistance;

    const glm::vec3 droneDir =
        safeNormalizeOr(
            dronePosition - fragmentCenterWorld,
            -outsideNormal
        );

    const float side =
        glm::dot(droneDir, outsideNormal);

    // Если дрон уже снаружи — летим прямо к внешней точке.
    if (side > 0.35f)
    {
        path.push_back({
            latchPoint,
            world::navigation::SmallCraftWaypointKind::WorkPoint
        });

        return path;
    }

    glm::vec3 tangent =
        glm::cross(outsideNormal, glm::vec3(0.0f, 1.0f, 0.0f));

    if (glm::length2(tangent) < 0.0001f)
    {
        tangent =
            glm::cross(outsideNormal, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    tangent = glm::normalize(tangent);

    const glm::vec3 sidePointA =
        fragmentCenterWorld + tangent * orbitRadius;

    const glm::vec3 sidePointB =
        fragmentCenterWorld - tangent * orbitRadius;

    const glm::vec3 sidePoint =
        glm::length2(sidePointA - dronePosition) <
        glm::length2(sidePointB - dronePosition)
            ? sidePointA
            : sidePointB;

    const glm::vec3 outerArcPoint =
        fragmentCenterWorld + outsideNormal * orbitRadius;

    path.push_back({
        sidePoint,
        world::navigation::SmallCraftWaypointKind::Transit
    });

    path.push_back({
        outerArcPoint,
        world::navigation::SmallCraftWaypointKind::Transit
    });

    path.push_back({
        latchPoint,
        world::navigation::SmallCraftWaypointKind::WorkPoint
    });

    return path;
}





static bool rebuildRepairDronePathThroughGoals(
    ObjectRepairJobRuntimeState& job,
    const glm::vec3& start,
    const std::vector<world::navigation::SmallCraftWaypoint>& goals,
    const std::vector<world::navigation::NavigationObstacle>& obstacles,
    bool towing,
    const char* label
)
{
    std::vector<world::navigation::SmallCraftWaypoint> fullPath;

    glm::vec3 segmentStart = start;

    for (size_t i = 0; i < goals.size(); ++i)
    {
        const bool last =
            i + 1 == goals.size();

        auto segment =
            world::navigation::buildObstacleAwarePath(
                segmentStart,
                goals[i].position,
                obstacles,
                makeRepairDronePathParams(towing),
                last
                    ? goals[i].kind
                    : world::navigation::SmallCraftWaypointKind::Transit
            );

        if (segment.empty())
        {
            (void)0;

            return false;
        }

        for (auto& wp : segment)
        {
            if (!last)
                wp.kind = world::navigation::SmallCraftWaypointKind::Transit;

            fullPath.push_back(wp);
        }

        segmentStart = goals[i].position;
    }

    job.droneNav.clear();
    job.droneNav.setPath(fullPath);

    (void)0;

    return true;
}







static bool rebuildRepairDronePath(
    ObjectRepairJobRuntimeState& job,
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<world::navigation::NavigationObstacle>& obstacles,
    bool towing,
    const char* label
)
{
    auto path =
        world::navigation::buildObstacleAwarePath(
            start,
            target,
            obstacles,
            makeRepairDronePathParams(towing),
            world::navigation::SmallCraftWaypointKind::WorkPoint
        );

    if (path.empty())
    {
        (void)0;

        return false;
    }

    job.droneNav.clear();
    job.droneNav.setPath(path);

    (void)0;

    return true;
}


static void debugPrintDronePath(
    const char* label,
    const ObjectRepairJobRuntimeState& job
);


static bool tryReplaceRepairDronePath(
    ObjectRepairJobRuntimeState& job,
    const glm::vec3& start,
    const glm::vec3& target,
    const std::vector<world::navigation::NavigationObstacle>& obstacles,
    bool towing,
    world::navigation::SmallCraftWaypointKind finalKind,
    const char* label
)
{
    auto path =
        world::navigation::buildObstacleAwarePath(
            start,
            target,
            obstacles,
            makeRepairDronePathParams(towing),
            finalKind
        );

    if (path.empty())
    {
        (void)0;

        return false;
    }

    job.droneNav.setPath(std::move(path));
    debugPrintDronePath(label, job);

    (void)0;

    return true;
}







static void debugPrintDronePath(
    const char* label,
    const ObjectRepairJobRuntimeState& job
)
{
    (void)0;

    for (size_t i = 0; i < job.droneNav.waypoints.size(); ++i)
    {
        const auto& wp = job.droneNav.waypoints[i];

        (void)0;
    }
}








static glm::vec3 chooseBestRepairWorkPoint(
    const glm::vec3& ownerCenterWorld,
    const glm::vec3& targetWorld,
    const std::vector<glm::vec3>& workPoints,
    const glm::vec3& fallback
)
{
    if (workPoints.empty())
        return fallback;

    const glm::vec3 targetDir =
        safeNormalizeOr(
            targetWorld - ownerCenterWorld,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    const glm::vec3 fallbackDir =
        safeNormalizeOr(
            fallback - ownerCenterWorld,
            targetDir
        );

    float bestScore = -999999.0f;
    glm::vec3 best = workPoints.front();

    for (const glm::vec3& p : workPoints)
    {
        const glm::vec3 pointDir =
            safeNormalizeOr(
                p - ownerCenterWorld,
                fallbackDir
            );

        const float sideScore =
            glm::dot(pointDir, targetDir);

        const float distancePenalty =
            0.015f * glm::length(p - fallback);

        const float score =
            sideScore - distancePenalty;

        if (score > bestScore)
        {
            bestScore = score;
            best = p;
        }
    }

    return best;
}




struct RepairUpdateContext
{
    float dt = 0.0f;

    const glm::mat4* ownerModel = nullptr;

    ObjectRepairJobRuntimeState* job = nullptr;
    ObjectDetachedFragmentRuntimeState* fragment = nullptr;

    glm::vec3 homeCenterWorld {0.0f};
    glm::mat4 homeOrientation {1.0f};

    const std::vector<world::navigation::NavigationObstacle>* obstacles = nullptr;
};



static bool updateFlyToPoint(
    RepairUpdateContext& ctx,
    const glm::vec3& targetPoint,
    float speed,
    float acceleration,
    float arrivalRadius,
    bool rebuildPathIfMissing,
    bool useObstacleAvoidance,
    world::navigation::SmallCraftWaypointKind finalKind
)
{
    auto& job = *ctx.job;

    if (rebuildPathIfMissing && !job.droneNav.hasPath())
    {
        if (useObstacleAvoidance && ctx.obstacles)
        {
            job.droneNav.setPath(
                world::navigation::buildSimpleAvoidancePath(
                    job.dronePosition,
                    targetPoint,
                    *ctx.obstacles,
                    3.0f,
                    finalKind,
                    0.0f
                )
            );

            debugPrintDronePath("имя-фазы", job);

           
        }
        else
        {
            job.droneNav.setPath({
                { targetPoint, finalKind }
            });
            debugPrintDronePath("имя-фазы", job);
        }
    }

    glm::vec3 nextDronePosition =
    job.dronePosition;

    const bool arrived =
        world::navigation::updateSmallCraftNavigation(
            nextDronePosition,
            job.droneVelocity,
            job.droneNav,
            speed,
            acceleration,
            arrivalRadius,
            ctx.dt
        );

    setDronePositionLocal(job, nextDronePosition);

    return arrived;
}





static bool updateDroneAlongCurrentPath(
    RepairUpdateContext& ctx,
    float maxSpeed,
    float maxAcceleration,
    float arrivalRadius
)
{
    auto& job = *ctx.job;

    const glm::vec3 oldPosition =
        job.dronePosition;

    glm::vec3 nextDronePosition =
    job.dronePosition;

    const bool arrived =
        world::navigation::updateSmallCraftNavigation(
            nextDronePosition,
            job.droneVelocity,
            job.droneNav,
            maxSpeed,
            maxAcceleration,
            arrivalRadius,
            ctx.dt
        );

    setDronePositionLocal(job, nextDronePosition);

    if (ctx.obstacles)
    {
        const bool blocked =
            world::navigation::debugSegmentBlockedByNavigationObstacles(
                oldPosition,
                job.dronePosition,
                *ctx.obstacles,
                1.0f
            );

        if (blocked)
        {
            (void)0;
        }
    }

    return arrived;
}






static bool updateDronePursuitToMovingPoint(
    RepairUpdateContext& ctx,
    const glm::vec3& targetPoint,
    float maxSpeed,
    float maxAcceleration,
    float arrivalRadius
)
{
    auto& job = *ctx.job;

    const glm::vec3 toTarget =
        targetPoint - job.dronePosition;

    const float distance =
        glm::length(toTarget);

    if (distance <= arrivalRadius)
    {
        setDronePositionLocal(job, targetPoint);
        job.droneVelocity = glm::vec3(0.0f);
        job.droneNav.clear();
        return true;
    }

    const glm::vec3 desiredDir =
        safeNormalizeOr(
            toTarget,
            glm::vec3(0.0f, 0.0f, -1.0f)
        );

    // Не тормозим как перед статичной точкой.
    // Это живая цель, её надо догонять.
    const glm::vec3 desiredVelocity =
        desiredDir * maxSpeed;

    const glm::vec3 dv =
        desiredVelocity - job.droneVelocity;

    const float maxDeltaSpeed =
        glm::max(0.0f, maxAcceleration * ctx.dt);

    glm::vec3 velocityDelta = dv;

    const float dvLen =
        glm::length(dv);

    if (dvLen > maxDeltaSpeed && dvLen > 0.000001f)
    {
        velocityDelta =
            dv / dvLen * maxDeltaSpeed;
    }

    job.droneVelocity += velocityDelta;

    const float speedNow =
        glm::length(job.droneVelocity);

    if (speedNow > maxSpeed && speedNow > 0.000001f)
    {
        job.droneVelocity =
            job.droneVelocity / speedNow * maxSpeed;
    }

    setDronePositionLocal(
        job,
        job.dronePosition + job.droneVelocity * ctx.dt
    );

    return false;
}









static bool updateTowFragmentToPoint(
    RepairUpdateContext& ctx,
    const glm::vec3& targetPoint,
    float speed,
    float acceleration,
    float arrivalRadius,
    bool useObstacleAvoidance,
    float orientationAlignRate
)
{

    (void)acceleration;

    auto& job = *ctx.job;
    auto& fragment = *ctx.fragment;
    const glm::vec3 droneTargetPoint =
        targetPoint - job.towOffsetWorld;

    fragment.linearVelocity = glm::vec3(0.0f);
    fragment.angularVelocity = glm::vec3(0.0f);

    if (!job.droneNav.hasPath())
    {
        if (useObstacleAvoidance && ctx.obstacles)
        {
            job.droneNav.setPath(
                world::navigation::buildSimpleAvoidancePath(
                    job.dronePosition,
                    droneTargetPoint,
                    *ctx.obstacles,
                    3.0f,
                    world::navigation::SmallCraftWaypointKind::WorkPoint,
                    4.0f
                )
            );
            debugPrintDronePath("имя-фазы", job);
        }
        else
        {
            job.droneNav.setPath({
                { droneTargetPoint, world::navigation::SmallCraftWaypointKind::WorkPoint }
            });
            debugPrintDronePath("имя-фазы", job);
        }
    }

    if (!job.droneNav.hasPath())
    {
        job.droneVelocity = glm::vec3(0.0f);
        moveFragmentCenterToWorld(
            fragment,
            job.dronePosition + job.towOffsetWorld
        );
        return true;
    }

    const auto& wp =
        job.droneNav.waypoints[job.droneNav.currentWaypoint];


    const float maxStep =
        glm::max(0.0f, speed * ctx.dt);

    moveDronePositionLocal(
        job,
        moveTowardsPoint(
            job.dronePosition,
            wp.position,
            maxStep
        ),
        ctx.dt
    );

    const float dist2 =
        glm::length2(wp.position - job.dronePosition);

    const float effectiveArrivalRadius =
        glm::max(arrivalRadius, 0.05f);

    if (dist2 <= effectiveArrivalRadius * effectiveArrivalRadius)
    {
        setDronePositionLocal(job, wp.position);
        job.droneVelocity = glm::vec3(0.0f);

        ++job.droneNav.currentWaypoint;

        if (!job.droneNav.hasPath())
        {
            moveFragmentCenterToWorld(
                fragment,
                job.dronePosition + job.towOffsetWorld
            );

            if (orientationAlignRate > 0.0f)
            {
                fragment.orientation =
                    slerpOrientationMatrix(
                        fragment.orientation,
                        ctx.homeOrientation,
                        glm::clamp(orientationAlignRate * ctx.dt, 0.0f, 1.0f)
                    );
            }

            return true;
        }
    }

    moveFragmentCenterToWorld(
        fragment,
        job.dronePosition + job.towOffsetWorld
    );

    if (orientationAlignRate > 0.0f)
    {
        fragment.orientation =
            slerpOrientationMatrix(
                fragment.orientation,
                ctx.homeOrientation,
                glm::clamp(orientationAlignRate * ctx.dt, 0.0f, 1.0f)
            );
    }

    return false;
}





static bool updateAlignFragmentToMount(
    RepairUpdateContext& ctx
)
{
    auto& job = *ctx.job;
    auto& fragment = *ctx.fragment;

    fragment.linearVelocity = glm::vec3(0.0f);
    fragment.angularVelocity = glm::vec3(0.0f);

    job.alignTimer += ctx.dt;

    const float t =
        job.alignDuration > 0.001f
            ? glm::clamp(job.alignTimer / job.alignDuration, 0.0f, 1.0f)
            : 1.0f;

    const float s = smooth01(t);

    fragment.orientation =
        slerpOrientationMatrix(
            job.alignStartOrientation,
            ctx.homeOrientation,
            s
        );

    const glm::vec3 startCenterWorld =
    getFragmentCenterWorldForPose(
        fragment,
        job.alignStartPosition,
        job.alignStartOrientation
    );

    const glm::vec3 desiredCenterWorld =
        glm::mix(startCenterWorld, job.stagingWorldPosition, s);

    moveFragmentCenterToWorld(
            fragment,
            desiredCenterWorld
        );

    moveDronePositionLocal(
        job,
        desiredCenterWorld - job.towOffsetWorld,
        ctx.dt
    );

    return t >= 1.0f;
}




static bool updateFinalMounting(
    RepairUpdateContext& ctx
)
{
    auto& job = *ctx.job;
    auto& fragment = *ctx.fragment;

    fragment.linearVelocity = glm::vec3(0.0f);
    fragment.angularVelocity = glm::vec3(0.0f);
    fragment.orientation = ctx.homeOrientation;

    const glm::vec3 currentCenterWorld =
    getFragmentCenterWorld(fragment);

    const glm::vec3 newCenterWorld =
        moveTowardsPoint(
            currentCenterWorld,
            ctx.homeCenterWorld,
            job.finalMountSpeed * ctx.dt
        );

    moveFragmentCenterToWorld(
        fragment,
        newCenterWorld
    );

    moveDronePositionLocal(
        job,
        newCenterWorld - job.towOffsetWorld,
        ctx.dt
    );

    const float dist2 =
        glm::length2(newCenterWorld - ctx.homeCenterWorld);

    if (dist2 <= job.reattachDistance * job.reattachDistance)
    {
        moveFragmentCenterToWorld(
            fragment,
            ctx.homeCenterWorld
        );

        return true;
    }

    return false;
}








bool ObjectRepairJobRuntime::startJob(
    const std::string& moduleId,
    const glm::mat4& ownerModel,
    const glm::vec3& ownerLinearVelocity,
    const glm::vec3& dockWorldPosition,
    const glm::vec3& launchWorldPosition,
    const glm::vec3& recoveryWorldPosition,
    const std::vector<glm::vec3>& repairWorkWorldPoints,
    const ObjectDetachedFragmentRuntime& detachedRuntime
)
{
    if (hasJob(moduleId))
    {
        (void)0;
        return false;
    }

    if (!detachedRuntime.hasFragment(moduleId))
    {
        (void)0;
        return false;
    }

    const glm::vec3 ownerCenter =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    glm::vec3 exitDir =
        safeNormalizeOr(
            dockWorldPosition - ownerCenter,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    ObjectRepairJobRuntimeState job;
    job.moduleId = moduleId;

    // Первый закон Ньютона:
    // дрон наследует скорость корабля при отделении.
    setDronePositionLocal(job, dockWorldPosition);
    job.droneVelocity = ownerLinearVelocity;
    job.inheritedOwnerVelocity = ownerLinearVelocity;

    job.dockWorldPosition = dockWorldPosition;
job.launchWorldPosition = launchWorldPosition;
job.recoveryWorldPosition = recoveryWorldPosition;
job.repairWorkWorldPoints = repairWorkWorldPoints;

// Старое safeExit оставляем как fallback/debug.
// Но основная первая точка теперь launchWorldPosition.
job.safeExitWorldPosition =
    ownerCenter + exitDir * job.safeExitDistance;

job.droneNav.setPath({
    { job.launchWorldPosition, world::navigation::SmallCraftWaypointKind::WorkPoint }
});
debugPrintDronePath("имя-фазы", job);

    job.state = ObjectRepairJobState::LeavingDock;

    m_jobs.push_back(std::move(job));

    (void)0;

    return true;
}











std::vector<std::string> ObjectRepairJobRuntime::update(
    float dt,
    const glm::mat4& ownerModel,
    ObjectDetachedFragmentRuntime& detachedRuntime,
    const std::vector<world::navigation::NavigationObstacle>& obstacles
)
{
    std::vector<std::string> completed;

    if (dt <= 0.0f)
        return completed;

for (auto& job : m_jobs)
{

    if (job.state == ObjectRepairJobState::Done ||
        job.state == ObjectRepairJobState::Failed)
    {
        continue;
    }

    if (job.state == ObjectRepairJobState::PostRepairPause)
    {
        job.postRepairPauseTimer += dt;
        job.droneVelocity = glm::vec3(0.0f);
        job.droneNav.clear();

        if (job.postRepairPauseTimer >= job.postRepairPauseDuration)
        {
            job.state = ObjectRepairJobState::PostRepairRetreat;
            job.droneNav.clear();

            if (!rebuildRepairDronePath(
                    job,
                    job.dronePosition,
                    job.postRepairExitWorldPosition,
                    obstacles,
                    false,
                    "post-repair-retreat"))
            {
                job.state = ObjectRepairJobState::Failed;
                continue;
            }

            debugPrintDronePath("post-repair-retreat", job);

            (void)0;
        }

        continue;
    }

    if (job.state == ObjectRepairJobState::PostRepairRetreat)
    {
        RepairUpdateContext ctx;
        ctx.dt = dt;
        ctx.job = &job;

        const bool retreated =
            updateDroneAlongCurrentPath(
                ctx,
                job.droneSpeed * 0.65f,
                job.droneAcceleration,
                0.35f
            );

        if (retreated)
        {
            job.state = ObjectRepairJobState::ReturningToDock;
            job.droneNav.clear();

            job.droneNav.setPath(
                world::navigation::buildObstacleAwarePath(
                    job.dronePosition,
                    job.recoveryWorldPosition,
                    obstacles,
                    makeRepairDronePathParams(false),
                    world::navigation::SmallCraftWaypointKind::WorkPoint
                )
            );

            (void)0;
        }

        continue;
    }

    if (job.state == ObjectRepairJobState::ReturningToDock)
    {
        RepairUpdateContext ctx;
        ctx.dt = dt;
        ctx.job = &job;

        const bool arrived =
            updateDroneAlongCurrentPath(
                ctx,
                job.droneSpeed,
                job.droneAcceleration,
                0.35f
            );

        if (arrived)
        {
            if (glm::length2(job.dronePosition - job.recoveryWorldPosition) > 0.35f * 0.35f)
            {
                job.droneNav.clear();

                job.droneNav.setPath(
                    world::navigation::buildObstacleAwarePath(
                        job.dronePosition,
                        job.recoveryWorldPosition,
                        obstacles,
                        makeRepairDronePathParams(false),
                        world::navigation::SmallCraftWaypointKind::WorkPoint
                    )
                );

                continue;
            }

            job.droneNav.clear();

            job.droneNav.setPath({
                { job.dockWorldPosition, world::navigation::SmallCraftWaypointKind::WorkPoint }
            });

            job.state = ObjectRepairJobState::Docking;

            (void)0;
                    }

        continue;
    }



    if (job.state == ObjectRepairJobState::Docking)
{
    RepairUpdateContext ctx;
    ctx.dt = dt;
    ctx.job = &job;

    const bool docked =
        updateDroneAlongCurrentPath(
            ctx,
            job.dockingSpeed,
            job.droneAcceleration,
            job.dockingArrivalRadius
        );

    if (docked)
    {
        setDronePositionLocal(job, job.dockWorldPosition);
        job.droneVelocity = glm::vec3(0.0f);
        job.droneNav.clear();

        job.state = ObjectRepairJobState::Done;

        (void)0;
    }

    continue;
}

      

        

        auto* fragment =
            detachedRuntime.findFragmentMutable(job.moduleId);

        if (!fragment)
        {
            job.state = ObjectRepairJobState::Failed;

            (void)0;

            continue;
        }

        const glm::mat4 targetWorldModel =
            ownerModel * fragment->homeLocalModel;

        const glm::vec3 homePosition =
            glm::vec3(targetWorldModel * glm::vec4(0, 0, 0, 1));

        const glm::vec3 homeCenterWorld =
            glm::vec3(ownerModel * glm::vec4(fragment->homeCenterLocal, 1.0f));

        glm::mat4 homeOrientation = targetWorldModel;
        homeOrientation[3] = glm::vec4(0, 0, 0, 1);




        RepairUpdateContext ctx;
        ctx.dt = dt;
        ctx.ownerModel = &ownerModel;
        ctx.job = &job;
        ctx.fragment = fragment;
        ctx.homeCenterWorld = homeCenterWorld;
        ctx.homeOrientation = homeOrientation;
        ctx.obstacles = &obstacles;

        debugEvaluateRepairDroneTacticalRisk(
            job,
            obstacles
        );


        switch (job.state)
        {


case ObjectRepairJobState::LeavingDock:
{
    const bool arrived =
        updateDroneAlongCurrentPath(
            ctx,
            job.droneSpeed,
            job.droneAcceleration,
            0.6f
        );

    if (arrived)
    {
        const glm::vec3 fragmentCenterWorld =
            getFragmentCenterWorld(*fragment);

        const glm::vec3 ownerCenter =
            glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

         glm::vec3 outsideNormal =
            safeNormalizeOr(
                homeCenterWorld - ownerCenter,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

        const float routeStandOffDistance = 8.0f;

        const glm::vec3 firstRouteTarget =
            fragmentCenterWorld + outsideNormal * routeStandOffDistance;

        job.state = ObjectRepairJobState::MovingToFragment;
        job.pathRebuildTimer = 0.0f;
        job.currentPathTarget = firstRouteTarget;

        // ВАЖНО:
        // строим первый маршрут СРАЗУ после выхода из дока.
        // Не ждём следующего кадра.
        tryReplaceRepairDronePath(
            job,
            job.dronePosition,
            firstRouteTarget,
            obstacles,
            false,
            world::navigation::SmallCraftWaypointKind::Transit,
            "initial-to-moving-fragment-zone"
        );

        (void)0;
    }

    break;
}






case ObjectRepairJobState::MovingToFragment:
{
    const glm::vec3 fragmentCenterWorld =
        getFragmentCenterWorld(*fragment);

    const glm::vec3 ownerCenter =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

     glm::vec3 outsideNormal =
        safeNormalizeOr(
            homeCenterWorld - ownerCenter,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

    if (job.moduleId.find("_top_") != std::string::npos ||
        job.moduleId.find("top") != std::string::npos)
    {
        outsideNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    /*
        Дальняя цель планировщика.

        Это НЕ точка захвата.
        Это безопасная внешняя зона рядом с деталью.
        Планировщик должен вывести дрона сюда, но не должен заставлять его
        останавливаться.
    */
    const float routeStandOffDistance = 12.0f;

    const glm::vec3 routeTargetWorld =
        fragmentCenterWorld + outsideNormal * routeStandOffDistance;

    /*
        Точная точка захвата.

        Это уже для ближнего pursuit-режима, не для obstacle path planner.
    */
    const float latchStandOffDistance =
        glm::max(job.captureFinalDistance, 2.0f);

    const glm::vec3 latchTargetWorld =
        fragmentCenterWorld + outsideNormal * latchStandOffDistance;



    const float distanceToRouteTarget =
        glm::length(routeTargetWorld - job.dronePosition);

    const float distanceToLatchTarget =
        glm::length(latchTargetWorld - job.dronePosition);

    const glm::vec3 droneFromFragment =
        safeNormalizeOr(
            job.dronePosition - fragmentCenterWorld,
            -outsideNormal
        );

    const float outsideSide =
        glm::dot(droneFromFragment, outsideNormal);

    const bool droneIsOutside =
        outsideSide > 0.10f;







    /*
        Ближний режим.

        Если дрон уже снаружи и близко — выключаем маршрут.
        Теперь нужно не "идти по waypoint", а догонять живую точку каждый кадр.
    */
    if (droneIsOutside && distanceToRouteTarget <= 10.0f)
    {
        job.droneNav.clear();

        const bool arrived =
            updateDronePursuitToMovingPoint(
                ctx,
                latchTargetWorld,
                job.droneSpeed * 1.75f,
                job.droneAcceleration,
                0.35f
            );

        if (arrived || distanceToLatchTarget <= 0.45f)
        {
            job.captureTimer = 0.0f;
            job.state = ObjectRepairJobState::CapturingFragment;
            job.droneNav.clear();

            (void)0;
        }

        break;
    }

    /*
        Дальняя навигация.

        Маршрут строится сразу и потом уточняется во время полёта.
        Старый маршрут НЕ стираем до успешной постройки нового.
    */
    job.pathRebuildTimer += dt;

    const bool targetMovedFar =
        glm::length2(routeTargetWorld - job.currentPathTarget) >
        2.0f * 2.0f;

    const bool mayRebuildPath =
        job.pathRebuildTimer >= 0.25f;

    if (!job.droneNav.hasPath() || (targetMovedFar && mayRebuildPath))
    {
        const bool replaced =
            tryReplaceRepairDronePath(
                job,
                job.dronePosition,
                routeTargetWorld,
                obstacles,
                false,
                world::navigation::SmallCraftWaypointKind::Transit,
                "replan-to-moving-fragment-zone"
            );

        if (replaced)
        {
            job.currentPathTarget = routeTargetWorld;
        }

        job.pathRebuildTimer = 0.0f;
    }

    const bool pathEnded =
        updateDroneAlongCurrentPath(
            ctx,
            job.droneSpeed * 1.25f,
            job.droneAcceleration,
            1.2f
        );

    /*
        Если Transit-путь закончился — это не ремонт и не захват.
        Это просто значит: мы пришли в старую внешнюю зону.
        На следующем кадре маршрут пересоберётся к новой позиции детали.
    */
    if (pathEnded)
    {
        job.droneNav.clear();
    }

    break;
}





case ObjectRepairJobState::CapturingFragment:
{
    // Пока дрон НЕ зацепился — деталь живёт своей физикой.
    // Не гасим скорость заранее, иначе она замирает перед захватом.

    const glm::vec3 fragmentCenterWorld =
        getFragmentCenterWorld(*fragment);

    const glm::vec3 ownerCenter =
        glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

    // Нормаль наружу берём от текущей позиции детали,
    // а не от посадочного места. Нам нужно подлететь к самой детали снаружи.
     glm::vec3 outsideNormal =
    safeNormalizeOr(
        homeCenterWorld - ownerCenter,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    if (job.moduleId.find("_top_") != std::string::npos ||
        job.moduleId.find("top") != std::string::npos)
    {
        outsideNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // Финальная точка захвата — чуть снаружи от центра детали.
    // Дрон не должен вставать в центр детали и не должен тянуть её к себе.
    const float latchDistance =
        glm::max(job.captureFinalDistance, 2.0f);

    const glm::vec3 latchPointWorld =
        fragmentCenterWorld + outsideNormal * latchDistance;

    moveDronePositionLocal(
        job,
        moveTowardsPoint(
            job.dronePosition,
            latchPointWorld,
            glm::max(job.captureAttachSpeed, job.droneSpeed * 1.5f) * ctx.dt
        ),
        ctx.dt
    );

    const float dist2 =
        glm::length2(job.dronePosition - latchPointWorld);

    const float latchRadius = 0.75f;

    if (dist2 > latchRadius * latchRadius)
        break;

    // Жёстко фиксируем дрон в точке захвата.
    fragment->linearVelocity = glm::vec3(0.0f);
    fragment->angularVelocity = glm::vec3(0.0f);
    setDronePositionLocal(job, latchPointWorld);
    job.droneVelocity = glm::vec3(0.0f);

    // КРИТИЧЕСКОЕ МЕСТО:
    // offset должен быть фактическим, а не вычисленным "примерно".
    // Тогда деталь не прыгает на дрон.
    job.towOffsetWorld =
        fragmentCenterWorld - job.dronePosition;

     glm::vec3 homeNormal =
        computeRepairOutsideNormal(
            job.moduleId,
            homeCenterWorld,
            ownerCenter
        );

    if (job.moduleId.find("top") != std::string::npos)
    {
        homeNormal = glm::vec3(0.0f, 1.0f, 0.0f);
    }

    job.stagingWorldPosition =
        homeCenterWorld + homeNormal * job.stagingDistance;

    job.droneNav.clear();

    const glm::vec3 droneStagingTarget =
        job.stagingWorldPosition - job.towOffsetWorld;

    if (!rebuildRepairDronePath(
            job,
            job.dronePosition,
            droneStagingTarget,
            obstacles,
            true,
            "to-staging"))
    {
        job.state = ObjectRepairJobState::Failed;
        break;
    }

    job.state = ObjectRepairJobState::ReturningToStaging;

    (void)0;

    
    break;
}



case ObjectRepairJobState::ReturningToStaging:
{
    const bool arrivedToStaging =
        updateTowFragmentToPoint(
            ctx,
            job.stagingWorldPosition,
            job.droneTowSpeed,
            job.droneTowAcceleration,
            0.75f,
            true,
            job.fragmentAlignRate
        );

    if (arrivedToStaging)
    {
        job.alignTimer = 0.0f;
        job.alignStartPosition = fragment->position;
        job.alignStartOrientation = fragment->orientation;

        job.state = ObjectRepairJobState::AligningToMount;
        job.droneNav.clear();

        (void)0;
    }

    break;
}




case ObjectRepairJobState::AligningToMount:
{
    if (updateAlignFragmentToMount(ctx))
    {
        fragment->orientation = homeOrientation;
        job.state = ObjectRepairJobState::FinalMounting;

        (void)0;
    }

    break;
}


case ObjectRepairJobState::FinalMounting:
{
    if (updateFinalMounting(ctx))
    {
        completed.push_back(job.moduleId);

        job.droneNav.clear();
        job.droneVelocity = glm::vec3(0.0f);
        job.postRepairPauseTimer = 0.0f;

        const glm::vec3 ownerCenter =
            glm::vec3(ownerModel * glm::vec4(0, 0, 0, 1));

        const glm::vec3 mountNormal =
            computeRepairOutsideNormal(
                job.moduleId,
                homeCenterWorld,
                ownerCenter
            );

        job.postRepairExitWorldPosition =
            homeCenterWorld + mountNormal * job.postRepairRetreatDistance;

        job.state = ObjectRepairJobState::PostRepairPause;

        (void)0;
    }

    break;
}

case ObjectRepairJobState::PostRepairPause:
{
    break;
}

case ObjectRepairJobState::PostRepairRetreat:
{
    break;
}

case ObjectRepairJobState::Docking:
{
    break;
}


case ObjectRepairJobState::ReadyToReattach:
{
    break;
}

case ObjectRepairJobState::ReturningToDock:
{
    break;
}

case ObjectRepairJobState::Done:
case ObjectRepairJobState::Failed:
{
    break;
}
        }
    }

    m_jobs.erase(
        std::remove_if(
            m_jobs.begin(),
            m_jobs.end(),
            [](const ObjectRepairJobRuntimeState& job)
            {
                return job.state == ObjectRepairJobState::Done ||
                        job.state == ObjectRepairJobState::Failed;
            }
        ),
        m_jobs.end()
    );

    return completed;
}





int ObjectRepairJobRuntime::activeJobCount() const
{
    int count = 0;

    for (const auto& job : m_jobs)
    {
        if (job.state != ObjectRepairJobState::Done &&
            job.state != ObjectRepairJobState::Failed)
        {
            ++count;
        }
    }

    return count;
}



std::vector<game::simulation::ObjectRepairJobSnapshot>
ObjectRepairJobRuntime::buildSnapshots(
    const glm::mat4& ownerLocalModel,
    const world::coordinates::WorldPosition& ownerWorldPosition,
    const ObjectDetachedFragmentRuntime& detachedRuntime
) const
{
    std::vector<game::simulation::ObjectRepairJobSnapshot> out;

    for (const auto& job : m_jobs)
    {
        const auto* fragment =
                detachedRuntime.findFragment(job.moduleId);


game::simulation::ObjectRepairJobSnapshot s;
s.moduleId = job.moduleId;
// Legacy mirror: owner-local.
s.dronePosition = job.dronePosition;

// Global WorldPosition для рендера/debug.
s.droneWorldPosition =
    world::coordinates::translated(
        ownerWorldPosition,
        glm::dvec3(job.dronePosition)
    );

s.state = static_cast<uint8_t>(job.state);

if (fragment)
{
    // Legacy mirrors: owner-local.
s.fragmentPosition =
    getFragmentCenterWorld(*fragment);

s.homePosition =
    glm::vec3(ownerLocalModel * glm::vec4(fragment->homeCenterLocal, 1.0f));

// Global WorldPosition для рендера/debug.
s.fragmentWorldPosition =
    world::coordinates::translated(
        ownerWorldPosition,
        glm::dvec3(s.fragmentPosition)
    );

s.homeWorldPosition =
    world::coordinates::translated(
        ownerWorldPosition,
        glm::dvec3(s.homePosition)
    );
}
else
{
s.fragmentWorldPosition =
    world::coordinates::translated(
        ownerWorldPosition,
        glm::dvec3(s.fragmentPosition)
    );

s.homeWorldPosition =
    world::coordinates::translated(
        ownerWorldPosition,
        glm::dvec3(s.homePosition)
    );
}

out.push_back(std::move(s));
            }

    return out;
}





} // namespace world::modules