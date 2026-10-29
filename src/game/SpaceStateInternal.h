#pragma once

// Private helpers shared by the SpaceState translation units
// (SpaceState*.cpp). Not a public interface.

#include "src/core/HitchProfiler.h"
#include "src/game/navigation/NavigationGeometryTolerance.h"
#include "src/game/navigation/AssistedConcurrentManeuverBudget.h"
#include "src/game/navigation/planner/BallisticEntryHandoffPolicy.h"
#include "src/game/navigation/debug/DockPlannerFileLog.h"
#include "src/game/navigation/diagnostics/AutopilotCaseFile.h"
#include "src/game/navigation/traffic/TrafficWindowCrossing.h"
#include "src/game/presentation/DockPlannerTerminalTelemetry.h"
#include "src/game/presentation/DockTerminalBitePresentation.h"
#include "src/audio/AudioEngine.h"
#include <stdexcept>
#include "src/render/legacy/CoreGlLegacyBridge.h"
#include <glad/gl.h>
#include "src/core/RuntimeTrace.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cmath>

#include <chrono>
#include <limits>
#include <algorithm>
#include <functional>
#include <filesystem>
#include <new>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "SpaceState.h"
#include "core/StateStack.h"
#include "core/log.h"
#include "input/Input.h"
#include <glm/gtx/norm.hpp>

#include "render/DebugGrid.h"

#include "src/game/player/ActorIdProvider.h"
#include "src/game/player/ActorCodeGenerator.h"


#include "src/render/camera/RenderCameraViewport.h"
#include "src/debug/DebugSettings.h"
#include "src/game/debug/DebugControlSettingsCodec.h"
#include "ui/components/UIText.h"
#include "src/game/equipment/radar/RadarDesc.h"

#include "ui/components/radar/RadarWidgetBase.h"
#include "src/game/network/ClientMessage.h"
#include "src/game/network/ClientObservationInterest.h"
#include "src/game/RuntimeFeatureFlags.h"

#include "src/render/InitShaders.h"

#include "src/game/ship/view/PlayerShipView.h"

#include "game/debug/AttachmentEditorPayload.h"
#include "game/debug/VolumeViewerPayload.h"
#include "ui/html/HtmlUiManager.h"
#include "ui/html/HtmlUiPanelId.h"
#include "ui/html/HtmlUiMessage.h"
#include <mutex>

#include "src/core/Application.h"
#include "src/game/session/IGameSession.h"
#include "src/game/client/ClientCelestialMapBridge.h"
#include "src/game/client/ClientGalaxyMapBridge.h"
#include "src/game/client/ClientModuleViewBuilder.h"
#include "src/world/descriptors/ObjectDescriptorRegistry.h"
#include "src/game/presentation/ClientHudPresentation.h"
#include "src/game/presentation/ShipBitePresentation.h"
#include "src/game/presentation/NavigationHudPresentation.h"
#include "src/game/presentation/GuidanceHudPresentation.h"
#include "src/game/presentation/GalacticCompassPresentation.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/presentation/SystemMapPanelPresentation.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/navigation/planner/RoutePlannerApi.h"
#include "src/game/navigation/planner/assisted/AssistedPlanningPipeline.h"
#include "src/game/navigation/planner/newtonian/NewtonianPlanningPipeline.h"
#include "src/game/navigation/traffic/TrafficRouteGraphCatalog.h"
#include "src/game/navigation/NavigationVolumeCatalog.h"
#include "src/game/navigation/traffic/RouteVolumeContainmentValidator.h"
#include "src/game/navigation/traffic/RouteStageCompiler.h"
#include "src/game/navigation/traffic/TrafficRoutePlanAdapter.h"
#include "src/game/navigation/traffic/SegmentedRouteComposer.h"
#include "src/game/navigation/RouteRequestCapability.h"
#include "src/game/navigation/LocalFlightControlStateMachine.h"
#include "src/game/navigation/DockingAdvisoryPortPrediction.h"
#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/DockingAutomaticRecoveryPolicy.h"
#include "src/game/navigation/DockingRouteEntryPolicy.h"
#include "src/game/navigation/assisted/AssistedSafeCourseExecutor.h"
#include "src/game/navigation/OrdinaryRoutePolicy.h"
#include "src/world/coordinates/WorldPosition.h"
#include "src/game/navigation/NavigationVehicleProfileAdapters.h"
#include "src/game/ship/ShipPropulsionState.h"
#include "src/game/ship/capability/ShipCapabilityBuilder.h"
#include "src/game/ship/computer/MotherComputerClientAdapter.h"
#include "src/game/ship/plant/ShipPhysicalControlModels.h"
#include "src/game/navigation/HubFrameBasis.h"
#include "src/game/client/ClientNavigationPlanningSnapshotFactory.h"

