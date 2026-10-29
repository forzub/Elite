#pragma once

// Private helpers shared by the GameSimulation translation units
// (GameSimulation*.cpp). Not a public interface.

#include "src/game/simulation/ObjectModuleSnapshotBuilder.h"
#include "GameSimulation.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/simulation/RuntimeSystemPolicy.h"
#include "src/game/simulation/activation/ActivationSpatialIndex.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <array>

#include "game/ship/ShipInitData.h"
#include "game/ship/ShipRoleType.h"
#include "game/ship/ShipRegistry.h"
#include "game/ship/ShipVisualIdentity.h"
#include "game/ship/descriptors/EliteCobraMk1.h"

#include "game/items/cryptocard/CryptoCard.h"
#include "game/player/ActorCodeGenerator.h"
#include "src/game/player/ActorIdProvider.h"

#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/geometry/ObjectAssemblyRegistry.h"
#include "src/world/types/ObjectType.h"


#include "game/equipment/radar/RadarModule.h"
#include "src/game/RuntimeFeatureFlags.h"
#include "src/game/diagnostics/InterplanetaryTransferLab.h"
// #include "src/world/modules/ObjectHitBuilder.h"
#include "src/game/geometry/AssemblyMeshLibrary.h"

#include "src/world/modules/ObjectRuntimeHitBuilder.h"
#include "src/world/modules/HitVolumeSnapshotBuilder.h"

#include <limits>
#include <glm/gtx/norm.hpp>
#include "src/world/modules/ObjectMissingPartRequest.h"

#include "game/promo/PromoFlybyScenario.h"
#include "game/scene/GameSceneSetup.h"

#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include "src/world/coordinates/WorldPosition.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/navigation/TravelFrameSystem.h"
#include "src/game/navigation/NpcNavigationIntentController.h"
#include "src/game/navigation/autopilot/AutopilotLawController.h"
#include "src/game/navigation/NavigationHitVolumeAdapter.h"
#include "src/game/navigation/NavigationFrameBoundary.h"
#include "src/world/navigation/local/PhysicalManeuverHorizon.h"
#include "src/game/navigation/NavigationExecutionSafetyProbeBuilder.h"
#include "src/game/diagnostics/NavigationRuntimeLab.h"
#include "src/game/diagnostics/NavigationRuntimeLabAcceptedProgramAdapter.h"

namespace game_simulation_detail
{
    inline bool environmentFlagEnabled(const char* name)
    {
        const char* raw = std::getenv(name);
        if (!raw || !*raw)
            return false;

        const std::string value(raw);
        return value != "0" &&
            value != "false" &&
            value != "FALSE" &&
            value != "off" &&
            value != "OFF";
    }

    inline constexpr std::size_t FiveHundredShipCount = 500;
    inline constexpr std::size_t HerdMaterializedInteractionCount = 24;
    inline constexpr std::size_t HerdFlightSize = 3;
    inline constexpr std::size_t HerdFlightCount =
        (FiveHundredShipCount + HerdFlightSize - 1) / HerdFlightSize;
    inline constexpr double HerdRadiusMeters = 4500.0;
    inline constexpr double HerdSpeedMetersPerSecond = 180.0;
    inline constexpr double HerdVerticalWaveMeters = 260.0;
    inline constexpr double HerdWingLateralMeters = 55.0;
    inline constexpr double HerdWingTrailMeters = 85.0;
    inline constexpr double TwoPi = 6.28318530717958647692;

    struct HerdRouteSample
    {
        glm::dvec3 positionMeters {0.0};
        glm::dvec3 velocityMps {0.0};
    };

