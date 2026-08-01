/*
    Hub map implementation.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/

// ============================================================================
// Hub projection, rendering and interaction
// ============================================================================



void SystemMapRenderer::drawHubMapBox(
    const glm::dvec3& center,
    const world::celestial::LocalSceneAxes& axes,
    const glm::dvec3& size,
    const glm::vec4& color,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::dvec3 hx =
        axes.x * (size.x * 0.5);

    const glm::dvec3 hy =
        axes.y * (size.y * 0.5);

    const glm::dvec3 hz =
        axes.z * (size.z * 0.5);

    glm::dvec3 points[8];

    points[0] = center - hx - hy - hz;
    points[1] = center + hx - hy - hz;
    points[2] = center + hx + hy - hz;
    points[3] = center - hx + hy - hz;

    points[4] = center - hx - hy + hz;
    points[5] = center + hx - hy + hz;
    points[6] = center + hx + hy + hz;
    points[7] = center - hx + hy + hz;

    constexpr int edges[12][2] =
    {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    if (m_hubMapGpuGeometryRenderer.active())
    {
        for (const auto& edge : edges)
        {
            m_hubMapGpuGeometryRenderer.submitHubLine(
                points[edge[0]],
                points[edge[1]],
                color
            );
        }

        return;
    }

    /*
        Старый путь остаётся fallback-ом,
        если новые shaders не загрузились.
    */
    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    for (const auto& edge : edges)
    {
        drawPlanetMapLine(
            activeLocalCameraSnapshot().project(points[edge[0]]),
            activeLocalCameraSnapshot().project(points[edge[1]])
        );
    }
}














void SystemMapRenderer::drawHubMapAxes(
    const glm::dvec3& center,
    const world::celestial::LocalSceneAxes& axes,
    double axisLenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const glm::vec4 xColor(
        1.0f,
        0.2f,
        0.2f,
        0.95f
    );

    const glm::vec4 yColor(
        0.2f,
        1.0f,
        0.2f,
        0.95f
    );

    const glm::vec4 zColor(
        0.25f,
        0.55f,
        1.0f,
        0.95f
    );

    if (m_hubMapGpuGeometryRenderer.active())
    {
        m_hubMapGpuGeometryRenderer.submitHubLine(
            center,
            center + axes.x * axisLenMeters,
            xColor
        );

        m_hubMapGpuGeometryRenderer.submitHubLine(
            center,
            center + axes.y * axisLenMeters,
            yColor
        );

        m_hubMapGpuGeometryRenderer.submitHubLine(
            center,
            center + axes.z * axisLenMeters,
            zColor
        );

        return;
    }

    const glm::dvec2 origin =
        activeLocalCameraSnapshot().project(center);

    glColor4f(xColor.r, xColor.g, xColor.b, xColor.a);

    drawPlanetMapLine(
        origin,
        activeLocalCameraSnapshot().project(center + axes.x * axisLenMeters)
    );

    glColor4f(yColor.r, yColor.g, yColor.b, yColor.a);

    drawPlanetMapLine(
        origin,
        activeLocalCameraSnapshot().project(center + axes.y * axisLenMeters)
    );

    glColor4f(zColor.r, zColor.g, zColor.b, zColor.a);

    drawPlanetMapLine(
        origin,
        activeLocalCameraSnapshot().project(center + axes.z * axisLenMeters)
    );
}










void SystemMapRenderer::drawHubMapVelocityArrow(
    const glm::dvec3& center,
    const glm::dvec3& velocity,
    double lenMeters,
    double scale,
    const glm::dvec2& centerPx
)
{
    const double speed =
        glm::length(velocity);

    if (speed < 0.001)
        return;

    const glm::dvec3 direction =
        velocity / speed;

    const glm::vec4 color(
        1.0f,
        0.9f,
        0.25f,
        0.95f
    );

    if (m_hubMapGpuGeometryRenderer.active())
    {
        m_hubMapGpuGeometryRenderer.submitHubLine(
            center,
            center + direction * lenMeters,
            color
        );

        return;
    }

    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    drawPlanetMapLine(
        activeLocalCameraSnapshot().project(center),
        activeLocalCameraSnapshot().project(center + direction * lenMeters)
    );
}










void SystemMapRenderer::drawHubMapScreenMarker(
    const glm::dvec2& screenPx,
    double radiusPx,
    const glm::vec4& color,
    bool drawCross,
    int segments
)
{
    if (radiusPx <= 0.5)
        return;

    segments =
        std::max(
            12,
            segments
        );



    if (m_hubMapGpuGeometryRenderer.active())
    {
        m_hubMapGpuGeometryRenderer.submitScreenCircle(
            screenPx,
            radiusPx,
            color,
            segments
        );

        if (drawCross)
        {
            m_hubMapGpuGeometryRenderer.submitScreenCross(
                screenPx,
                radiusPx * 0.62,
                color
            );
        }

        return;
    }








    glColor4f(
        color.r,
        color.g,
        color.b,
        color.a
    );

    drawPlanetMapCircle(
        screenPx,
        radiusPx,
        segments
    );

    if (!drawCross)
        return;

    const double s =
        radiusPx * 0.62;

    glBegin(GL_LINES);

    glVertex2d(
        screenPx.x - s,
        screenPx.y
    );

    glVertex2d(
        screenPx.x + s,
        screenPx.y
    );

    glVertex2d(
        screenPx.x,
        screenPx.y - s
    );

    glVertex2d(
        screenPx.x,
        screenPx.y + s
    );

    glEnd();
}
















