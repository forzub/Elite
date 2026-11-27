#pragma once

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

// A LANGUAGE-NEUTRAL MESSAGE (audit #7 D7.4 / D7.5): a localization key plus named parameters, carried in the existing
// string fields (notices, service results, dispatcher answers) instead of finished text in one language. The server
// and the simulation build these; the client renders them in the player's language (LocalizationService::render).
//
// Wire form (text, no schema change):   \x1F<key>\x1E<name>=<value>\x1E<name>=<value>...
// - \x1F (unit separator) starts a message: a plain string (old producers, names) is shown as it is;
// - a parameter VALUE that is itself a message (starts with \x1F) is rendered in its turn (nested, e.g. a reason);
//   nested messages escape their separators: \x1D = an inner \x1E, \x1C = an inner \x1F.
// - templates name their parameters: "Pad {pad}: cleared to land, place {place}" - never glued fragments.
namespace game::localization
{
inline constexpr char MessageStart = '\x1F';
inline constexpr char ParamSeparator = '\x1E';

struct MessageParam
{
    std::string name;
    std::string value;
};

inline std::string formatNumber(double value, int decimals)
{
    char b[64];
    std::snprintf(b, sizeof b, "%.*f", decimals, value);
    return b;
}

class Message
{
public:
    explicit Message(std::string key) : m_key(std::move(key)) {}
    Message& arg(const std::string& name, const std::string& value)
    {
        m_params.push_back({name, value});
        return *this;
    }
    Message& arg(const std::string& name, const char* value) { return arg(name, std::string(value ? value : "")); }
    Message& arg(const std::string& name, long long value) { return arg(name, std::to_string(value)); }
    Message& arg(const std::string& name, int value) { return arg(name, std::to_string(value)); }
    Message& arg(const std::string& name, double value, int decimals) { return arg(name, formatNumber(value, decimals)); }
    // a nested message as a parameter (a reason, a sub-sentence)
    Message& arg(const std::string& name, const Message& nested) { return arg(name, nested.str()); }

    std::string str() const
    {
        std::string out(1, MessageStart);
        out += m_key;
        for (const auto& p : m_params)
        {
            out += ParamSeparator;
            out += p.name;
            out += '=';
            for (char c : p.value)
                out += c == ParamSeparator ? '\x1D' : c == MessageStart ? '\x1C' : c;
        }
        return out;
    }
    operator std::string() const { return str(); }

private:
    std::string m_key;
    std::vector<MessageParam> m_params;
};

inline bool isMessage(const std::string& s) { return !s.empty() && s.front() == MessageStart; }

// split a wire message into its key and parameters (values un-escaped one level)
inline bool parseMessage(const std::string& s, std::string& key, std::vector<MessageParam>& params)
{
    if (!isMessage(s))
        return false;
    params.clear();
    std::size_t at = 1;
    std::size_t next = s.find(ParamSeparator, at);
    key = s.substr(at, next == std::string::npos ? std::string::npos : next - at);
    while (next != std::string::npos)
    {
        at = next + 1;
        next = s.find(ParamSeparator, at);
        const std::string item = s.substr(at, next == std::string::npos ? std::string::npos : next - at);
        const std::size_t eq = item.find('=');
        MessageParam p;
        p.name = item.substr(0, eq);
        std::string v = eq == std::string::npos ? std::string() : item.substr(eq + 1);
        for (char& c : v)
            c = c == '\x1D' ? ParamSeparator : c == '\x1C' ? MessageStart : c;
        p.value = v;
        params.push_back(std::move(p));
    }
    return true;
}

// fill "{name}" placeholders of a template (unknown placeholders stay visible - the completeness check reports them)
inline std::string fillTemplate(const std::string& tpl, const std::vector<MessageParam>& params)
{
    std::string out;
    out.reserve(tpl.size() + 32);
    for (std::size_t i = 0; i < tpl.size();)
    {
        if (tpl[i] == '{')
        {
            const std::size_t close = tpl.find('}', i + 1);
            if (close != std::string::npos)
            {
                const std::string name = tpl.substr(i + 1, close - i - 1);
                bool found = false;
                for (const auto& p : params)
                    if (p.name == name)
                    {
                        out += p.value;
                        found = true;
                        break;
                    }
                if (found)
                {
                    i = close + 1;
                    continue;
                }
            }
        }
        out += tpl[i++];
    }
    return out;
}
} // namespace game::localization
