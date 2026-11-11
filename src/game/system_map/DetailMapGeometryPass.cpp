#include "src/core/HitchProfiler.h"
#include "src/game/system_map/DetailMapGeometryPass.h"
#include "src/game/system_map/LocalMapPrimitiveRenderer.h"
#include "src/game/system_map/PlanetMapAxes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <glm/gtc/constants.hpp>

#include "render/HUD/TextRenderer.h"

namespace
{
constexpr double AU_KM = 149597870.7;

std::string formatDetailScaleDistance(double km)
{
    std::ostringstream stream;

    if (km >= AU_KM * 0.1)
        stream << std::fixed << std::setprecision(3) << (km / AU_KM) << " AU";
    else if (km >= 1000000.0)
        stream << std::fixed << std::setprecision(2) << (km / 1000000.0) << " M km";
    else if (km >= 1000.0)
        stream << std::fixed << std::setprecision(0) << km << " km";
    else
        stream << std::fixed << std::setprecision(1) << km << " km";

    return stream.str();
}
}

namespace game::system_map
{
const LocalMapCameraSnapshot& DetailMapGeometryPass::activeCamera() const
{
    if (!m_activeCamera)
        throw std::logic_error("DetailMapGeometryPass camera is unavailable");

    return *m_activeCamera;
}

void DetailMapGeometryPass::renderScene(
    const DetailMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& planet
)
{
    const LocalMapCameraSnapshot* previousCamera = m_activeCamera;
    m_activeCamera = &presentation.camera;

    struct RestoreCamera
    {
        const LocalMapCameraSnapshot*& slot;
        const LocalMapCameraSnapshot* previous;

        ~RestoreCamera()
        {
            slot = previous;
        }
    } restoreCamera {m_activeCamera, previousCamera};

    const glm::dvec2& centerPx = presentation.centerPx;
    const double maxRadiusMeters = presentation.maxRadiusMeters;
    const double scale = presentation.scale;
    (void)maxRadiusMeters;

    if (presentation.sceneIsSpatialVolume &&
        planet.detailHalfExtentMeters > 0.0)
    {
        const double h =
            planet.detailHalfExtentMeters;

        const std::array<glm::dvec3, 8> corners = {
            planet.planetCenterMeters + glm::dvec3(-h, -h, -h),
            planet.planetCenterMeters + glm::dvec3( h, -h, -h),
            planet.planetCenterMeters + glm::dvec3( h,  h, -h),
            planet.planetCenterMeters + glm::dvec3(-h,  h, -h),
            planet.planetCenterMeters + glm::dvec3(-h, -h,  h),
            planet.planetCenterMeters + glm::dvec3( h, -h,  h),
            planet.planetCenterMeters + glm::dvec3( h,  h,  h),
            planet.planetCenterMeters + glm::dvec3(-h,  h,  h)
        };

        std::array<glm::dvec2, 8> projected;

        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            projected[i] =
                activeCamera().project(corners[i]);
        }

        constexpr std::array<std::array<int, 2>, 12> edges = {{
            {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
            {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
        }};

        const glm::vec4 volumeColor(
            0.30f,
            0.66f,
            0.92f,
            0.42f
        );

        for (const auto& edge : edges)
        {
            drawPlanetMapLine(
                projected[edge[0]],
                projected[edge[1]],
                volumeColor
            );
        }
    }

    const glm::vec4 hubOrbitColor(
        0.45f,
        0.78f,
        1.0f,
        0.75f
    );

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192,
            hubOrbitColor
        );
    }

