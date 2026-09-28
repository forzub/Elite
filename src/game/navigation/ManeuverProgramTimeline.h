#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "src/game/navigation/AcceptedManeuverProgram.h"

namespace game::navigation
{

// Canonical time/index view over fixed-capacity pages of one accepted
// maneuver. Storage boundaries are not physical phases and never reset the
// accepted maneuver clock.
class ManeuverProgramTimeline final
{
public:
    enum class SelectionStatus : std::uint8_t
    {
        InvalidInput = 0,
        BeforeStart,
        Active
    };

    struct PageWindow
    {
        bool valid = false;
        double startUniverseTimeSeconds = 0.0;
        double endUniverseTimeSeconds = 0.0;
    };

    struct Selection
    {
        SelectionStatus status = SelectionStatus::InvalidInput;
        std::size_t pageIndex = 0;
        std::size_t pagesAdvanced = 0;
    };

    [[nodiscard]] static PageWindow pageWindow(
        const AcceptedManeuverProgram& page
    ) noexcept
    {
        constexpr double SampleOriginToleranceSeconds = 1.0e-12;

        PageWindow out;
        if (!page.valid ||
            page.sampleCount == 0 ||
            page.sampleCount > AcceptedManeuverProgram::kMaxSamples ||
            !std::isfinite(page.acceptedAtUniverseTimeSeconds) ||
            !std::isfinite(page.sequenceStartOffsetSeconds) ||
            page.sequenceStartOffsetSeconds < 0.0)
        {
            return out;
        }

        const auto& first = page.samples[0];
        const auto& last = page.samples[
            static_cast<std::size_t>(page.sampleCount - 1)
        ];
        if (!std::isfinite(first.timeOffsetSeconds) ||
            std::abs(first.timeOffsetSeconds) >
                SampleOriginToleranceSeconds ||
            !std::isfinite(last.timeOffsetSeconds) ||
            last.timeOffsetSeconds < 0.0)
        {
            return out;
        }

        out.startUniverseTimeSeconds =
            page.acceptedAtUniverseTimeSeconds +
            page.sequenceStartOffsetSeconds;
        out.endUniverseTimeSeconds =
            out.startUniverseTimeSeconds +
            last.timeOffsetSeconds;
        out.valid =
            std::isfinite(out.startUniverseTimeSeconds) &&
            std::isfinite(out.endUniverseTimeSeconds) &&
            out.endUniverseTimeSeconds >=
                out.startUniverseTimeSeconds;
        return out;
    }

    [[nodiscard]] static double elapsedPageSeconds(
        const AcceptedManeuverProgram& page,
        double universeTimeSeconds
    ) noexcept
    {
        const PageWindow window = pageWindow(page);
        if (!window.valid || !std::isfinite(universeTimeSeconds))
            return std::numeric_limits<double>::quiet_NaN();
        return universeTimeSeconds - window.startUniverseTimeSeconds;
    }

    [[nodiscard]] static Selection selectActivePage(
        const AcceptedManeuverProgram* pages,
        std::size_t pageCount,
        double universeTimeSeconds,
        std::size_t currentPageIndex
    ) noexcept
    {
        constexpr double PageContinuityToleranceSeconds = 1.0e-9;

        Selection out;
        if (pages == nullptr ||
            pageCount == 0 ||
            currentPageIndex >= pageCount ||
            !std::isfinite(universeTimeSeconds))
        {
            return out;
        }

        std::size_t selected = currentPageIndex;
        PageWindow selectedWindow = pageWindow(pages[selected]);
        if (!selectedWindow.valid)
            return out;

        if (universeTimeSeconds <
            selectedWindow.startUniverseTimeSeconds)
        {
            out.status = SelectionStatus::BeforeStart;
            out.pageIndex = selected;
            return out;
        }

        while (selected + 1 < pageCount)
        {
            const auto& current = pages[selected];
            const auto& next = pages[selected + 1];
            const PageWindow nextWindow = pageWindow(next);
            // Check adjacency in the maneuver's local clock. Subtracting two
            // independently rounded absolute universe timestamps loses
            // sub-microsecond precision at the live 1998-based epoch.
            const double localEnd =
                current.sequenceStartOffsetSeconds +
                current.samples[current.sampleCount - 1].timeOffsetSeconds;
            if (!nextWindow.valid ||
                next.acceptedAtUniverseTimeSeconds !=
                    current.acceptedAtUniverseTimeSeconds ||
                std::abs(
                    next.sequenceStartOffsetSeconds - localEnd
                ) > PageContinuityToleranceSeconds)
            {
                return Selection {};
            }

            // Activate a page by its own canonical start, never by a
            // separately reconstructed previous-page end plus an epsilon.
            if (universeTimeSeconds <
                nextWindow.startUniverseTimeSeconds)
            {
                break;
            }

            ++selected;
            selectedWindow = nextWindow;
        }

        out.status = SelectionStatus::Active;
        out.pageIndex = selected;
        out.pagesAdvanced = selected - currentPageIndex;
        return out;
    }

