/*
    Details / Planet map implementation.

    В интерфейсе режим называется Details.
    В исходном коде ему соответствует Mode::Planet.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/


// ============================================================================
// Details camera, projection and scene
// ============================================================================




glm::dvec3 SystemMapRenderer::planetMapCameraSpaceRelative(
    const glm::dvec3& relativeMeters
) const
{
    const double cy =
        std::cos(activeDetailCamera().yaw);

    const double sy =
        std::sin(activeDetailCamera().yaw);

    const double cp =
        std::cos(activeDetailCamera().pitch);

    const double sp =
        std::sin(activeDetailCamera().pitch);

    // yaw around Y
    glm::dvec3 a;
    a.x = relativeMeters.x * cy - relativeMeters.z * sy;
    a.y = relativeMeters.y;
    a.z = relativeMeters.x * sy + relativeMeters.z * cy;

    // pitch around X
    glm::dvec3 b;
    b.x = a.x;
    b.y = a.y * cp - a.z * sp;
    b.z = a.y * sp + a.z * cp;

    return b;
}


glm::mat3 SystemMapRenderer::planetBodyToDetailCameraMatrix(
    const world::celestial::PlanetMapSnapshot& planet
) const
{
    const glm::dvec3 north =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 prime0 =
        planetPrimeAxisWorld(
            north
        );

    const glm::dvec3 east0 =
        planetEastAxisWorld(
            north,
            prime0
        );

    /*
        Полностью повторяем planetSurfacePointMeters():

            worldLon =
                textureLongitude
                + textureOffset
                + rotationPhase
    */
    const double spin =
        degToRadD(
            planet.planetTextureLongitudeOffsetDeg
        ) +
        planet.planetRotationPhaseRad;

    const double cosine =
        std::cos(
            spin
        );

    const double sine =
        std::sin(
            spin
        );

    /*
        После включения spin локальная координата сферы:

            X = cos(longitude)
            Y = latitude
            Z = sin(longitude)
    */
    const glm::dvec3 rotatedPrime =
        prime0 *
            cosine +
        east0 *
            sine;

    const glm::dvec3 rotatedEast =
        -prime0 *
            sine +
        east0 *
            cosine;

    const glm::dvec3 cameraPrime =
        planetMapCameraSpaceRelative(
            rotatedPrime
        );

    const glm::dvec3 cameraNorth =
        planetMapCameraSpaceRelative(
            north
        );

    const glm::dvec3 cameraEast =
        planetMapCameraSpaceRelative(
            rotatedEast
        );

    /*
        glm::mat3 принимает столбцы.

        Поэтому:

            matrix * vec3(x, y, z)

        возвращает:

            prime * x
            + north * y
            + east * z
    */
    return
        glm::mat3(
            glm::vec3(
                cameraPrime
            ),
            glm::vec3(
                cameraNorth
            ),
            glm::vec3(
                cameraEast
            )
        );
}


glm::dvec2 SystemMapRenderer::planetMapProject(
    const glm::dvec3& worldMeters,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
) const
{
    const glm::dvec3 relative =
        worldMeters -
        planet.planetCenterMeters;

    const glm::dvec3 cameraSpace =
        planetMapCameraSpaceRelative(
            relative
        );

    const double finalScale =
        scale *
        activeDetailCamera().zoom;

    return glm::dvec2(
        centerPx.x + activeDetailCamera().pan.x + cameraSpace.x * finalScale,
        centerPx.y + activeDetailCamera().pan.y - cameraSpace.y * finalScale
    );
}


void SystemMapRenderer::drawPlanetMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    glBegin(GL_LINES);
    glVertex2d(a.x, a.y);
    glVertex2d(b.x, b.y);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCross(
    const glm::dvec2& p,
    float size
)
{
    glBegin(GL_LINES);
    glVertex2d(p.x - size, p.y);
    glVertex2d(p.x + size, p.y);
    glVertex2d(p.x, p.y - size);
    glVertex2d(p.x, p.y + size);
    glEnd();
}


void SystemMapRenderer::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    if (segments < 8)
        segments = 8;

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        glVertex2d(
            center.x + std::cos(a) * radiusPx,
            center.y + std::sin(a) * radiusPx
        );
    }

    glEnd();
}