    const glm::vec4 playerOrbitColor(
        1.0f,
        0.75f,
        0.25f,
        0.9f
    );

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192,
            playerOrbitColor
        );
    }

    const glm::vec4 shipOrbitColor(0.70f, 0.78f, 0.86f, 0.42f);
    for (const auto& orbit : planet.shipOrbits)
        if (orbit.valid)
            drawDetailMapOrbit3D(orbit, planet, scale, centerPx, 192, shipOrbitColor);

    // trajectory labels: "<label>  Pe .. / Ap .." at the far point of the path
    {
        auto& text = TextRenderer::instance();
        text.beginFrameForViewport(viewport.width, viewport.height);
        for (const auto& orbit : planet.playerOrbits)
        {
            if (!orbit.valid || orbit.label.empty() || orbit.pathOffsetsMeters.empty())
                continue;
            std::size_t far = orbit.pathOffsetsMeters.size() - 1;
            if (orbit.pathClosed)
            {
                double best = -1.0;
                for (std::size_t i = 0; i < orbit.pathOffsetsMeters.size(); ++i)
                    if (const double r = glm::length(orbit.pathOffsetsMeters[i]); r > best)
                    {
                        best = r;
                        far = i;
                    }
            }
            const glm::dvec2 at = activeCamera().project(orbit.centerMeters + orbit.pathOffsetsMeters[far]);
            std::ostringstream line;
            line << orbit.label << "  Pe " << formatDetailScaleDistance(orbit.periapsisAltitudeMeters / 1000.0);
            if (orbit.apoapsisAltitudeMeters >= 0.0)
                line << " / Ap " << formatDetailScaleDistance(orbit.apoapsisAltitudeMeters / 1000.0);
            else
                line << " / open";
            const float x = static_cast<float>(std::clamp(at.x + 8.0, 4.0, std::max(4.0, viewport.width - 260.0)));
            const float y = static_cast<float>(std::clamp(at.y - 6.0, 16.0, std::max(16.0, viewport.height - 40.0)));
            text.textDrawPx(line.str(), x, y, 12, glm::vec4(1.0f, 0.80f, 0.40f, 0.95f));
        }
        text.endFrame();
    }

    core::hitch::mark("geo_orbits");
    drawRingAsteroids(planet, viewport);
    core::hitch::mark("geo_rings");
    drawBeltAsteroids(planet, viewport);
    core::hitch::mark("geo_belts");

    // Hub/ship/infrastructure markers are rendered by the shared tactical
    // object overlay. This geometry pass keeps celestial/orbit structure only.

    // The selected hub's name is drawn by the object overlay next to its
    // marker (selectOnClick); a second copy here doubled the label.

    const glm::vec4 bodyMarkerColor(
        0.72f,
        0.74f,
        0.78f,
        0.92f
    );

    // Context celestial bodies, including authored or procedural asteroids.
    for (const auto& body : planet.scene.objects)
    {
        if (!body.valid ||
            body.objectClass !=
                world::celestial::DetailObjectClass::CelestialBody)
        {
            continue;
        }

        const glm::dvec2 p =
            activeCamera().project(body.positionMeters);

        const double radiusPx =
            body.boundingRadiusMeters *
            scale *
            activeCamera().state.zoom;

        // a body drawn by the body renderer (DetailMapBackend) needs no outline
        const bool rendered = std::any_of(
            planet.contextBodies.begin(), planet.contextBodies.end(),
            [&](const world::celestial::DetailMapContextBody& c) { return c.id == body.stableId; });
        if (radiusPx >= 1.0 && !(rendered && radiusPx >= 2.0))
        {
            drawPlanetMapCircle(
                p,
                radiusPx,
                96,
                bodyMarkerColor
            );
        }

        if (radiusPx < 12.0)
        {
            drawPlanetMapCross(
                p,
                3.0f,
                bodyMarkerColor
            );
        }
    }

    // Infrastructure and self-propelled craft use the same tactical glyph
    // layer as System and Hub maps. No legacy crosses/axes are drawn here.

    const bool detailVolumeEmpty =
        presentation.sceneIsSpatialVolume &&
        planet.scene.empty();

    if (detailVolumeEmpty)
    {
        auto& text = TextRenderer::instance();
        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );
        text.textDrawPx(
            "NO OBJECTS DETECTED",
            static_cast<float>(centerPx.x - 84.0),
            static_cast<float>(viewport.height - 45),
            12,
            glm::vec4(0.58f, 0.68f, 0.76f, 0.82f)
        );
        text.endFrame();
    }

    if (presentation.sceneIsSpatialVolume &&
        planet.detailHalfExtentMeters > 0.0)
    {
        const double edgeKm =
            planet.detailHalfExtentMeters *
            2.0 /
            1000.0;

        const std::string edgeLabel =
            "CELL EDGE: " +
            formatDetailScaleDistance(
                edgeKm
            );

        auto& text =
            TextRenderer::instance();

        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );

        text.textDrawPx(
            edgeLabel,
            static_cast<float>(
                centerPx.x -
                static_cast<double>(
                    edgeLabel.size()
                ) *
                3.25
            ),
            static_cast<float>(
                viewport.height - 22
            ),
            12,
            glm::vec4(
                0.62f,
                0.72f,
                0.82f,
                0.88f
            )
        );

        text.endFrame();
    }

    if (m_visuals.drawBodyTitle &&
        planet.hasCentralBody &&
        !planet.planetName.empty())
    {
        const int titleSizePx =
            std::clamp(
                static_cast<int>(
                    std::lround(
                        static_cast<double>(
                            viewport.height
                        ) *
                        static_cast<double>(
                            m_visuals
                                .bodyTitleHeightFraction
                        )
                    )
                ),
                m_visuals.bodyTitleMinimumPx,
                m_visuals.bodyTitleMaximumPx
            );

        const float titleMarginPx =
            std::max(
                12.0f,
                static_cast<float>(
                    viewport.height
                ) *
                m_visuals
                    .bodyTitleMarginHeightFraction
            );

        auto& text =
            TextRenderer::instance();

        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );

        text.textDrawPx(
            planet.planetName,
            titleMarginPx,
            static_cast<float>(
                viewport.height -
                titleSizePx
            ) -
            titleMarginPx,
            titleSizePx,
            m_visuals.bodyTitleColor
        );

        text.endFrame();
    }
}

