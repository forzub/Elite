#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <unordered_map>

#include "src/game/simulation/SimulationSnapshot.h"
#include "src/game/simulation/ShipReferenceFrameSnapshot.h"
#include "src/game/navigation/HubAttachedPose.h"

namespace game::network
{
namespace detail
{
template <typename T, typename IdFn, typename IdT>
inline void eraseById(
    std::vector<T>& values,
    const IdT& id,
    IdFn&& idOf
)
{
    values.erase(
        std::remove_if(
            values.begin(),
            values.end(),
            [&](const T& value)
            {
                return idOf(value) == id;
            }
        ),
        values.end()
    );
}

template <typename T, typename IdFn>
inline void upsertById(
    std::vector<T>& values,
    const T& incoming,
    IdFn&& idOf
)
{
    const auto incomingId = idOf(incoming);
    const auto it = std::find_if(
        values.begin(),
        values.end(),
        [&](const T& value)
        {
            return idOf(value) == incomingId;
        }
    );

    if (it == values.end())
        values.push_back(incoming);
    else
        *it = incoming;
}

inline game::simulation::ObjectGraphSnapshot materializeGraphSnapshot(
    const game::simulation::ObjectGraphSnapshot* previous,
    const game::simulation::ObjectGraphSnapshot& incoming
)
{
    auto canonical = incoming;
    if (!previous)
        return canonical;

    // Entity presence and field payload completeness are separate concepts.
    // Even a FullAuthoritativeSet snapshot intentionally omits heavy graph
    // fields unless they changed, so canonical/hydration state must retain
    // those fields independently from entity-set semantics.
    if (!incoming.hasModules)
    {
        canonical.hasModules = previous->hasModules;
        canonical.modules = previous->modules;
    }

    if (!incoming.hasStructuralLinks)
    {
        canonical.hasStructuralLinks = previous->hasStructuralLinks;
        canonical.structuralLinks = previous->structuralLinks;
    }

    if (!incoming.hasAssemblyModules)
    {
        canonical.hasAssemblyModules = previous->hasAssemblyModules;
        canonical.assemblyModules = previous->assemblyModules;
    }

    if (!incoming.hasDetachedFragments)
    {
        canonical.hasDetachedFragments = previous->hasDetachedFragments;
        canonical.detachedFragments = previous->detachedFragments;
    }

    if (!incoming.hasRepairJobs)
    {
        canonical.hasRepairJobs = previous->hasRepairJobs;
        canonical.repairJobs = previous->repairJobs;
    }

    if (!incoming.hasDebugHitVolumes)
    {
        canonical.hasDebugHitVolumes = previous->hasDebugHitVolumes;
        canonical.debugHitVolumes = previous->debugHitVolumes;
    }

    return canonical;
}

inline ShipSnapshot materializeShipSnapshot(
    const ShipSnapshot* previous,
    const ShipSnapshot& incoming
)
{
    ShipSnapshot canonical = incoming;
    canonical.graph = materializeGraphSnapshot(
        previous ? &previous->graph : nullptr,
        incoming.graph
    );
    return canonical;
}

inline ObjectSnapshot materializeObjectSnapshot(
    const ObjectSnapshot* previous,
    const ObjectSnapshot& incoming
)
{
    ObjectSnapshot canonical = incoming;
    canonical.graph = materializeGraphSnapshot(
        previous ? &previous->graph : nullptr,
        incoming.graph
    );
    return canonical;
}

// deltaSeconds: the SERVER time between the two epochs (the simulation's own clock - the kinematics); frameUniverseTime:
// the incoming epoch's UNIVERSE time when it continues the same timeline, NaN otherwise (audit #8 B1: universe time may
// branch on a debug rewind and need not advance with server time - the frame's universe time is set to the epoch's,
// never extrapolated by the server delta)
inline void advanceRetainedShipPresentation(
    ShipSnapshot& ship,
    double deltaSeconds,
    double frameUniverseTime = std::numeric_limits<double>::quiet_NaN()
)
{
    if (!std::isfinite(deltaSeconds) || deltaSeconds <= 0.0)
        return;

    // Sparse publication deliberately omits a ship for several transport
    // epochs.  A retained row must not pretend that its old position belongs
    // to each newer packet time: that creates A,A,A,B presentation history
    // and visible stop/jump motion at replication-tier boundaries.  Advance
    // the retained presentation row with the last authoritative kinematics;
    // the next real publication replaces this estimate completely.
    const auto& motion = ship.transform.motion;
    const glm::dvec3 accelerationMps2 =
        motion.engineAccelerationMps2 + motion.gravityAccelerationMps2;
    // ONE formula (audit #6 B1/B2): a ship in a reference frame is advanced IN its frame - the frame's origin, velocity
    // and AXES (its turn: a ring deck turns ~0.09 rad/s) and the ship's local position - and its world position /
    // velocity are DERIVED from them. The old row advanced the world point along its velocity, the frame origin along
    // its own, and never turned the axes: the world point and origin + axes * local drifted apart (~omega r per second
    // on a ring deck) and a ship standing on a turning deck slid off its arc.
    if (ship.referenceFrame.valid)
    {
        auto& frame = ship.referenceFrame;
        // the ship's OWN acceleration (its engines; gravity rides the frame's acceleration) in the frame's axes at the
        // start of the interval (audit #8 B1: a thrusting ship in a frame was extrapolated at constant local velocity)
        const glm::dmat3 B0(frame.progradeAxis, frame.radialAxis, frame.normalAxis);
        const glm::dvec3 ownLocalAcceleration = glm::transpose(B0) * motion.engineAccelerationMps2;
        frame.originMeters += frame.velocityMetersPerSecond * deltaSeconds +
                              0.5 * frame.accelerationMetersPerSecond2 * deltaSeconds * deltaSeconds;
        frame.velocityMetersPerSecond += frame.accelerationMetersPerSecond2 * deltaSeconds;
        const double turn = glm::length(frame.angularVelocityWorldRadPerSecond) * deltaSeconds;
        if (turn > 1.0e-12)
        {
            const glm::dvec3 axis = glm::normalize(frame.angularVelocityWorldRadPerSecond);
            const auto rotate = [&](const glm::dvec3& v) {
                return v * std::cos(turn) + glm::cross(axis, v) * std::sin(turn) + axis * glm::dot(axis, v) * (1.0 - std::cos(turn));
            };
            frame.progradeAxis = rotate(frame.progradeAxis);
            frame.radialAxis = rotate(frame.radialAxis);
            frame.normalAxis = rotate(frame.normalAxis);
            // the hull turns with the frame it rides (its own rates are relative motion on top)
            const glm::dmat3 turnM(rotate(glm::dvec3(1.0, 0.0, 0.0)), rotate(glm::dvec3(0.0, 1.0, 0.0)),
                                   rotate(glm::dvec3(0.0, 0.0, 1.0)));
            ship.transform.orientation = glm::mat4(glm::mat3(turnM)) * ship.transform.orientation;
        }
        frame.localPositionMeters += frame.localVelocityMetersPerSecond * deltaSeconds +
                                     0.5 * ownLocalAcceleration * deltaSeconds * deltaSeconds;
        frame.localVelocityMetersPerSecond += ownLocalAcceleration * deltaSeconds;
        if (std::isfinite(frameUniverseTime))
            frame.universeTimeSeconds = frameUniverseTime;
        const glm::dmat3 B(frame.progradeAxis, frame.radialAxis, frame.normalAxis);
        const glm::dvec3 worldNow = frame.originMeters + B * frame.localPositionMeters;
        ship.transform.addWorldMeters(worldNow - ship.transform.fullWorldMeters());
        ship.transform.motion.worldVelocityMps = frame.velocityMetersPerSecond +
                                                 glm::cross(frame.angularVelocityWorldRadPerSecond, B * frame.localPositionMeters) +
                                                 B * frame.localVelocityMetersPerSecond;
    }
    else
    {
        const glm::dvec3 displacementMeters =
            motion.worldVelocityMps * deltaSeconds +
            0.5 * accelerationMps2 * deltaSeconds * deltaSeconds;
        ship.transform.addWorldMeters(displacementMeters);
        ship.transform.motion.worldVelocityMps += accelerationMps2 * deltaSeconds;
    }

    // Keep remote attitude smooth across the same sparse interval. Angular
    // rates are already authoritative body-control state and use radians/sec.
    const float dt = static_cast<float>(deltaSeconds);
    const glm::vec3 right = ship.transform.right();
    const glm::vec3 up = ship.transform.up();
    const glm::vec3 forward = ship.transform.forward();
    const glm::mat4 yawRot = glm::rotate(
        glm::mat4(1.0f),
        ship.transform.yawRate * dt,
        up
    );
    const glm::mat4 pitchRot = glm::rotate(
        glm::mat4(1.0f),
        ship.transform.pitchRate * dt,
        right
    );
    const glm::mat4 rollRot = glm::rotate(
        glm::mat4(1.0f),
        ship.transform.rollRate * dt,
        forward
    );
    ship.transform.orientation =
        yawRot * pitchRot * rollRot * ship.transform.orientation;

    // the motion fields that ride as the referenceFrame (travel frame, local position / velocity, reference velocity)
    // are rebuilt from the advanced frame - the SAME rule as after decoding (audit #8 B1: one representation, no
    // second integration of motion.local* that could drift from the frame)
    if (ship.referenceFrame.valid)
        game::simulation::restoreMotionFromReferenceFrame(ship.transform.motion, ship.referenceFrame);
    else
        ship.transform.motion.localPositionMeters += ship.transform.motion.localVelocityMps * deltaSeconds;

    // (the reference frame was advanced above, with its turn, and the world point derived from it)
}
} // namespace detail

/*
    Materialize one canonical/full-presence history sample from a possibly
    sparse transport snapshot.

    ClientWorldState applies only the incoming updates/removals to live entity
    state, but map/interpolation history needs a complete retained set at each
    server epoch. Sparse omission therefore inherits the previous canonical
    entity row, while explicit removals erase it.

    The server protocol guarantees a FullAuthoritativeSet bootstrap before any
    SparseRetainMissing packet. If no baseline is supplied here, the incoming
    packet is returned as-is; callers should treat that as a bootstrap fault if
    sparse publication is ever enabled without a prior full sample.
*/
inline SimulationSnapshot materializeCanonicalReplicationSnapshot(
    const SimulationSnapshot* previousCanonical,
    const SimulationSnapshot& incoming
)
{
    using Mode = ReplicatedEntitySetMode;

    if (previousCanonical == nullptr)
    {
        SimulationSnapshot canonical = incoming;
        canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
        canonical.replication.removedShipIds.clear();
        canonical.replication.removedObjectIds.clear();
        canonical.replication.removedHubIds.clear();
        return canonical;
    }

    if (incoming.replication.entitySetMode == Mode::FullAuthoritativeSet)
    {
        // FullAuthoritativeSet means the entity vectors are a complete presence
        // set. It does NOT mean every heavy nested graph field is present. Build
        // a new complete entity set while retaining omitted graph fields from
        // the previous canonical sample for entities that still exist.
        SimulationSnapshot canonical = incoming;
        canonical.ships.clear();
        canonical.objects.clear();

        canonical.ships.reserve(incoming.ships.size());
        for (const auto& ship : incoming.ships)
        {
            const auto previousIt = std::find_if(
                previousCanonical->ships.begin(),
                previousCanonical->ships.end(),
                [&](const ShipSnapshot& value)
                {
                    return value.id == ship.id;
                }
            );

            canonical.ships.push_back(
                detail::materializeShipSnapshot(
                    previousIt == previousCanonical->ships.end()
                        ? nullptr
                        : &*previousIt,
                    ship
                )
            );
        }

        canonical.objects.reserve(incoming.objects.size());
        for (const auto& object : incoming.objects)
        {
            const auto previousIt = std::find_if(
                previousCanonical->objects.begin(),
                previousCanonical->objects.end(),
                [&](const ObjectSnapshot& value)
                {
                    return value.id == object.id;
                }
            );

            canonical.objects.push_back(
                detail::materializeObjectSnapshot(
                    previousIt == previousCanonical->objects.end()
                        ? nullptr
                        : &*previousIt,
                    object
                )
            );
        }

        canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
        canonical.replication.removedShipIds.clear();
        canonical.replication.removedObjectIds.clear();
        canonical.replication.removedHubIds.clear();
        return canonical;
    }

    SimulationSnapshot canonical = *previousCanonical;

    const double retainedDeltaSeconds =
        incoming.metadata.serverTimeSeconds -
        previousCanonical->metadata.serverTimeSeconds;
    if (std::isfinite(retainedDeltaSeconds) && retainedDeltaSeconds > 0.0)
    {
        for (auto& retainedShip : canonical.ships)
        {
            detail::advanceRetainedShipPresentation(
                retainedShip,
                retainedDeltaSeconds,
                incoming.metadata.universeTimelineRevision ==
                        previousCanonical->metadata.universeTimelineRevision
                    ? incoming.metadata.universeTimeSeconds
                    : std::numeric_limits<double>::quiet_NaN()
            );
        }
    }

    canonical.metadata = incoming.metadata;
    canonical.session = incoming.session;
    canonical.signals = incoming.signals;

    for (const auto& ship : incoming.ships)
    {
        const auto previousIt = std::find_if(
            canonical.ships.begin(),
            canonical.ships.end(),
            [&](const ShipSnapshot& value)
            {
                return value.id == ship.id;
            }
        );

        const auto materialized = detail::materializeShipSnapshot(
            previousIt == canonical.ships.end() ? nullptr : &*previousIt,
            ship
        );

        detail::upsertById(
            canonical.ships,
            materialized,
            [](const ShipSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& object : incoming.objects)
    {
        const auto previousIt = std::find_if(
            canonical.objects.begin(),
            canonical.objects.end(),
            [&](const ObjectSnapshot& value)
            {
                return value.id == object.id;
            }
        );

        const auto materialized = detail::materializeObjectSnapshot(
            previousIt == canonical.objects.end() ? nullptr : &*previousIt,
            object
        );

        detail::upsertById(
            canonical.objects,
            materialized,
            [](const ObjectSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& hub : incoming.hubs)
    {
        detail::upsertById(
            canonical.hubs,
            hub,
            [](const game::simulation::OrbitalHubSnapshot& value)
                -> const std::string&
            {
                return value.id;
            }
        );
    }

    // the hub-attached objects the server did NOT re-send (their pose follows from the hub: HubAttachedPose.h, the
    // server's own law, 1 mm live): re-posed from THIS snapshot's hub rows at its universe time - history, the render
    // interpolation, the maps and the planner read them as if sent (author 2026-11-24: «зачем пересылать объекты хабов»)
    {
        std::unordered_map<std::uint32_t, bool> sent;
        for (const auto& object : incoming.objects)
            sent[object.id.value] = true;
        for (auto& object : canonical.objects)
        {
            if (sent.count(object.id.value) || !game::navigation::hubAttachedObjectDerivable(object))
                continue;
            const auto hubIt = std::find_if(canonical.hubs.begin(), canonical.hubs.end(),
                                            [&](const game::simulation::OrbitalHubSnapshot& h) { return h.id == object.hubAttachment.hubId; });
            if (hubIt == canonical.hubs.end())
                continue;   // its hub not here: kept as it was
            game::navigation::reposeHubAttachedObject(object, *hubIt, incoming.metadata.universeTimeSeconds);
        }
    }

    for (const auto id : incoming.replication.removedShipIds)
    {
        detail::eraseById(
            canonical.ships,
            id.value,
            [](const ShipSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto id : incoming.replication.removedObjectIds)
    {
        detail::eraseById(
            canonical.objects,
            id.value,
            [](const ObjectSnapshot& value)
            {
                return value.id.value;
            }
        );
    }

    for (const auto& id : incoming.replication.removedHubIds)
    {
        detail::eraseById(
            canonical.hubs,
            id,
            [](const game::simulation::OrbitalHubSnapshot& value)
                -> const std::string&
            {
                return value.id;
            }
        );
    }

    // The history sample is now materialized/full even though the transport
    // packet that produced it was sparse.
    canonical.replication = incoming.replication;
    canonical.replication.entitySetMode = Mode::FullAuthoritativeSet;
    canonical.replication.removedShipIds.clear();
    canonical.replication.removedObjectIds.clear();
    canonical.replication.removedHubIds.clear();

    return canonical;
}

/*
    The server's own canonical sample from a FullAuthoritativeSet snapshot, consuming the previous
    canonical sample (load test 2026-11-08: the generic merge above deep-copied every ship 2-3
    times and searched linearly - 14 ms per publication at 161 ships). Same result as the generic
    FullAuthoritativeSet path: the incoming entity set, the omitted heavy graph fields taken (moved)
    from the previous sample.
*/
inline SimulationSnapshot materializeCanonicalFromFullSet(
    SimulationSnapshot&& previousCanonical,
    const SimulationSnapshot& incoming
)
{
    SimulationSnapshot canonical;
    canonical.metadata = incoming.metadata;
    canonical.replication = incoming.replication;
    canonical.replication.entitySetMode = ReplicatedEntitySetMode::FullAuthoritativeSet;
    canonical.replication.removedShipIds.clear();
    canonical.replication.removedObjectIds.clear();
    canonical.replication.removedHubIds.clear();
    canonical.signals = incoming.signals;
    canonical.hubs = incoming.hubs;
    canonical.session = incoming.session;

    const auto takeOmitted = [](game::simulation::ObjectGraphSnapshot& to, game::simulation::ObjectGraphSnapshot& from) {
        if (!to.hasModules)
        {
            to.hasModules = from.hasModules;
            to.modules = std::move(from.modules);
        }
        if (!to.hasStructuralLinks)
        {
            to.hasStructuralLinks = from.hasStructuralLinks;
            to.structuralLinks = std::move(from.structuralLinks);
        }
        if (!to.hasAssemblyModules)
        {
            to.hasAssemblyModules = from.hasAssemblyModules;
            to.assemblyModules = std::move(from.assemblyModules);
        }
        if (!to.hasDetachedFragments)
        {
            to.hasDetachedFragments = from.hasDetachedFragments;
            to.detachedFragments = std::move(from.detachedFragments);
        }
        if (!to.hasRepairJobs)
        {
            to.hasRepairJobs = from.hasRepairJobs;
            to.repairJobs = std::move(from.repairJobs);
        }
        if (!to.hasDebugHitVolumes)
        {
            to.hasDebugHitVolumes = from.hasDebugHitVolumes;
            to.debugHitVolumes = std::move(from.debugHitVolumes);
        }
    };

    std::unordered_map<std::uint32_t, ShipSnapshot*> previousShips;
    previousShips.reserve(previousCanonical.ships.size());
    for (auto& ship : previousCanonical.ships)
        previousShips.emplace(ship.id.value, &ship);
    canonical.ships.reserve(incoming.ships.size());
    for (const auto& ship : incoming.ships)
    {
        canonical.ships.push_back(ship);
        if (const auto it = previousShips.find(ship.id.value); it != previousShips.end())
            takeOmitted(canonical.ships.back().graph, it->second->graph);
    }

    std::unordered_map<std::uint32_t, ObjectSnapshot*> previousObjects;
    previousObjects.reserve(previousCanonical.objects.size());
    for (auto& object : previousCanonical.objects)
        previousObjects.emplace(object.id.value, &object);
    canonical.objects.reserve(incoming.objects.size());
    for (const auto& object : incoming.objects)
    {
        canonical.objects.push_back(object);
        if (const auto it = previousObjects.find(object.id.value); it != previousObjects.end())
            takeOmitted(canonical.objects.back().graph, it->second->graph);
    }
    return canonical;
}

} // namespace game::network