bool SystemMapRenderer::drawHubMapAssemblyWire(
    ObjectType typeId,
    const glm::dvec3& objectCenter,
    const world::celestial::LocalSceneAxes& objectAxes,
    const glm::vec4& color
)
{
    using game::ship::geometry::AssemblyMeshLibrary;

    if (!m_hubMapGpuGeometryRenderer.active() ||
        typeId == ObjectType::None)
    {
        return false;
    }

    if (!AssemblyMeshLibrary::has(typeId))
        return false;

    const auto& assembly =
        AssemblyMeshLibrary::get(typeId);

    /*
        Локальная система объекта → hub-local meters.

        Столбцы матрицы:
            0 = object X
            1 = object Y
            2 = object Z
            3 = object center
    */
    glm::mat4 objectToHub(1.0f);

    objectToHub[0] =
        glm::vec4(
            glm::vec3(objectAxes.x),
            0.0f
        );

    objectToHub[1] =
        glm::vec4(
            glm::vec3(objectAxes.y),
            0.0f
        );

    objectToHub[2] =
        glm::vec4(
            glm::vec3(objectAxes.z),
            0.0f
        );

    objectToHub[3] =
        glm::vec4(
            glm::vec3(objectCenter),
            1.0f
        );

    /*
        Сохраняем прежнюю логику:
        при наличии whole-object proxy модульную
        сборку не рисуем.
    */
    if (assembly.hasWholeShipProxy &&
        assembly.wholeShipProxyGpu.getEdgeVertexCount() > 0)
    {
        m_hubMapGpuGeometryRenderer.submitMesh(
            assembly.wholeShipProxyGpu,
            objectToHub,
            color
        );

        return true;
    }

    bool submittedAnything = false;

    for (const auto& module : assembly.modules)
    {
        for (const auto& part : module.meshes)
        {
            const auto& meshGpu =
                part.lod1Gpu.getEdgeVertexCount() > 0
                    ? part.lod1Gpu
                    : part.lod0Gpu;

            if (meshGpu.getEdgeVertexCount() == 0)
                continue;

            const glm::mat4 moduleToObject =
                world::modules::
                    buildAssemblyModuleStaticHierarchicalLocalModel(
                        assembly,
                        module.id
                    );

            const glm::mat4 partToModule =
                glm::translate(
                    glm::mat4(1.0f),
                    part.localOffset
                );

            m_hubMapGpuGeometryRenderer.submitMesh(
                meshGpu,
                objectToHub *
                    moduleToObject *
                    partToModule,
                color
            );

            submittedAnything = true;
        }
    }

    return submittedAnything;
}