    [[nodiscard]] static Selection selectSpatialPage(
        const AcceptedManeuverProgram* pages,
        std::size_t pageCount,
        double universeTimeSeconds,
        const glm::dvec3& positionMapMeters,
        std::size_t currentPageIndex
    ) noexcept
    {
        constexpr double PageContinuityToleranceSeconds = 1.0e-9;
        constexpr double SegmentEpsilon = 1.0e-12;

        Selection out;
        if (pages == nullptr ||
            pageCount == 0 ||
            currentPageIndex >= pageCount ||
            !std::isfinite(universeTimeSeconds) ||
            !std::isfinite(positionMapMeters.x) ||
            !std::isfinite(positionMapMeters.y) ||
            !std::isfinite(positionMapMeters.z))
        {
            return out;
        }

        std::size_t selected = currentPageIndex;
        const auto currentWindow = pageWindow(pages[selected]);
        if (!currentWindow.valid)
            return out;

        // Spatial pages share the maneuver acceptance epoch. Once the maneuver
        // has started, page-local nominal timestamps never gate progress.
        if (universeTimeSeconds <
            pages[0].acceptedAtUniverseTimeSeconds)
        {
            out.status = SelectionStatus::BeforeStart;
            out.pageIndex = selected;
            return out;
        }

        // Storage pages are sequential path chunks. Advance at most one page
        // per fixed-step; never let geometry coincidences skip a hairpin branch.
        if (selected + 1 < pageCount)
        {
            const auto& current = pages[selected];
            const auto& next = pages[selected + 1];
            const auto nextWindow = pageWindow(next);

            if (!nextWindow.valid ||
                current.referenceMode !=
                    AcceptedManeuverProgram::ReferenceMode::
                        SpatialCorridor ||
                next.referenceMode != current.referenceMode ||
                next.acceptedAtUniverseTimeSeconds !=
                    current.acceptedAtUniverseTimeSeconds ||
                next.objectiveRevision != current.objectiveRevision ||
                current.sampleCount < 2 ||
                next.sampleCount < 2)
            {
                return Selection {};
            }

            const double localEnd =
                current.sequenceStartOffsetSeconds +
                current.samples[current.sampleCount - 1].
                    timeOffsetSeconds;
            if (std::abs(
                    next.sequenceStartOffsetSeconds - localEnd
                ) > PageContinuityToleranceSeconds)
            {
                return Selection {};
            }

            const auto& beforeEnd =
                current.samples[
                    static_cast<std::size_t>(
                        current.sampleCount - 2
                    )
                ];
            const auto& end =
                current.samples[
                    static_cast<std::size_t>(
                        current.sampleCount - 1
                    )
                ];
            const glm::dvec3 terminalSegment =
                end.positionMapMeters -
                beforeEnd.positionMapMeters;
            const double segmentLength2 =
                glm::dot(terminalSegment, terminalSegment);
            if (!(segmentLength2 > SegmentEpsilon) ||
                !std::isfinite(segmentLength2))
            {
                return Selection {};
            }

            // Storage is not a maneuver phase. Advance only after the real
            // craft passes the endpoint plane in the accepted path direction.
            // Nominal time is deliberately irrelevant here.
            const double pastEndpoint =
                glm::dot(
                    positionMapMeters - end.positionMapMeters,
                    terminalSegment
                );
            if (pastEndpoint >= 0.0)
                ++selected;
        }

        out.status = SelectionStatus::Active;
        out.pageIndex = selected;
        out.pagesAdvanced = selected - currentPageIndex;
        return out;
    }
};

} // namespace game::navigation
