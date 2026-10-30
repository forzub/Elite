// DEV check: the Galaxy Map pointer contract on the PRODUCTION resolver,
// gesture and GalaxyMapInteraction::handleInput (no renderer: the presented
// stars / cube markers are published here exactly as GalaxyMapRenderer
// publishes them - projection of the view's camera, glyph radius
// starBaseRadiusPx * scale, marker radius 4 px (5 px hovered / selected)).
//  G1 star vs cube marker overlap: a click inside the star glyph -> STAR; a
//     click on the visible cube marker outside the star glyph -> CUBE.
//  G2 jitter: press, move 1-4 px, release -> click; camera unchanged until
//     the release.
//  G3 drag: press, move beyond the threshold -> rotation, no selection.
//  G4 cube double click: first click selects the cube; the second click of
//     the sequence on the waypoint glyph of that cube still refines.
//  G5 star depth: two stars at one screen point -> the foreground star,
//     whatever the input order; deterministic.
//  G6 cloud behind a star: click on the star -> STAR.
//  G7 cloud behind a cube marker: click on the marker -> CUBE; a click on
//     the cloud away from star / cube -> CLOUD.
//  G8 re-entry: a selected cloud / pending sequence never survives
//     onEntered (no phantom selection, no phantom double click, a held
//     button is not a press).
//  H1 the map opens on the galaxy overview framing the star-system roots
//     (not the whole empty domain); the overview and nested parents fit the
//     viewport (all corners >= 12 px inside) from any orbit angle.
//  H2 WYSIWYG hover: the highlighted child cube is the press target.
//  H3 the wheel zooms inside the parent; the level / parent never change.
//  H4 double click enters a child (it becomes the parent); up / breadcrumb
//     return with the cube the user left selected.
//  H5 star double click descends into the star's cube (star kept); on the
//     terminal level it opens the System Map.
//  H6 a hover marker never takes an object's click (cloud wins, enters its
//     cube).
//  H7 inner coordinate faces are stable under small orbits.
//  H8 neighbours of the parent are targets: hover, double click = sideways
//     into the neighbour (level unchanged).
//
//   EliteGalaxyPointerCheck -> PASS/FAIL lines, exit 0/1

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include <glm/gtc/matrix_transform.hpp>

#include "src/game/system_map/GalaxyMapInteraction.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/system_map/GalaxyPointerResolver.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"

namespace
{
using namespace game::system_map;
using world::celestial::GalaxyMapMediumAtlas;
using world::celestial::GalaxyMapSnapshot;

int failures = 0;
void check(bool condition, const std::string& what)
{
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", what.c_str());
    if (!condition)
        ++failures;
}

const Viewport kViewport {1280, 720, 0, 0};

glm::vec2 project(const glm::mat4& mvp, const glm::vec3& p, float* depth = nullptr)
{
    const glm::vec4 clip = mvp * glm::vec4(p, 1.0f);
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (depth)
        *depth = ndc.z;
    return {(ndc.x * 0.5f + 0.5f) * kViewport.width, (1.0f - (ndc.y * 0.5f + 0.5f)) * kViewport.height};
}

// A closed box shell (12 triangles) as the coarse range of one proxy.
void addBoxProxy(GalaxyMapMediumAtlas& atlas, const glm::dvec3& centreLy, float halfLy, int objectIndex)
{
    GalaxyMapMediumAtlas::Proxy proxy;
    proxy.centreLy = centreLy;
    proxy.radiusLy = halfLy * std::sqrt(3.0f);
    proxy.objectIndex = objectIndex;
    proxy.coarse.first = static_cast<std::uint32_t>(atlas.vertices.size());
    const glm::vec3 c(centreLy);
    const auto corner = [&](int i) {
        return c + glm::vec3((i & 1) ? halfLy : -halfLy, (i & 2) ? halfLy : -halfLy, (i & 4) ? halfLy : -halfLy);
    };
    const int faces[6][4] = {{0, 1, 3, 2}, {4, 5, 7, 6}, {0, 1, 5, 4}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 3, 7, 5}};
    for (const auto& f : faces)
        for (const int i : {f[0], f[1], f[2], f[0], f[2], f[3]})
        {
            GalaxyMapMediumAtlas::Vertex v;
            v.positionLy = corner(i);
            atlas.vertices.push_back(v);
        }
    proxy.coarse.count = 36;
    atlas.proxies.push_back(proxy);
}