void SystemMapRenderer::drawHubMapPlanetSoftBand(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const glm::vec4& peakColor,
    double startRadiusFactor,
    double peakRadiusFactor,
    double endRadiusFactor,
    int radialSteps,
    int segments
)
{
    if (planetRadiusPx <= 1.0)
        return;

    if (endRadiusFactor <= startRadiusFactor)
        return;

    if (peakColor.a <= 0.0001f)
        return;

    radialSteps =
        std::max(
            8,
            radialSteps
        );

    segments =
        std::max(
            96,
            segments
        );

    GLboolean textureWasEnabled =
        glIsEnabled(
            GL_TEXTURE_2D
        );

    GLboolean blendWasEnabled =
        glIsEnabled(
            GL_BLEND
        );

    GLboolean depthWasEnabled =
        glIsEnabled(
            GL_DEPTH_TEST
        );

    GLboolean depthMaskWasEnabled =
        GL_TRUE;

    glGetBooleanv(
        GL_DEPTH_WRITEMASK,
        &depthMaskWasEnabled
    );

    GLint oldTextureBinding = 0;

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &oldTextureBinding
    );

    glUseProgram(
        0
    );

    // ВАЖНО:
    // Atmosphere band — это чистая цветная геометрия.
    // Если оставить GL_TEXTURE_2D включённым, fixed pipeline будет
    // умножать цвет на текущую текстуру и текущие texture coords.
    // В результате band может стать полностью невидимым.
    glDisable(
        GL_TEXTURE_2D
    );

    glBindTexture(
        GL_TEXTURE_2D,
        0
    );

    glEnable(
        GL_BLEND
    );

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    glDisable(
        GL_DEPTH_TEST
    );

    glDepthMask(
        GL_FALSE
    );

    const double startR =
        planetRadiusPx *
        startRadiusFactor;

    const double peakR =
        planetRadiusPx *
        peakRadiusFactor;

    const double endR =
        planetRadiusPx *
        endRadiusFactor;

    const double totalSpan =
        std::max(
            0.000001,
            endR - startR
        );

    const double peakT =
        std::clamp(
            (peakR - startR) / totalSpan,
            0.0,
            1.0
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    auto alphaAt =
        [&](double t) -> float
        {
            double a = 0.0;

            if (t <= peakT)
            {
                a =
                    smoothStep(
                        0.0,
                        std::max(
                            0.000001,
                            peakT
                        ),
                        t
                    );
            }
            else
            {
                a =
                    1.0 -
                    smoothStep(
                        peakT,
                        1.0,
                        t
                    );
            }

            return static_cast<float>(
                a * peakColor.a
            );
        };

    for (int ring = 0; ring < radialSteps; ++ring)
    {
        const double t0 =
            static_cast<double>(ring) /
            static_cast<double>(radialSteps);

        const double t1 =
            static_cast<double>(ring + 1) /
            static_cast<double>(radialSteps);

        const double r0 =
            startR +
            (endR - startR) * t0;

        const double r1 =
            startR +
            (endR - startR) * t1;

        const float a0 =
            alphaAt(
                t0
            );

        const float a1 =
            alphaAt(
                t1
            );

        glBegin(
            GL_TRIANGLE_STRIP
        );

        for (int i = 0; i <= segments; ++i)
        {
            const double ang =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            const double ca =
                std::cos(
                    ang
                );

            const double sa =
                std::sin(
                    ang
                );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a0
            );

            glVertex2d(
                planetCenterPx.x + ca * r0,
                planetCenterPx.y + sa * r0
            );

            glColor4f(
                peakColor.r,
                peakColor.g,
                peakColor.b,
                a1
            );

            glVertex2d(
                planetCenterPx.x + ca * r1,
                planetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }

    glDepthMask(
        depthMaskWasEnabled
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(oldTextureBinding)
    );

    if (textureWasEnabled)
        glEnable(GL_TEXTURE_2D);
    else
        glDisable(GL_TEXTURE_2D);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}






void SystemMapRenderer::drawHubMapPlanetAtmosphereStack(
    const glm::dvec2& planetCenterPx,
    double planetRadiusPx,
    const HubPlanetAtmosphereStyle& style,
    bool premultipliedTarget
)
{
    if (!style.enabled ||
        planetRadiusPx <= 1.0)
    {
        return;
    }

    static GLuint atmosphereShader = 0;
    static GLuint fullscreenVao = 0;

    static GLint viewportOriginLocation = -1;
    static GLint viewportSizeLocation = -1;
    static GLint planetCenterLocation = -1;
    static GLint planetRadiusLocation = -1;

    static GLint radiusScaleLocation = -1;
    static GLint visualIntensityLocation = -1;

    static GLint surfaceHazeLocation = -1;
    static GLint limbCoreLocation = -1;
    static GLint nearAtmosphereLocation = -1;
    static GLint outerAtmosphereLocation = -1;

    if (atmosphereShader == 0)
    {
        atmosphereShader =
            ShaderLibrary::instance().get(
                "hub_planet_atmosphere"
            );

        if (atmosphereShader == 0)
        {
            static bool warned = false;

            if (!warned)
            {
                warned = true;

                std::cerr
                    << "[HubAtmosphere]"
                    << " shader not available"
                    << "\n";
            }

            return;
        }

        viewportOriginLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportOriginPx"
            );

        viewportSizeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uViewportSize"
            );

        planetCenterLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetCenterPx"
            );

        planetRadiusLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uPlanetRadiusPx"
            );

        radiusScaleLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uRadiusScale"
            );

        visualIntensityLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uVisualIntensity"
            );

        surfaceHazeLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uSurfaceHaze"
            );

        limbCoreLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uLimbCore"
            );

        nearAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uNearAtmosphere"
            );

        outerAtmosphereLocation =
            glGetUniformLocation(
                atmosphereShader,
                "uOuterAtmosphere"
            );
    }

    if (fullscreenVao == 0)
    {
        glGenVertexArrays(
            1,
            &fullscreenVao
        );
    }

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

    glGetIntegerv(
        GL_CURRENT_PROGRAM,
        &previousProgram
    );

    glGetIntegerv(
        GL_VERTEX_ARRAY_BINDING,
        &previousVao
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

    const GLboolean scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );



    GLint previousScissorBox[4] =
    {
        0,
        0,
        1,
        1
    };

    glGetIntegerv(
        GL_SCISSOR_BOX,
        previousScissorBox
    );


    GLint previousBlendSourceRgb =
        GL_SRC_ALPHA;

    GLint previousBlendDestinationRgb =
        GL_ONE_MINUS_SRC_ALPHA;

    GLint previousBlendSourceAlpha =
        GL_SRC_ALPHA;

    GLint previousBlendDestinationAlpha =
        GL_ONE_MINUS_SRC_ALPHA;


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



    /*
        Atmosphere shader нужен только в прямоугольнике,
        содержащем внешнюю атмосферную оболочку.
    */
    const double atmosphereOuterRadiusScale =
        std::max(
            1.001,
            static_cast<double>(
                style.radiusScale
            ) +
            0.075
        );

    const double atmosphereRadiusPx =
        planetRadiusPx *
        atmosphereOuterRadiusScale;

    const int localLeft =
        static_cast<int>(
            std::floor(
                planetCenterPx.x -
                atmosphereRadiusPx -
                2.0
            )
        );

    const int localRight =
        static_cast<int>(
            std::ceil(
                planetCenterPx.x +
                atmosphereRadiusPx +
                2.0
            )
        );

    const int localTop =
        static_cast<int>(
            std::floor(
                planetCenterPx.y -
                atmosphereRadiusPx -
                2.0
            )
        );

    const int localBottom =
        static_cast<int>(
            std::ceil(
                planetCenterPx.y +
                atmosphereRadiusPx +
                2.0
            )
        );

    const int clippedLeft =
        std::clamp(
            localLeft,
            0,
            viewport[2]
        );

    const int clippedRight =
        std::clamp(
            localRight,
            0,
            viewport[2]
        );

    const int clippedTop =
        std::clamp(
            localTop,
            0,
            viewport[3]
        );

    const int clippedBottom =
        std::clamp(
            localBottom,
            0,
            viewport[3]
        );

    const int scissorWidth =
        clippedRight -
        clippedLeft;

    const int scissorHeight =
        clippedBottom -
        clippedTop;

    if (scissorWidth <= 0 ||
        scissorHeight <= 0)
    {
        return;
    }

    /*
        planetCenterPx использует начало координат сверху,
        glScissor — снизу.
    */
    const int scissorX =
        viewport[0] +
        clippedLeft;

    const int scissorY =
        viewport[1] +
        viewport[3] -
        clippedBottom;




    glDisable(
        GL_DEPTH_TEST
    );

    glDisable(
        GL_CULL_FACE
    );

    glEnable(
        GL_SCISSOR_TEST
    );

    glScissor(
        scissorX,
        scissorY,
        scissorWidth,
        scissorHeight
    );



    glEnable(
        GL_BLEND
    );

    if (premultipliedTarget)
    {
        /*
            RGB остаётся premultiplied внутри прозрачного
            half-resolution overlay.

            Alpha накапливается отдельно.
        */
        glBlendFuncSeparate(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA,
            GL_ONE,
            GL_ONE_MINUS_SRC_ALPHA
        );
    }
    else
    {
        glBlendFunc(
            GL_SRC_ALPHA,
            GL_ONE_MINUS_SRC_ALPHA
        );
    }





    glUseProgram(
        atmosphereShader
    );

    glUniform2f(
        viewportOriginLocation,
        static_cast<float>(viewport[0]),
        static_cast<float>(viewport[1])
    );

    glUniform2f(
        viewportSizeLocation,
        static_cast<float>(
            viewport[2]
        ),
        static_cast<float>(
            viewport[3]
        )
    );

    glUniform2f(
        planetCenterLocation,
        static_cast<float>(
            planetCenterPx.x
        ),
        static_cast<float>(
            planetCenterPx.y
        )
    );

    glUniform1f(
        planetRadiusLocation,
        static_cast<float>(
            planetRadiusPx
        )
    );

    glUniform1f(
        radiusScaleLocation,
        style.radiusScale
    );

    glUniform1f(
        visualIntensityLocation,
        style.visualIntensity
    );

    glUniform4f(
        surfaceHazeLocation,
        style.surfaceHaze.r,
        style.surfaceHaze.g,
        style.surfaceHaze.b,
        style.surfaceHaze.a
    );

    glUniform4f(
        limbCoreLocation,
        style.limbCore.r,
        style.limbCore.g,
        style.limbCore.b,
        style.limbCore.a
    );

    glUniform4f(
        nearAtmosphereLocation,
        style.nearAtmosphere.r,
        style.nearAtmosphere.g,
        style.nearAtmosphere.b,
        style.nearAtmosphere.a
    );

    glUniform4f(
        outerAtmosphereLocation,
        style.outerAtmosphere.r,
        style.outerAtmosphere.g,
        style.outerAtmosphere.b,
        style.outerAtmosphere.a
    );

    glBindVertexArray(
        fullscreenVao
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

    glScissor(
        previousScissorBox[0],
        previousScissorBox[1],
        previousScissorBox[2],
        previousScissorBox[3]
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

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}






void SystemMapRenderer::drawHubMapCircleLocalXY(
    const glm::dvec3& center,
    double radiusMeters,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (radiusMeters <= 0.0)
        return;

    segments =
        std::max(
            24,
            segments
        );

    glBegin(GL_LINE_LOOP);

    for (int i = 0; i < segments; ++i)
    {
        const double a =
            glm::two_pi<double>() *
            static_cast<double>(i) /
            static_cast<double>(segments);

        const glm::dvec3 p =
            center +
            glm::dvec3(
                std::cos(a) * radiusMeters,
                std::sin(a) * radiusMeters,
                0.0
            );

        const glm::dvec2 s =
            activeLocalCameraSnapshot().project(p);

        glVertex2d(
            s.x,
            s.y
        );
    }

    glEnd();
}











glm::mat3
SystemMapRenderer::hubCameraToParentPlanetBodyMatrix(
    const world::celestial::HubMapSnapshot& hub
) const
{
    /*
    ====================================================
    1. Текущая серверная орбитальная система хаба
    ====================================================

    Эти оси уже соответствуют текущему server tick.

    Renderer не вычисляет orbital phase и не интегрирует
    положение хаба самостоятельно.
*/
glm::dvec3 progradeWorld =
    safeNormalizeD(
        hub.hubWorldAxes.x,
        glm::dvec3(
            1.0,
            0.0,
            0.0
        )
    );

glm::dvec3 radialWorld =
    safeNormalizeD(
        hub.hubWorldAxes.y,
        glm::dvec3(
            0.0,
            1.0,
            0.0
        )
    );

glm::dvec3 normalWorld =
    safeNormalizeD(
        glm::cross(
            progradeWorld,
            radialWorld
        ),
        safeNormalizeD(
            hub.hubWorldAxes.z,
            glm::dvec3(
                0.0,
                0.0,
                1.0
            )
        )
    );

/*
    Ортогонализируем frame после передачи double -> snapshot.
*/
progradeWorld =
    safeNormalizeD(
        glm::cross(
            radialWorld,
            normalWorld
        ),
        progradeWorld
    );

radialWorld =
    safeNormalizeD(
        glm::cross(
            normalWorld,
            progradeWorld
        ),
        radialWorld
    );

    /*
        ====================================================
        2. Экранная ориентация cinematic globe
        ====================================================

        Центр видимого диска всегда является sub-hub point:

            camera Z -> radialWorld

        Yaw вращает экранные tangent axes вокруг направления
        от планеты к хабу. Pitch меняет композицию горизонта,
        но не физическое положение наблюдателя над планетой.
    */
    const double cameraYaw =
        activeLocalCameraSnapshot().state.yaw;

    const double yawCos =
        std::cos(
            cameraYaw
        );

    const double yawSin =
        std::sin(
            cameraYaw
        );

    const glm::dvec3 screenRightWorld =
        safeNormalizeD(
            progradeWorld *
                yawCos -
            normalWorld *
                yawSin,
            progradeWorld
        );

    const glm::dvec3 screenUpWorld =
        safeNormalizeD(
            -normalWorld *
                yawCos -
            progradeWorld *
                yawSin,
            -normalWorld
        );

    const glm::dvec3 screenTowardViewerWorld =
        radialWorld;

    /*
        ====================================================
        3. Body-fixed система родительской планеты
        ====================================================
    */
    const double tilt =
        degToRadD(
            hub.parentPlanetAxialTiltDeg
        );

    const double node =
        degToRadD(
            hub.parentPlanetAxisNodeDeg
        );

    const glm::dvec3 planetNorthWorld =
        safeNormalizeD(
            glm::dvec3(
                std::sin(tilt) *
                    std::cos(node),

                std::cos(tilt),

                std::sin(tilt) *
                    std::sin(node)
            ),
            glm::dvec3(
                0.0,
                1.0,
                0.0
            )
        );

    const glm::dvec3 planetPrime0World =
        planetPrimeAxisWorld(
            planetNorthWorld
        );

    const glm::dvec3 planetEast0World =
        planetEastAxisWorld(
            planetNorthWorld,
            planetPrime0World
        );

    /*
        Полностью повторяет CelestialSystemRuntime:

            rotationPhase =
                universeTime / dayLength
                + rotationOffset
    */




        /*
    Rotation phase приходит из того же CelestialSystemRuntime,
    который используется Planet Details.
*/
const double rotationPhase =
    hub.parentPlanetRotationPhaseRad;








    /*
        Texture longitude zero в мировом пространстве.
    */
    const double textureSpin =
        rotationPhase +
        degToRadD(
            hub.parentPlanetTextureLongitudeOffsetDeg
        );

    const double textureCos =
        std::cos(
            textureSpin
        );

    const double textureSin =
        std::sin(
            textureSpin
        );

    const glm::dvec3 texturePrimeWorld =
        safeNormalizeD(
            planetPrime0World *
                textureCos +
            planetEast0World *
                textureSin,
            planetPrime0World
        );

    const glm::dvec3 textureEastWorld =
        safeNormalizeD(
            -planetPrime0World *
                textureSin +
            planetEast0World *
                textureCos,
            planetEast0World
        );

    /*
        Мировое направление -> координаты texture body:

            X = longitude 0;
            Y = north;
            Z = longitude +90°.
    */
    auto worldDirectionToBody =
        [&](
            const glm::dvec3& worldDirection
        ) -> glm::vec3
        {
            const glm::dvec3 direction =
                safeNormalizeD(
                    worldDirection,
                    glm::dvec3(
                        0.0,
                        0.0,
                        1.0
                    )
                );

            return
                glm::vec3(
                    static_cast<float>(
                        glm::dot(
                            direction,
                            texturePrimeWorld
                        )
                    ),

                    static_cast<float>(
                        glm::dot(
                            direction,
                            planetNorthWorld
                        )
                    ),

                    static_cast<float>(
                        glm::dot(
                            direction,
                            textureEastWorld
                        )
                    )
                );
        };

    /*
        glm::mat3 принимает столбцы.

        Поэтому matrix * cameraNormal переводит:

            camera X -> body direction screenRight;
            camera Y -> body direction screenUp;
            camera Z -> body direction sub-hub point.
    */
    return
        glm::mat3(
            worldDirectionToBody(
                screenRightWorld
            ),

            worldDirectionToBody(
                screenUpWorld
            ),

            worldDirectionToBody(
                screenTowardViewerWorld
            )
        );
}






void SystemMapRenderer::drawHubMapPlanetSurfaceHint(
    const world::celestial::HubMapSnapshot& hub,
    double scale,
    const glm::dvec2& centerPx
)
{
    if (hub.parentPlanetRadiusMeters <= 0.0 ||
        hub.hubOrbitRadiusMeters <= 0.0)
    {
        return;
    }


    beginEnvironmentRenderSessionIfNeeded(
        Mode::Hub,
        hub.systemId,
        hub.parentBodyId
    );



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

    const double viewW =
        static_cast<double>(
            std::max(
                viewport[2],
                1
            )
        );

    const double viewH =
        static_cast<double>(
            std::max(
                viewport[3],
                1
            )
        );

    const double maxDim =
        std::max(
            viewW,
            viewH
        );

    const double pitch =
        std::clamp(
            activeLocalCameraSnapshot().state.pitch,
            0.12,
            1.20
        );

    auto smoothStep =
        [](double edge0, double edge1, double x) -> double
        {
            const double t =
                std::clamp(
                    (x - edge0) /
                    std::max(
                        0.000001,
                        edge1 - edge0
                    ),
                    0.0,
                    1.0
                );

            return
                t * t *
                (3.0 - 2.0 * t);
        };

    const double lookDownT =
        smoothStep(
            0.12,
            1.20,
            pitch
        );

    // Оставляем текущую кинематографическую композицию:
    // при малом pitch виден горизонт,
    // при большом pitch уходим к взгляду вниз.
    const double horizonY =
        (1.0 - lookDownT) * (viewH * 0.74) +
        lookDownT * (-viewH * 0.38);

    const double visualRadiusPx =
        maxDim *
        (1.38 + 0.46 * lookDownT);

    const double horizonCenterY =
        horizonY + visualRadiusPx;

    const double nadirCenterY =
        viewH * 0.54;

    const glm::dvec2 visualPlanetCenterPx(
        viewW * 0.50 +
            activeLocalCameraSnapshot().state.pan.x * 0.015,
        (1.0 - lookDownT) * horizonCenterY +
            lookDownT * nadirCenterY
    );

    m_lastHubPlanetVisualRadiusPx =
        visualRadiusPx;

    m_lastHubPlanetVisualCenterPx =
        visualPlanetCenterPx;


    const HubPlanetAtmosphereStyle atmosphereStyle =
    hubPlanetAtmosphereStyleForHub(
        hub
    );


    auto mixColor =
        [](const glm::vec4& a, const glm::vec4& b, float t) -> glm::vec4
        {
            return glm::vec4(
                a.r + (b.r - a.r) * t,
                a.g + (b.g - a.g) * t,
                a.b + (b.b - a.b) * t,
                a.a + (b.a - a.a) * t
            );
        };







    const GLuint albedoTexture =
        globalAlbedoTextureForHubSnapshot(
            hub
        );

    const GLuint normalTexture =
        globalNormalTextureForHubSnapshot(
            hub
        );

    const GLuint previewTexture =
        mapPreviewTextureForHubSnapshot(
            hub
        );

    const GLuint surfaceTexture =
        albedoTexture != 0
            ? albedoTexture
            : previewTexture;

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glUseProgram(0);
    glEnable(GL_BLEND);
    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    // -----------------------------------------------------------------
// 1. Базовое тело планеты.
// Гладкая океаническая масса без текстурной грязи.
//
// ВАЖНО:
// Внутри GL_TRIANGLE_STRIP на каждый угол должно быть ровно две вершины:
//   - внутренняя окружность r0
//   - внешняя окружность r1
//
// Если оставить третью вершину, появятся треугольные "зубья"
// по всей планете.
// -----------------------------------------------------------------
m_hubBackend.beginGpuStage(
    game::system_map::HubMapBackend::GpuStage::FallbackBody
);

if (surfaceTexture == 0)
{
    constexpr int bands = 32;
    constexpr int segments = 256;

    for (int band = 0; band < bands; ++band)
    {
        const double t0 =
            static_cast<double>(band) /
            static_cast<double>(bands);

        const double t1 =
            static_cast<double>(band + 1) /
            static_cast<double>(bands);

        const double r0 =
            visualRadiusPx *
            t0;

        const double r1 =
            visualRadiusPx *
            t1;

        const float c0 =
            static_cast<float>(t0);

        const float c1 =
            static_cast<float>(t1);

        const glm::vec4 color0 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c0
            );

        const glm::vec4 color1 =
            mixColor(
                atmosphereStyle.oceanInner,
                atmosphereStyle.oceanOuter,
                c1
            );

        glBegin(GL_TRIANGLE_STRIP);

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

            // Вершина внутренней окружности.
            glColor4f(
                color0.r,
                color0.g,
                color0.b,
                color0.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r0,
                visualPlanetCenterPx.y + sa * r0
            );

            // Вершина внешней окружности.
            glColor4f(
                color1.r,
                color1.g,
                color1.b,
                color1.a
            );

            glVertex2d(
                visualPlanetCenterPx.x + ca * r1,
                visualPlanetCenterPx.y + sa * r1
            );
        }

        glEnd();
    }
}




