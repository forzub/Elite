/*
    Shared SystemMapRenderer implementation.

    Здесь находятся механизмы, общие для Galaxy, System,
    Details и Hub.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/






// ============================================================================
// Shared navigation coordinate overlay
// ============================================================================

void SystemMapRenderer::announceNavigationLevel(
    char mapPrefix,
    int level
)
{
    m_navigationLevelAnnouncement.text =
        "LEVEL " +
        std::string(1, mapPrefix) +
        std::to_string(level);

    m_navigationLevelAnnouncement.startedAtSeconds =
        glfwGetTime();
}


void SystemMapRenderer::drawNavigationCoordinateOverlay(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::PlayerNavigationState& nav
)
{
    using game::navigation::CubicNavigationCell;
    using game::navigation::GalaxyNavigationCell;
    using game::navigation::NavigationCellId;

    using render::navigation::
        NavigationCoordinateBlock;

    using render::navigation::
        NavigationCoordinateRole;

    std::vector<NavigationCoordinateBlock>
        blocks;

    blocks.reserve(3);

    const auto galaxyCellId =
        [&](const GalaxyNavigationCell& cell)
        {
            NavigationCellId id;

            id.frameId =
                m_galaxyNavigationGrid
                    .frame()
                    .id;

            id.level =
                cell.level;

            id.x =
                cell.index.x;

            id.y =
                cell.index.y;

            id.z =
                cell.index.z;

            return id;
        };

    const auto systemCellId =
        [&](const CubicNavigationCell& cell,
            const std::string& frameId)
        {
            NavigationCellId id;

            id.frameId =
                frameId;

            id.level =
                cell.level;

            id.x =
                cell.index.x;

            id.y =
                cell.index.y;

            id.z =
                cell.index.z;

            return id;
        };

    const std::string playerSystemFrameId =
        "system_barycentric:" +
        std::to_string(
            nav.currentSystemId
        );

    const std::string viewedSystemFrameId =
        m_systemNavigationGrid
            .frame()
            .id;

    /*
        Основное имя выбирается каталогом по текущей
        фракции и локали.

        Альтернативным сначала считаем название другой
        фракции. Если такого нет — название на другом языке.
    */
    const auto regionNames =
        [&](
            const NavigationCellId& cellId,
            int subdivision
        ) -> std::string
        {
            const auto primary =
                m_navigationRegionCatalog.resolve(
                    cellId,
                    subdivision,
                    m_navigationNamingFactionId,
                    m_navigationNamingLocale
                );

            if (!primary)
                return {};

            const game::navigation::
                NavigationRegionName*
                alternative = nullptr;

            const game::navigation::
                NavigationRegionName*
                fallback = nullptr;

            for (const auto& record :
                 m_navigationRegionCatalog.records())
            {
                if (!(record.cell ==
                      primary->sourceCell))
                {
                    continue;
                }

                for (const auto& name :
                     record.names)
                {
                    if (name.name.empty() ||
                        name.name ==
                            primary->value.name)
                    {
                        continue;
                    }

                    if (!fallback)
                    {
                        fallback =
                            &name;
                    }

                    if (name.factionId !=
                        primary->value.factionId)
                    {
                        alternative =
                            &name;

                        break;
                    }
                }

                break;
            }

            if (!alternative)
            {
                alternative =
                    fallback;
            }

            std::string result =
                primary->value.name;

            if (alternative &&
                alternative->name != result)
            {
                result +=
                    " / ";

                result +=
                    alternative->name;
            }

            return result;
        };

    const auto galaxyAddress =
        [&](const GalaxyNavigationCell& cell)
        {
            return
                game::navigation::
                    formatNavigationAddress(
                        m_navigationCoordinateFormat,
                        "G",
                        cell.level,
                        cell.index,
                        m_galaxyNavigationGrid.subdivision()
                    );
        };

    const auto systemAddress =
        [&](const CubicNavigationCell& cell)
        {
            return
            game::navigation::
                formatHierarchicalAddress(
                    "S",
                    cell.level,
                    cell.index,
                    m_systemNavigationGrid.subdivision()
                );
        };

    /*
        Ищем положение центра текущей системы
        в Galaxy-координатах.
    */
    const auto currentSystem =
        std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& system)
            {
                return
                    system.id ==
                    nav.currentSystemId;
            }
        );

    bool hasCurrentSystem =
        currentSystem !=
        galaxy.systems.end();

    GalaxyNavigationCell
        currentSystemGalaxyCell;

    GalaxyNavigationCell
        playerGalaxyCell;

    const glm::dvec3 playerPositionLy =
        playerGalaxyPositionLy(
            galaxy,
            nav,
            hasCurrentSystem
        );

    const int terminalGalaxyLevel =
        m_galaxyNavigationGrid.maximumLevel();

    const auto playerGalaxyIndex =
        m_galaxyNavigationGrid
            .nearestIndexForPositionLy(
                playerPositionLy,
                terminalGalaxyLevel
            );

    playerGalaxyCell =
        m_galaxyNavigationGrid.cell(
            playerGalaxyIndex,
            terminalGalaxyLevel
        );

    if (hasCurrentSystem)
    {
        const auto currentSystemIndex =
            m_galaxyNavigationGrid
                .nearestIndexForPositionLy(
                    currentSystem->positionLy,
                    terminalGalaxyLevel
                );

            currentSystemGalaxyCell =
                m_galaxyNavigationGrid.cell(
                    currentSystemIndex,
                    terminalGalaxyLevel
                );
    }

    /*
        Galaxy-родитель открытой System-карты.

        Для известной системы это координата звезды.
        Для пустого сектора — центр выбранного Galaxy-куба,
        сохранённый в synthetic SystemMapSnapshot.
    */
    const auto viewedSystemGalaxyIndex =
        m_galaxyNavigationGrid
            .nearestIndexForPositionLy(
                system.systemPositionLy,
                terminalGalaxyLevel
            );

    const GalaxyNavigationCell
        viewedSystemGalaxyCell =
            m_galaxyNavigationGrid.cell(
                viewedSystemGalaxyIndex,
                terminalGalaxyLevel
            );

    /*
        Полный адрес игрока всегда строится до
        терминального System-уровня S6.
    */
    const int terminalSystemLevel =
        m_systemNavigationGrid
            .definition()
            .maximumLevel;

    const auto playerSystemIndex =
        m_systemNavigationGrid
            .nearestIndexForPosition(
                nav.systemLocalAu,
                terminalSystemLevel
            );

    const CubicNavigationCell
        playerSystemCell =
            m_systemNavigationGrid.cell(
                playerSystemIndex,
                terminalSystemLevel
            );

    NavigationCoordinateBlock
        playerBlock;

    playerBlock.role =
        NavigationCoordinateRole::Player;

    playerBlock.title =
        "PLAYER";

    /*
        В Galaxy показываем имя межзвёздного сектора.
        В остальных режимах — имя сектора текущей системы.
    */
    if (m_mode == Mode::Galaxy)
    {
        playerBlock.regionNames =
            regionNames(
                galaxyCellId(
                    playerGalaxyCell
                ),
                m_galaxyNavigationGrid.subdivision()
            );
    }
    else
    {
        playerBlock.regionNames =
            regionNames(
                systemCellId(
                    playerSystemCell,
                    playerSystemFrameId
                ),
                m_systemNavigationGrid.subdivision()
            );

        if (playerBlock.regionNames.empty() &&
            hasCurrentSystem)
        {
            playerBlock.regionNames =
                regionNames(
                    galaxyCellId(
                        playerGalaxyCell
                    ),
                    m_galaxyNavigationGrid.subdivision()
                );
        }
    }

    if (m_mode == Mode::Galaxy ||
        hasCurrentSystem)
    {
        playerBlock.addressLines.push_back(
            galaxyAddress(
                playerGalaxyCell
            )
        );
    }

    if (m_mode != Mode::Galaxy)
    {
        playerBlock.addressLines.push_back(
            systemAddress(
                playerSystemCell
            )
        );
    }

    blocks.push_back(
        std::move(
            playerBlock
        )
    );

    /*
        Выбранный и hover-куб показываем только
        в режимах, где кубы реально отображаются.
    */
    if (m_mode == Mode::Galaxy &&
        m_galaxyNavigationGrid.enabled())
    {
        const GalaxyNavigationCell selected =
            m_galaxyNavigationGrid.hasSelectedCell()
                ? m_galaxyNavigationGrid.selectedCell()
                : m_galaxyNavigationGrid.anchorCell();

        NavigationCoordinateBlock selectedBlock;

        selectedBlock.role =
            NavigationCoordinateRole::Selected;

        selectedBlock.title =
            "SELECTED";

        selectedBlock.regionNames =
            regionNames(
                galaxyCellId(
                    selected
                ),
                m_galaxyNavigationGrid.subdivision()
            );

        selectedBlock.addressLines.push_back(
            galaxyAddress(
                selected
            )
        );

        blocks.push_back(
            std::move(
                selectedBlock
            )
        );

        if (m_galaxyNavigationGrid.hasHoveredCell())
        {
            const GalaxyNavigationCell hovered =
                m_galaxyNavigationGrid.hoveredCell();

            const bool sameAsSelected =
                hovered.level ==
                    selected.level &&
                hovered.index ==
                    selected.index;

            if (!sameAsSelected)
            {
                NavigationCoordinateBlock
                    hoveredBlock;

                hoveredBlock.role =
                    NavigationCoordinateRole::Hovered;

                hoveredBlock.title =
                    "CURSOR";

                hoveredBlock.regionNames =
                    regionNames(
                        galaxyCellId(
                            hovered
                        ),
                        m_galaxyNavigationGrid.subdivision()
                    );

                hoveredBlock.addressLines.push_back(
                    galaxyAddress(
                        hovered
                    )
                );

                blocks.push_back(
                    std::move(
                        hoveredBlock
                    )
                );
            }
        }
    }
    else if (
        m_mode == Mode::System &&
        m_systemNavigationGrid.enabled()
    )
    {
        const CubicNavigationCell selected =
            m_systemNavigationGrid.hasSelectedCell()
                ? m_systemNavigationGrid.selectedCell()
                : m_systemNavigationGrid.anchorCell();

        NavigationCoordinateBlock selectedBlock;

        selectedBlock.role =
            NavigationCoordinateRole::Selected;

        selectedBlock.title =
            "SELECTED";

        selectedBlock.regionNames =
            regionNames(
                systemCellId(
                    selected,
                    viewedSystemFrameId
                ),
                m_systemNavigationGrid.subdivision()
            );

        /*
            System-адрес без Galaxy-родителя не является
            глобально уникальным.
        */
        if (system.systemId != -1)
        {
            selectedBlock.addressLines.push_back(
                galaxyAddress(
                    viewedSystemGalaxyCell
                )
            );
        }

        selectedBlock.addressLines.push_back(
            systemAddress(
                selected
            )
        );

        blocks.push_back(
            std::move(
                selectedBlock
            )
        );

        if (m_systemNavigationGrid.hasHoveredCell())
        {
            const CubicNavigationCell hovered =
                m_systemNavigationGrid.hoveredCell();

            const bool sameAsSelected =
                hovered.level ==
                    selected.level &&
                hovered.index ==
                    selected.index;

            if (!sameAsSelected)
            {
                NavigationCoordinateBlock
                    hoveredBlock;

                hoveredBlock.role =
                    NavigationCoordinateRole::Hovered;

                hoveredBlock.title =
                    "CURSOR";

                hoveredBlock.regionNames =
                    regionNames(
                        systemCellId(
                            hovered,
                            viewedSystemFrameId
                        ),
                        m_systemNavigationGrid.subdivision()
                    );

                if (system.systemId != -1)
                {
                    hoveredBlock.addressLines.push_back(
                        galaxyAddress(
                            viewedSystemGalaxyCell
                        )
                    );
                }

                hoveredBlock.addressLines.push_back(
                    systemAddress(
                        hovered
                    )
                );

                blocks.push_back(
                    std::move(
                        hoveredBlock
                    )
                );
            }
        }
    }

    std::string footerText;

    if (m_mode == Mode::Galaxy)
    {
        const int level =
            m_galaxyNavigationGrid.level();

        const double edgeLy =
            m_galaxyNavigationGrid.cellSizeLy(level);

        std::ostringstream footer;
        footer << "GALAXY G" << level
               << " · EDGE ";

        const double edgeAu =
            edgeLy *
            game::navigation::SystemNavigationGrid::AuPerLightYear;

        if (edgeLy >= 0.1)
        {
            footer << std::fixed << std::setprecision(3)
                   << edgeLy << " ly";
        }
        else
        {
            footer << std::fixed << std::setprecision(2)
                   << edgeAu << " AU";
        }

        footer << " · FORMAT "
               << game::navigation::navigationCoordinateFormatName(
                    m_navigationCoordinateFormat
                  )
               << " [F9]";

        footerText = footer.str();
    }
    else if (m_mode == Mode::System &&
             m_systemNavigationGrid.enabled())
    {
        const int level =
            m_systemNavigationGrid.level();

        const double edgeAu =
            m_systemNavigationGrid.cellSize(level);

        const double edgeKm =
            edgeAu * AU_KM;

        std::ostringstream footer;
        footer << "SYSTEM S" << level
               << " · EDGE ";

        if (edgeAu >= 0.01)
        {
            footer << std::fixed << std::setprecision(3)
                   << edgeAu << " AU";
        }
        else if (edgeKm >= 1000.0)
        {
            footer << std::fixed << std::setprecision(0)
                   << edgeKm << " km";
        }
        else
        {
            footer << std::fixed << std::setprecision(2)
                   << edgeKm << " km";
        }

        footer << " · FORMAT "
               << game::navigation::navigationCoordinateFormatName(
                    m_navigationCoordinateFormat
                  )
               << " [F9]";

        footerText = footer.str();
    }

    float levelAnnouncementAlpha = 0.0f;

    if ((m_mode == Mode::Galaxy ||
         m_mode == Mode::System) &&
        !m_navigationLevelAnnouncement.text.empty() &&
        m_navigationLevelAnnouncement.startedAtSeconds >= 0.0)
    {
        const double ageSeconds =
            glfwGetTime() -
            m_navigationLevelAnnouncement.startedAtSeconds;

        const double durationSeconds =
            std::max(
                0.1,
                m_navigationLevelAnnouncement.durationSeconds
            );

        if (ageSeconds >= 0.0 &&
            ageSeconds < durationSeconds)
        {
            /*
                Briefly keep the new level fully readable, then fade it
                smoothly for the remainder of the 1.35 second lifetime.
            */
            const double holdSeconds =
                std::min(
                    0.18,
                    durationSeconds * 0.20
                );

            const double fadeT =
                std::clamp(
                    (ageSeconds - holdSeconds) /
                        std::max(
                            0.001,
                            durationSeconds - holdSeconds
                        ),
                    0.0,
                    1.0
                );

            const double smoothFade =
                fadeT *
                fadeT *
                (3.0 - 2.0 * fadeT);

            levelAnnouncementAlpha =
                static_cast<float>(
                    1.0 - smoothFade
                );
        }
        else if (ageSeconds >= durationSeconds)
        {
            m_navigationLevelAnnouncement.text.clear();
            m_navigationLevelAnnouncement.startedAtSeconds = -1.0;
        }
    }

    m_navigationCoordinateOverlay.draw(
        viewport,
        blocks,
        footerText,
        m_navigationLevelAnnouncement.text,
        levelAnnouncementAlpha
    );
}