// --- pure resolver scenes (G1, G5, G6, G7) ---------------------------------
struct Scene
{
    std::vector<GalaxyPointerStar> stars;
    std::vector<GalaxyNavigationMarker> cubes;
    std::vector<GalaxyPointerOverlayGlyph> glyphs;
    GalaxyMapSnapshot galaxy;
    GalaxyPointerScene scene;
    Scene()
    {
        scene.stars = &stars;
        scene.cubeMarkers = &cubes;
        scene.overlayGlyphs = &glyphs;
        scene.galaxy = &galaxy;
        scene.viewport = kViewport;
        scene.renderUnitsPerLy = GalaxyMapView::RenderUnitsPerLightYear;
        const glm::mat4 projection = glm::perspective(glm::radians(48.0f), 1280.0f / 720.0f, 0.01f, 1000.0f);
        const glm::mat4 view = glm::lookAt(glm::vec3(0, 0, 40), glm::vec3(0), glm::vec3(0, 1, 0));
        scene.mvp = projection * view;
    }
    GalaxyPointerTarget at(float x, float y) const { return resolveGalaxyPointer(scene, {x, y}).winner; }
};

GalaxyPointerStar star(int id, glm::vec2 screen, float depth, float glyph = 2.6f)
{
    GalaxyPointerStar s;
    s.systemId = id;
    s.screen = screen;
    s.depth = depth;
    s.glyphRadiusPx = glyph;
    s.visible = true;
    return s;
}

GalaxyNavigationMarker marker(std::int64_t x, glm::vec2 screen, float radius = 4.0f)
{
    GalaxyNavigationMarker m;
    m.cell.level = 2;
    m.cell.index = {x, 0, 0};
    m.screen = screen;
    m.radiusPx = radius;
    m.depth = 0.5f;
    m.visible = true;
    return m;
}

void resolverChecks()
{
    using K = GalaxyPointerTargetKind;
    // G1: star 6 px left of a cube marker centre.
    {
        Scene s;
        s.stars.push_back(star(7, {500, 300}, 0.5f));
        s.cubes.push_back(marker(1, {506, 300}));
        const auto onStar = s.at(500, 300);
        check(onStar.kind == K::Star && onStar.systemId == 7, "G1 click inside the star glyph -> STAR");
        const auto onStarEdge = s.at(502, 301);
        check(onStarEdge.kind == K::Star, "G1 click on the star glyph edge (cube margin overlaps) -> STAR");
        const auto onCube = s.at(507, 300);
        check(onCube.kind == K::NavigationCube && onCube.cell && onCube.cell->index.x == 1,
              "G1 click on the cube marker outside the star glyph -> CUBE");
        const auto between = s.at(504, 300);
        check(between.kind == K::NavigationCube,
              "G1 click inside the cube marker, in the star's UX margin only -> CUBE");
        // The old 18 px invisible cube circle: 15 px from the cube centre,
        // 9 px from the star -> no cube (nothing drawn there), star proximity.
        Scene same;
        same.stars.push_back(star(8, {640, 360}, 0.5f));
        same.cubes.push_back(marker(2, {640, 360}));
        check(same.at(641, 360).kind == K::Star, "G1 a star drawn exactly on a cube marker wins (star glyph first)");
        const auto oldZone = s.at(491, 300);
        check(oldZone.kind == K::Star && oldZone.reason == GalaxyPointerHitReason::StarProximity,
              "G1 old invisible cube zone no longer steals the star");
    }
    // G5: two stars at one screen point.
    {
        Scene a;
        a.stars.push_back(star(11, {400, 400}, 0.70f));
        a.stars.push_back(star(12, {400, 400}, 0.40f));
        Scene b;
        b.stars.push_back(star(12, {400, 400}, 0.40f));
        b.stars.push_back(star(11, {400, 400}, 0.70f));
        const auto wa = a.at(400.5f, 400);
        const auto wb = b.at(400.5f, 400);
        check(wa.kind == K::Star && wa.systemId == 12 && wb.systemId == 12,
              "G5 glyph overlap: the foreground star wins in any input order");
        const auto pa = a.at(415, 400);
        const auto pb = b.at(415, 400);
        check(pa.systemId == 12 && pb.systemId == 12 && pa.reason == GalaxyPointerHitReason::StarProximity,
              "G5 proximity at equal distance: the foreground star wins");
        Scene c;
        c.stars.push_back(star(21, {400, 400}, 0.5f));
        c.stars.push_back(star(20, {400, 400}, 0.5f));
        check(c.at(400, 400).systemId == 20, "G5 identical depth and distance: deterministic lower id");
        Scene d;
        auto behind = star(30, {400, 400}, 0.2f);
        behind.visible = false; // behind the camera / clipped: never published visible
        d.stars.push_back(behind);
        check(d.at(400, 400).kind == K::Empty, "G5 a star that is not visible is never picked");
    }
    // G6 / G7: a cloud box around the origin, a star and a cube marker inside.
    {
        Scene s;
        s.galaxy.objects.resize(1);
        s.galaxy.objects[0].name = "test cloud";
        s.galaxy.objects[0].objectType = "interstellar_medium_region";
        auto atlas = std::make_shared<GalaxyMapMediumAtlas>();
        addBoxProxy(*atlas, glm::dvec3(0.0), 4.0f, 0);
        s.galaxy.mediumAtlas = atlas;
        float starDepth = 0.0f;
        const glm::vec2 starScreen = project(s.scene.mvp, glm::vec3(0.5f, 0.0f, 0.0f) * 2.2f, &starDepth);
        s.stars.push_back(star(40, starScreen, starDepth));
        float cubeDepth = 0.0f;
        const glm::vec2 cubeScreen = project(s.scene.mvp, glm::vec3(-1.5f, 1.0f, 0.0f) * 2.2f, &cubeDepth);
        s.cubes.push_back(marker(3, cubeScreen));
        const auto onStar = s.at(starScreen.x, starScreen.y);
        check(onStar.kind == K::Star && onStar.systemId == 40, "G6 click on a star in front of / inside a cloud -> STAR");
        const auto onCube = s.at(cubeScreen.x + 1.0f, cubeScreen.y);
        check(onCube.kind == K::NavigationCube, "G7 click on a cube marker over a cloud -> CUBE");
        const glm::vec2 cloudOnly = project(s.scene.mvp, glm::vec3(1.0f, -3.0f, 0.0f) * 2.2f);
        const auto onCloud = s.at(cloudOnly.x, cloudOnly.y);
        check(onCloud.kind == K::Medium && onCloud.mediumObject == 0,
              "G7 click on the cloud away from star / cube -> CLOUD");
        const auto outside = s.at(20, 20);
        check(outside.kind == K::Empty, "G7 click outside the cloud -> EMPTY");
        // Camera INSIDE a cloud (the Local Interstellar Cloud case): its far
        // wall must not swallow clicks on empty space.
        {
            Scene inside;
            inside.galaxy.objects.resize(1);
            inside.galaxy.objects[0].objectType = "interstellar_medium_region";
            auto big = std::make_shared<GalaxyMapMediumAtlas>();
            addBoxProxy(*big, glm::dvec3(0.0), 40.0f, 0);
            inside.galaxy.mediumAtlas = big;
            check(inside.at(100, 100).kind == K::Empty && inside.at(640, 360).kind == K::Empty,
                  "G7 camera inside a cloud: empty-space click -> EMPTY (no far-wall pick)");
        }
        s.glyphs.push_back({"glyph", cubeScreen + glm::vec2(20, -2), 12.0f, std::nullopt});
        s.scene.panelUnderCursor = true;
        check(s.at(starScreen.x, starScreen.y).kind == K::Panel, "UI panel under the cursor wins over everything");
    }
}