#include <chrono>
#include <algorithm>


#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/constants.hpp>
#include "core/Application.h"
#include "game/scene/GameSceneSetup.h"
#include "render/HUD/TextRenderer.h"
#include "src/game/presentation/TemporaryShortcutStrip.h"

namespace space_state_detail
{
    using ClientStartupClock = std::chrono::steady_clock;

    inline double startupElapsedMs(const ClientStartupClock::time_point& begin)
    {
        return std::chrono::duration<double, std::milli>(
            ClientStartupClock::now() - begin
        ).count();
    }

    inline unsigned long startupProcessId()
    {
#ifdef _WIN32
        return static_cast<unsigned long>(GetCurrentProcessId());
#else
        return 0ul;
#endif
    }

    inline unsigned long startupForegroundProcessId()
    {
#ifdef _WIN32
        const HWND foreground = GetForegroundWindow();
        if (!foreground)
            return 0ul;

        DWORD pid = 0;
        GetWindowThreadProcessId(foreground, &pid);
        return static_cast<unsigned long>(pid);
#else
        return 0ul;
#endif
    }

    inline void traceStartupStage(
        const char* stage,
        const ClientStartupClock::time_point& stageBegin,
        const ClientStartupClock::time_point& totalBegin)
    {
        if (core::runtimeTraceEnabled())
            std::cerr
                << "[M8E-STARTUP][space] pid=" << startupProcessId()
                << " stage=" << stage
                << " duration_ms=" << startupElapsedMs(stageBegin)
                << " total_ms=" << startupElapsedMs(totalBegin)
    #ifdef _WIN32
                << " uptime_ms=" << static_cast<unsigned long long>(GetTickCount64())
    #endif
                << " foreground_pid=" << startupForegroundProcessId()
                << " thread=" << std::this_thread::get_id()
                << "\n";
    }

    inline double stableBodyPhaseRadians(const std::string& id)
    {
        uint32_t h = 2166136261u;

        for (unsigned char c : id)
        {
            h ^= c;
            h *= 16777619u;
        }

        const double t =
            static_cast<double>(h % 10000u) / 10000.0;

        return t * glm::two_pi<double>();
    }





    inline double nowMs()
    {
        using clock = std::chrono::high_resolution_clock;
        return std::chrono::duration<double, std::milli>(
            clock::now().time_since_epoch()
        ).count();
    }

    inline std::string localizedUiText(
        const Application* app,
        const std::string& key,
        const std::string& fallback
    )
    {
        return app
            ? app->localization().text(key, fallback)
            : fallback;
    }

