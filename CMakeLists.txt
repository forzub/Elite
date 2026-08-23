cmake_minimum_required(VERSION 3.16)

project(EliteGame LANGUAGES C CXX)
include(FetchContent)

option(ELITE_BUILD_CLIENT "Build the graphical EliteGame client" ON)
option(ELITE_BUILD_SERVER "Build the headless EliteServer executable" OFF)
option(ELITE_REQUIRE_BUNDLED_UI_FONTS "Fail client UI pack build when declared bundled fonts are missing" OFF)

if(ELITE_BUILD_CLIENT)
    set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE BOOL "" FORCE)
    set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
    set(WEBVIEW_MSWEBVIEW2_BUILTIN_IMPL ON CACHE BOOL "" FORCE)
    set(WEBVIEW_MSWEBVIEW2_EXPLICIT_LINK ON CACHE BOOL "" FORCE)

    add_subdirectory(third_party/webview)
endif()





include_directories(
    ${CMAKE_SOURCE_DIR}/third_party/websocketpp
    ${CMAKE_SOURCE_DIR}/third_party/asio/asio/include  # добавили /asio
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)

if(ELITE_BUILD_CLIENT)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/glad)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})


# Runtime deployment policy --------------------------------------------------
#
# Developer builds must be directly runnable outside the compiler shell.
# On MinGW/Windows we recursively stage non-system PE runtime dependencies
# beside each executable. On ELF platforms we keep system libraries system-
# owned and make any future project-local shared-library RPATH relative.
function(elite_configure_executable_runtime TARGET_NAME)
    if(WIN32 AND MINGW)
        get_filename_component(ELITE_MINGW_TOOLCHAIN_BIN
            "${CMAKE_CXX_COMPILER}" DIRECTORY)

        if(NOT CMAKE_OBJDUMP)
            find_program(ELITE_MINGW_OBJDUMP
                NAMES objdump
                HINTS "${ELITE_MINGW_TOOLCHAIN_BIN}")
            if(NOT ELITE_MINGW_OBJDUMP)
                message(FATAL_ERROR
                    "MinGW objdump is required to stage standalone runtime dependencies")
            endif()
        else()
            set(ELITE_MINGW_OBJDUMP "${CMAKE_OBJDUMP}")
        endif()

        add_custom_command(
            TARGET ${TARGET_NAME}
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                "-DELITE_RUNTIME_TARGET_FILE=$<TARGET_FILE:${TARGET_NAME}>"
                "-DELITE_RUNTIME_TARGET_DIR=$<TARGET_FILE_DIR:${TARGET_NAME}>"
                "-DELITE_RUNTIME_TOOLCHAIN_BIN=${ELITE_MINGW_TOOLCHAIN_BIN}"
                "-DELITE_RUNTIME_OBJDUMP=${ELITE_MINGW_OBJDUMP}"
                -P "${CMAKE_SOURCE_DIR}/cmake/StageMinGWRuntime.cmake"
            COMMENT "Staging standalone MinGW runtime for ${TARGET_NAME}"
            VERBATIM
        )
    elseif(UNIX AND NOT APPLE)
        # Linux keeps libc/libstdc++ and other system runtime libraries under
        # the distribution package manager. If we add our own shared objects
        # later, keep build-tree lookup relocatable instead of baking absolute
        # developer-machine paths into the executable.
        set_property(TARGET ${TARGET_NAME} PROPERTY BUILD_RPATH_USE_ORIGIN TRUE)
    endif()
endfunction()

add_compile_definitions(GLM_ENABLE_EXPERIMENTAL)