// --- production handleInput (G2, G3, G4, G8, H*) ----------------------------
struct Map
{
    GalaxyMapView view;
    GalaxyMapInteraction interaction;
    GalaxyMapSnapshot galaxy;
    world::celestial::PlayerNavigationState player;
    double now = 100.0;
    double scroll = 0.0;
    std::vector<GalaxyPointerOverlayGlyph> glyphs;
    GalaxyMapInputResult last;

    // enterRoot: work inside Sol's root (children: 1.6 ly G1 cubes) instead
    // of the galaxy overview the map opens with.
    explicit Map(bool enterRoot = true)
    {
        world::celestial::GalaxyMapSystem sol;
        sol.id = 0;
        sol.name = "Sol";
        sol.starType = "G2V";
        galaxy.systems.push_back(sol);
        player.currentSystemId = 0;
        view.state().lastViewport = kViewport;
        view.onEntered(galaxy, player);
        if (enterRoot)
        {
            const auto& grid = view.state().navigationGrid;
            view.enterHierarchyCell(grid.cell(grid.nearestIndexForPositionLy(glm::dvec3(0.0), 0), 0), kViewport,
                                    now);
            view.cancelCameraFlight(true);
            // Sol 0.3 cube off its cube centre (a star exactly on a marker
            // wins - checked separately in G1).
            const auto child = view.hierarchyChildAt(glm::dvec3(0.0));
            galaxy.systems[0].positionLy = child->center + glm::dvec3(0.0, -0.3 * child->size, 0.0);
        }
    }

    glm::mat4 mvp() const { return view.projectionMatrix(kViewport) * view.viewMatrix(); }

