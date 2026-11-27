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
//   nested messages escape at ANY depth: \x1B1 = \x1B, \x1B2 = \x1E, \x1B3 = \x1F (each parse undoes one level).
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
            for (char c : p.value)   // escape (any depth): \x1B -> \x1B1, \x1E -> \x1B2, \x1F -> \x1B3
                if (c == '\x1B' || c == ParamSeparator || c == MessageStart)
                {
                    out += '\x1B';
                    out += c == '\x1B' ? '1' : c == ParamSeparator ? '2' : '3';
                }
                else
                    out += c;
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
        std::string u;
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i] == '\x1B' && i + 1 < v.size())
            {
                const char e = v[++i];
                u += e == '1' ? '\x1B' : e == '2' ? ParamSeparator : MessageStart;
            }
            else
                u += v[i];
        p.value = u;
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
