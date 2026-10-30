#include "src/game/system_map/GalaxyPointerResolver.h"

#include "src/world/celestial/SystemMapTypes.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& viewport,
        bool& visible,
        float& depth
    )
    {
        const glm::vec4 clip = mvp * glm::vec4(world, 1.0f);
        visible = false;
        depth = 1.0f;
        // Behind the camera (w <= 0) is never a pointer target.
        if (clip.w <= 0.00001f)
            return glm::vec2(0.0f);
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        visible =
            ndc.x >= -1.0f && ndc.x <= 1.0f &&
            ndc.y >= -1.0f && ndc.y <= 1.0f &&
            ndc.z >= -1.0f && ndc.z <= 1.0f;
        depth = ndc.z;
        return glm::vec2(
            (ndc.x * 0.5f + 0.5f) * static_cast<float>(viewport.width),
            (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(viewport.height));
    }

    using game::system_map::GalaxyPointerHitReason;
    using game::system_map::GalaxyPointerTarget;
    using game::system_map::GalaxyPointerTargetKind;

    // A drawn control under / near the cursor (tier 2).
    struct DrawnHit
    {
        GalaxyPointerTarget target;
        bool inside = false;     // cursor inside the drawn shape
        float overshootPx = 0.0f; // distance beyond the drawn edge
        int kindRank = 0;        // star 0 < cube marker 1 < overlay glyph 2
        float tieKey = 0.0f;     // inside: depth for stars, distance otherwise
    };

    bool betterDrawnHit(const DrawnHit& a, const DrawnHit& b)
    {
        if (a.inside != b.inside)
            return a.inside;
        if (!a.inside && std::abs(a.overshootPx - b.overshootPx) > 1.0e-3f)
            return a.overshootPx < b.overshootPx;
        if (a.kindRank != b.kindRank)
            return a.kindRank < b.kindRank;
        if (std::abs(a.tieKey - b.tieKey) > 1.0e-6f)
            return a.tieKey < b.tieKey;
        // Deterministic identity tie-break.
        if (a.target.systemId != b.target.systemId)
            return a.target.systemId < b.target.systemId;
        if (a.target.cell && b.target.cell)
        {
            const auto& ia = a.target.cell->index;
            const auto& ib = b.target.cell->index;
            if (ia.x != ib.x) return ia.x < ib.x;
            if (ia.y != ib.y) return ia.y < ib.y;
            if (ia.z != ib.z) return ia.z < ib.z;
        }
        return a.target.overlayObjectId < b.target.overlayObjectId;
    }
}

namespace game::system_map
{
    const char* galaxyPointerTargetKindName(GalaxyPointerTargetKind kind)
    {
        switch (kind)
        {
            case GalaxyPointerTargetKind::Empty: return "empty";
            case GalaxyPointerTargetKind::Panel: return "panel";
            case GalaxyPointerTargetKind::OverlayGlyph: return "overlay_glyph";
            case GalaxyPointerTargetKind::Star: return "star";
            case GalaxyPointerTargetKind::NavigationCube: return "cube";
            case GalaxyPointerTargetKind::Medium: return "medium";
        }
        return "?";
    }

    const char* galaxyPointerHitReasonName(GalaxyPointerHitReason reason)
    {
        switch (reason)
        {
            case GalaxyPointerHitReason::None: return "none";
            case GalaxyPointerHitReason::PanelArea: return "panel_area";
            case GalaxyPointerHitReason::StarGlyph: return "star_glyph";
            case GalaxyPointerHitReason::CubeMarker: return "cube_marker";
            case GalaxyPointerHitReason::OverlayGlyph: return "overlay_glyph";
            case GalaxyPointerHitReason::StarProximity: return "star_proximity";
            case GalaxyPointerHitReason::MediumShell: return "medium_shell";
            case GalaxyPointerHitReason::IcyFieldMarker: return "icy_field_marker";
            case GalaxyPointerHitReason::CubeSequenceGlyph: return "cube_sequence_glyph";
            case GalaxyPointerHitReason::HierarchyLayer: return "hierarchy_layer";
            case GalaxyPointerHitReason::NeighbourCube: return "neighbour_cube";
        }
        return "?";
    }