    // What GalaxyMapRenderer publishes for the frame (see header comment):
    // star glyphs; the selected child's marker only for a CUBE selection.
    void present()
    {
        auto& state = view.state();
        state.presentedStars.clear();
        for (const auto& system : galaxy.systems)
        {
            GalaxyPointerStar s;
            s.systemId = system.id;
            s.glyphRadiusPx = view.visuals().starBaseRadiusPx;
            const glm::vec4 clip = mvp() * glm::vec4(view.positionLyToRender(system.positionLy), 1.0f);
            s.visible = clip.w > 0.0f;
            s.screen = project(mvp(), view.positionLyToRender(system.positionLy), &s.depth);
            state.presentedStars.push_back(s);
        }
        state.presentedCubeMarkers.clear();
        const auto box = view.hierarchyBox();
        const bool objectSelection = state.selectedSystemId >= 0 || state.selectedMediumObject >= 0;
        if (state.navigationGrid.hasSelectedCell() && galaxyHierarchyIsChild(box, state.navigationGrid.selectedCell()) &&
            !objectSelection)
        {
            GalaxyNavigationMarker m;
            m.cell = state.navigationGrid.selectedCell();
            m.radiusPx = 5.0f;
            m.screen = project(mvp(), view.positionLyToRender(m.cell.center), &m.depth);
            m.visible = true;
            state.presentedCubeMarkers.push_back(m);
        }
    }

    GalaxyMapInputFrame makeFrame(double x, double y, bool left, bool right = false) const
    {
        GalaxyMapInputFrame f;
        f.viewport = kViewport;
        f.mouseX = f.localMouseX = x;
        f.mouseY = f.localMouseY = y;
        f.inside = true;
        f.leftDown = left;
        f.rightDown = right;
        f.nowSeconds = now;
        f.overlayGlyphs = glyphs;
        return f;
    }

    GalaxyMapInputResult frame(double x, double y, bool left, bool right = false)
    {
        present();
        const GalaxyMapInputFrame f = makeFrame(x, y, left, right);
        now += 1.0 / 60.0;
        last = interaction.handleInput(view, galaxy, f, scroll);
        return last;
    }

    // Press, optional moves, release; returns the release frame's result.
    GalaxyMapInputResult click(glm::vec2 p, glm::vec2 jitter = glm::vec2(0.0f))
    {
        frame(p.x, p.y, false);
        frame(p.x, p.y, true);
        if (jitter != glm::vec2(0.0f))
            frame(p.x + jitter.x, p.y + jitter.y, true);
        return frame(p.x + jitter.x, p.y + jitter.y, false);
    }

    GalaxyPointerTarget resolveAt(glm::vec2 p)
    {
        present();
        const GalaxyMapInputFrame f = makeFrame(p.x, p.y, false);
        return resolveGalaxyPointer(interaction.pointerScene(view, galaxy, f), p).winner;
    }

    // An empty-space child cube point (hierarchy layer), away from stars.
    bool layerPoint(glm::vec2& out, int salt = 0)
    {
        for (int k = 0; k < 200; ++k)
        {
            const float angle = 0.61f * static_cast<float>(k + salt * 7);
            const float radius = 40.0f + 1.5f * static_cast<float>(k);
            const glm::vec2 p(kViewport.width * 0.5f + radius * std::cos(angle),
                              kViewport.height * 0.5f + radius * std::sin(angle));
            const auto hit = resolveAt(p);
            if (hit.kind == GalaxyPointerTargetKind::NavigationCube &&
                hit.reason == GalaxyPointerHitReason::HierarchyLayer)
            {
                out = p;
                return true;
            }
        }
        return false;
    }

    void settle()
    {
        view.cancelCameraFlight(true);
    }

    bool sameCamera(const game::system_map::GalaxyCameraState& a) const
    {
        const auto& b = view.state().camera;
        return a.yaw == b.yaw && a.pitch == b.pitch && a.distance == b.distance && a.target == b.target;
    }
};

bool cornersInside(const Map& map, float marginPx, float* worst = nullptr)
{
    const auto box = map.view.hierarchyBox();
    float minMargin = 1.0e9f;
    for (int i = 0; i < 8; ++i)
    {
        const double h = box.edgeLy * 0.5;
        const glm::dvec3 c = box.centerLy + glm::dvec3((i & 1) ? h : -h, (i & 2) ? h : -h, (i & 4) ? h : -h);
        const glm::vec4 clip = map.mvp() * glm::vec4(map.view.positionLyToRender(c), 1.0f);
        if (clip.w <= 0.0f)
            return false;
        const glm::vec2 s = project(map.mvp(), map.view.positionLyToRender(c));
        minMargin = std::min({minMargin, s.x, s.y, kViewport.width - s.x, kViewport.height - s.y});
    }
    if (worst)
        *worst = minMargin;
    return minMargin >= marginPx;
}