// ============================================================================
// Map transitions
// ============================================================================



bool SystemMapRenderer::beginMapTransition(
    const MapTransitionSpec& spec,
    std::function<void()> applyNewState
)
{
    return m_mapTransition.begin(
        spec,
        std::move(applyNewState)
    );
}



void SystemMapRenderer::ensureMapTransitionSnapshot(
    const Viewport& viewport
)
{
    if (viewport.width <= 0 ||
        viewport.height <= 0)
    {
        m_mapTransitionSnapshotReady =
            false;

        return;
    }

    /*
        Если framebuffer и текстура уже созданы под текущий
        размер map viewport, пересоздавать их не нужно.
    */
    if (m_mapTransitionSnapshotFramebuffer != 0 &&
        m_mapTransitionSnapshotTexture != 0 &&
        m_mapTransitionSnapshotReady &&
        m_mapTransitionSnapshotWidth == viewport.width &&
        m_mapTransitionSnapshotHeight == viewport.height)
    {
        return;
    }

    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;

    GLint previousActiveTexture = 0;
    GLint previousTexture = 0;

    glGetIntegerv(
        GL_READ_FRAMEBUFFER_BINDING,
        &previousReadFramebuffer
    );

    glGetIntegerv(
        GL_DRAW_FRAMEBUFFER_BINDING,
        &previousDrawFramebuffer
    );

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previousActiveTexture
    );

    glActiveTexture(
        GL_TEXTURE0
    );

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture
    );

    if (m_mapTransitionSnapshotTexture == 0)
    {
        glGenTextures(
            1,
            &m_mapTransitionSnapshotTexture
        );
    }

    if (m_mapTransitionSnapshotFramebuffer == 0)
    {
        glGenFramebuffers(
            1,
            &m_mapTransitionSnapshotFramebuffer
        );
    }

    m_mapTransitionSnapshotReady =
        false;

    /*
        Обычная single-sample RGBA8-текстура.
        Именно в неё будет разрешаться MSAA framebuffer карты.
    */
    glBindTexture(
        GL_TEXTURE_2D,
        m_mapTransitionSnapshotTexture
    );

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        viewport.width,
        viewport.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_S,
        GL_CLAMP_TO_EDGE
    );

    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_WRAP_T,
        GL_CLAMP_TO_EDGE
    );

    /*
        Прикрепляем текстуру к отдельному framebuffer.
    */
    glBindFramebuffer(
        GL_FRAMEBUFFER,
        m_mapTransitionSnapshotFramebuffer
    );

    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_mapTransitionSnapshotTexture,
        0
    );

    glDrawBuffer(
        GL_COLOR_ATTACHMENT0
    );

    glReadBuffer(
        GL_COLOR_ATTACHMENT0
    );

    const GLenum framebufferStatus =
        glCheckFramebufferStatus(
            GL_FRAMEBUFFER
        );

    if (framebufferStatus ==
        GL_FRAMEBUFFER_COMPLETE)
    {
        m_mapTransitionSnapshotWidth =
            viewport.width;

        m_mapTransitionSnapshotHeight =
            viewport.height;

        m_mapTransitionSnapshotReady =
            true;
    }
    else
    {
        m_mapTransitionSnapshotWidth = 0;
        m_mapTransitionSnapshotHeight = 0;

        std::cerr
            << "[MapTransition] snapshot framebuffer incomplete: 0x"
            << std::hex
            << static_cast<unsigned int>(
                framebufferStatus
            )
            << std::dec
            << '\n';
    }

    /*
        Восстанавливаем framebuffer, в который в данный момент
        рисуется карта. Обычно это Renderer::m_sceneFramebuffer.
    */
    glBindFramebuffer(
        GL_READ_FRAMEBUFFER,
        static_cast<GLuint>(
            previousReadFramebuffer
        )
    );

    glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER,
        static_cast<GLuint>(
            previousDrawFramebuffer
        )
    );

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(
            previousTexture
        )
    );

    glActiveTexture(
        static_cast<GLenum>(
            previousActiveTexture
        )
    );
}



