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
    const auto& ui =
        m_navigationCoordinateOverlay.textProfile();

    m_navigationLevelAnnouncement.text =
        ui.level + " " +
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

    const auto& ui =
        m_navigationCoordinateOverlay.textProfile();

    const auto coordinateFormat =
        game::navigation::CoordinateDisplayService::instance().format();

    const auto formatDisplayName = [&]() -> const std::string&
    {
        switch (coordinateFormat)
        {
            case game::navigation::CoordinateDisplayFormat::Axis:
                return ui.axisFormat;

            case game::navigation::CoordinateDisplayFormat::PackedBase32:
                return ui.packedFormat;

            case game::navigation::CoordinateDisplayFormat::Hierarchical:
            default:
                return ui.hierarchicalFormat;
        }
    };

    std::vector<NavigationCoordinateBlock>
        blocks;

    blocks.reserve(3);

    const auto galaxyCellId =
        [&](const GalaxyNavigationCell& cell)
        {
            NavigationCellId id;

            id.frameId =
                m_galaxyView.state().navigationGrid
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
        m_systemView.state().navigationGrid
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

            int alternativeLanguageScore = -1;

            const auto baseLocale =
                [](const std::string& locale)
                {
                    const std::size_t split =
                        locale.find_first_of("-_");
                    return split == std::string::npos
                        ? locale
                        : locale.substr(0, split);
                };

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
                        name.name == primary->value.name ||
                        name.factionId == primary->value.factionId)
                    {
                        continue;
                    }

                    int languageScore = -1;
                    if (name.language == m_navigationNamingLocale)
                    {
                        languageScore = 30;
                    }
                    else if (baseLocale(name.language) ==
                             baseLocale(m_navigationNamingLocale))
                    {
                        languageScore = 20;
                    }
                    else if (name.language == "en")
                    {
                        languageScore = 10;
                    }

                    if (languageScore > alternativeLanguageScore)
                    {
                        alternative = &name;
                        alternativeLanguageScore = languageScore;
                    }
                }

                break;
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
                    formatNavigationAddressLine(
                        coordinateFormat,
                        formatDisplayName(),
                        "G",
                        cell.level,
                        cell.index,
                        m_galaxyView.state().navigationGrid.subdivision()
                    );
        };

    const auto systemAddress =
        [&](const CubicNavigationCell& cell)
        {
            return
            game::navigation::
                formatNavigationAddressLine(
                    coordinateFormat,
                    formatDisplayName(),
                    "S",
                    cell.level,
                    cell.index,
                    m_systemView.state().navigationGrid.subdivision()
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
        m_galaxyView.playerPositionLy(
            galaxy,
            nav,
            hasCurrentSystem
        );

    const int terminalGalaxyLevel =
        m_galaxyView.state().navigationGrid.maximumLevel();

    const auto playerGalaxyIndex =
        m_galaxyView.state().navigationGrid
            .nearestIndexForPositionLy(
                playerPositionLy,
                terminalGalaxyLevel
            );

    playerGalaxyCell =
        m_galaxyView.state().navigationGrid.cell(
            playerGalaxyIndex,
            terminalGalaxyLevel
        );

    if (hasCurrentSystem)
    {
        const auto currentSystemIndex =
            m_galaxyView.state().navigationGrid
                .nearestIndexForPositionLy(
                    currentSystem->positionLy,
                    terminalGalaxyLevel
                );

            currentSystemGalaxyCell =
                m_galaxyView.state().navigationGrid.cell(
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
        m_galaxyView.state().navigationGrid
            .nearestIndexForPositionLy(
                system.systemPositionLy,
                terminalGalaxyLevel
            );

    const GalaxyNavigationCell
        viewedSystemGalaxyCell =
            m_galaxyView.state().navigationGrid.cell(
                viewedSystemGalaxyIndex,
                terminalGalaxyLevel
            );

    /*
        Полный адрес игрока всегда строится до
        терминального System-уровня S5.
    */
    const int terminalSystemLevel =
        m_systemView.state().navigationGrid
            .definition()
            .maximumLevel;

    const auto playerSystemIndex =
        m_systemView.state().navigationGrid
            .nearestIndexForPosition(
                nav.systemLocalAu,
                terminalSystemLevel
            );

    const CubicNavigationCell
        playerSystemCell =
            m_systemView.state().navigationGrid.cell(
                playerSystemIndex,
                terminalSystemLevel
            );

    NavigationCoordinateBlock
        playerBlock;

    playerBlock.role =
        NavigationCoordinateRole::Player;

    playerBlock.title =
        ui.player;

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
                m_galaxyView.state().navigationGrid.subdivision()
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
                m_systemView.state().navigationGrid.subdivision()
            );

        if (playerBlock.regionNames.empty() &&
            hasCurrentSystem)
        {
            playerBlock.regionNames =
                regionNames(
                    galaxyCellId(
                        playerGalaxyCell
                    ),
                    m_galaxyView.state().navigationGrid.subdivision()
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
        m_galaxyView.state().navigationGrid.enabled())
    {
        const GalaxyNavigationCell selected =
            m_galaxyView.state().navigationGrid.hasSelectedCell()
                ? m_galaxyView.state().navigationGrid.selectedCell()
                : m_galaxyView.state().navigationGrid.anchorCell();

        NavigationCoordinateBlock selectedBlock;

        selectedBlock.role =
            NavigationCoordinateRole::Selected;

        selectedBlock.title =
            ui.selected;

        selectedBlock.regionNames =
            regionNames(
                galaxyCellId(
                    selected
                ),
                m_galaxyView.state().navigationGrid.subdivision()
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

        if (m_galaxyView.state().navigationGrid.hasHoveredCell())
        {
            const GalaxyNavigationCell hovered =
                m_galaxyView.state().navigationGrid.hoveredCell();

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
                    ui.cursor;

                hoveredBlock.regionNames =
                    regionNames(
                        galaxyCellId(
                            hovered
                        ),
                        m_galaxyView.state().navigationGrid.subdivision()
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
        m_systemView.state().navigationGrid.enabled()
    )
    {
        const CubicNavigationCell selected =
            m_systemView.state().navigationGrid.hasSelectedCell()
                ? m_systemView.state().navigationGrid.selectedCell()
                : m_systemView.state().navigationGrid.anchorCell();

        NavigationCoordinateBlock selectedBlock;

        selectedBlock.role =
            NavigationCoordinateRole::Selected;

        selectedBlock.title =
            ui.selected;

        selectedBlock.regionNames =
            regionNames(
                systemCellId(
                    selected,
                    viewedSystemFrameId
                ),
                m_systemView.state().navigationGrid.subdivision()
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

        if (m_systemView.state().navigationGrid.hasHoveredCell())
        {
            const CubicNavigationCell hovered =
                m_systemView.state().navigationGrid.hoveredCell();

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
                    ui.cursor;

                hoveredBlock.regionNames =
                    regionNames(
                        systemCellId(
                            hovered,
                            viewedSystemFrameId
                        ),
                        m_systemView.state().navigationGrid.subdivision()
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
            m_galaxyView.state().navigationGrid.level();

        const double edgeLy =
            m_galaxyView.state().navigationGrid.cellSizeLy(level);

        std::ostringstream footer;
        footer << ui.galaxy << " G" << level
               << " · " << ui.edge << " ";

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

        footer << " · " << ui.format << " "
               << formatDisplayName()
               << " [CTRL+F11]";

        footerText = footer.str();
    }
    else if (m_mode == Mode::System &&
             m_systemView.state().navigationGrid.enabled())
    {
        const int level =
            m_systemView.state().navigationGrid.level();

        const double edgeAu =
            m_systemView.state().navigationGrid.cellSize(level);

        const double edgeKm =
            edgeAu * AU_KM;

        std::ostringstream footer;
        footer << ui.system << " S" << level
               << " · " << ui.edge << " ";

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

        footer << " · " << ui.format << " "
               << formatDisplayName()
               << " [CTRL+F11]";

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
        levelAnnouncementAlpha,
        m_mode == Mode::Galaxy ||
            m_mode == Mode::System,
        m_navigationLevelZeroButtonHovered,
        m_mode == Mode::System,
        m_navigationTrackButtonHovered,
        m_systemView.state().selectedBodyTrackingEnabled,
        !m_systemView.state().selectedBodyId.empty()
    );
}







void SystemMapRenderer::toggleSelectedBodyTracking()
{
    auto& state =
        m_systemView.state();

    if (m_mode != Mode::System ||
        state.selectedBodyId.empty())
    {
        return;
    }

    state.selectedBodyTrackingEnabled =
        !state.selectedBodyTrackingEnabled;

    state.trackedBodyId.clear();
    state.trackedBodyPositionValid = false;

    /*
        A camera flight owns the target absolutely. Cancel it when entering
        TRACK so the following frame cannot fight the flight interpolator.
    */
    if (state.selectedBodyTrackingEnabled)
        state.cameraFlight.active = false;
}


void SystemMapRenderer::updateSelectedBodyTracking(
    const game::system_map::SystemMapPresentation& presentation
)
{
    auto& state =
        m_systemView.state();

    if (!state.selectedBodyTrackingEnabled)
    {
        state.trackedBodyId.clear();
        state.trackedBodyPositionValid = false;
        return;
    }

    if (state.selectedBodyId.empty())
    {
        state.selectedBodyTrackingEnabled = false;
        state.trackedBodyId.clear();
        state.trackedBodyPositionValid = false;
        return;
    }

    const auto bodyIt =
        std::find_if(
            presentation.bodies.begin(),
            presentation.bodies.end(),
            [&](const auto& body)
            {
                return body.id == state.selectedBodyId;
            }
        );

    if (bodyIt == presentation.bodies.end())
    {
        state.selectedBodyTrackingEnabled = false;
        state.trackedBodyId.clear();
        state.trackedBodyPositionValid = false;
        return;
    }

    const glm::dvec3 bodyAbsolute =
        bodyIt->positionAu *
        static_cast<double>(
            presentation.systemScale
        );

    const bool sameBody =
        state.trackedBodyPositionValid &&
        state.trackedBodyId == state.selectedBodyId;

    if (sameBody)
    {
        const glm::dvec3 delta =
            bodyAbsolute -
            state.trackedBodyLastAbsolute;

        state.camera.target += delta;

        if (state.orbitPivotActive)
            state.orbitPivotAbsolute += delta;
    }

    state.trackedBodyId = state.selectedBodyId;
    state.trackedBodyLastAbsolute = bodyAbsolute;
    state.trackedBodyPositionValid = true;
}


void SystemMapRenderer::resetNavigationViewToLevelZero(
    const Viewport& viewport
)
{
    if (m_mode == Mode::Galaxy)
    {
        m_galaxyView.resetNavigationToEntry();

        announceNavigationLevel(
            'G',
            m_galaxyView.state().navigationGrid.level()
        );

        return;
    }

    if (m_mode == Mode::System &&
        m_systemView.state().navigationGrid.enabled())
    {
        m_systemView.resetNavigationToLevelZero(
            viewport
        );

        announceNavigationLevel(
            'S',
            m_systemView.state().navigationGrid.level()
        );
    }
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
    const auto result =
        m_localMapInteraction.handle(
            m_mode,
            m_detailView,
            m_hubView,
            m_detailPresentation.frame,
            m_hubPresentation.frame,
            vp,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown,
            m_pendingScrollY
        );

    using SelectionAction =
        game::system_map::LocalMapInteractionResult::SelectionAction;

    if (m_mode != Mode::Detail)
        return;

    if (result.selectionAction == SelectionAction::SelectHub)
    {
        m_detailView.selectHub(
            result.hubId,
            result.parentBodyId
        );
    }
    else if (result.selectionAction == SelectionAction::ClearHub)
    {
        m_detailView.clearHubSelection();
    }
}


// Shared billboard halo used by Galaxy and System map stars.

void SystemMapRenderer::addBillboardHalo(
    const glm::vec3& center,
    float starRadius,
    float outerRadiusScale,
    float baseAlpha,
    const glm::vec4& color,
    const glm::mat4& view,
    int ringCount,
    int segments
)
{
    if (starRadius <= 0.0f ||
        outerRadiusScale <= 1.0f ||
        baseAlpha <= 0.0f ||
        ringCount <= 0 ||
        segments < 8)
    {
        return;
    }

    glm::vec3 cameraRight(
        view[0][0],
        view[1][0],
        view[2][0]
    );

    glm::vec3 cameraUp(
        view[0][1],
        view[1][1],
        view[2][1]
    );

    if (glm::length(cameraRight) < 0.000001f ||
        glm::length(cameraUp) < 0.000001f)
    {
        return;
    }

    cameraRight =
        glm::normalize(cameraRight);

    cameraUp =
        glm::normalize(cameraUp);

    ringCount =
        std::max(
            ringCount,
            1
        );

    segments =
        std::max(
            segments,
            8
        );

    for (int ring = 0; ring < ringCount; ++ring)
    {
        const float t =
            ringCount > 1
                ? static_cast<float>(ring) /
                    static_cast<float>(ringCount - 1)
                : 0.0f;

        /*
            Между ядром звезды и гало оставляем заметный
            пустой промежуток. Поэтому шарик остаётся чётким,
            а гало читается как отдельный объект.
        */
        constexpr float innerHaloRadiusScale =
            2.10f;

        const float radiusScale =
            innerHaloRadiusScale +
            (
                outerRadiusScale -
                innerHaloRadiusScale
            ) *
            t;

        const float radius =
            starRadius *
            radiusScale;


        /*
            Даже внешнее кольцо остаётся видимым.
            Внутренние кольца заметно ярче.
        */
        const float inverseT =
            1.0f - t;

        const float fade =
            0.28f +
            0.72f *
            std::pow(
                inverseT,
                1.65f
            );

        glm::vec4 ringColor = color;

        ringColor.a =
            baseAlpha *
            fade;

        for (int segment = 0;
             segment < segments;
             ++segment)
        {
            const float angle0 =
                glm::two_pi<float>() *
                static_cast<float>(segment) /
                static_cast<float>(segments);

            const float angle1 =
                glm::two_pi<float>() *
                static_cast<float>(segment + 1) /
                static_cast<float>(segments);

            const glm::vec3 point0 =
                center +
                (
                    std::cos(angle0) *
                    cameraRight +
                    std::sin(angle0) *
                    cameraUp
                ) *
                radius;

            const glm::vec3 point1 =
                center +
                (
                    std::cos(angle1) *
                    cameraRight +
                    std::sin(angle1) *
                    cameraUp
                ) *
                radius;

            addLine(
                point0,
                point1,
                ringColor
            );
        }
    }
}