    inline std::filesystem::path debugControlDefaultsPath()
    {
        namespace fs = std::filesystem;

#ifdef _WIN32
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            if (*localAppData != '\0')
            {
                return fs::path(localAppData) /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }
#endif

        if (const char* xdgConfig = std::getenv("XDG_CONFIG_HOME"))
        {
            if (*xdgConfig != '\0')
            {
                return fs::path(xdgConfig) /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }

        if (const char* home = std::getenv("HOME"))
        {
            if (*home != '\0')
            {
                return fs::path(home) /
                    ".config" /
                    "EliteGame" /
                    "debug_control_defaults.json";
            }
        }

        return fs::current_path() /
            "debug_control_defaults.json";
    }




    inline bool isPromo1SceneMode()
    {
        return debug::get().render.sceneMode == "promo1";
    }

    template<typename TShipMap>
    inline uint64_t findAnyShipEntityId(const TShipMap& ships)
    {
        if (ships.empty())
            return 0;

        return ships.begin()->first;
    }

    template<typename TShipMap>
    inline uint64_t findPlayerShipEntityId(const TShipMap& ships)
    {
        for (const auto& [id, ship] : ships)
        {
            if (ship.role == ShipRole::Player)
                return id;
        }

        return findAnyShipEntityId(ships);
    }




    inline glm::mat4 makeCameraLookOrientation(
        const glm::vec3& cameraPos,
        const glm::vec3& target,
        const glm::vec3& upHint = glm::vec3(0.0f, 1.0f, 0.0f)
    )
    {
        glm::vec3 forward =
            target - cameraPos;

        if (glm::length2(forward) < 0.000001f)
            forward = glm::vec3(0.0f, 0.0f, -1.0f);

        forward =
            glm::normalize(forward);

        glm::vec3 right =
            glm::cross(forward, upHint);

        if (glm::length2(right) < 0.000001f)
            right = glm::cross(forward, glm::vec3(0.0f, 0.0f, 1.0f));

        right =
            glm::normalize(right);

        glm::vec3 up =
            glm::normalize(glm::cross(right, forward));

        glm::mat4 orientation(1.0f);

        orientation[0] = glm::vec4(right, 0.0f);
        orientation[1] = glm::vec4(up, 0.0f);
        orientation[2] = glm::vec4(-forward, 0.0f);
        orientation[3] = glm::vec4(0, 0, 0, 1);

        return orientation;
    }




    inline glm::vec3 safeNormalizePromo(
        const glm::vec3& v,
        const glm::vec3& fallback
    )
    {
        if (glm::length2(v) < 0.000001f)
            return fallback;

        return glm::normalize(v);
    }

    inline float smootherStepPromo(float x)
    {
        x = std::clamp(x, 0.0f, 1.0f);
        return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
    }

    inline glm::mat4 makePromoLookOrientationClient(
        const glm::vec3& forward
    )
    {
        const glm::vec3 f =
            safeNormalizePromo(
                forward,
                glm::vec3(0.0f, 0.0f, 1.0f)
            );

        const glm::vec3 worldUp =
            glm::vec3(0.0f, 1.0f, 0.0f);

        glm::vec3 right =
            glm::cross(f, worldUp);

        if (glm::length2(right) < 0.000001f)
        {
            right =
                glm::cross(
                    f,
                    glm::vec3(1.0f, 0.0f, 0.0f)
                );
        }

        right =
            glm::normalize(right);

        glm::vec3 up =
            glm::normalize(glm::cross(right, f));

        glm::mat4 m(1.0f);

        // Engine convention:
        // +X = right
        // +Y = up
        // -Z = forward
        m[0] = glm::vec4(right, 0.0f);
        m[1] = glm::vec4(up, 0.0f);
        m[2] = glm::vec4(-f, 0.0f);
        m[3] = glm::vec4(0, 0, 0, 1);

        return m;
    }


    inline json celestialDefinitionToJson(
        const world::celestial::CelestialSystemDefinition& system
    )
    {
        json bodies = json::array();

        std::unordered_map<std::string, glm::dvec3> positions;

        int index = 0;

        for (const auto& b : system.bodies)
        {
            glm::dvec3 pos = b.staticPositionAu;

            if (b.distanceAu > 0.0)
            {
                glm::dvec3 parentPos(0.0);

                auto parentIt = positions.find(b.parentId);
                if (parentIt != positions.end())
                    parentPos = parentIt->second;

                const double phase = double(index) * 0.77;

                pos = parentPos + glm::dvec3(
                    std::cos(phase) * b.distanceAu,
                    0.0,
                    std::sin(phase) * b.distanceAu
                );
            }

            positions[b.id] = pos;

            json item;
            item["id"] = b.id;
            item["name"] = b.name;
            item["type"] = world::celestial::toString(b.type);
            item["parentId"] = b.parentId;
            item["radiusKm"] = b.radiusKm;

            item["positionAu"] = {
                {"x", pos.x},
                {"y", pos.y},
                {"z", pos.z}
            };

            bodies.push_back(std::move(item));
            ++index;
        }

        return bodies;
    }





} // namespace space_state_detail

using namespace space_state_detail;