void DetailMapGeometryPass::drawPlanetMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b,
    const glm::vec4& color
)
{
    drawLocalMapLine(a, b, color);
}

void DetailMapGeometryPass::drawPlanetMapCross(
    const glm::dvec2& point,
    float size,
    const glm::vec4& color
)
{
    drawLocalMapCross(point, size, color);
}

void DetailMapGeometryPass::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments,
    const glm::vec4& color
)
{
    drawLocalMapCircle(center, radiusPx, segments, color);
}

void DetailMapGeometryPass::drawPlanetMapAxes(
    const glm::dvec3& originMeters,
    const world::celestial::LocalSceneAxes& axes,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double axisLenMeters
)
{
    (void)planet;
    (void)scale;
    (void)centerPx;

    const glm::dvec2 o =
        activeCamera().project(originMeters);

    const glm::dvec2 x =
        activeCamera().project(originMeters + axes.x * axisLenMeters);

    const glm::dvec2 y =
        activeCamera().project(originMeters + axes.y * axisLenMeters);

    const glm::dvec2 z =
        activeCamera().project(originMeters + axes.z * axisLenMeters);

    drawPlanetMapLine(
        o,
        x,
        glm::vec4(1.0f, 0.25f, 0.25f, 0.9f)
    );

    drawPlanetMapLine(
        o,
        y,
        glm::vec4(0.25f, 1.0f, 0.25f, 0.9f)
    );

    drawPlanetMapLine(
        o,
        z,
        glm::vec4(0.25f, 0.55f, 1.0f, 0.9f)
    );
}

void DetailMapGeometryPass::drawPlanetMapVelocityArrow(
    const glm::dvec3& originMeters,
    const glm::dvec3& velocityMps,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double lenMeters
)
{
    (void)planet;
    (void)scale;
    (void)centerPx;

    const double speed =
        glm::length(velocityMps);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocityMps / speed;

    const glm::dvec2 a =
        activeCamera().project(originMeters);

    const glm::dvec2 b =
        activeCamera().project(originMeters + dir * lenMeters);

    const glm::vec4 color(
        1.0f,
        0.92f,
        0.25f,
        0.95f
    );

    drawPlanetMapLine(a, b, color);
    drawPlanetMapCross(b, 4.0f, color);
}

