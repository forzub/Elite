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
//
//   EliteGalaxyPointerCheck -> PASS/FAIL lines, exit 0/1

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

// --- production handleInput (G2, G3, G4, G8) --------------------------------
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

    Map()
    {
        world::celestial::GalaxyMapSystem sol;
        sol.id = 0;
        sol.name = "Sol";
        sol.starType = "G2V";
        galaxy.systems.push_back(sol);
        player.currentSystemId = 0;
        view.onEntered(galaxy, player);
        // The entry cube is centred on Sol; keep Sol 0.3 cell off its marker
        // (a star exactly on a marker wins - checked separately in G1).
        const auto anchor = view.state().navigationGrid.anchorCell();
        galaxy.systems[0].positionLy = anchor.center + glm::dvec3(0.0, -0.3 * anchor.size, 0.0);
    }

    glm::mat4 mvp() const { return view.projectionMatrix(kViewport) * view.viewMatrix(); }

    // What GalaxyMapRenderer publishes for the frame (see header comment).
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
        if (!view.navigationCellsInteractive(kViewport))
            return;
        std::vector<game::navigation::CubicNavigationCell> cells {state.navigationGrid.anchorCell()};
        if (state.navigationGrid.hasSelectedCell() &&
            state.navigationGrid.selectedCell().level == cells[0].level &&
            !(state.navigationGrid.selectedCell().index == cells[0].index))
            cells.push_back(state.navigationGrid.selectedCell());
        for (const auto& cell : cells)
        {
            GalaxyNavigationMarker m;
            m.cell = cell;
            const bool selected = state.navigationGrid.hasSelectedCell() &&
                                  state.navigationGrid.selectedCell().index == cell.index;
            m.radiusPx = selected ? 5.0f : 4.0f;
            m.screen = project(mvp(), view.positionLyToRender(cell.center), &m.depth);
            m.visible = true;
            state.presentedCubeMarkers.push_back(m);
        }
    }

    GalaxyMapInputResult frame(double x, double y, bool left, bool right = false)
    {
        present();
        GalaxyMapInputFrame f;
        f.viewport = kViewport;
        f.mouseX = f.localMouseX = x;
        f.mouseY = f.localMouseY = y;
        f.inside = true;
        f.leftDown = left;
        f.rightDown = right;
        f.nowSeconds = now;
        f.overlayGlyphs = glyphs;
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

    bool sameCamera(const game::system_map::GalaxyCameraState& a) const
    {
        const auto& b = view.state().camera;
        return a.yaw == b.yaw && a.pitch == b.pitch && a.distance == b.distance && a.target == b.target;
    }
};