void inputChecks()
{
    // G2: jitter on an empty-space cube and on a star.
    {
        Map map;
        glm::vec2 cube;
        check(map.layerPoint(cube), "setup: an empty-space child cube under the cursor");
        const auto before = map.view.state().camera;
        bool unchanged = true;
        map.frame(cube.x, cube.y, false);
        map.frame(cube.x, cube.y, true);
        for (int k = 1; k <= 4; ++k)
        {
            map.frame(cube.x + k, cube.y - 0.5f * k, true);
            unchanged = unchanged && map.sameCamera(before);
        }
        const auto release = map.frame(cube.x + 4, cube.y - 2, false);
        check(unchanged, "G2 press + 1-4 px jitter: camera unchanged until the release");
        check(release.clickedNavigationCell.has_value(), "G2 jitter release is a click on the press target (cube)");

        Map stars;
        world::celestial::GalaxyMapSystem other;
        other.id = 5;
        other.name = "Near";
        other.starType = "K1V";
        const auto child = stars.view.hierarchyChildAt(glm::dvec3(0.0));
        other.positionLy = child->center + glm::dvec3(child->size * 3.3, 0.0, 0.0);
        stars.galaxy.systems.push_back(other);
        stars.present();
        const glm::vec2 starPx = stars.view.state().presentedStars[1].screen;
        stars.click(starPx, {3.0f, 2.0f});
        check(stars.view.state().selectedSystemId == 5, "G2 star click with 3 px jitter selects the star");
    }
    // G3: drag beyond the threshold rotates, never selects.
    {
        Map map;
        glm::vec2 cube;
        map.layerPoint(cube);
        const int selectedBefore = map.view.state().selectedSystemId;
        const auto cellBefore = map.view.state().navigationGrid.selectedCell().index;
        const auto before = map.view.state().camera;
        bool anyClick = false;
        map.frame(cube.x, cube.y, false);
        map.frame(cube.x, cube.y, true);
        for (int k = 1; k <= 12; ++k)
            anyClick = map.frame(cube.x + 3 * k, cube.y, true).clickedNavigationCell.has_value() || anyClick;
        anyClick = map.frame(cube.x + 36, cube.y, false).clickedNavigationCell.has_value() || anyClick;
        check(!map.sameCamera(before), "G3 drag beyond the threshold rotates the camera");
        check(!anyClick && map.view.state().selectedSystemId == selectedBefore &&
                  map.view.state().navigationGrid.selectedCell().index == cellBefore,
              "G3 drag never produces a selection click");
    }
    // G4: cube double click (the cube becomes the current parent); second
    // click on the same point or on the waypoint glyph of the selected cube
    // (SystemMapRenderer: at the cube centre + (20, -2), hit radius 12).
    {
        int enteredSame = 0, enteredGlyph = 0, glyphOutsideSequence = 0;
        for (int repeat = 0; repeat < 10; ++repeat)
        {
            for (int variant = 0; variant < 2; ++variant)
            {
                // Glyph variant in the overview (root cubes ~75 px): the
                // first click near the cube centre, where the waypoint glyph
                // (centre + (20, -2), r 12) then appears under the cursor.
                Map map(variant == 0);
                glm::vec2 p;
                if (!map.layerPoint(p, repeat))
                    continue;
                if (variant == 1)
                {
                    const auto probe = map.resolveAt(p);
                    const glm::vec2 nearGlyph =
                        project(map.mvp(), map.view.positionLyToRender(probe.cell->center)) + glm::vec2(14.0f, -1.0f);
                    const auto there = map.resolveAt(nearGlyph);
                    if (there.cell && there.cell->index == probe.cell->index)
                        p = nearGlyph;
                }
                const auto first = map.click(p);
                if (!first.clickedNavigationCell)
                    continue;
                const auto cube = *first.clickedNavigationCell;
                const glm::vec2 glyph = project(map.mvp(), map.view.positionLyToRender(cube.center)) +
                                        glm::vec2(20.0f, -2.0f);
                map.glyphs.push_back({"waypoint_candidate:G", glyph, 12.0f,
                                      map.view.state().navigationGrid.selectedCell()});
                map.now += 0.10;
                map.click(variant == 0 ? p + glm::vec2(1.0f, 0.0f) : glyph + glm::vec2(1.0f, 0.0f));
                const auto& parent = map.view.state().hierarchyParent;
                const bool entered = parent.level == cube.level && parent.index == cube.index;
                (variant == 0 ? enteredSame : enteredGlyph) += entered ? 1 : 0;
            }
            Map map;
            glm::vec2 p;
            map.layerPoint(p, repeat);
            const auto selected = *map.click(p).clickedNavigationCell;
            const glm::vec2 glyph = project(map.mvp(), map.view.positionLyToRender(selected.center)) +
                                    glm::vec2(20.0f, -2.0f);
            map.glyphs.push_back({"waypoint_candidate:G", glyph, 12.0f, map.view.state().navigationGrid.selectedCell()});
            map.now += 2.0;
            GalaxyMapInputFrame f = map.makeFrame(glyph.x, glyph.y, true);
            map.present();
            const auto target = map.interaction.resolvePressTarget(map.view, map.galaxy, f);
            glyphOutsideSequence += target.kind == GalaxyPointerTargetKind::OverlayGlyph ? 1 : 0;
        }
        check(enteredSame == 10, "G4 cube double click: the cube becomes the parent (10/10), got " +
                                     std::to_string(enteredSame));
        check(enteredGlyph == 10,
              "G4 cube + waypoint-glyph second click enters the same cube (10/10), got " + std::to_string(enteredGlyph));
        check(glyphOutsideSequence == 10, "G4 glyph click outside a cube sequence stays the overlay's (10/10)");
    }
    // G8: re-entry resets transient pointer state.
    {
        Map map;
        auto atlas = std::make_shared<GalaxyMapMediumAtlas>();
        map.galaxy.objects.resize(1);
        map.galaxy.objects[0].name = "test cloud";
        map.galaxy.objects[0].objectType = "interstellar_medium_region";
        const auto box = map.view.hierarchyBox();
        const glm::dvec3 cloudCentre = box.centerLy + glm::dvec3(box.edgeLy * 0.2, box.edgeLy * 0.15, 0.0);
        addBoxProxy(*atlas, cloudCentre, static_cast<float>(box.edgeLy * 0.06), 0);
        map.galaxy.mediumAtlas = atlas;
        const glm::vec2 cloud = project(map.mvp(), map.view.positionLyToRender(cloudCentre));
        map.click(cloud);
        check(map.view.state().selectedMediumObject == 0, "G8 setup: the cloud is selected by a click");
        map.frame(cloud.x + 30, cloud.y, false);
        map.view.onEntered(map.galaxy, map.player);
        check(map.view.state().selectedMediumObject == -1 && map.view.state().hoveredMediumObject == -1 &&
                  map.view.state().presentedStars.empty() && map.view.state().presentedCubeMarkers.empty(),
              "G8 re-entry drops cloud hover / selection and the presented geometry");
        const auto cameraBefore = map.view.state().camera;
        const auto parentBefore = map.view.state().hierarchyParent;
        map.click(cloud); // within the double-click interval of the pre-entry click
        check(!map.view.state().cameraFlight.active() && map.sameCamera(cameraBefore) &&
                  map.view.state().hierarchyParent == parentBefore,
              "G8 first click after re-entry is a single click (no phantom double click)");
        map.frame(cloud.x, cloud.y, true);
        map.view.onEntered(map.galaxy, map.player);
        const int selectedBefore = map.view.state().selectedMediumObject;
        map.frame(cloud.x, cloud.y, true);
        map.frame(cloud.x, cloud.y, false);
        check(map.view.state().selectedMediumObject == selectedBefore,
              "G8 a button held across entry does not click on release");
    }
}