    inline HerdRouteSample sampleHerdRoute(
        const glm::dvec3& centerMeters,
        std::size_t flightIndex,
        std::size_t slotInFlight,
        double elapsedSeconds
    )
    {
        const double omega =
            HerdSpeedMetersPerSecond / HerdRadiusMeters;
        const double phase =
            TwoPi *
                static_cast<double>(flightIndex) /
                static_cast<double>(HerdFlightCount) +
            omega * std::max(0.0, elapsedSeconds);

        const glm::dvec3 radial(
            std::cos(phase),
            0.0,
            std::sin(phase)
        );
        const glm::dvec3 tangent(
            -std::sin(phase),
            0.0,
            std::cos(phase)
        );
        const glm::dvec3 up(0.0, 1.0, 0.0);

        double lateral = 0.0;
        double trail = 0.0;
        if (slotInFlight == 1)
        {
            lateral = -HerdWingLateralMeters;
            trail = -HerdWingTrailMeters;
        }
        else if (slotInFlight == 2)
        {
            lateral = HerdWingLateralMeters;
            trail = -HerdWingTrailMeters;
        }

        const double vertical =
            HerdVerticalWaveMeters * std::sin(phase * 2.0);
        const double verticalVelocity =
            HerdVerticalWaveMeters * 2.0 * omega *
            std::cos(phase * 2.0);

        HerdRouteSample sample;
        sample.positionMeters =
            centerMeters +
            radial * (HerdRadiusMeters + lateral) +
            tangent * trail +
            up * vertical;

        // d(radial)/dt = tangent*omega,
        // d(tangent)/dt = -radial*omega.
        sample.velocityMps =
            tangent * (omega * (HerdRadiusMeters + lateral)) -
            radial * (omega * trail) +
            up * verticalVelocity;
        return sample;
    }

    inline game::navigation::KinematicFrame makeDiagnosticIdentityFrame(
        int systemId,
        const std::string& id,
        const glm::dvec3& originMeters
    )
    {
        game::navigation::KinematicFrame frame;
        frame.systemId = systemId;
        frame.frameId = id;
        frame.originMeters = originMeters;
        frame.localToWorldBasis = glm::dmat3(1.0);
        frame.valid = true;
        return frame;
    }

    inline bool physicalModelFromLabel(
        const std::string& label,
        game::ship::plant::PhysicalControlModelId& outModel
    )
    {
        using Model = game::ship::plant::PhysicalControlModelId;

        if (label == "Assisted-I")
            outModel = Model::AssistedI;
        else if (label == "Assisted-Y")
            outModel = Model::AssistedY;
        else if (label == "Assisted-X")
            outModel = Model::AssistedX;
        else if (label == "Newtonian-I")
            outModel = Model::NewtonianI;
        else if (label == "Newtonian-Y")
            outModel = Model::NewtonianY;
        else if (label == "Newtonian-X")
            outModel = Model::NewtonianX;
        else
            return false;

        return true;
    }

    inline const char* physicalModelName(
        game::ship::plant::PhysicalControlModelId model
    )
    {
        using Model = game::ship::plant::PhysicalControlModelId;
        switch (model)
        {
            case Model::AssistedI: return "Assisted-I";
            case Model::AssistedY: return "Assisted-Y";
            case Model::AssistedX: return "Assisted-X";
            case Model::NewtonianI: return "Newtonian-I";
            case Model::NewtonianY: return "Newtonian-Y";
            case Model::NewtonianX: return "Newtonian-X";
            default: return "unknown";
        }
    }

    inline const char* physicalDamageProfileName(
        game::ship::plant::PhysicalControlModelId model,
        std::size_t profile
    )
    {
        const bool assisted =
            game::ship::plant::familyOf(model) ==
            game::ship::plant::PhysicalControlFamily::Assisted;

        switch (profile)
        {
            case 0: return "healthy";
            case 1: return "aft-main-thrust-50";
            case 2: return "aft-main-off";
            case 3: return "main-vectoring-50";
            case 4: return "main-vectoring-off";
            case 5:
                return assisted
                    ? "fore-main-thrust-50"
                    : "manoeuvre-thrust-50";
            case 6:
                return assisted
                    ? "fore-main-off"
                    : "manoeuvre-off";
            default: return "unknown";
        }
    }