void inputChecks()
{
    // G2: jitter on the anchor cube marker and on a star.
    {
        Map map;
        map.present();
        check(!map.view.state().presentedCubeMarkers.empty(), "setup: entry cube marker presented");
        const glm::vec2 cube = map.view.state().presentedCubeMarkers[0].screen;
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

        // A star on screen (Sol is at the entry cube's centre region).
        Map stars;
        world::celestial::GalaxyMapSystem other;
        other.id = 5;
        other.name = "Near";
        other.starType = "K1V";
        other.positionLy = stars.view.state().navigationGrid.anchorCell().center +
                           glm::dvec3(stars.view.state().navigationGrid.anchorCell().size * 0.3, 0.0, 0.0);
        stars.galaxy.systems.push_back(other);
        stars.present();
        const glm::vec2 starPx = stars.view.state().presentedStars[1].screen;
        const auto starBefore = stars.view.state().camera;
        stars.click(starPx, {3.0f, 2.0f});
        check(stars.view.state().selectedSystemId == 5, "G2 star click with 3 px jitter selects the star");
        (void)starBefore;
    }
    // G3: drag beyond the threshold rotates, never selects.
    {
        Map map;
        map.present();
        const glm::vec2 cube = map.view.state().presentedCubeMarkers[0].screen;
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
    // G4: cube double click, second click on the waypoint glyph of that cube.
    {
        int refinedMarker = 0, refinedGlyph = 0, glyphOutsideSequence = 0;
        for (int repeat = 0; repeat < 10; ++repeat)
        {
            for (int variant = 0; variant < 2; ++variant)
            {
                Map map;
                map.present();
                const auto marker = map.view.state().presentedCubeMarkers[0];
                const int levelBefore = map.view.state().navigationGrid.level();
                const auto first = map.click(marker.screen);
                if (!first.clickedNavigationCell)
                    continue;
                // SystemMapRenderer now shows the waypoint candidate glyph of
                // the selected cube at marker + (20, -2), hit radius 12.
                map.glyphs.push_back({"waypoint_candidate:G", marker.screen + glm::vec2(20.0f, -2.0f), 12.0f,
                                      map.view.state().navigationGrid.selectedCell()});
                map.now += 0.10;
                // variant 0: the marker again; 1: 10 px right (glyph area only).
                const glm::vec2 second = marker.screen + glm::vec2(variant == 0 ? 1.0f : 10.0f, 0.0f);
                map.click(second);
                const bool refined = map.view.state().navigationGrid.level() > levelBefore;
                (variant == 0 ? refinedMarker : refinedGlyph) += refined ? 1 : 0;
            }
            // A deliberate glyph click outside a cube sequence belongs to the
            // overlay (its card), not to the cube.
            Map map;
            map.present();
            const auto marker = map.view.state().presentedCubeMarkers[0];
            map.click(marker.screen);
            map.glyphs.push_back({"waypoint_candidate:G", marker.screen + glm::vec2(20.0f, -2.0f), 12.0f,
                                  map.view.state().navigationGrid.selectedCell()});
            map.now += 2.0;
            GalaxyMapInputFrame f;
            f.viewport = kViewport;
            f.localMouseX = marker.screen.x + 20.0;
            f.localMouseY = marker.screen.y - 2.0;
            f.inside = true;
            f.leftDown = true;
            f.nowSeconds = map.now;
            f.overlayGlyphs = map.glyphs;
            const auto target = map.interaction.resolvePressTarget(map.view, map.galaxy, f);
            glyphOutsideSequence += target.kind == GalaxyPointerTargetKind::OverlayGlyph ? 1 : 0;
        }
        check(refinedMarker == 10, "G4 marker + marker double click refines (10/10), got " + std::to_string(refinedMarker));
        check(refinedGlyph == 10,
              "G4 marker + waypoint-glyph second click refines the same cube (10/10), got " +
                  std::to_string(refinedGlyph));
        check(glyphOutsideSequence == 10, "G4 glyph click outside a cube sequence stays the overlay's (10/10)");
    }
    // G8: re-entry resets transient pointer state.
    {
        Map map;
        auto atlas = std::make_shared<GalaxyMapMediumAtlas>();
        map.galaxy.objects.resize(1);
        map.galaxy.objects[0].name = "test cloud";
        map.galaxy.objects[0].objectType = "interstellar_medium_region";
        const auto anchor = map.view.state().navigationGrid.anchorCell();
        addBoxProxy(*atlas, anchor.center + glm::dvec3(0.0, anchor.size * 0.25, 0.0),
                    static_cast<float>(anchor.size * 0.2), 0);
        map.galaxy.mediumAtlas = atlas;
        const glm::vec2 cloud =
            project(map.mvp(), map.view.positionLyToRender(anchor.center + glm::dvec3(0.0, anchor.size * 0.25, 0.0)));
        map.click(cloud);
        check(map.view.state().selectedMediumObject == 0, "G8 setup: the cloud is selected by a click");
        map.frame(cloud.x + 30, cloud.y, false);
        map.view.onEntered(map.galaxy, map.player);
        check(map.view.state().selectedMediumObject == -1 && map.view.state().hoveredMediumObject == -1 &&
                  map.view.state().presentedStars.empty() && map.view.state().presentedCubeMarkers.empty(),
              "G8 re-entry drops cloud hover / selection and the presented geometry");
        const auto cameraBefore = map.view.state().camera;
        map.click(cloud); // within the double-click interval of the pre-entry click
        check(!map.view.state().cameraFlight.active() && map.sameCamera(cameraBefore),
              "G8 first click after re-entry is a single click (no phantom double click)");
        // A button held across entry is not a press.
        map.frame(cloud.x, cloud.y, true);
        map.view.onEntered(map.galaxy, map.player);
        const int selectedBefore = map.view.state().selectedMediumObject;
        map.frame(cloud.x, cloud.y, true);
        map.frame(cloud.x, cloud.y, false);
        check(map.view.state().selectedMediumObject == selectedBefore,
              "G8 a button held across entry does not click on release");
    }
}
} // namespace

int main()
{
    resolverChecks();
    inputChecks();
    std::printf("%s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