if(ELITE_BUILD_CLIENT)
add_executable(EliteGame
    src/main.cpp
    src/platform/RuntimeRoot.cpp
    src/platform/ProcessSingleInstanceGuard.cpp

    glad/src/gl.c

    src/core/Application.cpp
    src/core/StateStack.cpp
    src/core/StateContext.cpp
    src/game/localization/LocalizationService.cpp


    src/window/Window.cpp

    src/ui/MainMenuState.cpp
    src/ui/presentation/GamePresentationCoordinator.cpp
    src/ui/presentation/PresentationFunctionKeyRouter.cpp
    src/ui/services/ServiceUiRegistry.cpp
    src/ui/services/GovernmentServicesUi.cpp
    src/ui/services/ShipyardUi.cpp
    src/ui/services/RepairRefitUi.cpp
    src/ui/services/TradeUi.cpp
    src/ui/HudRenderer.cpp
    src/ui/hud/HudEdgeMapper.cpp


    src/input/Input.cpp

    src/game/SpaceState.cpp
    src/game/SpaceState-Init.cpp

    src/game/equipment/signalNode/processing/SignalReceiver.cpp
    src/world/WorldSignalTxSystem.cpp

    src/game/equipment/data/transmitters.cpp
    src/game/equipment/data/receivers.cpp
    src/game/equipment/data/jammers.cpp
    src/game/equipment/data/decryptors.cpp

    src/game/ship/hud/worldlabels/WorldLabelSystem.cpp
    src/game/ship/hud/worldlabels/WorldSignalWaves.cpp

    src/game/signals/SignalPattern.cpp
    src/game/signals/SignalPatternLibrary.cpp


    src/game/ship/descriptors/EliteCobraMk1.cpp
    src/game/ship/descriptors/EliteCobraMk1_Cockpit.cpp

    src/game/ship/controller/PlayerInputMapper.cpp
    
    src/game/ship/ShipController.cpp
    src/game/ship/physics/ShipImpulseSystem.cpp
    src/game/ship/Ship.cpp
    src/game/ship/ShipDescriptorRegistry.cpp
    
    src/game/ship/core/ShipSignalController.cpp
    src/game/ship/core/ShipCore.cpp
    
    src/game/ship/view/PlayerShipView.cpp
    src/game/ship/view/ShipCameraController.cpp

    src/game/ship/sensors/ShipSignalPresentation.cpp

    src/galaxy/GalaxyDatabase.cpp

    src/game/ship/cockpit/CockpitMeshBuilder.cpp
    src/game/ship/cockpit/CockpitStrokeBuilder.cpp
    src/game/ship/cockpit/ShipCockpitUpdater.cpp
    
    src/render/Camera.cpp
    src/render/DebugGrid.cpp
    src/render/Renderer.cpp
    src/render/ScreenUtils.cpp
    src/render/ShaderUtils.cpp
    src/render/Font.cpp
    src/render/HUD/TextRenderer.cpp
    src/render/HUD/WorldLabelRenderer.cpp
    src/render/RenderCockpitPass.cpp
    src/render/RenderCockpitBitmapPass.cpp
    src/render/cockpit/FlightVectorIndicatorRenderer.cpp
    src/render/cockpit/GuidanceCorridorRenderer.cpp
    src/render/cockpit/GalacticCompassRenderer.cpp
    src/render/HUD/HudPrimitiveBatch.cpp
    src/render/bitmap/TextureLoader.cpp
    src/render/camera/RenderCameraViewport.cpp

    src/ui/components/minicamera/UICameraView.cpp
    src/ui/components/UIComponent.cpp
    src/ui/components/UIContainer.cpp
    src/ui/components/UIText.cpp

    src/game/simulation/GameSimulation.cpp
    src/game/simulation/NpcAiSystem.cpp

    src/game/diagnostics/ClientAcceptanceHarness.cpp
    src/game/diagnostics/MultiplayerClientAcceptanceHarness.cpp
    src/game/presentation/ClientHudPresentation.cpp
    src/game/presentation/GalaxyNavigationPresentation.cpp
    src/game/presentation/StarSystemLabelPresentation.cpp
    src/game/presentation/SystemMapPanelPresentation.cpp
    src/ui/presentation/InSessionPresentationRenderer.cpp

    src/game/client/ClientWorldState.cpp
    src/game/server/GameServer.cpp
    src/game/server/ServerRunner.cpp
    src/game/server/ServerRuntime.cpp
    src/game/server/ServerWorker.cpp

    src/game/client/GameClient.cpp
    src/game/network/LocalLoopbackTransport.cpp
    src/game/network/TcpWireStream.cpp
    src/game/network/TcpTransport.cpp
    src/game/shared/SharedShipPhysics.cpp

    src/game/equipment/DamageModel.cpp
    src/game/equipment/radar/RadarModule.cpp
    src/game/equipment/radar/TestIdealRadarUnit.cpp

    src/game/equipment/signalNode/ReceiverModule.cpp
    src/game/equipment/EquipmentModule.cpp

    src/game/ship/core/PowerBus.cpp
    src/game/ship/core/ThermalSystem.cpp
    src/game/ship/core/ReactorSystem.cpp

    src/game/equipment/data/radars.cpp
    src/game/equipment/signalNode/SignalTransmitterModule.cpp

    src/scene/SceneRenderer.cpp
    
    src/ui/components/radar/RadarWidgetBase.cpp
    src/ui/components/radar/RadarWidgetFactory.cpp
    src/ui/components/radar/RadarRenderTarget.cpp
    src/ui/components/radar/ppi/RadarPPIWidget.cpp

    src/ui/components/radar/ppi/effects/LCDEffect.cpp
    src/ui/components/radar/ppi/effects/CRTEffect.cpp

    src/game/ship/core/CoolingSystem.cpp


    # src/WebSocket/DebugServer.cpp
    src/game/ship/core/systems/CoreSystem.cpp

    src/game/damage/DamageTestObject.cpp
    src/game/damage/DamageSystem.cpp
    src/game/ship/damage/ShipDamageHandler.cpp
    src/game/ship/core/ShipHitBuilder.cpp

    src/game/damage/TestDamageHandler.cpp

    src/render/ship/ShipMeshRenderer.cpp
    src/render/geometry/AssemblyGpuLibrary.cpp
    
    src/game/geometry/ObjLoader.cpp
    # src/game/geometry/MeshLibrary.cpp
    
    src/render/ShaderLibrary.cpp
    src/render/InitShaders.cpp

    src/render/renderers/MeshRenderer.cpp
    src/debug/render/DebugRenderer.cpp

    src/render/frustum/Frustum.cpp
    src/debug/FrustumDebugData.cpp

    src/world/descriptors/ObjectDescriptorRegistry.cpp
    src/game/geometry/ObjectAssemblyRegistry.cpp
    src/game/geometry/AssemblyMeshLibrary.cpp

    src/world/modules/ObjectModuleRuntime.cpp
    src/world/modules/ObjectHitBuilder.cpp
    src/world/objects/StaticObjectDamage.cpp
    src/debug/render/DebugHitVolumeRenderer.cpp
    src/debug/DebugSettings.cpp
    src/world/modules/ObjectAssemblyRuntime.cpp
    src/world/modules/HitVolumeSnapshotBuilder.cpp
    src/world/modules/ObjectRuntimeHitBuilder.cpp

    src/debug/render/ServerHitVolumeRenderer.cpp
    src/game/ship/view/ShipAttachmentResolver.cpp

    src/ui/html/HtmlUiServer.cpp
    src/ui/platform/UiResourcePack.cpp
    src/ui/platform/ClientPreferencesStore.cpp
    src/ui/html/HtmlUiCommandQueue.cpp
    src/ui/html/HtmlUiBridge.cpp
    src/ui/html/HtmlUiManager.cpp

    src/game/debug/AttachmentEditorPayload.cpp
    src/game/debug/AttachmentEditorResolve.cpp
    src/game/debug/LocalDebugSessionControl.cpp

    src/world/modules/ObjectAssemblyTransformUtils.cpp
    src/game/debug/VolumeViewerPayload.cpp

    src/world/modules/ObjectStructuralLinkRuntime.cpp
    src/world/modules/ObjectDetachedFragmentRuntime.cpp
    src/world/modules/ObjectRepairJobRuntime.cpp
    src/world/navigation/ObstacleAvoidance.cpp
    src/world/navigation/SmallCraftNavigation.cpp

    src/world/navigation/TacticalCollisionMonitor.cpp
    src/world/navigation/NavigationContactAdapters.cpp
    src/world/navigation/ObstaclePathPlanner.cpp

    src/world/navigation/NavigationBodyAdapter.cpp
    
    src/render/starfield/GalaxyStarfieldRenderer.cpp
    src/render/starfield/ConstellationOverlayRenderer.cpp
    src/render/starfield/SkyCultureCatalog.cpp
    src/render/starfield/MilkyWayRenderer.cpp

    src/game/promo/PromoFlybyScenario.cpp
    src/game/promo/PromoSceneScenario.cpp

    src/game/scene/GameSceneSetup.cpp
    src/game/traffic/StationTrafficSystem.cpp

    src/render/planet/PlanetWireRenderer.cpp
    src/ui/browser/GameWebView.cpp

    src/world/celestial/StarAtlasDatabase.cpp
    src/world/celestial/CelestialSystemRuntime.cpp

    src/game/system_map/SystemMapRenderer.cpp
    src/game/system_map/SystemMapInteraction.cpp
    src/game/system_map/SystemMapFrameInteractionContext.cpp
    src/game/system_map/SystemMapSceneRenderer.cpp
    src/game/system_map/SystemMapSceneFrameBuilder.cpp
    src/game/system_map/SystemMapPresentationBuilder.cpp
    src/game/system_map/SystemMapView.cpp
    src/game/system_map/LocalMapInteraction.cpp
    src/game/system_map/LocalMapPresentationBuilder.cpp
    src/game/system_map/LocalMapAtmosphereRenderer.cpp
    src/game/system_map/LocalMapPrimitiveRenderer.cpp
    src/game/system_map/MapObjectOverlayRenderer.cpp
    src/game/system_map/NavigationRouteOverlay.cpp
    src/game/system_map/MapCelestialRenderResources.cpp
    src/game/system_map/DetailMapSceneRenderer.cpp
    src/game/system_map/HubMapSceneRenderer.cpp
    src/game/system_map/DetailMapBackend.cpp
    src/game/system_map/DetailMapGeometryPass.cpp
    src/game/system_map/DetailMapPlanetPass.cpp
    src/game/system_map/HubMapBackend.cpp
    src/game/system_map/HubMapGeometryPass.cpp
    src/game/system_map/HubMapPlanetPass.cpp
    src/game/system_map/GalaxyMapView.cpp
    src/game/system_map/GalaxyMapInteraction.cpp
    src/game/system_map/GalaxyMapRenderer.cpp
    src/game/navigation/DynamicMotionSystem.cpp
    src/game/navigation/TrajectoryPredictor.cpp
    src/game/navigation/TrajectorySafetyEvaluator.cpp
    src/game/navigation/LocalGuidancePlanner.cpp
    src/game/navigation/HubSemanticAnchorCatalog.cpp
    src/game/navigation/GalacticReferenceFrame.cpp
    src/game/navigation/CoordinateDisplayService.cpp
    src/game/navigation/PlayerSpatialDomainResolver.cpp

    src/world/celestial/visual/CelestialVisualLibrary.cpp
    src/world/celestial/visual/CelestialTextureBaker.cpp
    src/world/celestial/visual/CelestialGeneratedAssetLibrary.cpp
    src/world/celestial/visual/CelestialEnvironmentProfile.cpp

    src/render/celestial/CelestialShapeMesh.cpp
    src/render/celestial/ProceduralCloudLayer.cpp
    src/render/celestial/HubSphericalGridRenderer.cpp

    src/render/celestial/clouds/HubBackdropCloudRenderer.cpp

    src/render/celestial/clouds/PlanetaryWeatherMapGenerator.cpp
    src/render/celestial/clouds/CloudAppearanceTextureGenerator.cpp

    src/render/celestial/HubPlanetSurfaceRenderer.cpp
    src/render/celestial/rings/PlanetRingRenderer.cpp
    src/render/celestial/PlanetGlobeMeshRenderer.cpp

    src/render/system_map/HubMapGpuGeometryRenderer.cpp
    src/render/system_map/HubPlanetOverlayRenderer.cpp

    src/game/navigation/GalaxyNavigationGrid.cpp

    src/game/navigation/CubicNavigationGrid.cpp
    src/game/navigation/NavigationRegionCatalog.cpp
    src/game/navigation/SystemNavigationGrid.cpp
    src/render/navigation/NavigationCellLabelLayer.cpp

    src/render/navigation/NavigationCoordinateOverlay.cpp
    src/game/navigation/GalaxyNavigationConfig.cpp

    src/game/host/LocalGameHost.cpp
    src/game/host/LocalGameSession.h
    src/game/host/LocalGameSession.cpp
    src/game/session/RemoteGameSession.cpp
    src/game/identity/ClientIdentityProfile.cpp
    src/game/identity/SecureRandom.cpp
    src/game/identity/Sha256.cpp
    src/game/client/ClientMapService.cpp
    src/game/client/ClientCatalogService.cpp

    src/game/system_map/AuthoritativeMapInterpolator.cpp
    src/world/celestial/CelestialRuntimeRegistry.cpp
)
endif()

