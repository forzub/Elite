#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "src/render/celestial/flight/CelestialFlightTypes.h"
#include "src/render/celestial/flight/CelestialRenderStyle.h"

namespace world::celestial::surface
{
class SurfaceField;
}

namespace render::celestial_flight
{
/*
    Runtime renderer of the celestial world (flight views; Planet Lab
    hosts the same class). Representation by distance, all from the same
    continuous surface field of each body:

      sub-pixel            point sprite (brightness from size, albedo,
                           phase and light), stars as glow sprites;
      disc .. orbit        globe: one cube-sphere mesh per frame, relief
                           displacement + albedo / normal / relief cube maps
                           baked from the field at a face size chosen from
                           the projected size (32 .. maxGlobeFace), clouds,
                           atmosphere, rings;
      low orbit .. ground  camera-centred local terrain (LocalTerrain
                           clipmap levels, worker-built) drawn over the
                           globe (stencil), aerial haze; a depth-only copy
                           in the near pass occludes ships and hubs;
      asteroid belts       bodies of the reservoir lattice around the camera
                           (meshes from SmallBodyField, sprites beyond).

    Resources are a cache: globe tiers, cloud covers, terrain levels and
    asteroid meshes are built on worker threads on demand and evicted by
    the texture budget / when unused. Nothing is stored per patch or per
    tile and nothing persists between sessions.

    GL: call from the thread owning the context. render() draws into the
    bound framebuffer (colour kept, depth / stencil cleared) and leaves
    standard state (depth LESS, blend SRC_ALPHA / ONE_MINUS, no stencil).
*/
class CelestialFlightRenderer
{
public:
    CelestialFlightRenderer();
    ~CelestialFlightRenderer();
    CelestialFlightRenderer(const CelestialFlightRenderer&) = delete;
    CelestialFlightRenderer& operator=(const CelestialFlightRenderer&) = delete;

    bool initialize(const FlightRenderQuality& quality = {});
    void shutdown();
    bool initialized() const;

    void setStyle(CelestialRenderStyle style);
    CelestialRenderStyle style() const;
    void setQuality(const FlightRenderQuality& quality);
    const FlightRenderQuality& quality() const;

    // viewRotation: world -> view rotation (translation ignored); the
    // camera position is frame.cameraPositionM.
    // primaryView: the view that plans resources (main camera). Secondary
    // views (rear monitor, mini views) draw the existing representation
    // only - they never request tiers or move the local terrain.
    void render(const FlightCelestialFrame& frame, const glm::mat4& viewRotation, float fovYRadians, int viewportWidth,
                int viewportHeight, bool primaryView = true);

    // Near pass helper: depth of the local terrain with the caller's
    // ordinary projection (camera-relative view rotation), no colour.
    void renderTerrainDepth(const glm::mat4& viewRotation, const glm::mat4& projection);

    // Blocks until the resources requested by the last render() are built
    // (captures / benchmarks); false on timeout.
    bool waitUntilIdle(double timeoutSeconds);

    const FlightRenderStats& stats() const;

    // Ground height (m above the reference radius) under a body-frame
    // direction of a loaded body, or NaN (collision / altitude queries).
    double groundHeightM(const std::string& bodyId, const glm::dvec3& bodyDirection) const;
    // The continuous surface of a loaded body (null until loaded).
    std::shared_ptr<const world::celestial::surface::SurfaceField> surfaceField(const std::string& bodyId) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m;
};
} // namespace render::celestial_flight