m_hubBackend.endGpuStage();




// -----------------------------------------------------------------
    // 2. Поверхность родительского тела.
    // Renderer получает ориентацию из server snapshot и использует
    // отдельную Hub-map screen-space проекцию, а не Detail globe path.
    // -----------------------------------------------------------------
        /*
        Одно общее время для поверхности и облаков.
        */
/*
    Визуальное время используется только для движения
    procedural clouds.
*/
const double cloudVisualTimeSeconds =
    environmentVisualTimeSeconds(
        hub.universeTimeSeconds
    );

/*
    Геометрическая ориентация планеты берётся строго
    из текущего server snapshot.
*/
const glm::mat3 cameraToPlanetBody =
    hubCameraToParentPlanetBodyMatrix(
        hub
    );



    m_hubBackend.beginGpuStage(
        game::system_map::HubMapBackend::GpuStage::Surface
    );


    if (surfaceTexture != 0)
    {
        /*
            Пока это screen-space свет.

            Позже вместо фиксированного направления передадим
            реальное направление к звезде системы.
        */
        const glm::vec3 lightDirection =
            glm::normalize(
                glm::vec3(
                    -0.42f,
                    0.34f,
                    0.84f
                )
            );

        m_hubPlanetSurfaceRenderer.render(
            surfaceTexture,
            normalTexture,
            visualPlanetCenterPx,
            visualRadiusPx,
            cameraToPlanetBody,
            lightDirection
        );
    }


    m_hubBackend.endGpuStage();

    /*
        Облака и атмосфера являются мягкими слоями. Рисуем их
        в половинном разрешении и один раз композим поверх
        full-resolution поверхности.
    */
    constexpr float hubSoftLayerResolutionScale =
        0.50f;

    const bool softLayerTargetActive =
        m_hubPlanetOverlayRenderer.begin(
            viewport[2],
            viewport[3],
            hubSoftLayerResolutionScale
        );

    const double softLayerScale =
        softLayerTargetActive
            ? static_cast<double>(
                m_hubPlanetOverlayRenderer.resolutionScale()
            )
            : 1.0;

    const glm::dvec2 softPlanetCenterPx =
        visualPlanetCenterPx *
        softLayerScale;

    const double softPlanetRadiusPx =
        visualRadiusPx *
        softLayerScale;


        m_hubBackend.beginGpuStage(
            game::system_map::HubMapBackend::GpuStage::Clouds
        );

        const auto cloudStyles =
            hubPlanetCloudStylesForHub(
                hub
            );

        for (std::size_t layerIndex = 0;
            layerIndex < cloudStyles.size();
            ++layerIndex)
        {
            const auto& cloudStyle =
                cloudStyles[layerIndex];

            if (!cloudStyle.enabled)
                continue;

            /*
                ProceduralCloudLayer по-прежнему изменяет форму
                облаков и выполняет blending между состояниями.

                Новый mesh renderer только дешёво натягивает
                готовую динамическую текстуру на сферу.
            */
            const GLuint cloudTexture =
                m_proceduralCloudLayer.textureForStyle(
                    cloudStyle,
                    cloudVisualTimeSeconds
                );

            if (cloudTexture == 0)
                continue;

            const double meanHeightMeters =
                (
                    static_cast<double>(
                        cloudStyle.baseHeightKm
                    ) +
                    static_cast<double>(
                        cloudStyle.topHeightKm
                    )
                ) *
                500.0;

            const double physicalRadiusScale =
                1.0 +
                meanHeightMeters /
                    std::max(
                        1.0,
                        hub.parentPlanetRadiusMeters
                    );

            /*
                На огромной cinematic-сфере физическая разница
                высот почти незаметна. Оставляем слабое визуальное
                разделение слоёв.
            */
            const double cloudRadiusScale =
                std::clamp(
                    physicalRadiusScale +
                        0.0025 *
                        static_cast<double>(
                            layerIndex + 1
                        ),
                    1.003,
                    1.055
                );

            /*
                Независимый дрейф текущего облачного слоя.
            */
            const double driftU =
                std::fmod(
                    cloudVisualTimeSeconds *
                        static_cast<double>(
                            cloudStyle.driftSpeed
                        ),
                    1.0
                );

            render::celestial::PlanetGlobeLayerDraw draw;

            draw.texture =
                cloudTexture;

            /*
                visualPlanetCenterPx задан относительно текущего
                viewport и использует начало координат слева сверху.
            */
            draw.centerPx =
                softPlanetCenterPx;

            draw.radiusPx =
                softPlanetRadiusPx *
                cloudRadiusScale;

            /*
                Hub backdrop является художественной экранной
                сферой, поэтому используем прямую ориентацию:

                    X — вправо;
                    Y — вверх;
                    Z — к камере.
            */
            /*
                cameraToPlanetBody является чистым вращением.
                Его inverse равен transpose.
            */
            draw.bodyToCamera =
                glm::transpose(
                    cameraToPlanetBody
                );

            /*
                Вращение самой планеты уже содержится
                в bodyToCamera.

                Здесь остаётся только относительный
                атмосферный дрейф облаков.
            */
            draw.longitudeUvOffset =
                static_cast<float>(
                    driftU
                );

            /*
                Hub backdrop textures use a top-left screen-space origin.
            */
            draw.flipV =
                true;

            draw.color =
                glm::vec4(
                    1.0f
                );

            /*
                Сохраняем прежнее художественное усиление alpha.
            */
            draw.opacity =
                std::clamp(
                    cloudStyle.opacity *
                        2.6f,
                    0.0f,
                    0.72f
                );

            draw.blending = true;
            draw.premultipliedTarget = softLayerTargetActive;

            /*
                Растворение облаков возле горизонта.
            */
            draw.useHorizonFade =
                true;

            draw.horizonFadeStart =
                0.05f;

            draw.horizonFadeEnd =
                0.32f;

            draw.usePolarFade =
                false;

            m_planetGlobeMeshRenderer.render(
                draw
            );
        }



        m_hubBackend.endGpuStage();


        m_hubBackend.beginGpuStage(
            game::system_map::HubMapBackend::GpuStage::Atmosphere
        );



        drawHubMapPlanetAtmosphereStack(
            softPlanetCenterPx,
            softPlanetRadiusPx,
            atmosphereStyle,
            softLayerTargetActive
        );

        if (softLayerTargetActive)
        {
            /*
                Composite учитываем в Atmosphere-stage:
                это один bilinear fullscreen pass.
            */
            m_hubPlanetOverlayRenderer.endAndComposite();
        }

        m_hubBackend.endGpuStage();






    // -----------------------------------------------------------------
    // 6. Тактический оверлей оставляем.
    // -----------------------------------------------------------------
    const glm::dvec3 planetCenterLocal =
        hub.parentPlanetCenterLocalMeters;






        /*
            Полная круговая орбита выбранного хаба вокруг
            родительской планеты.

            Hub-local convention:
                X = prograde;
                Y = radial;
                Z = orbital normal.

            Поэтому орбита лежит в локальной плоскости XY.
        */
        glColor4f(
            m_hubVisuals.planetOrbitColor.r,
            m_hubVisuals.planetOrbitColor.g,
            m_hubVisuals.planetOrbitColor.b,
            m_hubVisuals.planetOrbitColor.a
        );

        drawHubMapCircleLocalXY(
            planetCenterLocal,
            hub.hubOrbitRadiusMeters,
            scale,
            centerPx,
            256
        );










    const glm::dvec3 surfacePoint =
        planetCenterLocal +
        glm::dvec3(
            0.0,
            hub.parentPlanetRadiusMeters,
            0.0
        );

    glColor4f(
        m_hubVisuals.hubOriginColor.r,
        m_hubVisuals.hubOriginColor.g,
        m_hubVisuals.hubOriginColor.b,
        m_hubVisuals.hubOriginColor.a
    );

    drawPlanetMapCross(
        activeLocalCameraSnapshot().project(surfacePoint),
        5.0f
    );

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}