if(ELITE_BUILD_SERVER)
    add_executable(EliteServer
        src/server_main.cpp
        src/platform/RuntimeRoot.cpp
        src/platform/ProcessSingleInstanceGuard.cpp

        src/game/equipment/signalNode/processing/SignalReceiver.cpp
        src/world/WorldSignalTxSystem.cpp

        src/game/equipment/data/transmitters.cpp
        src/game/equipment/data/receivers.cpp
        src/game/equipment/data/jammers.cpp
        src/game/equipment/data/decryptors.cpp
        src/game/equipment/data/radars.cpp

        src/game/signals/SignalPattern.cpp
        src/game/signals/SignalPatternLibrary.cpp

        src/game/ship/descriptors/EliteCobraMk1.cpp
        src/game/ship/ShipController.cpp
        src/game/ship/physics/ShipImpulseSystem.cpp
        src/game/ship/Ship.cpp
        src/game/ship/ShipDescriptorRegistry.cpp
        src/game/ship/core/ShipSignalController.cpp
        src/game/ship/core/ShipCore.cpp
        src/game/ship/core/PowerBus.cpp
        src/game/ship/core/ThermalSystem.cpp
        src/game/ship/core/ReactorSystem.cpp
        src/game/ship/core/CoolingSystem.cpp
        src/game/ship/core/systems/CoreSystem.cpp
        src/game/ship/core/ShipHitBuilder.cpp
        src/game/ship/damage/ShipDamageHandler.cpp
        src/game/ship/view/ShipAttachmentResolver.cpp

        src/game/simulation/GameSimulation.cpp
        src/game/simulation/NpcAiSystem.cpp
        src/game/server/GameServer.cpp
        src/game/server/ServerRunner.cpp
        src/game/server/ServerRuntime.cpp
        src/game/server/NetworkServerHost.cpp
        src/game/identity/Sha256.cpp
        src/game/network/TcpWireStream.cpp
        src/game/network/TcpTransport.cpp
        src/game/shared/SharedShipPhysics.cpp

        src/game/equipment/DamageModel.cpp
        src/game/equipment/radar/RadarModule.cpp
        src/game/equipment/radar/TestIdealRadarUnit.cpp
        src/game/equipment/signalNode/ReceiverModule.cpp
        src/game/equipment/signalNode/SignalTransmitterModule.cpp
        src/game/equipment/EquipmentModule.cpp

        src/game/damage/DamageSystem.cpp

        src/game/geometry/ObjLoader.cpp
        src/game/geometry/ObjectAssemblyRegistry.cpp
        src/game/geometry/AssemblyMeshLibrary.cpp

        src/world/descriptors/ObjectDescriptorRegistry.cpp
        src/world/objects/StaticObjectDamage.cpp
        src/world/modules/ObjectModuleRuntime.cpp
        src/world/modules/ObjectAssemblyRuntime.cpp
        src/world/modules/HitVolumeSnapshotBuilder.cpp
        src/world/modules/ObjectRuntimeHitBuilder.cpp
        src/world/modules/ObjectAssemblyTransformUtils.cpp
        src/world/modules/ObjectStructuralLinkRuntime.cpp
        src/world/modules/ObjectDetachedFragmentRuntime.cpp
        src/world/modules/ObjectRepairJobRuntime.cpp

        src/world/navigation/ObstacleAvoidance.cpp
        src/world/navigation/SmallCraftNavigation.cpp
        src/world/navigation/TacticalCollisionMonitor.cpp
        src/world/navigation/NavigationContactAdapters.cpp
        src/world/navigation/ObstaclePathPlanner.cpp
        src/world/navigation/NavigationBodyAdapter.cpp

        src/game/promo/PromoFlybyScenario.cpp
        src/game/scene/GameSceneSetup.cpp

        src/world/celestial/StarAtlasDatabase.cpp
        src/world/celestial/CelestialSystemRuntime.cpp
        src/world/celestial/CelestialRuntimeRegistry.cpp

        src/game/navigation/DynamicMotionSystem.cpp
        src/game/navigation/TrajectoryPredictor.cpp
        src/game/navigation/TrajectorySafetyEvaluator.cpp
        src/game/navigation/LocalGuidancePlanner.cpp
        src/game/navigation/HubSemanticAnchorCatalog.cpp
        src/game/navigation/GalaxyNavigationConfig.cpp
        src/game/navigation/PlayerSpatialDomainResolver.cpp

        src/debug/DebugSettings.cpp
    )

    target_include_directories(EliteServer PRIVATE
        src
    )

    target_link_libraries(EliteServer PRIVATE
        nlohmann_json::nlohmann_json
        Threads::Threads
    )

    if(WIN32)
        target_link_libraries(EliteServer PRIVATE
            ws2_32
            mswsock
        )
    endif()