void SystemMapRenderer::captureMapTransitionSnapshot(
    const Viewport& viewport
)
{
    ensureMapTransitionSnapshot(
        viewport
    );

    if (!m_mapTransitionSnapshotReady ||
        m_mapTransitionSnapshotFramebuffer == 0 ||
        m_mapTransitionSnapshotTexture == 0)
    {
        return;
    }

    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;

    glGetIntegerv(
        GL_READ_FRAMEBUFFER_BINDING,
        &previousReadFramebuffer
    );

    glGetIntegerv(
        GL_DRAW_FRAMEBUFFER_BINDING,
        &previousDrawFramebuffer
    );

    const GLboolean scissorWasEnabled =
        glIsEnabled(
            GL_SCISSOR_TEST
        );

    /*
        glBlitFramebuffer учитывает scissor destination.
        Текущий scissor задан в глобальных координатах окна,
        а snapshot framebuffer начинается с 0/0.

        Поэтому на время resolve scissor нужно отключить.
    */
    glDisable(
        GL_SCISSOR_TEST
    );

    /*
        READ остаётся текущим MSAA framebuffer карты.
        DRAW переключается на single-sample snapshot framebuffer.
    */
    glBindFramebuffer(
        GL_READ_FRAMEBUFFER,
        static_cast<GLuint>(
            previousReadFramebuffer
        )
    );

    glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER,
        m_mapTransitionSnapshotFramebuffer
    );

    /*
        Одновременно:
        - копируем старый кадр;
        - разрешаем MSAA;
        - переносим map viewport в текстуру размером width×height.
    */
    glBlitFramebuffer(
        viewport.x,
        viewport.y,
        viewport.x + viewport.width,
        viewport.y + viewport.height,

        0,
        0,
        viewport.width,
        viewport.height,

        GL_COLOR_BUFFER_BIT,
        GL_NEAREST
    );

    /*
        Возвращаем framebuffer карты.
    */
    glBindFramebuffer(
        GL_READ_FRAMEBUFFER,
        static_cast<GLuint>(
            previousReadFramebuffer
        )
    );

    glBindFramebuffer(
        GL_DRAW_FRAMEBUFFER,
        static_cast<GLuint>(
            previousDrawFramebuffer
        )
    );

    if (scissorWasEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);
}



