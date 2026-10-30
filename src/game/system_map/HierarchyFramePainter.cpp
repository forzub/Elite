#include "src/game/system_map/HierarchyFramePainter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/GalaxyMapRenderContext.h"

namespace game::system_map
{
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
        if (clip.w <= 0.00001f)
            return {0.0f, 0.0f};
        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        visible = ndc.x >= -1.0f && ndc.x <= 1.0f && ndc.y >= -1.0f && ndc.y <= 1.0f && ndc.z >= -1.0f &&
                  ndc.z <= 1.0f;
        depth = ndc.z;
        return {(ndc.x * 0.5f + 0.5f) * static_cast<float>(viewport.width),
                (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(viewport.height)};
    }

    glm::dvec3 axisUnit(int axis)
    {
        return glm::dvec3(axis == 0 ? 1.0 : 0.0, axis == 1 ? 1.0 : 0.0, axis == 2 ? 1.0 : 0.0);
    }

    bool sameCell(const navigation::CubicNavigationCell& a, const navigation::CubicNavigationCell& b)
    {
        return a.level == b.level && a.index == b.index;
    }
}

    void paintHierarchyFrame(
        GalaxyMapRenderContext& context,
        const HierarchyFramePaintInput& in,
        std::vector<GalaxyNavigationMarker>& presentedMarkers,
        std::vector<GalaxyFrameText>& frameTexts
    )
    {
        const auto& visuals = *in.visuals;
        const GalaxyHierarchyBox& box = in.box;
        *in.faces = galaxyHierarchyChooseFaces(*in.faces, in.eyeUnits, in.viewInto, box);
        const GalaxyHierarchyFaces faces = *in.faces;
        const auto& toRender = in.toRender;
        const Viewport& vp = in.viewport;
        const float frameFade = in.frameFade;

        const auto addBoxEdges = [&](const glm::dvec3& center, double edge, const glm::vec4& color) {
            const double h = edge * 0.5;
            glm::vec3 c[8];
            for (int i = 0; i < 8; ++i)
                c[i] = toRender(center + glm::dvec3((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h));
            static constexpr int edges[12][2] = {
                {0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            for (const auto& e : edges)
                context.addLine(c[e[0]], c[e[1]], color);
        };
        const auto addChildCube = [&](const navigation::CubicNavigationCell& cell, const glm::vec4& color) {
            addBoxEdges(cell.center, cell.size, color);
        };

        // The parent the camera is leaving (nested flight).
        if (in.flightContextBox)
        {
            glm::vec4 color = visuals.parentFrameColor;
            color.a *= 0.55f * (1.0f - frameFade);
            if (color.a > 0.002f)
                addBoxEdges(in.flightContextBox->centerLy, in.flightContextBox->edgeLy, color);
        }

        // The current parent.
        addBoxEdges(box.centerLy, box.edgeLy, visuals.parentFrameColor);

        // Inner coordinate faces: floor (ceiling) and the far wall.
        const double half = box.edgeLy * 0.5;
        const int divisions = std::max(1, box.divisions);
        const bool everyLineMajor = divisions <= 12;
        const int otherHorizontal = faces.wallAxis == 0 ? 2 : 0;

        const auto addFaceGrid = [&](int normalAxis, int sign, int axisU, int axisV) {
            const glm::dvec3 faceCentre = box.centerLy + axisUnit(normalAxis) * (half * sign);
            const glm::dvec3 u = axisUnit(axisU);
            const glm::dvec3 v = axisUnit(axisV);
            for (int k = 0; k <= divisions; ++k)
            {
                const double t = -half + box.childEdgeLy * static_cast<double>(k);
                const bool major = everyLineMajor || k % 5 == 0;
                glm::vec4 color = major ? visuals.faceMajorColor : visuals.faceMinorColor;
                color.a *= frameFade;
                if (color.a <= 0.002f)
                    continue;
                context.addLine(toRender(faceCentre + u * t - v * half), toRender(faceCentre + u * t + v * half), color);
                context.addLine(toRender(faceCentre + v * t - u * half), toRender(faceCentre + v * t + u * half), color);
            }
        };
        addFaceGrid(1, faces.horizontalSign, 0, 2);
        addFaceGrid(faces.wallAxis, faces.wallSign, otherHorizontal, 1);

        const double floorY = box.centerLy.y + half * faces.horizontalSign;
        const double wallCoord = box.centerLy[faces.wallAxis] + half * faces.wallSign;
        // The vertical ruler stands in the far corner of floor and wall.
        const int sideSign = (in.eyeUnits[otherHorizontal] - box.centerLy[otherHorizontal]) >= 0.0 ? -1 : 1;
        const double sideCoord = box.centerLy[otherHorizontal] + half * sideSign;

        // Labels at major children: offsets from the centre child (root
        // indices in the Galaxy overview).
        const auto childLabel = [&](int axis, int offset) -> std::string {
            if (box.parent.overview())
            {
                const std::int64_t first =
                    axis == 0 ? box.firstChild.x : (axis == 1 ? box.firstChild.y : box.firstChild.z);
                return std::to_string(first + offset);
            }
            return std::to_string(offset - (divisions - 1) / 2);
        };
        const auto labelledOffset = [&](int offset) {
            return everyLineMajor || (offset - (divisions - 1) / 2) % 5 == 0;
        };
        const glm::vec4 labelColor(visuals.frameLabelColor.r, visuals.frameLabelColor.g, visuals.frameLabelColor.b,
                                   visuals.frameLabelColor.a * frameFade);
        const auto addAxisLabels = [&](int axis, const glm::dvec3& lineOrigin, const char* axisName) {
            for (int offset = 0; offset < divisions; ++offset)
            {
                if (!labelledOffset(offset))
                    continue;
                glm::dvec3 p = lineOrigin;
                p[axis] = box.centerLy[axis] - half + box.childEdgeLy * (static_cast<double>(offset) + 0.5);
                frameTexts.push_back({toRender(p), childLabel(axis, offset), labelColor, 10, false});
            }
            glm::dvec3 end = lineOrigin;
            end[axis] = box.centerLy[axis] + half + box.childEdgeLy * 0.6;
            frameTexts.push_back({toRender(end), axisName, labelColor, 11, true});
        };
        if (frameFade > 0.05f)
        {
            glm::dvec3 junction = box.centerLy;
            junction.y = floorY;
            junction[faces.wallAxis] = wallCoord;
            addAxisLabels(otherHorizontal, junction, otherHorizontal == 0 ? "X" : "Z");

            glm::dvec3 side = box.centerLy;
            side.y = floorY;
            side[otherHorizontal] = sideCoord;
            addAxisLabels(faces.wallAxis, side, faces.wallAxis == 0 ? "X" : "Z");

            glm::dvec3 vertical = box.centerLy;
            vertical[faces.wallAxis] = wallCoord;
            vertical[otherHorizontal] = sideCoord;
            addAxisLabels(1, vertical, "Y");
        }

        const float pixelsPerDepth =
            2.0f * in.tanHalfFovY / static_cast<float>(std::max(vp.height, 1));

        // Projection of a point onto floor and wall: guides, rulers, the
        // child's footprint, a cross at the exact feet of an object.
        const auto addProjection = [&](const glm::dvec3& point,
                                       const std::optional<navigation::CubicNavigationCell>& child, float alpha,
                                       bool exactPoint) {
            if (alpha <= 0.002f || !galaxyHierarchyContains(box, point))
                return;
            glm::vec4 guide = visuals.projectionColor;
            guide.a *= alpha;
            glm::vec4 foot = visuals.projectionFootColor;
            foot.a *= alpha;

            glm::dvec3 onFloor = point;
            onFloor.y = floorY;
            glm::dvec3 onWall = point;
            onWall[faces.wallAxis] = wallCoord;
            context.addLine(toRender(point), toRender(onFloor), guide);
            context.addLine(toRender(point), toRender(onWall), guide);

            glm::vec4 ruler = guide;
            ruler.a *= 0.55f;
            glm::dvec3 floorToJunction = onFloor;
            floorToJunction[faces.wallAxis] = wallCoord;
            glm::dvec3 floorToSide = onFloor;
            floorToSide[otherHorizontal] = sideCoord;
            glm::dvec3 wallToSide = onWall;
            wallToSide[otherHorizontal] = sideCoord;
            context.addLine(toRender(onFloor), toRender(floorToJunction), ruler);
            context.addLine(toRender(onFloor), toRender(floorToSide), ruler);
            context.addLine(toRender(onWall), toRender(wallToSide), ruler);

            if (child)
            {
                const double h = child->size * 0.5;
                const auto square = [&](const glm::dvec3& c, int axisU, int axisV) {
                    const glm::dvec3 u = axisUnit(axisU) * h;
                    const glm::dvec3 v = axisUnit(axisV) * h;
                    context.addLine(toRender(c - u - v), toRender(c + u - v), foot);
                    context.addLine(toRender(c + u - v), toRender(c + u + v), foot);
                    context.addLine(toRender(c + u + v), toRender(c - u + v), foot);
                    context.addLine(toRender(c - u + v), toRender(c - u - v), foot);
                };
                glm::dvec3 floorCell = child->center;
                floorCell.y = floorY;
                glm::dvec3 wallCell = child->center;
                wallCell[faces.wallAxis] = wallCoord;
                square(floorCell, 0, 2);
                square(wallCell, otherHorizontal, 1);
            }

            if (exactPoint)
            {
                for (const glm::dvec3& f : {onFloor, onWall})
                {
                    const glm::vec3 c = toRender(f);
                    const float depth = std::max(1.0e-9f, glm::dot(c - in.cameraEyeRender, -in.cameraDirection));
                    const float s = 4.0f * depth * pixelsPerDepth;
                    context.addLine(c - in.cameraRight * s, c + in.cameraRight * s, foot);
                    context.addLine(c - in.cameraUp * s, c + in.cameraUp * s, foot);
                }
            }
        };

        const auto coordinateText = [&](const navigation::CubicNavigationCell& child) -> std::string {
            if (box.parent.overview())
                return std::string(1, in.levelPrefix) + "0 [" + std::to_string(child.index.x) + ", " +
                       std::to_string(child.index.y) + ", " + std::to_string(child.index.z) + "]";
            const glm::ivec3 o = galaxyHierarchyChildOffset(box, child.index);
            const int c = (divisions - 1) / 2;
            return "x " + std::to_string(o.x - c) + "  y " + std::to_string(o.y - c) + "  z " + std::to_string(o.z - c);
        };

        // Marker diamond; only a CUBE selection's marker is a pointer target.
        const auto publishMarker = [&](const navigation::CubicNavigationCell& cell, float radiusPx,
                                       const glm::vec4& color, bool pointerTarget) {
            const glm::vec3 center = toRender(cell.center);
            const float depth = std::max(1.0e-9f, glm::dot(center - in.cameraEyeRender, -in.cameraDirection));
            const float markerSize = depth * pixelsPerDepth * radiusPx;
            if (pointerTarget)
            {
                GalaxyNavigationMarker presented;
                presented.cell = cell;
                presented.radiusPx = radiusPx;
                presented.screen = projectToScreen(center, in.mvp, vp, presented.visible, presented.depth);
                presentedMarkers.push_back(presented);
            }
            const glm::vec3 top = center + in.cameraUp * markerSize;
            const glm::vec3 right = center + in.cameraRight * markerSize;
            const glm::vec3 bottom = center - in.cameraUp * markerSize;
            const glm::vec3 left = center - in.cameraRight * markerSize;
            context.addLine(top, right, color);
            context.addLine(right, bottom, color);
            context.addLine(bottom, left, color);
            context.addLine(left, top, color);
        };

        const auto isChild = [&](const navigation::CubicNavigationCell& cell) {
            return galaxyHierarchyIsChild(box, cell);
        };
        const std::optional<navigation::CubicNavigationCell> selectedChild =
            in.selectedCell && isChild(*in.selectedCell) ? in.selectedCell : std::nullopt;

        if (in.outgoingCell && isChild(*in.outgoingCell) && !(selectedChild && sameCell(*selectedChild, *in.outgoingCell)))
        {
            glm::vec4 edge = visuals.hoveredEdgeColor;
            edge.a *= in.outgoingAlpha;
            addChildCube(*in.outgoingCell, edge);
        }

        if (in.hoverCell && isChild(*in.hoverCell) && in.hoverAlpha > 0.001f)
        {
            const auto& hovered = *in.hoverCell;
            const float a = in.hoverAlpha;
            const bool alsoSelected = selectedChild && sameCell(*selectedChild, hovered);
            glm::vec4 edge = visuals.hoveredEdgeColor;
            edge.a *= a;
            glm::vec4 marker = visuals.hoveredMarkerColor;
            marker.a *= a;
            if (!alsoSelected)
            {
                addChildCube(hovered, edge);
                publishMarker(hovered, 4.0f, marker, false);
            }

            // Hover layer: the cross-section of the parent the cursor slides on.
            if (in.hoverOnLayer)
            {
                const int axis = in.layerAxis >= 0 && in.layerAxis <= 2 ? in.layerAxis : 1;
                const int u = axis == 0 ? 1 : 0;
                const int v = axis == 2 ? 1 : 2;
                glm::dvec3 c = box.centerLy;
                c[axis] = hovered.center[axis];
                glm::vec4 layer = visuals.hoverLayerColor;
                layer.a *= a;
                const glm::dvec3 du = axisUnit(u) * half;
                const glm::dvec3 dv = axisUnit(v) * half;
                context.addLine(toRender(c - du - dv), toRender(c + du - dv), layer);
                context.addLine(toRender(c + du - dv), toRender(c + du + dv), layer);
                context.addLine(toRender(c + du + dv), toRender(c - du + dv), layer);
                context.addLine(toRender(c - du + dv), toRender(c - du - dv), layer);
            }

            addProjection(in.hoverObjectPoint ? *in.hoverObjectPoint : hovered.center, hovered,
                          visuals.hoverProjectionAlpha * a, in.hoverObjectPoint.has_value());

            bool visible = false;
            float depth = 1.0f;
            const glm::vec2 screen = projectToScreen(toRender(hovered.center), in.mvp, vp, visible, depth);
            if (visible && a > 0.3f)
            {
                glm::vec4 color = visuals.hoveredMarkerColor;
                color.a = 0.80f * a;
                frameTexts.push_back(
                    {glm::vec3(screen.x + 10.0f, screen.y + 16.0f, 0.0f), coordinateText(hovered), color, 11, false, true});
            }
        }

        if (selectedChild)
        {
            addChildCube(*selectedChild, visuals.selectedEdgeColor);
            publishMarker(*selectedChild, 5.0f, visuals.selectedMarkerColor, !in.objectSelection);

            const bool objectPoint =
                in.selectedObjectPoint && galaxyHierarchyContains(box, *in.selectedObjectPoint);
            addProjection(objectPoint ? *in.selectedObjectPoint : selectedChild->center, selectedChild,
                          visuals.selectedProjectionAlpha, objectPoint);

            bool visible = false;
            float depth = 1.0f;
            const glm::vec2 screen = projectToScreen(toRender(selectedChild->center), in.mvp, vp, visible, depth);
            const bool hoverShowsSame =
                in.hoverCell && sameCell(*in.hoverCell, *selectedChild) && in.hoverAlpha > 0.3f;
            if (visible && !hoverShowsSame)
            {
                glm::vec4 color = visuals.selectedMarkerColor;
                color.a = 0.85f;
                frameTexts.push_back({glm::vec3(screen.x + 10.0f, screen.y + 16.0f, 0.0f),
                                      coordinateText(*selectedChild), color, 11, false, true});
            }
        }
    }

    void paintHierarchyFrameTexts(
        GalaxyMapRenderContext& context,
        const Viewport& vp,
        const glm::mat4& mvp,
        const glm::mat4& view,
        const std::vector<GalaxyFrameText>& frameTexts
    )
    {
        const glm::mat4 ortho =
            glm::ortho(0.0f, static_cast<float>(vp.width), static_cast<float>(vp.height), 0.0f, -1.0f, 1.0f);

        // Orientation triad (secondary cue): X / Y / Z of the frame as seen now.
        const glm::vec2 origin(static_cast<float>(vp.width) - 58.0f, 92.0f);
        const float length = 30.0f;
        struct Axis { glm::vec3 dir; const char* name; glm::vec4 color; };
        const Axis axes[3] = {
            {{1.0f, 0.0f, 0.0f}, "X", {0.90f, 0.52f, 0.46f, 0.80f}},
            {{0.0f, 1.0f, 0.0f}, "Y", {0.56f, 0.86f, 0.56f, 0.80f}},
            {{0.0f, 0.0f, 1.0f}, "Z", {0.52f, 0.70f, 0.98f, 0.80f}}};
        const glm::mat3 view3(view);
        context.beginLines();
        glm::vec2 tips[3];
        for (int i = 0; i < 3; ++i)
        {
            const glm::vec3 v = view3 * axes[i].dir;
            tips[i] = origin + glm::vec2(v.x, -v.y) * length;
            context.addLine(glm::vec3(origin, 0.0f), glm::vec3(tips[i], 0.0f), axes[i].color);
        }
        context.flushLines(ortho);

        context.beginTextFrame(vp.width, vp.height);
        for (int i = 0; i < 3; ++i)
            context.drawTextPx(axes[i].name, tips[i].x + 2.0f, tips[i].y + 4.0f, 11, axes[i].color);
        for (const auto& text : frameTexts)
        {
            glm::vec2 screen(text.position.x, text.position.y);
            if (!text.screenSpace)
            {
                bool visible = false;
                float depth = 1.0f;
                screen = projectToScreen(text.position, mvp, vp, visible, depth);
                if (!visible)
                    continue;
                screen += glm::vec2(-3.0f, 4.0f);
            }
            context.drawTextPx(text.text, screen.x, screen.y, text.px, text.color);
        }
        context.endTextFrame();
    }

    void updateHierarchyHoverFade(
        const HierarchyHoverFade& fade,
        const std::optional<navigation::CubicNavigationCell>& logicalHover,
        double nowSeconds,
        float fadeInSeconds,
        float fadeOutSeconds
    )
    {
        double delta = 0.0;
        if (*fade.lastTimeSeconds > 0.0)
            delta = std::clamp(nowSeconds - *fade.lastTimeSeconds, 0.0, 0.10);
        *fade.lastTimeSeconds = nowSeconds;

        auto& visual = *fade.visualCell;
        auto& alpha = *fade.visualAlpha;
        auto& outgoing = *fade.outgoingCell;
        auto& outgoingAlpha = *fade.outgoingAlpha;
        fadeInSeconds = std::max(0.001f, fadeInSeconds);
        fadeOutSeconds = std::max(0.001f, fadeOutSeconds);

        if (logicalHover && (!visual || !sameCell(*logicalHover, *visual)))
        {
            if (visual && alpha > 0.001f)
            {
                outgoing = visual;
                outgoingAlpha = alpha;
            }
            visual = logicalHover;
            alpha = 0.0f;
        }
        if (logicalHover)
        {
            alpha = std::min(1.0f, alpha + static_cast<float>(delta) / fadeInSeconds);
        }
        else
        {
            alpha = std::max(0.0f, alpha - static_cast<float>(delta) / fadeOutSeconds);
            if (alpha <= 0.001f)
            {
                alpha = 0.0f;
                visual.reset();
            }
        }
        if (outgoing)
        {
            outgoingAlpha = std::max(0.0f, outgoingAlpha - static_cast<float>(delta) / fadeOutSeconds);
            if (outgoingAlpha <= 0.001f)
            {
                outgoingAlpha = 0.0f;
                outgoing.reset();
            }
        }
    }
}