namespace
{
// splitmix64: a stable hash -> the same cell always yields the same bodies
std::uint64_t ringHash(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ull;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebull;
    return x ^ (x >> 31);
}

double ringUnit(std::uint64_t& state)
{
    state = ringHash(state);
    return static_cast<double>(state >> 11) * (1.0 / 9007199254740992.0);
}

// a cross as two line segments in a batch: ring / belt bodies go to the GPU in one draw call
// (one call per body was ~80 ms a frame on Jupiter's rings)
void appendCross(std::vector<glm::dvec2>& lines, const glm::dvec2& p, double size)
{
    lines.emplace_back(p.x - size, p.y);
    lines.emplace_back(p.x + size, p.y);
    lines.emplace_back(p.x, p.y - size);
    lines.emplace_back(p.x, p.y + size);
}
} // namespace

void DetailMapGeometryPass::drawRingAsteroids(
    const world::celestial::DetailMapSnapshot& planet,
    const Viewport& viewport
)
{
    if (planet.rings.empty() || planet.planetRadiusMeters <= 0.0)
        return;

    // ring plane: the same basis as the ring renderer (prime / tilted east)
    const glm::dvec3 north = planetNorthAxisWorld(planet);
    const glm::dvec3 prime = planetPrimeAxisWorld(north);
    const glm::dvec3 east = planetEastAxisWorld(north, prime);
    const double inclination = glm::radians(planet.ringPlaneInclinationOffsetDeg);
    const glm::dvec3 axisX = prime;
    const glm::dvec3 axisY = glm::normalize(east * std::cos(inclination) + north * std::sin(inclination));
    const glm::dvec3 normal = glm::normalize(glm::cross(axisX, axisY));

    const double pixelsPerMeter =
        activeCamera().scale * activeCamera().state.zoom;
    const glm::vec4 colour(0.74f, 0.71f, 0.64f, 0.85f);

    std::uint64_t bodySeed = 1469598103934665603ull;
    for (const char c : planet.planetBodyId)
        bodySeed = (bodySeed ^ static_cast<unsigned char>(c)) * 1099511628211ull;

    constexpr int SectorCount = 2048;
    constexpr int LaneCount = 24;
    constexpr int MaxPerCell = 8;
    constexpr double PixelsPerBody = 22.0;   // ~one body per 22 x 22 px of ring
    std::vector<glm::dvec2> crosses;

    // The camera's rotation once (project() recomputes its trig per call) and the sector directions
    // once: ~50 000 cells a ring, most of them culled by the sector's screen box or empty at this zoom.
    const LocalMapCameraSnapshot& cam = activeCamera();
    const double cy = std::cos(cam.state.yaw), sy = std::sin(cam.state.yaw);
    const double cp = std::cos(cam.state.pitch), sp = std::sin(cam.state.pitch);
    const auto toCamera = [&](const glm::dvec3& v) {
        const double yx = v.x * cy - v.z * sy;
        const double yz = v.x * sy + v.z * cy;
        return glm::dvec3(yx, v.y * cp - yz * sp, v.y * sp + yz * cp);
    };
    const auto projectCamera = [&](const glm::dvec3& c) {
        const double k = pixelsPerMeter * cam.perspectiveFactor(c.z);
        return glm::dvec2(cam.centerPx.x + cam.state.pan.x + c.x * k, cam.centerPx.y + cam.state.pan.y - c.y * k);
    };
    // camera-space ring geometry: centre + (X cos a + Y sin a) r + N h
    const glm::dvec3 centreCam = toCamera(planet.planetCenterMeters - cam.originMeters);
    const glm::dvec3 xCam = toCamera(axisX), yCam = toCamera(axisY), nCam = toCamera(normal);
    static const std::vector<glm::dvec2> sectorDir = [] {
        std::vector<glm::dvec2> d(SectorCount + 1);
        for (int i = 0; i <= SectorCount; ++i)
        {
            const double a = glm::two_pi<double>() * i / SectorCount;
            d[static_cast<std::size_t>(i)] = glm::dvec2(std::cos(a), std::sin(a));
        }
        return d;
    }();
    const double radius2 = planet.planetRadiusMeters * planet.planetRadiusMeters;

    for (std::size_t ringIndex = 0; ringIndex < planet.rings.size(); ++ringIndex)
    {
        const auto& ring = planet.rings[ringIndex];
        const double inner = ring.innerRadiusKm * 1000.0;
        const double outer = ring.outerRadiusKm * 1000.0;
        if (outer <= inner || inner <= 0.0)
            continue;
        // the whole ring below a few pixels: the ring texture says enough
        if (outer * pixelsPerMeter < 40.0)
            continue;
        const double density = std::clamp(static_cast<double>(ring.opticalDepth) * 1.5, 0.03, 1.0);

        const double laneWidth = (outer - inner) / LaneCount;
        for (int sector = 0; sector < SectorCount; ++sector)
        {
            const double a0 = glm::two_pi<double>() * sector / SectorCount;
            // the sector's radial span on screen (both edges, inner and outer) - all its lanes culled at once
            const double sectorArcPx = outer * glm::two_pi<double>() / SectorCount * pixelsPerMeter;
            const double margin = std::max(laneWidth * pixelsPerMeter, sectorArcPx) + 8.0;
            glm::dvec2 lo(1.0e30), hi(-1.0e30);
            for (const int edgeIndex : {sector, sector + 1})
            {
                const glm::dvec2 d = sectorDir[static_cast<std::size_t>(edgeIndex)];
                const glm::dvec3 dirCam = xCam * d.x + yCam * d.y;
                for (const double r : {inner, outer})
                {
                    const glm::dvec2 q = projectCamera(centreCam + dirCam * r);
                    lo = glm::min(lo, q);
                    hi = glm::max(hi, q);
                }
            }
            if (hi.x < -margin || hi.y < -margin || lo.x > viewport.width + margin || lo.y > viewport.height + margin)
                continue;
            for (int lane = 0; lane < LaneCount; ++lane)
            {
                const double rMid = inner + (lane + 0.5) * laneWidth;
                // cell footprint on screen (a perspective-free estimate)
                const double cellPx = std::max(laneWidth, rMid * glm::two_pi<double>() / SectorCount) * pixelsPerMeter;

                std::uint64_t state = bodySeed ^ ringHash((static_cast<std::uint64_t>(ringIndex) << 40) ^
                                                          (static_cast<std::uint64_t>(sector) << 12) ^
                                                          static_cast<std::uint64_t>(lane));
                // bodies of this cell shown at the current zoom
                // dense rings (high optical depth) show many bodies, dusty ones few
                const double wanted = cellPx * cellPx / (PixelsPerBody * PixelsPerBody) * density;
                int count = static_cast<int>(std::min<double>(MaxPerCell, std::floor(wanted)));
                if (count == 0 && ringUnit(state) < wanted)
                    count = 1;
                for (int k = 0; k < count; ++k)
                {
                    const double u = ringUnit(state);
                    const double v = ringUnit(state);
                    const double w = ringUnit(state) - 0.5;
                    const double size = ringUnit(state);
                    const double a = a0 + glm::two_pi<double>() / SectorCount * u;
                    const double r = inner + (lane + v) * laneWidth;
                    // planet-relative, camera space
                    const glm::dvec3 rel = (xCam * std::cos(a) + yCam * std::sin(a)) * r +
                        nCam * (w * 0.002 * planet.planetRadiusMeters);   // a thin ring
                    // behind the planet disc: hidden (planet is opaque)
                    if (rel.z < 0.0 && rel.x * rel.x + rel.y * rel.y < radius2)
                        continue;
                    const glm::dvec2 spt = projectCamera(centreCam + rel);
                    if (spt.x < -8.0 || spt.y < -8.0 || spt.x > viewport.width + 8.0 || spt.y > viewport.height + 8.0)
                        continue;
                    // a few big bodies, many small ones (power law of the visible size)
                    const double px = 1.2 + 3.0 * std::pow(size, 6.0);
                    appendCross(crosses, spt, px);
                }
            }
        }
    }
    drawLocalMapLines(crosses.data(), crosses.size(), colour);
}

