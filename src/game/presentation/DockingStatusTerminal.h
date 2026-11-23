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
    Error,
    Alert,   // the dispatcher: a place taken - bright red (author 2026-11-22: «сообщение красным в терминале HUD»)
    Go       // the dispatcher: free again - green («как только док свободен - зелёный свет»)
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
    // The dock dispatcher's status for this ship: printed when it changes.
    std::string dispatcher;
    // The pilot's takeover / the station's release (printed when it changes).
    std::string pilotNotice;
    std::string error;
    std::string biteNominalText;
    // the prefix of a diagnostic that cleared (localized by the HUD: cockpit.terminal.recovered_prefix)
    std::string recoveredPrefix = "RECOVERED: ";
    // Completed immutable planner measurements (separate from BITE faults).
    std::vector<std::string> plannerDetails;

    // Generic ship diagnostics (BITE, later other read-only diagnostics).
    // The terminal remains presentation-only; producers provide ready lines.
    std::vector<std::pair<std::string, DockingTerminalSeverity>> diagnostics;

    // The interstellar jump (author 2026-11-09): its event / report lines, each printed once, and
    // the live line under the history (the charge bar) - rewritten in place, never kept.
    std::vector<std::pair<std::string, DockingTerminalSeverity>> jumpLines;
    std::string live;
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

        // The UI locale changed (author 2026-11-23: «терминал HUD при переключении языка начинает выводить текст
        // заново. можно его просто заменить на перевод ... перевод можно делать мгновенно»): every line that came from
        // a slot of the snapshot (phase, planner, a diagnostic, the dispatcher ...) is REPLACED by that slot's text in
        // the new language, at once - typed lines stay typed; old events no longer in the snapshot keep their text
        if (m_initialized && snapshot.localeId != m_last.localeId)
        {
            translateInPlace(m_last, snapshot);
            m_last = snapshot;
        }

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
            for (const auto& line : snapshot.jumpLines)
                enqueue(line.first, line.second);
            m_last = snapshot;
            m_lastReminderSeconds = nowSeconds;
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
                {
                    if (std::find(m_last.diagnostics.begin(),
                                  m_last.diagnostics.end(),
                                  diagnostic) == m_last.diagnostics.end())
                        enqueuePriority(diagnostic.first,diagnostic.second);
                }
                for (const auto& previous : m_last.diagnostics)
                {
                    // the dispatcher's lines are events, not faults: no "recovered" echo when they change / end
                    if (previous.second == DockingTerminalSeverity::Alert || previous.second == DockingTerminalSeverity::Go)
                        continue;
                    if (std::find(snapshot.diagnostics.begin(),
                                  snapshot.diagnostics.end(),
                                  previous) == snapshot.diagnostics.end())
                        enqueuePriority(snapshot.recoveredPrefix + previous.first,
                                        DockingTerminalSeverity::Info);
                }
                m_lastReminderSeconds = nowSeconds;
                if (snapshot.diagnostics.empty() &&
                    !m_last.diagnostics.empty())
                    enqueuePriority(snapshot.biteNominalText,
                                    DockingTerminalSeverity::Info);
            }

            if (!snapshot.diagnostics.empty() &&
                nowSeconds - m_lastReminderSeconds >= 10.0)
            {
                for (const auto& diagnostic : snapshot.diagnostics)
                    if (diagnostic.second != DockingTerminalSeverity::Alert && diagnostic.second != DockingTerminalSeverity::Go)   // events: no 10 s reminder
                        enqueuePriority(diagnostic.first,diagnostic.second);
                m_lastReminderSeconds = nowSeconds;
            }
            enqueueChanged(m_last.dispatcher, snapshot.dispatcher,
                           DockingTerminalSeverity::Info);
            enqueueChanged(m_last.pilotNotice, snapshot.pilotNotice,
                           DockingTerminalSeverity::Warning);
            enqueueChanged(m_last.activity, snapshot.activity,
                           DockingTerminalSeverity::Info);
            if (snapshot.plannerDetails != m_last.plannerDetails)
            {
                for (const auto& line : snapshot.plannerDetails)
                    enqueue(line, DockingTerminalSeverity::Info);
            }
            if (snapshot.jumpLines != m_last.jumpLines)
            {
                for (const auto& line : snapshot.jumpLines)
                    if (std::find(m_last.jumpLines.begin(), m_last.jumpLines.end(), line) == m_last.jumpLines.end())
                        enqueue(line.first, line.second);
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

        if (!snapshot.live.empty())
        {
            if (out.lines.size() >= MaxVisibleLines)
                out.lines.erase(out.lines.begin());
            out.lines.push_back({snapshot.live, DockingTerminalSeverity::Info, false});
        }

        return out;
    }

private:
    // old text -> new text by slot (the same field / the same index of a list) between two snapshots
    void translateInPlace(const DockingTerminalSnapshot& from, const DockingTerminalSnapshot& to)
    {
        std::vector<std::pair<std::string, std::string>> map;
        const auto add = [&](const std::string& a, const std::string& b) {
            if (!a.empty() && !b.empty() && a != b)
                map.emplace_back(a, b);
        };
        add(from.title, to.title);
        add(from.phase, to.phase);
        add(from.planner, to.planner);
        add(from.safety, to.safety);
        add(from.handoff, to.handoff);
        add(from.ship, to.ship);
        add(from.physics, to.physics);
        add(from.activity, to.activity);
        add(from.dispatcher, to.dispatcher);
        add(from.pilotNotice, to.pilotNotice);
        add(from.error, to.error);
        add(from.biteNominalText, to.biteNominalText);
        for (std::size_t i = 0; i < std::min(from.plannerDetails.size(), to.plannerDetails.size()); ++i)
            add(from.plannerDetails[i], to.plannerDetails[i]);
        for (std::size_t i = 0; i < std::min(from.diagnostics.size(), to.diagnostics.size()); ++i)
        {
            add(from.diagnostics[i].first, to.diagnostics[i].first);
            add(from.recoveredPrefix + from.diagnostics[i].first, to.recoveredPrefix + to.diagnostics[i].first);
        }
        for (std::size_t i = 0; i < std::min(from.jumpLines.size(), to.jumpLines.size()); ++i)
            add(from.jumpLines[i].first, to.jumpLines[i].first);
        const auto translate = [&](std::string& text) {
            for (const auto& [a, b] : map)
                if (text == a)
                {
                    text = b;
                    return;
                }
        };
        for (auto& line : m_history)
            translate(line.text);
        for (std::size_t i = 0; i < m_pending.size(); ++i)
        {
            const std::size_t before = m_pending[i].first.size();
            translate(m_pending[i].first);
            // the line being typed: keep its progress in proportion (never past its new end)
            if (i == 0 && before > 0)
                m_revealedCharacters = std::min(m_pending[i].first.size(), m_revealedCharacters * m_pending[i].first.size() / before);
        }
    }

    static constexpr std::size_t MaxHistoryLines = 64;
    double m_lastReminderSeconds = 0.0;
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