endif()


set(ELITE_STATIC_ASSETS_DIR
    "${CMAKE_SOURCE_DIR}/src/assets"
)

set(ELITE_EXTERNAL_ASSETS_DIR
    "${CMAKE_SOURCE_DIR}/assets"
)

set(ELITE_RUNTIME_ASSETS_DIR
    "${CMAKE_BINARY_DIR}/assets"
)

function(elite_copy_asset_tree TARGET_NAME SOURCE_DIR DEST_DIR)
    if(NOT EXISTS "${SOURCE_DIR}")
        add_custom_target(${TARGET_NAME})
        return()
    endif()

    file(
        GLOB_RECURSE ASSET_FILES
        CONFIGURE_DEPENDS
        "${SOURCE_DIR}/*"
    )

    set(COPIED_ASSET_FILES)

    foreach(ASSET_FILE IN LISTS ASSET_FILES)
        if(IS_DIRECTORY "${ASSET_FILE}")
            continue()
        endif()

        file(
            RELATIVE_PATH RELATIVE_ASSET_PATH
            "${SOURCE_DIR}"
            "${ASSET_FILE}"
        )

        set(OUTPUT_ASSET_FILE
            "${DEST_DIR}/${RELATIVE_ASSET_PATH}"
        )

        get_filename_component(
            OUTPUT_ASSET_DIR
            "${OUTPUT_ASSET_FILE}"
            DIRECTORY
        )

        add_custom_command(
            OUTPUT "${OUTPUT_ASSET_FILE}"

            COMMAND ${CMAKE_COMMAND} -E make_directory
                    "${OUTPUT_ASSET_DIR}"

            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${ASSET_FILE}"
                    "${OUTPUT_ASSET_FILE}"

            DEPENDS "${ASSET_FILE}"

            COMMENT "Copying asset: ${RELATIVE_ASSET_PATH}"

            VERBATIM
        )

        list(
            APPEND COPIED_ASSET_FILES
            "${OUTPUT_ASSET_FILE}"
        )
    endforeach()

    add_custom_target(
        ${TARGET_NAME}
        DEPENDS ${COPIED_ASSET_FILES}
    )