void SystemMapRenderer::drawHubMapAdaptiveGrid(
    const Viewport& viewport,
    double scale,
    const glm::dvec2& centerPx,
    double worldRadiusMeters
)
{
    if (scale <= 0.0)
        return;

    const double visibleMeters =
        std::max(
            1000.0,
            worldRadiusMeters
        );

    double gridStep =
        100.0;

    while ((gridStep * scale * activeLocalCameraSnapshot().state.zoom) < 28.0)
        gridStep *= 2.0;

    while ((gridStep * scale * activeLocalCameraSnapshot().state.zoom) > 90.0 &&
           gridStep > 25.0)
    {
        gridStep *= 0.5;
    }

    const int gridN =
        static_cast<int>(
            std::ceil(
                visibleMeters / gridStep
            )
        ) + 2;

    glColor4f(
        m_hubVisuals.localGridColor.r,
        m_hubVisuals.localGridColor.g,
        m_hubVisuals.localGridColor.b,
        m_hubVisuals.localGridColor.a
    );

    for (int i = -gridN; i <= gridN; ++i)
    {
        const double v =
            static_cast<double>(i) *
            gridStep;

        drawPlanetMapLine(
            activeLocalCameraSnapshot().project(glm::dvec3(-gridN * gridStep, 0.0, v)),
            activeLocalCameraSnapshot().project(glm::dvec3( gridN * gridStep, 0.0, v))
        );

        drawPlanetMapLine(
            activeLocalCameraSnapshot().project(glm::dvec3(v, 0.0, -gridN * gridStep)),
            activeLocalCameraSnapshot().project(glm::dvec3(v, 0.0,  gridN * gridStep))
        );
    }

    // Главные оси плоскости хаба.
    glColor4f(
        m_hubVisuals.localGridAxisColor.r,
        m_hubVisuals.localGridAxisColor.g,
        m_hubVisuals.localGridAxisColor.b,
        m_hubVisuals.localGridAxisColor.a
    );

    drawPlanetMapLine(
        activeLocalCameraSnapshot().project(glm::dvec3(-gridN * gridStep, 0.0, 0.0)),
        activeLocalCameraSnapshot().project(glm::dvec3( gridN * gridStep, 0.0, 0.0))
    );

    drawPlanetMapLine(
        activeLocalCameraSnapshot().project(glm::dvec3(0.0, 0.0, -gridN * gridStep)),
        activeLocalCameraSnapshot().project(glm::dvec3(0.0, 0.0,  gridN * gridStep))
    );
}



