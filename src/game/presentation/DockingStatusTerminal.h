#pragma once

#include <algorithm>
#include <cstddef>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace game::presentation
{

enum class DockingTerminalSeverity
{
    Info,
    Warning,
    Error
};

struct DockingTerminalSnapshot
{
    bool visible = false;
    // Language switches invalidate rendered history, but not ship diagnostics.
    std::string localeId;
    std::string title;
    std::string phase;
    std::string planner;
    std::string safety;
    std::string handoff;
    std::string ship;
    std::string physics;
    std::string activity;
    std::string error;
    std::string biteNominalText;
    // Completed immutable planner measurements (separate from BITE faults).
    std::vector<std::string> plannerDetails;

    // Generic ship diagnostics (BITE, later other read-only diagnostics).
    // The terminal remains presentation-only; producers provide ready lines.
    std::vector<std::pair<std::string, DockingTerminalSeverity>> diagnostics;
};

struct DockingTerminalLine
{
    std::string text;
    DockingTerminalSeverity severity = DockingTerminalSeverity::Info;
    bool cursor = false;
};

// Pure presentation state machine.
//
// HARD BOUNDARY:
// - consumes only the snapshot DTO above;
// - owns no SpaceState/navigation/render/audio pointers;
// - never calls planner, autopilot or docking code;
// - removal of this file cannot change flight behavior.
//
// Rendering and optional character-tick audio are adapters outside this class.
class DockingStatusTerminal final
{
public:
    struct Output
    {
        std::vector<DockingTerminalLine> lines;
        std::size_t typedCharacters = 0; // optional audio hook
    };

    void reset() noexcept
    {
        *this = {};
    }

    [[nodiscard]] Output update(
        const DockingTerminalSnapshot& snapshot,
        double nowSeconds
    )
    {
        Output out;

        // Do not retain or type out old-language messages when UI locale
        // changes. The next snapshot rebuilds visible lines in the new locale.
        if (m_initialized && snapshot.localeId != m_last.localeId)
            reset();

        // Visibility is a renderer concern. Keep history/state while hidden so
        // model-family/layout changes and planner activity continue as one
        // scrolling diagnostic stream when the terminal becomes visible again.
        if (!snapshot.visible)
        {
            if (m_initialized)
                m_last = snapshot;
            return out;
        }

        if (!m_initialized)
        {
            m_initialized = true;
            m_lastNowSeconds = nowSeconds;
            enqueue(snapshot.title, DockingTerminalSeverity::Info);
            enqueue(snapshot.phase, DockingTerminalSeverity::Info);
            enqueue(snapshot.planner, DockingTerminalSeverity::Info);
            enqueue(snapshot.safety, DockingTerminalSeverity::Info);
            enqueue(snapshot.handoff, DockingTerminalSeverity::Info);
            enqueue(snapshot.ship, DockingTerminalSeverity::Info);
            enqueue(snapshot.physics, DockingTerminalSeverity::Info);
            if (!snapshot.error.empty())
                enqueue(snapshot.error, DockingTerminalSeverity::Error);
            for (const auto& diagnostic : snapshot.diagnostics)
                enqueue(diagnostic.first, diagnostic.second);
            enqueue(snapshot.activity, DockingTerminalSeverity::Info);
            for (const auto& line : snapshot.plannerDetails)
                enqueue(line, DockingTerminalSeverity::Info);
            m_last = snapshot;
        }
        else
        {
            enqueueChanged(m_last.phase, snapshot.phase,
                           DockingTerminalSeverity::Info);
            enqueueChanged(m_last.planner, snapshot.planner,
                           DockingTerminalSeverity::Info);
            enqueueChanged(m_last.safety, snapshot.safety,
                           DockingTerminalSeverity::Info);
            enqueueChanged(m_last.handoff, snapshot.handoff,
                           DockingTerminalSeverity::Warning);
            enqueueChanged(m_last.ship, snapshot.ship,
                           DockingTerminalSeverity::Info);
            enqueueChanged(m_last.physics, snapshot.physics,
                           DockingTerminalSeverity::Info);

            if (snapshot.error != m_last.error &&
                !snapshot.error.empty())
            {
                enqueuePriority(
                    snapshot.error,
                    DockingTerminalSeverity::Error
                );
            }

            if (snapshot.diagnostics != m_last.diagnostics)
            {
                for (const auto& diagnostic : snapshot.diagnostics)
                    enqueuePriority(diagnostic.first, diagnostic.second);

                if (snapshot.diagnostics.empty() &&
                    !m_last.diagnostics.empty())
                {
                    enqueuePriority(
                        snapshot.biteNominalText,
                        DockingTerminalSeverity::Info
                    );
                }
            }

            enqueueChanged(m_last.activity, snapshot.activity,
                           DockingTerminalSeverity::Info);
            if (snapshot.plannerDetails != m_last.plannerDetails)
            {
                for (const auto& line : snapshot.plannerDetails)
                    enqueue(line, DockingTerminalSeverity::Info);
            }
            m_last = snapshot;
        }

        const double dt =
            std::clamp(nowSeconds - m_lastNowSeconds, 0.0, 0.25);
        m_lastNowSeconds = nowSeconds;

        constexpr double CharactersPerSecond = 38.0;
        m_characterBudget += dt * CharactersPerSecond;

        while (m_characterBudget >= 1.0 && !m_pending.empty())
        {
            auto& front = m_pending.front();
            if (m_revealedCharacters < front.first.size())
            {
                ++m_revealedCharacters;
                ++out.typedCharacters;
                m_characterBudget -= 1.0;
            }

            if (m_revealedCharacters >= front.first.size())
            {
                m_history.push_back({
                    front.first,
                    front.second,
                    false
                });
                while (m_history.size() > MaxHistoryLines)
                    m_history.pop_front();

                m_pending.pop_front();
                m_revealedCharacters = 0;
            }
        }

        out.lines.assign(m_history.begin(), m_history.end());

        if (!m_pending.empty())
        {
            const auto& front = m_pending.front();
            out.lines.push_back({
                front.first.substr(0, m_revealedCharacters),
                front.second,
                true
            });
        }
        else if (!out.lines.empty())
        {
            out.lines.back().cursor = cursorVisible(nowSeconds);
        }

        if (out.lines.size() > MaxVisibleLines)
        {
            out.lines.erase(
                out.lines.begin(),
                out.lines.begin() +
                    static_cast<std::ptrdiff_t>(
                        out.lines.size() - MaxVisibleLines
                    )
            );
        }

        return out;
    }

private:
    static constexpr std::size_t MaxHistoryLines = 64;
    static constexpr std::size_t MaxVisibleLines = 14;

    void enqueue(
        const std::string& text,
        DockingTerminalSeverity severity
    )
    {
        if (text.empty())
            return;

        if (!m_pending.empty() &&
            m_pending.back().first == text)
        {
            return;
        }

        if (!m_history.empty() &&
            m_history.back().text == text)
        {
            return;
        }

        m_pending.emplace_back(text, severity);
    }

    void enqueuePriority(
        const std::string& text,
        DockingTerminalSeverity severity
    )
    {
        if (text.empty())
            return;

        for (const auto& pending : m_pending)
        {
            if (pending.first == text)
                return;
        }

        if (!m_history.empty() &&
            m_history.back().text == text)
        {
            return;
        }

        // Preserve the line currently being typed, but put urgent diagnostics
        // immediately after it so decorative/activity traffic cannot starve
        // warnings or errors.
        if (m_pending.empty())
        {
            m_pending.emplace_back(text, severity);
            return;
        }

        m_pending.insert(
            std::next(m_pending.begin()),
            std::make_pair(text, severity)
        );
    }

    void enqueueChanged(
        const std::string& previous,
        const std::string& current,
        DockingTerminalSeverity severity
    )
    {
        if (current != previous && !current.empty())
            enqueue(current, severity);
    }

    [[nodiscard]] static bool cursorVisible(double nowSeconds) noexcept
    {
        const auto phase =
            static_cast<long long>(nowSeconds * 2.0);
        return (phase & 1LL) == 0;
    }

    bool m_initialized = false;
    double m_lastNowSeconds = 0.0;
    double m_characterBudget = 0.0;
    std::size_t m_revealedCharacters = 0;

    DockingTerminalSnapshot m_last {};
    std::deque<std::pair<std::string, DockingTerminalSeverity>> m_pending;
    std::deque<DockingTerminalLine> m_history;
};

} // namespace game::presentation