// --- spatial frame (current parent cube) ------------------------------------
void hierarchyChecks()
{
    using K = GalaxyPointerTargetKind;
    // H1: the map opens on the galaxy overview; it and every nested parent
    // fit the viewport (all 8 corners, margin) from any orbit angle.
    {
        Map map(false);
        check(map.view.state().hierarchyParent.overview(), "H1 the map opens on the galaxy overview");
        // The overview frames the roots holding star systems + the player's,
        // not the whole (much larger) empty navigation domain.
        const auto box = map.view.hierarchyBox();
        const auto& grid0 = map.view.state().navigationGrid;
        bool contentRoots = true;
        for (const auto& system : map.galaxy.systems)
            contentRoots = contentRoots && galaxyHierarchyIsChild(
                box, grid0.cell(grid0.nearestIndexForPositionLy(system.positionLy, 0), 0));
        const auto full = galaxyHierarchyBox(grid0, GalaxyHierarchyParent {});
        check(contentRoots && box.edgeLy < full.edgeLy,
              "H1 the overview frames the star-system roots (edge " + std::to_string(box.edgeLy) +
                  " ly, whole domain " + std::to_string(full.edgeLy) + " ly)");
        int fits = 0, total = 0;
        float worst = 1.0e9f;
        for (int level = -1; level <= 2; ++level)
        {
            if (level >= 0)
            {
                const auto& grid = map.view.state().navigationGrid;
                const auto cell = grid.cell(grid.nearestIndexForPositionLy(glm::dvec3(0.0), level), level);
                map.view.enterHierarchyCell(cell, kViewport, map.now);
                map.settle();
            }
            for (int a = 0; a < 12; ++a)
            {
                map.view.state().camera.yaw = -3.1f + 0.53f * static_cast<float>(a);
                map.view.state().camera.pitch = -1.4f + 0.25f * static_cast<float>(a);
                float m = 0.0f;
                fits += cornersInside(map, 12.0f, &m) ? 1 : 0;
                worst = std::min(worst, m);
                ++total;
            }
        }
        check(fits == total, "H1 overview + G0..G2 parents fit (all corners >= 12 px from the edge) at 12 angles: " +
                                 std::to_string(fits) + "/" + std::to_string(total) + ", worst " +
                                 std::to_string(static_cast<int>(worst)) + " px");
    }
    // H2: WYSIWYG hover - the highlighted child is exactly the press target.
    {
        Map map;
        int same = 0, total = 0;
        for (int k = 0; k < 30; ++k)
        {
            glm::vec2 p;
            if (!map.layerPoint(p, k))
                continue;
            map.frame(p.x, p.y, false);
            const auto& grid = map.view.state().navigationGrid;
            GalaxyMapInputFrame f = map.makeFrame(p.x, p.y, true);
            const auto press = map.interaction.resolvePressTarget(map.view, map.galaxy, f);
            ++total;
            same += grid.hasHoveredCell() && press.cell && grid.hoveredCell().index == press.cell->index &&
                            grid.hoveredCell().level == press.cell->level
                        ? 1
                        : 0;
        }
        check(total >= 20 && same == total,
              "H2 hovered child == press target (" + std::to_string(same) + "/" + std::to_string(total) + ")");
    }
    // H3: the wheel zooms only; the parent and level never change.
    {
        Map map;
        const auto parent = map.view.state().hierarchyParent;
        const int level = map.view.state().navigationGrid.level();
        const float distance = map.view.state().camera.distance;
        bool same = true;
        for (int k = 0; k < 60; ++k)
        {
            map.scroll = 1.0;
            map.frame(640, 360, false);
            same = same && map.view.state().hierarchyParent == parent && map.view.state().navigationGrid.level() == level;
        }
        const float zoomed = map.view.state().camera.distance;
        for (int k = 0; k < 120; ++k)
        {
            map.scroll = -1.0;
            map.frame(640, 360, false);
            same = same && map.view.state().hierarchyParent == parent;
        }
        float minD = 0.0f, maxD = 0.0f;
        map.view.cameraDistanceLimits(kViewport, minD, maxD);
        check(same && zoomed < distance && map.view.state().camera.distance <= maxD + 1.0e-3f,
              "H3 wheel: continuous zoom inside the parent, level / parent never change, range bounded");
    }
    // H4: enter / up / breadcrumb: the cube we left stays selected.
    {
        Map map;
        glm::vec2 p;
        map.layerPoint(p);
        const auto cube = *map.click(p).clickedNavigationCell;
        map.now += 0.1;
        map.click(p);
        map.settle();
        const bool entered = map.view.state().hierarchyParent.level == cube.level;
        glm::vec2 q;
        map.layerPoint(q, 3);
        const auto inner = *map.click(q).clickedNavigationCell;
        map.now += 0.1;
        map.click(q);
        map.settle();
        const bool enteredAgain = map.view.state().hierarchyParent.level == inner.level;
        map.view.ascendHierarchy(kViewport, map.now);
        map.settle();
        const auto& grid = map.view.state().navigationGrid;
        const bool upOk = map.view.state().hierarchyParent.level == cube.level && grid.hasSelectedCell() &&
                          grid.selectedCell().index == inner.index &&
                          galaxyHierarchyIsChild(map.view.hierarchyBox(), grid.selectedCell());
        map.view.jumpToHierarchyLevel(-1, kViewport, map.now);
        map.settle();
        const bool crumbOk = map.view.state().hierarchyParent.overview() && grid.selectedCell().level == 0 &&
                             grid.selectedCell().index ==
                                 grid.ancestorIndex(cube.index, cube.level, 0);
        check(entered && enteredAgain, "H4 double click on a child: it becomes the parent (two levels)");
        check(upOk, "H4 up: the parent of the parent, the cube we left selected inside it");
        check(crumbOk, "H4 breadcrumb GALAXY: the overview, the root we came from selected");
    }
    // H5: star double click: one level down into the star's cube; on the
    // terminal Galaxy level it opens the System Map (MapIntent).
    {
        Map map;
        int descents = 0;
        bool intent = false;
        for (int k = 0; k < 4; ++k)
        {
            map.present();
            const glm::vec2 star = map.view.state().presentedStars[0].screen;
            const int levelBefore = map.view.state().hierarchyParent.level;
            map.click(star);
            map.now += 0.1;
            const auto r = map.click(star);
            map.settle();
            if (map.view.state().hierarchyParent.level == levelBefore + 1 && map.view.state().selectedSystemId == 0)
                ++descents;
            if (r.mapIntent && r.mapIntent->entersKnownSystem() && r.mapIntent->systemId == 0)
                intent = true;
            map.now += 1.0;
        }
        check(descents == 2, "H5 star double click descends into the star's cube, star kept (G0 -> G1 -> G2), got " +
                                 std::to_string(descents));
        check(intent, "H5 star double click on the terminal level opens the System Map");
    }
    // H6: a hover marker never takes an object's click: a cloud whose
    // child-cube centre is right under the cursor stays the target.
    {
        Map map;
        auto atlas = std::make_shared<GalaxyMapMediumAtlas>();
        map.galaxy.objects.resize(1);
        map.galaxy.objects[0].objectType = "interstellar_medium_region";
        const auto box = map.view.hierarchyBox();
        const auto child = map.view.hierarchyChildAt(box.centerLy + glm::dvec3(box.edgeLy * 0.2, 0.0, 0.0));
        addBoxProxy(*atlas, child->center, static_cast<float>(box.childEdgeLy * 0.45), 0);
        map.galaxy.mediumAtlas = atlas;
        const glm::vec2 at = project(map.mvp(), map.view.positionLyToRender(child->center));
        map.frame(at.x, at.y, false); // hover: the cloud's cube is highlighted
        map.click(at);
        const bool selected = map.view.state().selectedMediumObject == 0;
        map.now += 0.1;
        map.click(at);
        map.settle();
        const auto& parent = map.view.state().hierarchyParent;
        check(selected && map.view.state().selectedMediumObject == 0 && parent.level == child->level &&
                  parent.index == child->index,
              "H6 cloud click / double click with its cube's marker under the cursor: the cloud wins, enters its cube");
        (void)K::Empty;
    }
    // H8: neighbours of the parent (same level, outside it) are pointer
    // targets: hover highlights the neighbour, double click moves the frame
    // sideways into it (level unchanged), up still returns.
    {
        Map map;
        const auto parentBefore = map.view.state().hierarchyParent;
        // A screen point outside the parent's projection: scan the edges.
        glm::vec2 p(-1.0f);
        GalaxyPointerTarget hit;
        for (int k = 0; k < 400 && p.x < 0.0f; ++k)
        {
            const float t = static_cast<float>(k) / 400.0f;
            const glm::vec2 candidates[4] = {{6.0f + t * 1268.0f, 6.0f}, {6.0f + t * 1268.0f, 714.0f},
                                             {6.0f, 6.0f + t * 708.0f}, {1274.0f, 6.0f + t * 708.0f}};
            for (const auto& c : candidates)
            {
                const auto r = map.resolveAt(c);
                if (r.reason == GalaxyPointerHitReason::NeighbourCube && r.cell)
                {
                    p = c;
                    hit = r;
                    break;
                }
            }
        }
        bool hovered = false, sideways = false, upOk = false;
        if (p.x >= 0.0f)
        {
            map.frame(p.x, p.y, false);
            const auto& grid = map.view.state().navigationGrid;
            hovered = grid.hasHoveredCell() && grid.hoveredCell().index == hit.cell->index &&
                      grid.hoveredCell().level == parentBefore.level;
            map.click(p);
            map.now += 0.1;
            map.click(p);
            map.settle();
            const auto& parent = map.view.state().hierarchyParent;
            sideways = parent.level == parentBefore.level && parent.index == hit.cell->index;
            map.view.ascendHierarchy(kViewport, map.now);
            map.settle();
            upOk = map.view.state().hierarchyParent.level == parentBefore.level - 1;
        }
        check(p.x >= 0.0f && hovered, "H8 cursor outside the parent: a neighbour cube is hovered");
        check(sideways, "H8 double click on a neighbour: it becomes the parent, level unchanged");
        check(upOk, "H8 up from the neighbour returns to the common parent level");
    }
    // H7: inner faces are stable under small orbits (hysteresis), and the
    // layer axis does not flip on a small rotation.
    {
        GalaxyHierarchyBox box;
        box.centerLy = glm::dvec3(0.0);
        box.edgeLy = 10.0;
        GalaxyHierarchyFaces faces;
        const glm::dvec3 start = glm::normalize(glm::dvec3(-1.0, -0.05, -1.03));
        faces = galaxyHierarchyChooseFaces(faces, -start * 30.0, start, box);
        const auto first = faces;
        bool stable = true;
        for (int k = 0; k < 40; ++k)
        {
            const double jiggle = 0.04 * std::sin(0.7 * k);
            const glm::dvec3 dir = glm::normalize(glm::dvec3(-1.0 - jiggle, -0.01 * std::cos(k), -1.03 + jiggle));
            faces = galaxyHierarchyChooseFaces(faces, -dir * 30.0, dir, box);
            stable = stable && faces.wallAxis == first.wallAxis && faces.wallSign == first.wallSign &&
                     faces.horizontalSign == first.horizontalSign;
        }
        check(stable, "H7 inner coordinate faces do not flip under small orbits (hysteresis)");
    }
}
} // namespace

int main()
{
    resolverChecks();
    inputChecks();
    hierarchyChecks();
    std::printf("%s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