void DetailMapGeometryPass::drawBeltAsteroids(
    const world::celestial::DetailMapSnapshot& planet,
    const Viewport& viewport
)
{
    if (!planet.asteroidBelt || planet.beltOuterMeters <= planet.beltInnerMeters)
        return;

    // star-fixed polar cells of ~100 000 km x 100 000 km
    const glm::dvec3 n = glm::normalize(planet.beltNormal);
    glm::dvec3 axisX = glm::normalize(glm::cross(n, glm::dvec3(0.0, 0.0, 1.0)));
    if (!std::isfinite(axisX.x))
        axisX = glm::dvec3(1.0, 0.0, 0.0);
    const glm::dvec3 axisY = glm::normalize(glm::cross(n, axisX));
    const double inner = planet.beltInnerMeters;
    const double outer = planet.beltOuterMeters;
    constexpr double CellMeters = 1.0e8;
    const int LaneCount = std::max(1, static_cast<int>((outer - inner) / CellMeters));
    const double laneWidth = (outer - inner) / LaneCount;
    const double midRadius = 0.5 * (inner + outer);
    const int sectorCount = std::max(64, static_cast<int>(glm::two_pi<double>() * midRadius / CellMeters));

    // the viewed window: the belt point +- (view half width) along the orbit
    const double pixelsPerMeter = activeCamera().scale * activeCamera().state.zoom;
    const double viewHalfMeters = 0.75 * std::max(viewport.width, viewport.height) / std::max(pixelsPerMeter, 1e-18);
    const glm::dvec3 rel = planet.planetCenterMeters - planet.beltCenterMeters;
    const double theta0 = std::atan2(glm::dot(rel, axisY), glm::dot(rel, axisX));
    const double halfSpan = std::min(glm::pi<double>(), viewHalfMeters / std::max(midRadius, 1.0) + 2.0 * glm::two_pi<double>() / sectorCount);
    const int s0 = static_cast<int>(std::floor((theta0 - halfSpan) / glm::two_pi<double>() * sectorCount));
    const int s1 = static_cast<int>(std::ceil((theta0 + halfSpan) / glm::two_pi<double>() * sectorCount));
    const double rView = glm::length(rel - n * glm::dot(rel, n));
    const int lane0 = std::max(0, static_cast<int>(std::floor((rView - viewHalfMeters - inner) / laneWidth)) - 1);
    const int lane1 = std::min(LaneCount - 1, static_cast<int>(std::ceil((rView + viewHalfMeters - inner) / laneWidth)) + 1);
    if (static_cast<double>(s1 - s0) * std::max(0, lane1 - lane0) > 60000.0)
        return;   // too far out: the System map draws the belt

    std::uint64_t bodySeed = 1469598103934665603ull;
    for (const char c : planet.planetBodyId)
        bodySeed = (bodySeed ^ static_cast<unsigned char>(c)) * 1099511628211ull;

    // orientation: the belt edges (arcs) and the direction to the star
    {
        const glm::vec4 edge(0.55f, 0.62f, 0.72f, 0.45f);
        std::vector<glm::dvec2> arcs;
        for (const double radius : {inner, outer})
        {
            glm::dvec2 previous {0.0};
            for (int i = 0; i <= 96; ++i)
            {
                const double a = theta0 - halfSpan + 2.0 * halfSpan * i / 96.0;
                const glm::dvec2 q = activeCamera().project(
                    planet.beltCenterMeters + (axisX * std::cos(a) + axisY * std::sin(a)) * radius);
                if (i > 0)
                {
                    arcs.push_back(previous);
                    arcs.push_back(q);
                }
                previous = q;
            }
        }
        drawLocalMapLines(arcs.data(), arcs.size(), edge);
        const glm::dvec2 here = activeCamera().project(planet.planetCenterMeters);
        const glm::dvec2 star = activeCamera().project(planet.beltCenterMeters);
        glm::dvec2 dir = star - here;
        if (glm::length(dir) > 1.0)
        {
            dir = glm::normalize(dir);
            const double len = 0.38 * std::min(viewport.width, viewport.height);
            const glm::dvec2 tip = here + dir * len;
            const glm::vec4 arrow(1.0f, 0.86f, 0.45f, 0.75f);
            drawPlanetMapLine(here + dir * 20.0, tip, arrow);
            const glm::dvec2 side(-dir.y, dir.x);
            drawPlanetMapLine(tip, tip - dir * 12.0 + side * 6.0, arrow);
            drawPlanetMapLine(tip, tip - dir * 12.0 - side * 6.0, arrow);
            auto& text = TextRenderer::instance();
            text.beginFrameForViewport(viewport.width, viewport.height);
            char label[48];
            std::snprintf(label, sizeof label, "* %.2f AU",
                          glm::length(planet.planetCenterMeters - planet.beltCenterMeters) / 1.495978707e11);
            text.textDrawPx(label,
                            static_cast<float>(tip.x + 8.0), static_cast<float>(tip.y - 7.0), 13, arrow);
            text.endFrame();
        }
    }

    const glm::vec4 colour(0.70f, 0.66f, 0.58f, 0.85f);
    constexpr int MaxPerCell = 12;
    constexpr double PixelsPerBody = 26.0;
    std::vector<glm::dvec2> crosses;
    for (int sRaw = s0; sRaw <= s1; ++sRaw)
    {
        const int sector = ((sRaw % sectorCount) + sectorCount) % sectorCount;
        const double a0 = glm::two_pi<double>() * sector / sectorCount;
        const double arc = glm::two_pi<double>() / sectorCount;
        for (int lane = lane0; lane <= lane1; ++lane)
        {
            // denser in the middle of the belt (Kirkwood-free bell profile)
            const double laneMid = (lane + 0.5) / static_cast<double>(LaneCount);
            const double profile = std::exp(-std::pow((laneMid - 0.5) / 0.22, 2.0));
            const double rMid = inner + (lane + 0.5) * laneWidth;
            const glm::dvec3 centre = planet.beltCenterMeters +
                (axisX * std::cos(a0 + 0.5 * arc) + axisY * std::sin(a0 + 0.5 * arc)) * rMid;
            const glm::dvec2 c = activeCamera().project(centre);
            const double cellPx = std::max(laneWidth, rMid * arc) * pixelsPerMeter;
            const double margin = cellPx + 8.0;
            if (c.x < -margin || c.y < -margin || c.x > viewport.width + margin || c.y > viewport.height + margin)
                continue;
            std::uint64_t state = bodySeed ^ ringHash((static_cast<std::uint64_t>(sector) << 16) ^
                                                      static_cast<std::uint64_t>(lane) ^ 0xB317ull << 48);
            const double wanted = cellPx * cellPx / (PixelsPerBody * PixelsPerBody) * profile;
            int count = static_cast<int>(std::min<double>(MaxPerCell, std::floor(wanted)));
            if (count == 0 && ringUnit(state) < wanted)
                count = 1;
            for (int k = 0; k < count; ++k)
            {
                const double u = ringUnit(state);
                const double v = ringUnit(state);
                const double w = ringUnit(state) - 0.5;
                const double size = ringUnit(state);
                const double a = a0 + arc * u;
                const double r = inner + (lane + v) * laneWidth;
                const glm::dvec3 p = planet.beltCenterMeters +
                    (axisX * std::cos(a) + axisY * std::sin(a)) * r +
                    n * (w * 2.0 * planet.beltHalfThicknessMeters * profile);
                const glm::dvec2 sp = activeCamera().project(p);
                const double px = 1.2 + 3.5 * std::pow(size, 7.0);
                appendCross(crosses, sp, px);
            }
        }
    }
    drawLocalMapLines(crosses.data(), crosses.size(), colour);
}