void SystemMapRenderer::drawMapTransitionSnapshot(
    const Viewport& viewport,
    float alpha
)
{
    if (!m_bgShader ||
        !m_bgVao ||
        !m_mapTransitionSnapshotReady ||
        !m_mapTransitionSnapshotFramebuffer ||
        !m_mapTransitionSnapshotTexture ||
        alpha <= 0.0f)
    {
        return;
    }

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

    GLboolean depthWasEnabled =
        glIsEnabled(GL_DEPTH_TEST);

    GLboolean blendWasEnabled =
        glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);

    glBlendFunc(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA
    );

    GLint previousActiveTexture = 0;
    GLint previousTexture = 0;

    glGetIntegerv(
        GL_ACTIVE_TEXTURE,
        &previousActiveTexture
    );

    glActiveTexture(GL_TEXTURE0);

    glGetIntegerv(
        GL_TEXTURE_BINDING_2D,
        &previousTexture
    );

    glBindTexture(
        GL_TEXTURE_2D,
        m_mapTransitionSnapshotTexture
    );

    glUseProgram(m_bgShader);

    const GLint passLoc =
        glGetUniformLocation(m_bgShader, "uPass");

    const GLint samplerLoc =
        glGetUniformLocation(
            m_bgShader,
            "uTransitionSnapshot"
        );

    const GLint alphaLoc =
        glGetUniformLocation(
            m_bgShader,
            "uTransitionAlpha"
        );

    if (passLoc >= 0)
        glUniform1i(passLoc, 1);

    if (samplerLoc >= 0)
        glUniform1i(samplerLoc, 0);

    if (alphaLoc >= 0)
        glUniform1f(alphaLoc, alpha);

    glBindVertexArray(m_bgVao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);

    if (passLoc >= 0)
        glUniform1i(passLoc, 0);

    glUseProgram(0);

    glBindTexture(
        GL_TEXTURE_2D,
        static_cast<GLuint>(previousTexture)
    );

    glActiveTexture(
        static_cast<GLenum>(previousActiveTexture)
    );

    if (depthWasEnabled)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}