endfunction()

elite_copy_asset_tree(
    copy_static_assets
    "${ELITE_STATIC_ASSETS_DIR}"
    "${ELITE_RUNTIME_ASSETS_DIR}"
)

elite_copy_asset_tree(
    copy_external_assets
    "${ELITE_EXTERNAL_ASSETS_DIR}"
    "${ELITE_RUNTIME_ASSETS_DIR}"
)

add_custom_target(
    copy_assets ALL
    DEPENDS
        copy_static_assets
        copy_external_assets
)

if(TARGET EliteGame)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)

    set(ELITE_UI_FONT_DIR
        "${CMAKE_SOURCE_DIR}/third_party/fonts/noto"
    )
    set(ELITE_UI_PACK_FILE
        "${ELITE_RUNTIME_ASSETS_DIR}/ui/elite_ui.pak"
    )
    set(ELITE_THIRD_PARTY_LICENSE_INDEX
        "${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md"
    )
    set(ELITE_RUNTIME_THIRD_PARTY_LICENSE_INDEX
        "${ELITE_RUNTIME_ASSETS_DIR}/licenses/THIRD_PARTY_LICENSES.md"
    )

    set(ELITE_UI_PACK_FONT_REQUIRE_ARG)
    if(ELITE_REQUIRE_BUNDLED_UI_FONTS)
        set(ELITE_UI_PACK_FONT_REQUIRE_ARG --require-fonts)
    endif()

    file(GLOB_RECURSE ELITE_UI_WEB_FILES CONFIGURE_DEPENDS
        "${ELITE_STATIC_ASSETS_DIR}/webui/*"
    )
    file(GLOB_RECURSE ELITE_UI_LICENSE_FILES CONFIGURE_DEPENDS
        "${ELITE_STATIC_ASSETS_DIR}/licenses/*"
    )
    file(GLOB_RECURSE ELITE_UI_FONT_FILES CONFIGURE_DEPENDS
        "${ELITE_UI_FONT_DIR}/*.ttf"
        "${ELITE_UI_FONT_DIR}/*.otf"
        "${ELITE_UI_FONT_DIR}/*.woff2"
        "${ELITE_UI_FONT_DIR}/font-lock.json"
    )

    add_custom_command(
        OUTPUT "${ELITE_UI_PACK_FILE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${ELITE_RUNTIME_ASSETS_DIR}/ui"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_SOURCE_DIR}/tools/build_ui_pack.py"
                --webui "${ELITE_STATIC_ASSETS_DIR}/webui"
                --fonts "${ELITE_UI_FONT_DIR}"
                --licenses "${ELITE_STATIC_ASSETS_DIR}/licenses"
                --font-manifest "${ELITE_STATIC_ASSETS_DIR}/ui/font_manifest.json"
                --license-index "${ELITE_THIRD_PARTY_LICENSE_INDEX}"
                ${ELITE_UI_PACK_FONT_REQUIRE_ARG}
                --output "${ELITE_UI_PACK_FILE}"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/build_ui_pack.py"
            "${ELITE_STATIC_ASSETS_DIR}/ui/font_manifest.json"
            "${ELITE_THIRD_PARTY_LICENSE_INDEX}"
            ${ELITE_UI_WEB_FILES}
            ${ELITE_UI_LICENSE_FILES}
            ${ELITE_UI_FONT_FILES}
        COMMENT "Building binary Elite UI resource pack"
        VERBATIM
    )
    add_custom_target(build_ui_pack DEPENDS "${ELITE_UI_PACK_FILE}")

    add_custom_command(
        OUTPUT "${ELITE_RUNTIME_THIRD_PARTY_LICENSE_INDEX}"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${ELITE_RUNTIME_ASSETS_DIR}/licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${ELITE_THIRD_PARTY_LICENSE_INDEX}"
                "${ELITE_RUNTIME_THIRD_PARTY_LICENSE_INDEX}"
        DEPENDS "${ELITE_THIRD_PARTY_LICENSE_INDEX}"
        COMMENT "Copying third-party license index"
        VERBATIM
    )
    add_custom_target(copy_third_party_license_index
        DEPENDS "${ELITE_RUNTIME_THIRD_PARTY_LICENSE_INDEX}")

    add_dependencies(EliteGame
        copy_assets
        build_ui_pack
        copy_third_party_license_index
    )
