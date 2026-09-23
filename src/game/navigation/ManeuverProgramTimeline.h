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
            if (!nextWindow.valid ||
                next.acceptedAtUniverseTimeSeconds !=
                    current.acceptedAtUniverseTimeSeconds ||
                std::abs(
                    nextWindow.startUniverseTimeSeconds -
                    selectedWindow.endUniverseTimeSeconds
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
};

} // namespace game::navigation