// ============================================================================
// Shared navigation rendering
// ============================================================================




void SystemMapRenderer::addNavigationCubeEdges(
    const glm::vec3& center,
    const glm::vec3& halfAxisX,
    const glm::vec3& halfAxisY,
    const glm::vec3& halfAxisZ,
    const glm::vec4& color
)
{
    const std::array<glm::vec3, 8> corners =
    {
        center - halfAxisX - halfAxisY - halfAxisZ,
        center + halfAxisX - halfAxisY - halfAxisZ,
        center - halfAxisX + halfAxisY - halfAxisZ,
        center + halfAxisX + halfAxisY - halfAxisZ,
        center - halfAxisX - halfAxisY + halfAxisZ,
        center + halfAxisX - halfAxisY + halfAxisZ,
        center - halfAxisX + halfAxisY + halfAxisZ,
        center + halfAxisX + halfAxisY + halfAxisZ
    };

    static constexpr int edges[12][2] =
    {
        {0, 1}, {2, 3}, {4, 5}, {6, 7},
        {0, 2}, {1, 3}, {4, 6}, {5, 7},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    const glm::vec4 weakColor(
        color.r,
        color.g,
        color.b,
        color.a * 0.38f
    );

    for (const auto& edge : edges)
    {
        const glm::vec3 a = corners[edge[0]];
        const glm::vec3 b = corners[edge[1]];
        const glm::vec3 d = b - a;

        addLine(a, a + d * 0.18f, color);
        addLine(a + d * 0.18f, a + d * 0.32f, weakColor);
        addLine(a + d * 0.68f, a + d * 0.82f, weakColor);
        addLine(a + d * 0.82f, b, color);
    }
}



// ============================================================================
// Shared Details / Hub input
// ============================================================================



void SystemMapRenderer::handleDetailAndHubInput(
    const Viewport& vp,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown
)
{

    // =========================================================
    // DETAILS MODE INPUT: PLANET + HUB
    // =========================================================

        DetailCamera& camera = activeDetailCamera();
        const DetailControlSettings& controls = activeDetailControls();
        double wheel = 0.0;

        if (inside &&
            m_pendingScrollY != 0.0)
        {
            wheel = m_pendingScrollY;

            m_pendingScrollY = 0.0;
        }

        bool leftStartedThisFrame = false;
        bool rightStartedThisFrame = false;

        if (inside &&
            leftDown &&
            !camera.rotating)
        {
            leftStartedThisFrame = true;

            camera.rotating = true;

            camera.lastMouseX = mx;

            camera.lastMouseY = my;

            if (m_mode == Mode::Hub)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const glm::dvec2 mousePx(
                    localMx,
                    localMy
                );

                const double safeScale =
                    std::max(
                        0.000001,
                        m_lastHubMapScale
                    );

                glm::dvec3 pickedPivotLocalMeters(
                    0.0,
                    0.0,
                    0.0
                );

                const bool pickedObject =
                    pickHubMapOrbitPivot(
                        mousePx,
                        pickedPivotLocalMeters
                    );

                if (pickedObject)
                {
                    m_hubMapOrbitPivotLocalMeters =
                        pickedPivotLocalMeters;
                }
                else
                {
                    m_hubMapOrbitPivotLocalMeters =
                        hubMapUnprojectCursorToLocal(
                            mousePx,
                            safeScale,
                            centerPx
                        );
                }

                m_hubMapOrbitPivotScreenPx =
                    hubMapProject(
                        m_hubMapOrbitPivotLocalMeters,
                        safeScale,
                        centerPx
                    );
            }
        }

        if (!leftDown)
        {
            camera.rotating = false;
        }

        if (inside &&
            rightDown &&
            !camera.panning)
        {
            rightStartedThisFrame = true;
            camera.panning = true;
            camera.lastMouseX = mx;
            camera.lastMouseY = my;
        }

        if (!rightDown)
        {
            camera.panning = false;
        }

        const double dx = mx - camera.lastMouseX;
        const double dy = my - camera.lastMouseY;

        if (camera.rotating &&
            leftDown &&
            !leftStartedThisFrame)
        {
            if (m_mode == Mode::Hub)
            {
                camera.yaw +=
                    dx *
                    controls.rotateSensitivity *
                    0.65;

                camera.pitch +=
                    dy *
                    controls.rotateSensitivity *
                    0.65;

                camera.yaw =
                    wrapAngleRadD(
                        camera.yaw
                    );

                // 0.12: низкий orbital horizon view.
                // 1.20: почти взгляд вниз.
                // Отрицательный pitch запрещён, иначе визуально камера
                // оказывается "изнутри планеты".
                camera.pitch =
                    std::clamp(
                        camera.pitch,
                        0.12,
                        1.20
                    );
            }
            else
            {
                camera.yaw += dx * controls.rotateSensitivity;
                camera.pitch += dy * controls.rotateSensitivity;

                camera.yaw =
                    wrapAngleRadD(
                        camera.yaw
                    );

                camera.pitch =
                    wrapAngleRadD(
                        camera.pitch
                    );
            }
        }

        if (camera.panning &&
            rightDown &&
            !rightStartedThisFrame)
        {
            camera.pan.x += dx;
            camera.pan.y += dy;
        }

        if (std::abs(wheel) > 0.001)
        {
            const double oldZoom = camera.zoom;
            const double zoomStep = controls.zoomStep;


            const double newZoom =
                std::clamp(
                    oldZoom *
                    std::pow(zoomStep, wheel),
                    controls.minZoom,
                    controls.maxZoom
                );

            if (std::abs(newZoom - oldZoom) > 0.000001)
            {
                const glm::dvec2 centerPx(
                    static_cast<double>(vp.width) * 0.5,
                    static_cast<double>(vp.height) * 0.5
                );

                const glm::dvec2 mousePx(
                    localMx,
                    localMy
                );

                const double zoomFactor =
                    newZoom /
                    oldZoom;

                camera.pan =
                    mousePx -
                    centerPx -
                    (mousePx - centerPx - camera.pan) *
                    zoomFactor;

                camera.zoom = newZoom;
            }
        }

        if (m_mode == Mode::Hub)
{
    camera.pitch =
        std::clamp(
            camera.pitch,
            -0.95,
            0.95
        );

    const double maxPanX =
        static_cast<double>(vp.width) *
        0.55;

    const double maxPanY =
        static_cast<double>(vp.height) *
        0.45;

    camera.pan.x =
        std::clamp(
            camera.pan.x,
            -maxPanX,
            maxPanX
        );

    camera.pan.y =
        std::clamp(
            camera.pan.y,
            -maxPanY,
            maxPanY
        );
}

        if (m_mode == Mode::Hub)
        {
            camera.pitch =
                std::clamp(
                    camera.pitch,
                    0.12,
                    1.20
                );
        }

        camera.lastMouseX = mx;
        camera.lastMouseY = my;

}