    inline glm::mat4 orientationFromVelocity(const glm::dvec3& velocity)
    {
        glm::vec3 forward = glm::vec3(velocity);
        if (glm::length2(forward) < 1.0e-8f)
            forward = glm::vec3(0.0f, 0.0f, -1.0f);
        else
            forward = glm::normalize(forward);

        glm::vec3 up(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::cross(forward, up);
        if (glm::length2(right) < 1.0e-8f)
            right = glm::vec3(1.0f, 0.0f, 0.0f);
        else
            right = glm::normalize(right);
        up = glm::normalize(glm::cross(right, forward));

        glm::mat4 out(1.0f);
        out[0] = glm::vec4(right, 0.0f);
        out[1] = glm::vec4(up, 0.0f);
        out[2] = glm::vec4(-forward, 0.0f);
        return out;
    }

    inline glm::dvec3 passiveTrajectoryAcceleration(
        const glm::dvec3& relativePositionMeters,
        double gravitationalParameterM3s2
    )
    {
        const double radiusSquared =
            glm::length2(relativePositionMeters);

        if (gravitationalParameterM3s2 <= 0.0 ||
            radiusSquared <= 1.0)
        {
            return glm::dvec3(0.0);
        }

        const double radius =
            std::sqrt(radiusSquared);

        return
            -gravitationalParameterM3s2 *
            relativePositionMeters /
            (radiusSquared * radius);
    }

    inline bool isFiniteVector(const glm::dvec3& value)
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    inline void integratePassiveTrajectoryStep(
        glm::dvec3& relativePositionMeters,
        glm::dvec3& relativeVelocityMps,
        double gravitationalParameterM3s2,
        double dt
    )
    {
        const glm::dvec3 accelerationBefore =
            passiveTrajectoryAcceleration(
                relativePositionMeters,
                gravitationalParameterM3s2
            );

        const glm::dvec3 halfStepVelocity =
            relativeVelocityMps +
            accelerationBefore * (0.5 * dt);

        relativePositionMeters +=
            halfStepVelocity * dt;

        const glm::dvec3 accelerationAfter =
            passiveTrajectoryAcceleration(
                relativePositionMeters,
                gravitationalParameterM3s2
            );

        relativeVelocityMps =
            halfStepVelocity +
            accelerationAfter * (0.5 * dt);
    }

    inline glm::dvec3 safeNormalizeD(
        const glm::dvec3& v,
        const glm::dvec3& fallback
    )
    {
        const double len2 =
            glm::dot(v, v);

        if (len2 < 1e-12)
            return fallback;

        return v / std::sqrt(len2);
    }





    inline game::navigation::NavigationFrameBoundary
    makeNavigationRuntimeLabBoundary(
        const game::navigation::HubNavigationFrame& hub
    )
    {
        game::navigation::KinematicFrame frame = hub.kinematicFrame();
        frame.frameId = hub.hubId + ":navigation_v2";
        frame.localToWorldBasis = glm::dmat3(
            hub.normalAxis,
            hub.radialAxis,
            -hub.progradeAxis
        );
        return game::navigation::NavigationFrameBoundary(frame);
    }

    inline glm::dvec3 matAxisX(const glm::mat4& m)
    {
        return glm::dvec3(m[0]);
    }

    inline glm::dvec3 matAxisY(const glm::mat4& m)
    {
        return glm::dvec3(m[1]);
    }

    inline glm::dvec3 matAxisZ(const glm::mat4& m)
    {
        return glm::dvec3(m[2]);
    }

    inline double dotD(const glm::dvec3& a, const glm::dvec3& b)
    {
        return glm::dot(a, b);
    }






    inline game::simulation::ObjectModuleSnapshot buildModuleSnapshot(
        const ModuleDescriptor& descMod,
        const world::modules::ObjectModuleState* rt
    )
    {
        return game::simulation::makeObjectModuleSnapshot(descMod, rt);
    }

    inline game::simulation::StructuralLinkSnapshot buildStructuralLinkSnapshot(
        const world::modules::StructuralLinkState& link
    )
    {
        game::simulation::StructuralLinkSnapshot ls;

        ls.id = link.id;
        ls.ownerModuleId = link.ownerModuleId;
        ls.moduleAId = link.moduleAId;
        ls.moduleBId = link.moduleBId;
        ls.kind = static_cast<int>(link.kind);

        ls.health = link.health;
        ls.maxHealth = link.maxHealth;
        ls.impulseTolerance = link.impulseTolerance;

        ls.loadBearing = link.loadBearing;
        ls.destroyed = link.destroyed;
        ls.autoGenerated = link.autoGenerated;

        ls.center = link.center;
        ls.halfSize = link.halfSize;
        ls.orientation = link.orientation;

        return ls;
    }
} // namespace game_simulation_detail

using namespace game_simulation_detail;