void SystemMapRenderer::drawPlanetMapAxes(
    const glm::dvec3& originMeters,
    const world::celestial::PlanetMapAxisSet& axes,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double axisLenMeters
)
{
    const glm::dvec2 o =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 x =
        planetMapProject(
            originMeters + axes.x * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 y =
        planetMapProject(
            originMeters + axes.y * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 z =
        planetMapProject(
            originMeters + axes.z * axisLenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.25f, 0.25f, 0.9f);
    drawPlanetMapLine(o, x);

    glColor4f(0.25f, 1.0f, 0.25f, 0.9f);
    drawPlanetMapLine(o, y);

    glColor4f(0.25f, 0.55f, 1.0f, 0.9f);
    drawPlanetMapLine(o, z);
}


void SystemMapRenderer::drawPlanetMapVelocityArrow(
    const glm::dvec3& originMeters,
    const glm::dvec3& velocityMps,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double lenMeters
)
{
    const double speed =
        glm::length(velocityMps);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocityMps / speed;

    const glm::dvec2 a =
        planetMapProject(
            originMeters,
            planet,
            scale,
            centerPx
        );

    const glm::dvec2 b =
        planetMapProject(
            originMeters + dir * lenMeters,
            planet,
            scale,
            centerPx
        );

    glColor4f(1.0f, 0.92f, 0.25f, 0.95f);
    drawPlanetMapLine(a, b);

    drawPlanetMapCross(
        b,
        4.0f
    );
}


void SystemMapRenderer::renderPlanetMap(
    const Viewport& viewport,
    const world::celestial::PlanetMapSnapshot& planet
)
{
    ensureGeneratedCelestialAssets();
    ensureEnvironmentProfiles();

    glViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glScissor(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(0.02f, 0.025f, 0.035f, 1.0f);

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!planet.valid)
        return;


    drawMapStarfield(
        viewport,
        planet.systemPositionLy
    );



    beginEnvironmentRenderSessionIfNeeded(
        Mode::Planet,
        planet.systemId,
        planet.planetBodyId
    );

    const glm::dvec2 centerPx(
        static_cast<double>(viewport.width) * 0.5,
        static_cast<double>(viewport.height) * 0.5
    );

    double maxRadiusMeters =
        planet.planetRadiusMeters;

    for (const auto& o : planet.hubOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    for (const auto& o : planet.playerOrbits)
    {
        if (o.valid)
            maxRadiusMeters = std::max(maxRadiusMeters, o.radiusMeters);
    }

    const double mapHalfPx =
        std::min(
            viewport.width,
            viewport.height
        ) * 0.42;

    const double scale =
        mapHalfPx / std::max(1.0, maxRadiusMeters);



    std::vector<
        world::celestial::SystemMapRing
    > normalizedRingBands;

    const auto ringContext =
        planetRingRenderContext(
            planet,
            scale,
            centerPx,
            normalizedRingBands
        );

    



        const auto environmentProfile =
    resolvedEnvironmentProfileForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId
    );

const bool hidePhysicalSurface =
    environmentProfile.found &&
    (
        environmentProfile.rendering.surfaceVisibility ==
            "hidden" ||
        !environmentProfile.rendering.loadSurfaceTextures
    );






m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Back
);



const bool shapeModelDrawn =
    !hidePhysicalSurface &&
    drawPlanetShapeModelDetail(
        planet,
        scale,
        centerPx
    );



   

// ------------------------------------------------------------
// 1. Базовая поверхность планеты.
// Если shape model не нарисован, рисуем fallback disk + textured globe.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
    /*
        Даже при скрытой поверхности оставляем тёмный
        непрозрачный fallback disk под primary cloud deck.

        Он нужен только как страховка от щелей на краю.
    */
    drawPlanetFilledDisk(
        planet,
        scale,
        centerPx
    );

    if (!hidePhysicalSurface)
    {
        drawPlanetTexturedGlobe(
            planet,
            scale,
            centerPx
        );
    }
}

// ------------------------------------------------------------
// 2. Environment layer.
// Для Planet Details НЕ используем Hub screen-space clouds.
// Здесь должна работать отдельная spherical Details-цепочка:
// drawPlanetEnvironmentLayers()
//     -> drawPlanetAtmosphereInterior()
//     -> drawPlanetAnimatedCloudLayers()
//         -> drawPlanetProceduralCloudGlobeLayer()
//     -> drawPlanetAtmosphereLimb()
// ------------------------------------------------------------
drawPlanetEnvironmentLayers(
    planet,
    scale,
    centerPx,
    !shapeModelDrawn
);



m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Front
);