glm::dvec3 SystemMapRenderer::visualSizeForHubShip(
    const world::celestial::HubMapShip& ship,
    double scale
) const
{
    // Реальный корабль можно держать маленьким, но на карте он не должен
    // превращаться в субатомную пыль. Поэтому размер физический,
    // но с минимальным экранным размером.
    glm::dvec3 physicalSizeMeters(
        90.0,
        35.0,
        160.0
    );

    if (ship.player)
    {
        physicalSizeMeters =
            glm::dvec3(
                130.0,
                50.0,
                210.0
            );
    }

    const double pixelsPerMeter =
        scale *
        activeLocalCameraSnapshot().state.zoom;

    if (pixelsPerMeter <= 0.0)
        return physicalSizeMeters;

    const double longestPx =
        std::max(
            physicalSizeMeters.x,
            std::max(
                physicalSizeMeters.y,
                physicalSizeMeters.z
            )
        ) * pixelsPerMeter;

    constexpr double minPlayerLongestPx = 18.0;
    constexpr double minOtherLongestPx = 11.0;

    const double minPx =
        ship.player
            ? minPlayerLongestPx
            : minOtherLongestPx;

    if (longestPx >= minPx)
        return physicalSizeMeters;

    const double factor =
        minPx /
        std::max(
            1.0,
            longestPx
        );

    return physicalSizeMeters * factor;
}