void DetailMapGeometryPass::drawDetailMapOrbit3D(
    const world::celestial::DetailMapOrbit& orbit,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments,
    const glm::vec4& color
)
{
    (void)scale;
    (void)centerPx;

    if (!orbit.valid || orbit.radiusMeters <= 1.0)
        return;

    const bool path = !orbit.pathOffsetsMeters.empty();
    segments = path
        ? static_cast<int>(orbit.pathOffsetsMeters.size()) - (orbit.pathClosed ? 0 : 1)
        : std::max(segments, 32);
    if (segments < 1)
        return;

    glm::dvec3 radial =
        orbit.radialAxis;

    glm::dvec3 prograde =
        orbit.progradeAxis;

    if (glm::length(radial) < 0.001)
        radial = glm::dvec3(1.0, 0.0, 0.0);

    if (glm::length(prograde) < 0.001)
        prograde = glm::dvec3(0.0, 0.0, 1.0);

    radial =
        glm::normalize(radial);

    prograde =
        glm::normalize(
            prograde -
            radial * glm::dot(prograde, radial)
        );

    auto orbitPoint =
        [&](int i) -> glm::dvec3
        {
            if (path)
                return orbit.centerMeters +
                    orbit.pathOffsetsMeters[static_cast<std::size_t>(i) % orbit.pathOffsetsMeters.size()];
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            return
                orbit.centerMeters +
                radial * std::cos(a) * orbit.radiusMeters +
                prograde * std::sin(a) * orbit.radiusMeters;
        };

    auto isHiddenBehindPlanet =
        [&](const glm::dvec3& worldPoint) -> bool
        {
            const glm::dvec3 relative =
                worldPoint -
                planet.planetCenterMeters;

            const glm::dvec3 cameraSpace =
                activeCamera().vectorToCamera(relative);

            const double projectedDistance2 =
                cameraSpace.x * cameraSpace.x +
                cameraSpace.y * cameraSpace.y;

            const double planetRadius =
                planet.hasCentralBody ? planet.planetRadiusMeters : 0.0;

            const bool behindPlanetCenter =
                cameraSpace.z < 0.0;

            const bool insidePlanetDisc =
                projectedDistance2 <
                planetRadius * planetRadius;

            return
                behindPlanetCenter &&
                insidePlanetDisc;
        };

    std::vector<glm::dvec2> visibleEndpoints;
    std::vector<glm::dvec2> hiddenEndpoints;
    visibleEndpoints.reserve(static_cast<std::size_t>(segments) * 2u);
    hiddenEndpoints.reserve(static_cast<std::size_t>(segments) * 2u);

    for (int i = 0; i < segments; ++i)
    {
        const glm::dvec3 p0 =
            orbitPoint(i);

        const glm::dvec3 p1 =
            orbitPoint(path && !orbit.pathClosed ? i + 1 : (i + 1) % segments);

        const glm::dvec3 mid =
            (p0 + p1) * 0.5;

        /*
            In Planet Details the far half of the orbit remains visible as a
            very weak navigation cue. Hub Map does not use this function.
        */
        const bool hidden =
            isHiddenBehindPlanet(mid);

        auto& endpoints =
            hidden
                ? hiddenEndpoints
                : visibleEndpoints;

        endpoints.push_back(activeCamera().project(p0));
        endpoints.push_back(activeCamera().project(p1));
    }

    glm::vec4 hiddenColor = color;
    hiddenColor.a *= 0.16f;

    drawLocalMapLines(
        hiddenEndpoints.data(),
        hiddenEndpoints.size(),
        hiddenColor
    );

    drawLocalMapLines(
        visibleEndpoints.data(),
        visibleEndpoints.size(),
        color
    );
}

}