    bool GalaxyPointerTarget::sameSemanticTarget(const GalaxyPointerTarget& other) const
    {
        if (kind != other.kind)
            return false;
        switch (kind)
        {
            case GalaxyPointerTargetKind::Star:
                return systemId == other.systemId;
            case GalaxyPointerTargetKind::NavigationCube:
                return cell && other.cell && cell->level == other.cell->level &&
                       cell->index == other.cell->index;
            case GalaxyPointerTargetKind::Medium:
                return mediumObject == other.mediumObject;
            case GalaxyPointerTargetKind::OverlayGlyph:
                return overlayObjectId == other.overlayObjectId;
            default:
                return true;
        }
    }

    void galaxyPointerRayLy(
        const GalaxyPointerScene& scene,
        const glm::vec2& mousePx,
        glm::dvec3& originLy,
        glm::dvec3& directionLy
    )
    {
        const Viewport& viewport = scene.viewport;
        const double ndcX = 2.0 * mousePx.x / static_cast<double>(std::max(viewport.width, 1)) - 1.0;
        const double ndcY = 1.0 - 2.0 * mousePx.y / static_cast<double>(std::max(viewport.height, 1));
        if (scene.camera)
        {
            const auto& c = *scene.camera;
            originLy = c.eyeLy;
            directionLy = glm::normalize(
                c.forward +
                c.right * (ndcX * c.tanHalfFovY * c.aspect) +
                c.up * (ndcY * c.tanHalfFovY));
            return;
        }
        const double renderUnitsPerLy = static_cast<double>(scene.renderUnitsPerLy);
        const glm::dmat4 inverse = glm::inverse(glm::dmat4(scene.mvp));
        glm::dvec4 nearPoint = inverse * glm::dvec4(ndcX, ndcY, -1.0, 1.0);
        glm::dvec4 farPoint = inverse * glm::dvec4(ndcX, ndcY, 1.0, 1.0);
        nearPoint /= nearPoint.w;
        farPoint /= farPoint.w;
        originLy = glm::dvec3(nearPoint) / renderUnitsPerLy;
        directionLy = glm::normalize(glm::dvec3(farPoint - nearPoint));
    }