endif()

if(TARGET EliteServer)
    add_dependencies(EliteServer copy_assets)
endif()

if(TARGET EliteGame)
set_source_files_properties(
    glad/src/gl.c
    PROPERTIES
    LANGUAGE C
)

target_compile_definitions(EliteGame PRIVATE GLFW_INCLUDE_NONE)

target_compile_definitions(EliteGame PRIVATE ELITE_CLIENT_PRESENTATION=1)

target_include_directories(EliteGame PRIVATE
    src
    glad/include
)

target_link_libraries(EliteGame PRIVATE
    glfw
    opengl32
    Freetype::Freetype
    nlohmann_json::nlohmann_json
    webview::core_static
    
    
    # Библиотеки для ASIO/WebSocket
    ws2_32      # уже есть
    mswsock     # НУЖНО ДОБАВИТЬ (для AcceptEx)
    iphlpapi    # можно добавить на всякий случай
    pthread     # уже есть
    
    advapi32
    bcrypt
    ole32
    shell32
    shlwapi
    user32
    version
    dwmapi

    # Порядок важен - сначала наши, потом системные
    ${FREETYPE_LIBRARY}
    glfw3
    opengl32



)
endif()


if(TARGET EliteGame)
    elite_configure_executable_runtime(EliteGame)
endif()

if(TARGET EliteServer)
    elite_configure_executable_runtime(EliteServer)
endif()
