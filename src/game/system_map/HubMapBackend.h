#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <glad/gl.h>

#include "src/game/system_map/HubMapRenderContext.h"
#include "src/game/system_map/HubMapGeometryPass.h"
#include "src/game/system_map/HubMapPlanetPass.h"

class SystemMapRenderer;

namespace game::system_map
{
struct HubMapPerformanceStats
{
    double cpuTotalMs = 0.0;
    double cpuBackgroundMs = 0.0;
    double cpuPlanetBackdropMs = 0.0;
    double cpuGeometryMs = 0.0;
    double cpuLabelsMs = 0.0;

    bool gpuValid = false;
    double gpuTotalMs = 0.0;
    double gpuBackgroundMs = 0.0;
    double gpuFallbackBodyMs = 0.0;
    double gpuSurfaceMs = 0.0;
    double gpuCloudsMs = 0.0;
    double gpuAtmosphereMs = 0.0;
    double gpuGeometryMs = 0.0;
    double gpuLabelsMs = 0.0;
};

class HubMapBackend final : public HubMapRenderContext
{
public:
    explicit HubMapBackend(SystemMapRenderer& host) noexcept;

    void renderHubMapPasses(
        const HubMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) override;

    const HubMapPerformanceStats& performanceStats() const noexcept
    {
        return m_performanceStats;
    }

private:
    friend class ::SystemMapRenderer;
    friend class HubMapPlanetPass;

    enum class GpuStage : std::size_t
    {
        Background = 0,
        FallbackBody,
        Surface,
        Clouds,
        Atmosphere,
        Geometry,
        Labels,
        Count
    };

    static constexpr std::size_t kGpuStageCount =
        static_cast<std::size_t>(GpuStage::Count);
    static constexpr std::size_t kGpuQuerySlotCount = 4;

    void ensureGpuQueries();
    void collectGpuQueries();
    void beginGpuFrame();
    void endGpuFrame();
    void beginGpuStage(GpuStage stage);
    void endGpuStage();

private:
    SystemMapRenderer& m_host;
    HubMapGeometryPass m_geometryPass;
    HubMapPlanetPass m_planetPass;
    HubMapPerformanceStats m_performanceStats;

    std::array<
        std::array<GLuint, kGpuStageCount>,
        kGpuQuerySlotCount
    > m_gpuQueries {};

    std::array<std::uint32_t, kGpuQuerySlotCount>
        m_gpuIssuedMasks {};
    std::array<bool, kGpuQuerySlotCount>
        m_gpuSlotPending {};
    std::array<std::uint64_t, kGpuQuerySlotCount>
        m_gpuSlotSerials {};

    bool m_gpuQueriesInitialized = false;
    bool m_gpuFrameActive = false;
    bool m_gpuStageOpen = false;
    std::size_t m_gpuCurrentSlot = 0;
    std::uint64_t m_gpuFrameSerial = 0;
    std::uint64_t m_gpuLastCollectedSerial = 0;
};
}
