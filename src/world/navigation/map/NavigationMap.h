#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace world::navigation
{

// NavigationMap is the ownership boundary for the active ship-centered
// navigation working set. Callers publish authoritative snapshots by value and
// receive only compact query products by value. Internal actor storage, spatial
// cells, prediction caches and future CPU/GPU backend state never cross this API.
class NavigationMap final
{
public:
    using EntityId = std::uint64_t;
    using Revision = std::uint64_t;

    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
    };

    // NavigationMap is deliberately NavLocal-only. Coordinate conversion is
    // owned by the game/navigation boundary adapter before publication.
    struct Config
    {
        double halfExtentMeters = 12000.0;
        double cellSizeMeters = 600.0;
        // Fallback only. Runtime queries may override this with their current
        // physical maneuver look-ahead.
        double predictionHorizonSeconds = 3.0;
        double interactionMarginMeters = 60.0;
    };

    // Input DTO only. NavigationMap does not retain references to caller-owned
    // state and does not expose this record again after publication.
    struct DynamicActorInput
    {
        EntityId entityId = 0;
        Vec3d positionMapMeters {};
        Vec3d velocityMapMetersPerSecond {};
        Vec3d accelerationMapMetersPerSecond2 {};
        Vec3d angularVelocityMapRadPerSecond {};

        double radiusMeters = 1.0;
        std::uint32_t flags = 0;
        Revision motionRevision = 0;
    };

    // Whole dynamic-world publication for one authoritative source revision.
    // Passed by value so the block takes ownership of the snapshot boundary.
    struct DynamicWorldUpdate
    {
        Revision sourceRevision = 0;
        std::vector<DynamicActorInput> actors;
    };

    struct CorridorQuery
    {
        Vec3d startMapMeters {};
        Vec3d endMapMeters {};
        double radiusMeters = 0.0;

        // When present, broadphase sweep/prediction uses this exact horizon.
        // When absent, Config::predictionHorizonSeconds is the fallback.
        std::optional<double> lookAheadSeconds;
    };

    struct SphereQuery
    {
        Vec3d centerMapMeters {};
        double radiusMeters = 0.0;

        // Same physical time horizon contract as CorridorQuery.
        std::optional<double> lookAheadSeconds;
    };

    // Compact derived query product. This is intentionally not a reference or
    // view into NavigationMap internals; consumers may keep it independently.
    struct Candidate
    {
        EntityId entityId = 0;
        Vec3d positionMapMeters {};
        Vec3d velocityMapMetersPerSecond {};
        Vec3d accelerationMapMetersPerSecond2 {};
        Vec3d angularVelocityMapRadPerSecond {};
        Vec3d predictedEndPositionMapMeters {};
        Vec3d conservativeSweptCenterMapMeters {};
        double predictionHorizonSeconds = 0.0;
        double actorRadiusMeters = 0.0;
        double conservativeSweptRadiusMeters = 0.0;
        std::uint32_t flags = 0;
        Revision motionRevision = 0;
    };

    struct QueryDiagnostics
    {
        std::size_t gridCellsVisited = 0;
        std::size_t occupiedCellsVisited = 0;
        std::size_t actorsExamined = 0;
    };

    struct QueryResult
    {
        Revision mapRevision = 0;
        Revision sourceRevision = 0;
        double lookAheadSeconds = 0.0;
        QueryDiagnostics diagnostics {};
        std::vector<Candidate> candidates;
    };

    struct Stats
    {
        Revision mapRevision = 0;
        Revision sourceRevision = 0;
        std::size_t actorCount = 0;
        std::size_t indexedActorCount = 0;
        std::size_t occupiedCellCount = 0;
        std::size_t outOfBoundsActorCount = 0;
        std::size_t rejectedActorCount = 0;
        double maxConservativeSweptRadiusMeters = 0.0;
    };

    NavigationMap();
    explicit NavigationMap(const Config& config);
    ~NavigationMap();

    NavigationMap(NavigationMap&&) noexcept;
    NavigationMap& operator=(NavigationMap&&) noexcept;

    NavigationMap(const NavigationMap&) = delete;
    NavigationMap& operator=(const NavigationMap&) = delete;

    // Atomically replaces the block-owned NavLocal dynamic working set.
    // System/world/model/render coordinates are not accepted by this API.
    void replaceDynamicWorld(DynamicWorldUpdate update);

    [[nodiscard]] QueryResult queryCorridor(const CorridorQuery& query) const;
    [[nodiscard]] QueryResult querySphere(const SphereQuery& query) const;
    [[nodiscard]] Stats stats() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace world::navigation