// ------------------------------------------------------------
// 3. Сетка/ориентационный оверлей поверх поверхности,
// облаков и атмосферы.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
    drawPlanetSphereGrid(
        planet,
        scale,
        centerPx
    );
}






    // Орбиты хабов.
    glColor4f(0.45f, 0.78f, 1.0f, 0.75f);

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    // Орбита игрока.
    glColor4f(1.0f, 0.75f, 0.25f, 0.9f);

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        drawPlanetMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    const double axisLenMeters =
        std::max(
            50000.0,
            maxRadiusMeters * 0.035
        );

    const double velocityArrowLenMeters =
        std::max(
            70000.0,
            maxRadiusMeters * 0.05
        );

    // Хабы.
    for (const auto& hub : planet.hubs)
    {
        if (!hub.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                hub.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.3f, 0.9f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 7.0f);

        drawPlanetMapAxes(
            hub.positionMeters,
            hub.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 hubRelativeVelocityMps =
            hub.velocityMps -
            planet.planetVelocityMps;

        drawPlanetMapVelocityArrow(
            hub.positionMeters,
            hubRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    // Станции.
    for (const auto& station : planet.stations)
    {
        if (!station.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                station.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(0.8f, 0.95f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 6.0f);

        drawPlanetMapAxes(
            station.positionMeters,
            station.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 stationRelativeVelocityMps =
            station.velocityMps -
            planet.planetVelocityMps;

        drawPlanetMapVelocityArrow(
            station.positionMeters,
            stationRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters * 0.8
        );
    }

    // Корабли.
    for (const auto& ship : planet.ships)
    {
        if (!ship.valid)
            continue;

        const glm::dvec2 p =
            planetMapProject(
                ship.positionMeters,
                planet,
                scale,
                centerPx
            );

        glColor4f(1.0f, 0.85f, 0.25f, 1.0f);
        drawPlanetMapCross(p, 8.0f);

        drawPlanetMapAxes(
            ship.positionMeters,
            ship.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 shipRelativeVelocityMps =
            ship.velocityMps -
            planet.planetVelocityMps;

        drawPlanetMapVelocityArrow(
            ship.positionMeters,
            shipRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    glEnable(GL_DEPTH_TEST);
}



// ============================================================================
// Details geometry and textured planet rendering
// ============================================================================




void SystemMapRenderer::drawPlanetSphereGrid(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters;

    if (r <= 1.0)
        return;

    // Чем больше segments — тем чище линия горизонта.
    constexpr int segments = 96;

    // Шаг сетки. Если будет слишком густо — поставь 30.
    constexpr int gridStepDeg = 30;

    auto cameraSpaceForWorldPoint =
        [&](const glm::dvec3& worldPoint) -> glm::dvec3
        {
            const glm::dvec3 relative =
                worldPoint -
                planet.planetCenterMeters;

            return planetMapCameraSpaceRelative(
                relative
            );
        };

    auto emitVisibleSurfaceSegment =
        [&](const glm::dvec3& aWorld, const glm::dvec3& bWorld)
        {
            const glm::dvec3 aCam =
                cameraSpaceForWorldPoint(
                    aWorld
                );

            const glm::dvec3 bCam =
                cameraSpaceForWorldPoint(
                    bWorld
                );

            // В planet-map convention:
            // +Z — сторона, обращённая к камере.
            //
            // Если оба конца за планетой — сегмент не рисуем.
            if (aCam.z < 0.0 && bCam.z < 0.0)
                return;

            glm::dvec3 a =
                aWorld;

            glm::dvec3 b =
                bWorld;

            // Если сегмент пересекает горизонт, обрезаем его по плоскости z = 0.
            // Так сетка не вылезает с обратной стороны даже на краю диска.
            if ((aCam.z < 0.0 && bCam.z >= 0.0) ||
                (aCam.z >= 0.0 && bCam.z < 0.0))
            {
                const double denom =
                    aCam.z - bCam.z;

                if (std::abs(denom) < 1e-12)
                    return;

                const double t =
                    std::clamp(
                        aCam.z / denom,
                        0.0,
                        1.0
                    );

                const glm::dvec3 hit =
                    aWorld +
                    (bWorld - aWorld) * t;

                if (aCam.z < 0.0)
                    a = hit;
                else
                    b = hit;
            }

            const glm::dvec2 sa =
                planetMapProject(
                    a,
                    planet,
                    scale,
                    centerPx
                );

            const glm::dvec2 sb =
                planetMapProject(
                    b,
                    planet,
                    scale,
                    centerPx
                );

            glVertex2d(sa.x, sa.y);
            glVertex2d(sb.x, sb.y);
        };

    glColor4f(
        0.18f,
        0.42f,
        0.85f,
        0.72f
    );

    glBegin(GL_LINES);

    // =========================================================
    // Параллели: полные кольца вокруг планеты.
    // Задняя часть каждого кольца будет отрезана камерой.
    // =========================================================
    for (int latDeg = -75; latDeg <= 75; latDeg += gridStepDeg)
    {
        const double lat =
            glm::radians(
                static_cast<double>(latDeg)
            );

        for (int i = 0; i < segments; ++i)
        {
            const double lon0 =
                -glm::pi<double>() +
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double lon1 =
                -glm::pi<double>() +
                glm::two_pi<double>() *
                static_cast<double>(i + 1) /
                static_cast<double>(segments);

            const glm::dvec3 p0 =
                planetSurfacePointMeters(
                    planet,
                    lat,
                    lon0,
                    1.004
                );

            const glm::dvec3 p1 =
                planetSurfacePointMeters(
                    planet,
                    lat,
                    lon1,
                    1.004
                );

            emitVisibleSurfaceSegment(
                p0,
                p1
            );
        }
    }

    // =========================================================
    // Меридианы: теперь 0..360, а не 0..180.
    // Это возвращает полноценную сетку.
    // =========================================================
    for (int lonDeg = 0; lonDeg < 360; lonDeg += gridStepDeg)
    {
        const double lon =
            glm::radians(
                static_cast<double>(lonDeg)
            );

        for (int i = 0; i < segments; ++i)
        {
            const double lat0 =
                -glm::half_pi<double>() +
                glm::pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double lat1 =
                -glm::half_pi<double>() +
                glm::pi<double>() *
                static_cast<double>(i + 1) /
                static_cast<double>(segments);

            const glm::dvec3 p0 =
                planetSurfacePointMeters(
                    planet,
                    lat0,
                    lon,
                    1.004
                );

            const glm::dvec3 p1 =
                planetSurfacePointMeters(
                    planet,
                    lat1,
                    lon,
                    1.004
                );

            emitVisibleSurfaceSegment(
                p0,
                p1
            );
        }
    }

    glEnd();

    // =========================================================
    // Полюса. Показываем только тот, который на видимой стороне.
    // =========================================================
    const glm::dvec3 northAxis =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 northWorld =
        planet.planetCenterMeters +
        northAxis * r * 1.018;

    const glm::dvec3 southWorld =
        planet.planetCenterMeters -
        northAxis * r * 1.018;

    glColor4f(
        0.55f,
        0.8f,
        1.0f,
        0.95f
    );

    if (cameraSpaceForWorldPoint(northWorld).z >= 0.0)
    {
        const glm::dvec2 north =
            planetMapProject(
                northWorld,
                planet,
                scale,
                centerPx
            );

        drawPlanetMapCross(
            north,
            5.0f
        );
    }

    if (cameraSpaceForWorldPoint(southWorld).z >= 0.0)
    {
        const glm::dvec2 south =
            planetMapProject(
                southWorld,
                planet,
                scale,
                centerPx
            );

        drawPlanetMapCross(
            south,
            5.0f
        );
    }
}



void SystemMapRenderer::drawPlanetFilledDisk(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double r =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const glm::dvec2 c {
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    };

    glColor4f(0.035f, 0.09f, 0.18f, 0.92f);

    glBegin(GL_TRIANGLE_FAN);

    glVertex2d(c.x, c.y);

    for (int i = 0; i <= 192; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            192.0;

        glVertex2d(
            c.x + std::cos(a) * r,
            c.y + std::sin(a) * r
        );
    }

    glEnd();
}


void SystemMapRenderer::drawPlanetTexturedGlobe(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (planet.planetRadiusMeters <= 1.0)
        return;

    const GLuint texture =
        globalAlbedoTextureForPlanetSnapshot(
            planet
        );

    if (texture == 0)
        return;

    render::celestial::PlanetGlobeLayerDraw draw;

    draw.texture =
        texture;

    draw.centerPx =
        glm::dvec2(
            centerPx.x +
                activeDetailCamera().pan.x,

            centerPx.y +
                activeDetailCamera().pan.y
        );

    draw.radiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    draw.bodyToCamera =
        planetBodyToDetailCameraMatrix(
            planet
        );

    /*
        Spin уже включён в bodyToCamera.
    */
    draw.longitudeUvOffset =
        0.0f;

    /*
        Сохраняем ориентацию V старого surface renderer:
            south pole -> V = 0
    */
    draw.flipV =
        false;

    draw.color =
        glm::vec4(
            1.0f
        );

    draw.opacity =
        1.0f;

    draw.blending =
        true;

    draw.useHorizonFade =
        false;

    draw.usePolarFade =
        false;

    m_planetGlobeMeshRenderer.render(
        draw
    );
}


void SystemMapRenderer::drawTexturedDisk2D(
    GLuint texture,
    const glm::dvec2& centerPx,
    double radiusPx,
    const glm::vec4& color,
    int segments
)
{
    if (texture == 0 ||
        radiusPx <= 1.0)
    {
        return;
    }

    segments =
        std::max(segments, 32);

    GLboolean textureWasEnabled =
        glIsEnabled(GL_TEXTURE_2D);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    glBegin(GL_TRIANGLE_FAN);

    glTexCoord2d(0.5, 0.5);
    glVertex2d(centerPx.x, centerPx.y);

    for (int i = 0; i <= segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const double ca =
            std::cos(a);

        const double sa =
            std::sin(a);

        const double u =
            0.5 + ca * 0.5;

        const double v =
            0.5 + sa * 0.5;

        glTexCoord2d(u, v);

        glVertex2d(
            centerPx.x + ca * radiusPx,
            centerPx.y + sa * radiusPx
        );
    }

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}



void SystemMapRenderer::drawPlanetTexturedDisk(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const GLuint texture =
        mapPreviewTextureForPlanetSnapshot(planet);

    if (texture == 0)
        return;

    const double radiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const glm::dvec2 screenCenter {
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    };

    drawTexturedDisk2D(
        texture,
        screenCenter,
        radiusPx,
        glm::vec4(1.0f, 1.0f, 1.0f, 0.96f),
        192
    );
}



void SystemMapRenderer::drawPlanetMapOrbit3D(
    const world::celestial::PlanetMapOrbit& orbit,
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (!orbit.valid || orbit.radiusMeters <= 1.0)
        return;

    segments =
        std::max(segments, 32);

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

    GLfloat baseColor[4] =
    {
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    glGetFloatv(
        GL_CURRENT_COLOR,
        baseColor
    );

    auto orbitPoint =
        [&](int i) -> glm::dvec3
        {
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
                planetMapCameraSpaceRelative(
                    relative
                );

            const double projectedDistance2 =
                cameraSpace.x * cameraSpace.x +
                cameraSpace.y * cameraSpace.y;

            const double planetRadius =
                planet.planetRadiusMeters;

            const bool behindPlanetCenter =
                cameraSpace.z < 0.0;

            const bool insidePlanetDisc =
                projectedDistance2 <
                planetRadius * planetRadius;

            return
                behindPlanetCenter &&
                insidePlanetDisc;
        };

    glBegin(GL_LINES);

    for (int i = 0; i < segments; ++i)
    {
        const glm::dvec3 p0 =
            orbitPoint(i);

        const glm::dvec3 p1 =
            orbitPoint((i + 1) % segments);

 
            
const glm::dvec3 mid =
    (p0 + p1) * 0.5;

/*
    В Planet Details дальняя половина орбиты остаётся
    видимой как очень слабая навигационная подсказка.

    Это относится только к карте Details. На карте Hub
    эта функция не используется.
*/
const bool hidden =
    isHiddenBehindPlanet(mid);

const float alpha =
    hidden
        ? baseColor[3] * 0.16f
        : baseColor[3];

glColor4f(
    baseColor[0],
    baseColor[1],
    baseColor[2],
    alpha
);




        const glm::dvec2 s0 =
            planetMapProject(
                p0,
                planet,
                scale,
                centerPx
            );

        const glm::dvec2 s1 =
            planetMapProject(
                p1,
                planet,
                scale,
                centerPx
            );

        glVertex2d(s0.x, s0.y);
        glVertex2d(s1.x, s1.y);
    }

    glEnd();

    glColor4f(
        baseColor[0],
        baseColor[1],
        baseColor[2],
        baseColor[3]
    );
}



// ============================================================================
// Details environment, rings, atmosphere and clouds
// ============================================================================




std::vector<
    render::celestial::ProceduralCloudStyle
>
SystemMapRenderer::planetCloudStylesForPlanet(
    const world::celestial::PlanetMapSnapshot& planet
) const
{
    return cloudStylesForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId,
        planet.planetRadiusMeters,
        1024,
        512
    );
}



SystemMapRenderer::HubPlanetAtmosphereStyle
SystemMapRenderer::planetAtmosphereStyleForPlanet(
    const world::celestial::PlanetMapSnapshot& planet
) const
{
    return atmosphereStyleForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId
    );
}


render::celestial::rings::PlanetRingRenderContext
SystemMapRenderer::planetRingRenderContext(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    std::vector<
        world::celestial::SystemMapRing
    >& normalizedBands
) const
{
    using render::celestial::rings::
        PlanetRingRenderContext;

    PlanetRingRenderContext context;

    context.planetCenterPx =
        glm::dvec2(
            centerPx.x +
                activeDetailCamera().pan.x,
            centerPx.y +
                activeDetailCamera().pan.y
        );

    if (planet.planetRadiusMeters <= 1.0)
        return context;

    const double planetRadiusKm =
        planet.planetRadiusMeters /
        1000.0;

    normalizedBands.clear();

    normalizedBands.reserve(
        planet.rings.size()
    );

    /*
        Renderer ожидает радиусы в planet-radius units.
    */
    for (const auto& sourceBand :
         planet.rings)
    {
        if (sourceBand.outerRadiusKm <=
            sourceBand.innerRadiusKm)
        {
            continue;
        }

        auto band =
            sourceBand;

        band.innerRadiusKm /=
            planetRadiusKm;

        band.outerRadiusKm /=
            planetRadiusKm;

        normalizedBands.push_back(
            std::move(band)
        );
    }

    context.visual = planet.ringVisual;

    context.bands =
        &normalizedBands;

    const glm::dvec3 north =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 prime =
        planetPrimeAxisWorld(
            north
        );

    const glm::dvec3 east =
        planetEastAxisWorld(
            north,
            prime
        );

    /*
        Опциональный наклон плоскости колец
        вокруг prime axis.
    */
    const double inclination =
        glm::radians(
            planet.
                ringPlaneInclinationOffsetDeg
        );

    const glm::dvec3 tiltedEast =
        glm::normalize(
            east *
                std::cos(inclination) +
            north *
                std::sin(inclination)
        );

    const glm::dvec3 ringAxisXWorld =
        prime;

    const glm::dvec3 ringAxisYWorld =
        tiltedEast;

    const glm::dvec3 ringAxisXCamera =
        planetMapCameraSpaceRelative(
            ringAxisXWorld *
                planet.planetRadiusMeters
        );

    const glm::dvec3 ringAxisYCamera =
        planetMapCameraSpaceRelative(
            ringAxisYWorld *
                planet.planetRadiusMeters
        );

    const double finalScale =
        scale *
        activeDetailCamera().zoom;

    /*
        planetMapProject использует:
            screen.x += camera.x * finalScale
            screen.y -= camera.y * finalScale
    */
    context.ringAxisXPx =
        glm::dvec2(
            ringAxisXCamera.x *
                finalScale,
            -ringAxisXCamera.y *
                finalScale
        );

    context.ringAxisYPx =
        glm::dvec2(
            ringAxisYCamera.x *
                finalScale,
            -ringAxisYCamera.y *
                finalScale
        );

    /*
        Координаты shader-а выражены в planet radii,
        поэтому depth coefficients нормализуем обратно.
    */
    context.ringDepthCoefficients =
        glm::dvec2(
            ringAxisXCamera.z /
                planet.planetRadiusMeters,
            ringAxisYCamera.z /
                planet.planetRadiusMeters
        );

    context.planetRotationPhaseRad =
        planet.planetRotationPhaseRad;

    return context;
}


void SystemMapRenderer::drawPlanetDetailSculpt(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx
)
{
    if (planetRadiusPx <= 1.0)
        return;

    /*
        Shader и VAO инициализируются лениво,
        только при первом открытии Planet Details.
    */
    if (m_planetDetailSculptShader == 0)
    {
        m_planetDetailSculptShader =
            ShaderLibrary::instance().get(
                "planet_detail_sculpt"
            );

        if (m_planetDetailSculptShader == 0)
        {
            if (!m_planetDetailSculptWarningPrinted)
            {
                m_planetDetailSculptWarningPrinted = true;

                std::cerr
                    << "[SystemMapRenderer]"
                    << " shader 'planet_detail_sculpt'"
                    << " is unavailable\n";
            }

            return;
        }
    }

    if (m_planetDetailSculptVao == 0)
    {
        glGenVertexArrays(
            1,
            &m_planetDetailSculptVao
        );
    }

    if (m_planetDetailSculptVao == 0)
        return;

    GLint viewport[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_VIEWPORT,
        viewport
    );

    GLint previousProgram = 0;
    GLint previousVao = 0;

    GLint previousBlendSourceRgb = GL_SRC_ALPHA;
    GLint previousBlendDestinationRgb = GL_ONE_MINUS_SRC_ALPHA;
    GLint previousBlendSourceAlpha = GL_SRC_ALPHA;
    GLint previousBlendDestinationAlpha = GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendEquationRgb = GL_FUNC_ADD;
    GLint previousBlendEquationAlpha = GL_FUNC_ADD;

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
    );

    glGetIntegerv(
        GL_BLEND_SRC_RGB,
        &previousBlendSourceRgb
    );

    glGetIntegerv(
        GL_BLEND_DST_RGB,
        &previousBlendDestinationRgb
    );

    glGetIntegerv(
        GL_BLEND_SRC_ALPHA,
        &previousBlendSourceAlpha
    );

    glGetIntegerv(
        GL_BLEND_DST_ALPHA,
        &previousBlendDestinationAlpha
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_RGB,
        &previousBlendEquationRgb
    );

    glGetIntegerv(
        GL_BLEND_EQUATION_ALPHA,
        &previousBlendEquationAlpha
    );

    const GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    const GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    const GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    /*
        Multiplicative blend:

            result =
                shaderMultiplier *
                existingPlanetColor
    */
    glEnable(
        GL_BLEND
    );

    glBlendEquation(
        GL_FUNC_ADD
    );

    glBlendFunc(
        GL_DST_COLOR,
        GL_ZERO
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glUseProgram(
        m_planetDetailSculptShader
    );

    const GLint viewportOriginLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uViewportOriginPx"
        );

    const GLint viewportSizeLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uViewportSize"
        );

    const GLint planetCenterLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uPlanetCenterPx"
        );

    const GLint planetRadiusLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uPlanetRadiusPx"
        );

    const GLint lightDirectionLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uLightDirection"
        );

    const GLint shadowStrengthLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uShadowStrength"
        );

    const GLint limbDarkeningLocation =
        glGetUniformLocation(
            m_planetDetailSculptShader,
            "uLimbDarkening"
        );

    if (viewportOriginLocation >= 0)
    {
        glUniform2f(
            viewportOriginLocation,
            static_cast<float>(
                viewport[0]
            ),
            static_cast<float>(
                viewport[1]
            )
        );
    }

    if (viewportSizeLocation >= 0)
    {
        glUniform2f(
            viewportSizeLocation,
            static_cast<float>(
                viewport[2]
            ),
            static_cast<float>(
                viewport[3]
            )
        );
    }

    if (planetCenterLocation >= 0)
    {
        glUniform2f(
            planetCenterLocation,
            static_cast<float>(
                planetCenterPx.x
            ),
            static_cast<float>(
                planetCenterPx.y
            )
        );
    }

    if (planetRadiusLocation >= 0)
    {
        glUniform1f(
            planetRadiusLocation,
            static_cast<float>(
                planetRadiusPx
            )
        );
    }

    /*
        Фиксированный художественный свет.

        Он идёт немного сверху-слева и сильно направлен
        на наблюдателя. Поэтому настоящей ночной стороны
        на диске не возникает.
    */
    const glm::vec3 lightDirection =
        glm::normalize(
            glm::vec3(
                -0.58f,
                0.24f,
                0.78f
            )
        );

    if (lightDirectionLocation >= 0)
    {
        glUniform3f(
            lightDirectionLocation,
            lightDirection.x,
            lightDirection.y,
            lightDirection.z
        );
    }

    if (shadowStrengthLocation >= 0)
    {
        glUniform1f(
            shadowStrengthLocation,
            0.34f
        );
    }

    if (limbDarkeningLocation >= 0)
    {
        glUniform1f(
            limbDarkeningLocation,
            0.16f
        );
    }

    glBindVertexArray(
        m_planetDetailSculptVao
    );

    glDrawArrays(
        GL_TRIANGLES,
        0,
        3
    );

    glBindVertexArray(
        static_cast<GLuint>(
            previousVao
        )
    );

    glUseProgram(
        static_cast<GLuint>(
            previousProgram
        )
    );

    /*
        Полностью возвращаем прежний blending.
    */
    glBlendEquationSeparate(
        static_cast<GLenum>(
            previousBlendEquationRgb
        ),
        static_cast<GLenum>(
            previousBlendEquationAlpha
        )
    );

    glBlendFuncSeparate(
        static_cast<GLenum>(
            previousBlendSourceRgb
        ),
        static_cast<GLenum>(
            previousBlendDestinationRgb
        ),
        static_cast<GLenum>(
            previousBlendSourceAlpha
        ),
        static_cast<GLenum>(
            previousBlendDestinationAlpha
        )
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
}



void SystemMapRenderer::drawPlanetEnvironmentLayers(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    bool applySphericalSculpt
)
{
    if (!planet.valid ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const auto cloudStyles =
        planetCloudStylesForPlanet(
            planet
        );

    const HubPlanetAtmosphereStyle atmosphereStyle =
        planetAtmosphereStyleForPlanet(
            planet
        );

    /*
        Центр и радиус экранного диска Details.

        Эти вычисления должны совпадать с теми,
        которые используются старым atmosphere renderer.
    */
    const glm::dvec2 planetCenterPx(
        centerPx.x +
            activeDetailCamera().pan.x,
        centerPx.y +
            activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    /*
        Сначала рисуем облачные оболочки.
    */
    for (const auto& cloudStyle : cloudStyles)
    {
        if (!cloudStyle.enabled)
            continue;

        drawPlanetAnimatedCloudLayers(
            planet,
            scale,
            centerPx,
            cloudStyle
        );
    }

    /*
        Затемняем поверхность и облака вместе.

        Atmosphere рисуется после этого pass,
        поэтому её светлый край не будет убит тенью.
    */
    if (applySphericalSculpt)
    {
        drawPlanetDetailSculpt(
            planetCenterPx,
            planetRadiusPx
        );
    }

    /*
        Затем один GPU-pass рисует:

        - внутреннюю дымку;
        - атмосферный лимб;
        - внешнее свечение.

        Функция больше не является специфичной для Hub:
        она работает с любым экранным центром и радиусом.
    */
    if (atmosphereStyle.enabled)
    {
        drawHubMapPlanetAtmosphereStack(
            planetCenterPx,
            planetRadiusPx,
            atmosphereStyle
        );
    }
}


void SystemMapRenderer::drawPlanetAtmosphereInterior(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const HubPlanetAtmosphereStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const glm::dvec2 planetCenterPx(
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    glm::vec4 haze =
        style.surfaceHaze;

    haze.a *=
        std::max(
            0.0f,
            style.visualIntensity
        );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        haze,
        0.58,
        0.86,
        1.002,
        24,
        256
    );
}


void SystemMapRenderer::drawPlanetAtmosphereLimb(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const HubPlanetAtmosphereStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const glm::dvec2 planetCenterPx(
        centerPx.x + activeDetailCamera().pan.x,
        centerPx.y + activeDetailCamera().pan.y
    );

    const double planetRadiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom;

    const float intensity =
        std::max(
            0.0f,
            style.visualIntensity
        );

    glm::vec4 limbCore =
        style.limbCore;

    glm::vec4 nearAtmosphere =
        style.nearAtmosphere;

    glm::vec4 outerAtmosphere =
        style.outerAtmosphere;

    limbCore.a *= intensity;
    nearAtmosphere.a *= intensity;
    outerAtmosphere.a *= intensity;

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        limbCore,
        0.986,
        1.001,
        1.020,
        14,
        256
    );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        nearAtmosphere,
        0.998,
        1.018,
        static_cast<double>(style.radiusScale) + 0.025,
        20,
        256
    );

    drawHubMapPlanetSoftBand(
        planetCenterPx,
        planetRadiusPx,
        outerAtmosphere,
        1.012,
        static_cast<double>(style.radiusScale) + 0.020,
        static_cast<double>(style.radiusScale) + 0.075,
        26,
        256
    );
}



void SystemMapRenderer::drawPlanetAnimatedCloudLayers(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    const render::celestial::ProceduralCloudStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    const double renderTimeSeconds =
        environmentVisualTimeSeconds(
            planet.universeTimeSeconds
        );

    const double meanHeightMeters =
        (
            static_cast<double>(
                style.baseHeightKm
            ) +
            static_cast<double>(
                style.topHeightKm
            )
        ) *
        500.0;

    const double physicalRadiusScale =
        1.0 +
        meanHeightMeters /
            std::max(
                1.0,
                planet.planetRadiusMeters
            );

    /*
        Даже физически существующая разница высот облачных
        слоёв может быть меньше одного экранного пикселя.

        Небольшой художественный offset предотвращает
        наложение облаков точно на поверхность.
    */
    const double cloudRadiusScale =
        std::clamp(
            physicalRadiusScale +
                0.0015,
            1.0015,
            1.045
        );

    drawPlanetProceduralCloudGlobeLayer(
        planet,
        scale,
        centerPx,
        cloudRadiusScale,
        0.0,
        renderTimeSeconds,
        style
    );
}



void SystemMapRenderer::drawPlanetProceduralCloudGlobeLayer(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double cloudRadiusScale,
    double longitudeOffset,
    double timeSeconds,
    const render::celestial::ProceduralCloudStyle& style
)
{
    if (!style.enabled ||
        planet.planetRadiusMeters <= 1.0)
    {
        return;
    }

    /*
        Эта функция по-прежнему обновляет форму облаков
        средствами GPU и возвращает актуальную display texture.
    */
    const GLuint texture =
        m_proceduralCloudLayer.textureForStyle(
            style,
            timeSeconds
        );

    if (texture == 0)
        return;

    static GLuint lastLoggedDetailsTexture = 0;

    if (lastLoggedDetailsTexture != texture)
    {
        lastLoggedDetailsTexture =
            texture;

        std::cout
            << "[PlanetDetailsCloudRenderer]"
            << " texture=" << texture
            << " enabled=" << style.enabled
            << " opacity=" << style.opacity
            << " driftSpeed=" << style.driftSpeed
            << " radiusScale=" << cloudRadiusScale
            << " renderer=GPU_mesh"
            << "\n";
    }

    const double driftU =
        std::fmod(
            timeSeconds *
                static_cast<double>(
                    style.driftSpeed
                ),
            1.0
        );

    render::celestial::PlanetGlobeLayerDraw draw;

    draw.texture =
        texture;

    draw.centerPx =
        glm::dvec2(
            centerPx.x +
                activeDetailCamera().pan.x,

            centerPx.y +
                activeDetailCamera().pan.y
        );

    draw.radiusPx =
        planet.planetRadiusMeters *
        scale *
        activeDetailCamera().zoom *
        cloudRadiusScale;

    draw.bodyToCamera =
        planetBodyToDetailCameraMatrix(
            planet
        );

    /*
        Форма облаков меняется внутри ProceduralCloudLayer.

        Этот offset отвечает за непрерывное движение
        облачного слоя по долготе.
    */
    draw.longitudeUvOffset =
        static_cast<float>(
            longitudeOffset +
            driftU
        );

    /*
        Старый cloud renderer использовал:

            v = 0.5 - latitude / PI

        то есть south pole -> V = 1.
    */
    draw.flipV =
        true;

    /*
        Цвета cloudColor/shadowColor уже записаны
        в GPU-generated cloud texture.
    */
    draw.color =
        glm::vec4(
            1.0f
        );

    draw.opacity =
        std::clamp(
            style.opacity,
            0.0f,
            0.92f
        );

    draw.blending =
        true;

    /*
        Полностью сохраняем старый horizonFade:

            smoothStep(0.035, 0.30, front)
    */
    draw.useHorizonFade =
        true;

    draw.horizonFadeStart =
        0.035f;

    draw.horizonFadeEnd =
        0.30f;

    /*
        Полностью сохраняем старый polarGeometryFade:

            1 - smoothStep(0.82, 0.995, normalizedLatitude)
    */
    draw.usePolarFade =
        true;

    draw.polarFadeStart =
        0.82f;

    draw.polarFadeEnd =
        0.995f;

    m_planetGlobeMeshRenderer.render(
        draw
    );
}


// ============================================================================
// Details planet shape model
// ============================================================================





bool SystemMapRenderer::drawPlanetShapeModelDetail(
    const world::celestial::PlanetMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset ||
        !asset->hasShapeModel())
    {
        return false;
    }

    if (planet.planetRadiusMeters <= 1.0)
        return false;

    const std::string objPath =
        resolveSystemMapAssetPath(
            asset->base.shapeModelPath
        ).generic_string();

    std::string albedoPath;

    if (!asset->global.albedoPath.empty())
    {
        albedoPath =
            resolveSystemMapAssetPath(
                asset->global.albedoPath
            ).generic_string();
    }
    else if (!asset->base.albedoPath.empty())
    {
        albedoPath =
            resolveSystemMapAssetPath(
                asset->base.albedoPath
            ).generic_string();
    }

    const render::celestial::CelestialShapeMesh* mesh =
        m_celestialShapeMeshes.load(
            objPath,
            albedoPath,
            true
        );

    if (!mesh ||
        !mesh->valid())
    {
        return false;
    }

    const double meshToMeters =
        planet.planetRadiusMeters /
        static_cast<double>(
            mesh->boundRadius
        );

    const glm::dvec3 north =
        planetNorthAxisWorld(
            planet
        );

    const glm::dvec3 prime =
        planetPrimeAxisWorld(
            north
        );

    const glm::dvec3 east =
        planetEastAxisWorld(
            north,
            prime
        );

    const double cp =
        std::cos(
            planet.planetRotationPhaseRad
        );

    const double sp =
        std::sin(
            planet.planetRotationPhaseRad
        );

    auto meshPointToRelativeMeters =
        [&](const glm::vec3& p) -> glm::dvec3
        {
            const glm::dvec3 local(
                static_cast<double>(p.x - mesh->boundCenter.x) * meshToMeters,
                static_cast<double>(p.y - mesh->boundCenter.y) * meshToMeters,
                static_cast<double>(p.z - mesh->boundCenter.z) * meshToMeters
            );

            const glm::dvec3 spun(
                local.x * cp - local.z * sp,
                local.y,
                local.x * sp + local.z * cp
            );

            return
                prime * spun.x +
                north * spun.y +
                east  * spun.z;
        };

    struct ProjectedTri
    {
        glm::dvec2 aScreen {0.0};
        glm::dvec2 bScreen {0.0};
        glm::dvec2 cScreen {0.0};

        glm::vec2 aUv {0.0f};
        glm::vec2 bUv {0.0f};
        glm::vec2 cUv {0.0f};

        double depth = 0.0;
    };

    std::vector<ProjectedTri> projected;
    projected.reserve(
        mesh->triangles.size()
    );

    for (const render::celestial::CelestialShapeTriangle& tri : mesh->triangles)
    {
        const glm::dvec3 aRel =
            meshPointToRelativeMeters(
                tri.a.pos
            );

        const glm::dvec3 bRel =
            meshPointToRelativeMeters(
                tri.b.pos
            );

        const glm::dvec3 cRel =
            meshPointToRelativeMeters(
                tri.c.pos
            );

        const glm::dvec3 aCam =
            planetMapCameraSpaceRelative(
                aRel
            );

        const glm::dvec3 bCam =
            planetMapCameraSpaceRelative(
                bRel
            );

        const glm::dvec3 cCam =
            planetMapCameraSpaceRelative(
                cRel
            );

        const glm::dvec3 midCam =
            (
                aCam +
                bCam +
                cCam
            ) / 3.0;

        const glm::dvec3 faceNormal =
            glm::cross(
                bCam - aCam,
                cCam - aCam
            );

        if (glm::length(faceNormal) <= 1e-9)
            continue;

        glm::dvec3 outwardNormal =
            glm::normalize(
                faceNormal
            );

        // OBJ winding может быть любым.
        // Для тела, центрированного около нуля, midCam задаёт наружное направление.
        // Если normal смотрит внутрь, разворачиваем его.
        if (glm::dot(
                outwardNormal,
                midCam
            ) < 0.0)
        {
            outwardNormal =
                -outwardNormal;
        }

        // В planet detail convention видимая сторона имеет +Z.
        //
        // ВАЖНО:
        // Для irregular OBJ нельзя резать силуэт строго по normal.z <= 0.
        // Большие треугольники на краю могут быть частично видимыми.
        // Если выбрасывать их целиком, появляется "рваный" зубчатый край.
        //
        // Поэтому оставляем небольшой допуск за линию силуэта.
        constexpr double kSilhouetteNormalTolerance =
            -0.08;

        if (outwardNormal.z < kSilhouetteNormalTolerance)
            continue;

        const glm::dvec3 aWorld =
            planet.planetCenterMeters +
            aRel;

        const glm::dvec3 bWorld =
            planet.planetCenterMeters +
            bRel;

        const glm::dvec3 cWorld =
            planet.planetCenterMeters +
            cRel;

        ProjectedTri out;

        out.aScreen =
            planetMapProject(
                aWorld,
                planet,
                scale,
                centerPx
            );

        out.bScreen =
            planetMapProject(
                bWorld,
                planet,
                scale,
                centerPx
            );

        out.cScreen =
            planetMapProject(
                cWorld,
                planet,
                scale,
                centerPx
            );




        const double screenArea =
        (
            out.bScreen.x - out.aScreen.x
        ) *
        (
            out.cScreen.y - out.aScreen.y
        ) -
        (
            out.bScreen.y - out.aScreen.y
        ) *
        (
            out.cScreen.x - out.aScreen.x
        );

    if (std::abs(screenArea) < 0.05)
        continue;







        out.aUv =
            tri.a.uv;

        out.bUv =
            tri.b.uv;

        out.cUv =
            tri.c.uv;

        out.depth =
            midCam.z;

        projected.push_back(
            out
        );
    }

    if (projected.empty())
        return false;

    // Рисуем дальние front-facing треугольники раньше,
    // ближние позже. Это заменяет depth buffer для 2D-проекции.
    std::sort(
        projected.begin(),
        projected.end(),
        [](const ProjectedTri& a, const ProjectedTri& b)
        {
            return a.depth < b.depth;
        }
    );

   
    
    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean cullWasEnabled =
        glIsEnabled(
            GL_CULL_FACE
        );

    GLboolean alphaTestWasEnabled =
        glIsEnabled(
            GL_ALPHA_TEST
        );







    GLint oldTextureBinding =
        0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(
        0
    );

    // Shape model должен быть opaque.
    // Blend/alpha-test дают эффект дыр, просвечивания и "внутренней" текстуры.
    glDisable(
        GL_BLEND
    );

    glDisable(
        GL_ALPHA_TEST
    );

    // Shape model должен быть opaque.
    // Blend даёт эффект "текстура видна изнутри".
    glDisable(
        GL_BLEND
    );

    if (mesh->albedoTexture != 0)
    {
        glEnable(
            GL_TEXTURE_2D
        );

        glBindTexture(
            GL_TEXTURE_2D,
            mesh->albedoTexture
        );

        glColor4f(
            1.0f,
            1.0f,
            1.0f,
            1.0f
        );
    }
    else
    {
        glDisable(
            GL_TEXTURE_2D
        );

        glColor4f(
            0.62f,
            0.60f,
            0.56f,
            1.0f
        );
    }

    glBegin(
        GL_TRIANGLES
    );

    for (const ProjectedTri& tri : projected)
    {
        glTexCoord2f(
            tri.aUv.x,
            tri.aUv.y
        );

        glVertex2d(
            tri.aScreen.x,
            tri.aScreen.y
        );

        glTexCoord2f(
            tri.bUv.x,
            tri.bUv.y
        );

        glVertex2d(
            tri.bScreen.x,
            tri.bScreen.y
        );

        glTexCoord2f(
            tri.cUv.x,
            tri.cUv.y
        );

        glVertex2d(
            tri.cScreen.x,
            tri.cScreen.y
        );
    }

    glEnd();

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            oldTextureBinding
        )
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (cullWasEnabled)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);



    if (alphaTestWasEnabled)
        glEnable(GL_ALPHA_TEST);
    else
        glDisable(GL_ALPHA_TEST);







    return true;
}


// ============================================================================
// Details resource adapters
// ============================================================================


GLuint SystemMapRenderer::mapPreviewTextureForPlanetSnapshot(
    const world::celestial::PlanetMapSnapshot& planet
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset)
        return 0;

    return mapPreviewTextureForGeneratedAsset(*asset);
}



GLuint SystemMapRenderer::globalAlbedoTextureForPlanetSnapshot(
    const world::celestial::PlanetMapSnapshot& planet
)
{
    const auto* asset =
        generatedAssetForIdentity(
            planet.systemId,
            planet.planetBodyId,
            planet.planetName
        );

    if (!asset)
        return 0;

    return globalAlbedoTextureForGeneratedAsset(
        *asset
    );
}