    int pickGalaxyMedium(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const glm::mat4& mvp,
        const Viewport& viewport,
        float renderUnitsPerLy,
        const glm::vec2& mousePx,
        const glm::dvec3& origin,
        const glm::dvec3& direction,
        float icyFieldPickRadiusPx,
        glm::dvec3& hitLy,
        float& hitDepth,
        float minRadiusPx
    )
    {
        const auto& atlas = galaxy.mediumAtlas;

        // Medium LOD: projected radius in pixels from the clip w.
        const glm::mat4 mvpLy = mvp * glm::mat4(
            glm::vec4(renderUnitsPerLy, 0.0f, 0.0f, 0.0f), glm::vec4(0.0f, renderUnitsPerLy, 0.0f, 0.0f),
            glm::vec4(0.0f, 0.0f, renderUnitsPerLy, 0.0f), glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        const float clipYPerLy = glm::length(glm::vec3(mvpLy[0][1], mvpLy[1][1], mvpLy[2][1]));
        const float halfHeightPx = 0.5f * static_cast<float>(std::max(viewport.height, 1));

        int best = -1;
        double bestT = 1.0e30;
        if (atlas)
            for (const auto& proxy : atlas->proxies)
            {
                // Prefilter: the ray against the proxy's bounding sphere
                // (exact, also for a shell around or behind the camera).
                const glm::dvec3 toCentre = proxy.centreLy - origin;
                const double along = glm::dot(toCentre, direction);
                const double radius = static_cast<double>(proxy.radiusLy);
                const double missSquared = glm::dot(toCentre, toCentre) - along * along;
                if (missSquared > radius * radius || along + radius < 0.0 || along - radius > bestT)
                    continue;
                if (minRadiusPx > 0.0f)
                {
                    const float w = (mvpLy * glm::vec4(glm::vec3(proxy.centreLy), 1.0f)).w;
                    if (w > 1.0e-4f && proxy.radiusLy * clipYPerLy / w * halfHeightPx < minRadiusPx)
                        continue;
                }
                // Exact hit on the coarse shell (Moller-Trumbore). A shell
                // the camera is INSIDE (odd number of crossings ahead) is not
                // a visible object under the cursor: its far wall would
                // otherwise swallow every click on empty space.
                const auto& range = proxy.coarse;
                std::vector<double> crossings;
                for (std::uint32_t v = range.first; v + 2 < range.first + range.count; v += 3)
                {
                    const glm::dvec3 a(atlas->vertices[v].positionLy);
                    const glm::dvec3 e1 = glm::dvec3(atlas->vertices[v + 1].positionLy) - a;
                    const glm::dvec3 e2 = glm::dvec3(atlas->vertices[v + 2].positionLy) - a;
                    const glm::dvec3 pv = glm::cross(direction, e2);
                    const double det = glm::dot(e1, pv);
                    if (std::abs(det) < 1.0e-12)
                        continue;
                    const glm::dvec3 tv = origin - a;
                    const double u = glm::dot(tv, pv) / det;
                    if (u < 0.0 || u > 1.0)
                        continue;
                    const glm::dvec3 qv = glm::cross(tv, e1);
                    const double w = glm::dot(direction, qv) / det;
                    if (w < 0.0 || u + w > 1.0)
                        continue;
                    const double t = glm::dot(e2, qv) / det;
                    if (t > 0.0)
                        crossings.push_back(t);
                }
                if (crossings.empty())
                    continue;
                // A ray through a shared edge / vertex hits two triangles at
                // the same distance: one crossing.
                std::sort(crossings.begin(), crossings.end());
                const double tolerance = 1.0e-7 * std::max(1.0, crossings.back());
                std::size_t distinct = 1;
                for (std::size_t k = 1; k < crossings.size(); ++k)
                    if (crossings[k] - crossings[k - 1] > tolerance)
                        ++distinct;
                if (distinct % 2 == 0 && crossings.front() < bestT)
                {
                    bestT = crossings.front();
                    best = proxy.objectIndex;
                }
            }
        if (best >= 0)
        {
            hitLy = origin + direction * bestT;
            bool visible = false;
            projectToScreen(glm::vec3(hitLy) * renderUnitsPerLy, mvp, viewport, visible, hitDepth);
            return best;
        }
        // Icy debris fields: their sparse marker (screen distance).
        float bestPx = icyFieldPickRadiusPx;
        for (std::size_t i = 0; i < galaxy.objects.size(); ++i)
        {
            if (galaxy.objects[i].objectType != "icy_debris_field")
                continue;
            bool visible = false;
            float depth = 1.0f;
            const glm::vec2 screen = projectToScreen(
                glm::vec3(galaxy.objects[i].positionLy) * renderUnitsPerLy, mvp, viewport, visible, depth);
            const float d = glm::length(mousePx - screen);
            if (visible && d < bestPx)
            {
                bestPx = d;
                best = static_cast<int>(i);
                hitLy = galaxy.objects[i].positionLy;
                hitDepth = depth;
            }
        }
        return best;
    }

    GalaxyPointerResolution resolveGalaxyPointer(
        const GalaxyPointerScene& scene,
        const glm::vec2& mousePx,
        bool collectCandidates
    )
    {
        GalaxyPointerResolution resolution;
        auto& winner = resolution.winner;

        if (scene.panelUnderCursor)
        {
            winner.kind = GalaxyPointerTargetKind::Panel;
            winner.reason = GalaxyPointerHitReason::PanelArea;
            if (collectCandidates)
                resolution.candidates.push_back(winner);
            return resolution;
        }

        // Tier 2: drawn controls. Tier 3 (star proximity) is collected in
        // the same pass over the presented stars.
        std::optional<DrawnHit> bestDrawn;
        const auto consider = [&](const DrawnHit& hit)
        {
            if (collectCandidates)
                resolution.candidates.push_back(hit.target);
            if (!bestDrawn || betterDrawnHit(hit, *bestDrawn))
                bestDrawn = hit;
        };

        std::optional<GalaxyPointerTarget> nearestStar;
        if (scene.stars)
            for (const auto& star : *scene.stars)
            {
                if (!star.visible)
                    continue;
                const float d = glm::length(mousePx - star.screen);
                if (d <= star.glyphRadiusPx + scene.glyphHitMarginPx)
                {
                    DrawnHit hit;
                    hit.target.kind = GalaxyPointerTargetKind::Star;
                    hit.target.reason = GalaxyPointerHitReason::StarGlyph;
                    hit.target.systemId = star.systemId;
                    hit.target.screenDistancePx = d;
                    hit.target.depth = star.depth;
                    hit.inside = d <= star.glyphRadiusPx;
                    hit.overshootPx = std::max(0.0f, d - star.glyphRadiusPx);
                    hit.kindRank = 0;
                    // Inside overlapping glyphs: the foreground star is drawn
                    // on top. Near the edge: the nearer centre.
                    hit.tieKey = hit.inside ? star.depth : d;
                    consider(hit);
                }
                if (d <= scene.starProximityRadiusPx)
                {
                    GalaxyPointerTarget candidate;
                    candidate.kind = GalaxyPointerTargetKind::Star;
                    candidate.reason = GalaxyPointerHitReason::StarProximity;
                    candidate.systemId = star.systemId;
                    candidate.screenDistancePx = d;
                    candidate.depth = star.depth;
                    const bool better =
                        !nearestStar ||
                        d < nearestStar->screenDistancePx - 1.0f ||
                        (std::abs(d - nearestStar->screenDistancePx) <= 1.0f &&
                         (star.depth < nearestStar->depth - 1.0e-6f ||
                          (std::abs(star.depth - nearestStar->depth) <= 1.0e-6f &&
                           star.systemId < nearestStar->systemId)));
                    if (better)
                        nearestStar = candidate;
                }
            }

        if (scene.cubeMarkers)
            for (const auto& marker : *scene.cubeMarkers)
            {
                if (!marker.visible)
                    continue;
                const float d = glm::length(mousePx - marker.screen);
                if (d > marker.radiusPx + scene.glyphHitMarginPx)
                    continue;
                DrawnHit hit;
                hit.target.kind = GalaxyPointerTargetKind::NavigationCube;
                hit.target.reason = GalaxyPointerHitReason::CubeMarker;
                hit.target.cell = marker.cell;
                hit.target.screenDistancePx = d;
                hit.target.depth = marker.depth;
                hit.inside = d <= marker.radiusPx;
                hit.overshootPx = std::max(0.0f, d - marker.radiusPx);
                hit.kindRank = 1;
                hit.tieKey = d;
                consider(hit);
            }

        if (scene.overlayGlyphs)
            for (const auto& glyph : *scene.overlayGlyphs)
            {
                const float d = glm::length(mousePx - glyph.screen);
                if (d > glyph.hitRadiusPx)
                    continue;
                DrawnHit hit;
                hit.target.kind = GalaxyPointerTargetKind::OverlayGlyph;
                hit.target.reason = GalaxyPointerHitReason::OverlayGlyph;
                hit.target.overlayObjectId = glyph.objectId;
                hit.target.cell = glyph.semanticCell;
                hit.target.screenDistancePx = d;
                hit.target.depth = -1.0f; // screen overlay
                // The overlay glyph's declared hit circle is its drawn area.
                hit.inside = true;
                hit.kindRank = 2;
                hit.tieKey = d;
                consider(hit);
            }

        if (collectCandidates && nearestStar)
            resolution.candidates.push_back(*nearestStar);

        if (bestDrawn)
        {
            winner = bestDrawn->target;
            if (!collectCandidates)
                return resolution;
        }
        else if (nearestStar)
        {
            winner = *nearestStar;
            if (!collectCandidates)
                return resolution;
        }

        // Tier 4: the medium (only asked when nothing more specific won, or
        // when the trace wants every candidate).
        if (scene.galaxy)
        {
            glm::dvec3 hitLy(0.0);
            float depth = 1.0f;
            glm::dvec3 rayOrigin(0.0);
            glm::dvec3 rayDirection(0.0, 0.0, -1.0);
            galaxyPointerRayLy(scene, mousePx, rayOrigin, rayDirection);
            const int object = pickGalaxyMedium(
                *scene.galaxy, scene.mvp, scene.viewport, scene.renderUnitsPerLy, mousePx,
                rayOrigin, rayDirection, scene.icyFieldPickRadiusPx, hitLy, depth, scene.mediumMinRadiusPx);
            if (object >= 0)
            {
                GalaxyPointerTarget medium;
                medium.kind = GalaxyPointerTargetKind::Medium;
                const auto& objects = scene.galaxy->objects;
                medium.reason =
                    static_cast<std::size_t>(object) < objects.size() &&
                            objects[static_cast<std::size_t>(object)].objectType == "icy_debris_field"
                        ? GalaxyPointerHitReason::IcyFieldMarker
                        : GalaxyPointerHitReason::MediumShell;
                medium.mediumObject = object;
                medium.hitLy = hitLy;
                medium.depth = depth;
                if (collectCandidates)
                    resolution.candidates.push_back(medium);
                if (winner.kind == GalaxyPointerTargetKind::Empty)
                    winner = medium;
            }
        }

        // Tier 5: empty space inside the current parent - the child cube
        // where the cursor ray meets the hover layer.
        if (scene.hierarchy && scene.hierarchy->grid && scene.hierarchy->layer.valid &&
            (collectCandidates || winner.kind == GalaxyPointerTargetKind::Empty))
        {
            const auto& hierarchy = *scene.hierarchy;
            glm::dvec3 origin(0.0);
            glm::dvec3 direction(0.0, 0.0, -1.0);
            galaxyPointerRayLy(scene, mousePx, origin, direction);
            const int axis = hierarchy.layer.axis;
            const double along = direction[axis];
            bool insideParent = false;
            if (std::abs(along) > 1.0e-9)
            {
                const double t = (hierarchy.layer.coordinateLy - origin[axis]) / along;
                if (t > 0.0)
                {
                    const glm::dvec3 point = origin + direction * t;
                    insideParent = galaxyHierarchyContains(hierarchy.box, point);
                    if (const auto child = galaxyHierarchyChildAt(*hierarchy.grid, hierarchy.box, point))
                    {
                        GalaxyPointerTarget cube;
                        cube.kind = GalaxyPointerTargetKind::NavigationCube;
                        cube.reason = GalaxyPointerHitReason::HierarchyLayer;
                        cube.cell = *child;
                        cube.hitLy = point;
                        bool visible = false;
                        projectToScreen(glm::vec3(point) * scene.renderUnitsPerLy, scene.mvp, scene.viewport,
                                        visible, cube.depth);
                        if (collectCandidates)
                            resolution.candidates.push_back(cube);
                        if (winner.kind == GalaxyPointerTargetKind::Empty)
                            winner = cube;
                    }
                }
            }

            // Outside the parent: the neighbour cube the ray enters first.
            if (!insideParent && !hierarchy.neighbours.empty())
            {
                double bestT = 1.0e300;
                const navigation::CubicNavigationCell* best = nullptr;
                for (const auto& cell : hierarchy.neighbours)
                {
                    const double half = cell.size * 0.5;
                    double tNear = -1.0e300;
                    double tFar = 1.0e300;
                    bool miss = false;
                    for (int a = 0; a < 3 && !miss; ++a)
                    {
                        const double lo = cell.center[a] - half - origin[a];
                        const double hi = cell.center[a] + half - origin[a];
                        if (std::abs(direction[a]) < 1.0e-12)
                        {
                            miss = lo > 0.0 || hi < 0.0;
                            continue;
                        }
                        double t0 = lo / direction[a];
                        double t1 = hi / direction[a];
                        if (t0 > t1)
                            std::swap(t0, t1);
                        tNear = std::max(tNear, t0);
                        tFar = std::min(tFar, t1);
                        miss = tNear > tFar;
                    }
                    if (miss || tFar < 0.0)
                        continue;
                    const double t = std::max(tNear, 0.0);
                    if (t < bestT)
                    {
                        bestT = t;
                        best = &cell;
                    }
                }
                if (best)
                {
                    GalaxyPointerTarget cube;
                    cube.kind = GalaxyPointerTargetKind::NavigationCube;
                    cube.reason = GalaxyPointerHitReason::NeighbourCube;
                    cube.cell = *best;
                    cube.hitLy = origin + direction * bestT;
                    bool visible = false;
                    projectToScreen(glm::vec3(cube.hitLy) * scene.renderUnitsPerLy, scene.mvp, scene.viewport,
                                    visible, cube.depth);
                    if (collectCandidates)
                        resolution.candidates.push_back(cube);
                    if (winner.kind == GalaxyPointerTargetKind::Empty)
                        winner = cube;
                }
            }
        }
        return resolution;
    }

    GalaxyPointerGesture::Event GalaxyPointerGesture::update(
        bool leftDown,
        bool inside,
        double x,
        double y,
        double nowSeconds,
        double dragThresholdPx
    )
    {
        Event event = Event::None;
        const bool pressed = leftDown && !m_leftWasDown;
        const bool released = !leftDown && m_leftWasDown;

        if (pressed && inside)
        {
            m_phase = Phase::PendingClick;
            m_pressX = x;
            m_pressY = y;
            m_pressTime = nowSeconds;
            m_maxMovementPx = 0.0;
            event = Event::Pressed;
        }
        else if (pressed)
        {
            // A press outside the map is not a map gesture.
            m_phase = Phase::Cancelled;
        }
        else if (leftDown && m_phase == Phase::PendingClick)
        {
            const double movement = std::hypot(x - m_pressX, y - m_pressY);
            m_maxMovementPx = std::max(m_maxMovementPx, movement);
            if (movement > dragThresholdPx)
            {
                m_phase = Phase::Dragging;
                event = Event::DragStarted;
            }
        }
        else if (released)
        {
            event = m_phase == Phase::PendingClick && inside ? Event::Clicked : Event::Released;
            m_phase = Phase::Idle;
        }

        m_leftWasDown = leftDown;
        return event;
    }

    void GalaxyPointerGesture::cancelPendingClick()
    {
        if (m_phase == Phase::PendingClick)
            m_phase = Phase::Cancelled;
    }

    void GalaxyPointerGesture::suppress(bool leftDown)
    {
        m_phase = leftDown ? Phase::Cancelled : Phase::Idle;
        m_leftWasDown = leftDown;
    }

    void GalaxyPointerGesture::reset()
    {
        m_phase = Phase::Idle;
        // Conservative: until a release is seen, a held button is not a
        // fresh press (a click that opened / re-entered the map).
        m_leftWasDown = true;
        m_pressX = 0.0;
        m_pressY = 0.0;
        m_pressTime = 0.0;
        m_maxMovementPx = 0.0;
    }
}