// Hub GPU profiling moved to HubMapBackend.cpp.

// Hub orchestration moved to HubMapBackend.cpp.

// ============================================================================
// Hub resource adapters
// ============================================================================


GLuint SystemMapRenderer::mapPreviewTextureForHubSnapshot(
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* asset =
        generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (!asset)
        return 0;

    return mapPreviewTextureForGeneratedAsset(*asset);
}



// ============================================================================
// Hub planet styles
// ============================================================================



GLuint SystemMapRenderer::globalNormalTextureForHubSnapshot(
    const world::celestial::HubMapSnapshot& hub
)
{
    const auto* asset =
        generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (!asset)
        return 0;

    return globalNormalTextureForGeneratedAsset(
        *asset
    );
}


SystemMapRenderer::HubPlanetAtmosphereStyle
SystemMapRenderer::hubPlanetAtmosphereStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    return atmosphereStyleForBody(
        hub.systemId,
        hub.parentBodyId,
        hub.parentBodyId,
        hub.parentEnvironmentPresetId
    );
}



render::celestial::HubSphericalGridStyle
SystemMapRenderer::hubSphericalGridStyleForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    render::celestial::HubSphericalGridStyle style;

    std::string bodyKey =
        normalizeGeneratedIdentityToken(
            lastGeneratedIdentityPathPart(
                hub.parentBodyId
            )
        );

    const auto* asset =
        generatedAssetForIdentity(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId
        );

    if (asset)
    {
        const std::string assetKey =
            normalizeGeneratedIdentityToken(
                asset->bodyFolderName
            );

        if (!assetKey.empty())
            bodyKey = assetKey;
    }

    // По умолчанию — холодная голубая сетка.
    style.radiusScale = 1.12;
    style.latitudeStepDeg =
        m_hubVisuals.sphericalGridLatitudeStepDeg;
    style.longitudeStepDeg =
        m_hubVisuals.sphericalGridLongitudeStepDeg;
    style.majorEvery =
        m_hubVisuals.sphericalGridMajorEvery;
    style.samplesPerLine =
        m_hubVisuals.sphericalGridSamplesPerLine;
    style.minorColor =
        m_hubVisuals.sphericalGridMinorColor;
    style.majorColor =
        m_hubVisuals.sphericalGridMajorColor;
    style.horizonFadeStart =
        m_hubVisuals.sphericalGridHorizonFadeStart;
    style.horizonFadeEnd =
        m_hubVisuals.sphericalGridHorizonFadeEnd;

    if (bodyKey == "mars" ||
        bodyKey == "ares")
    {
        style.minorColor =
            m_hubVisuals.marsGridMinorColor;
        style.majorColor =
            m_hubVisuals.marsGridMajorColor;
    }
    else if (bodyKey == "venus")
    {
        style.minorColor =
            m_hubVisuals.venusGridMinorColor;
        style.majorColor =
            m_hubVisuals.venusGridMajorColor;
    }
    else if (bodyKey == "titan")
    {
        style.minorColor =
            m_hubVisuals.titanGridMinorColor;
        style.majorColor =
            m_hubVisuals.titanGridMajorColor;
    }

    return style;
}


std::vector<
    render::celestial::ProceduralCloudStyle
>
SystemMapRenderer::hubPlanetCloudStylesForHub(
    const world::celestial::HubMapSnapshot& hub
) const
{
    auto styles =
        cloudStylesForBody(
            hub.systemId,
            hub.parentBodyId,
            hub.parentBodyId,
            hub.parentEnvironmentPresetId,
            hub.parentPlanetRadiusMeters,
            1024,
            512
        );

    /*
        Hub — режим просмотра огромной планеты.
        Здесь разрешаем усиленное preview-движение,
        но сохраняем относительные скорости слоёв.
    */
    for (auto& style : styles)
    {
        style.driftSpeed *=
            2.5f;
    }

    return styles;
}





