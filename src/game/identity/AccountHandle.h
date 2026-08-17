#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace game::identity
{
inline constexpr std::size_t AccountHandleMinLength = 3u;
inline constexpr std::size_t AccountHandleMaxLength = 24u;

/*
    Stable human-entered account identifier.

    Account handles are deliberately conservative ASCII identifiers rather
    than display names. This avoids Unicode normalization/confusable problems
    at the authentication boundary. Localized/display player names remain a
    separate future presentation field and may use full Unicode.

    Grammar:
      - 3..24 characters;
      - lowercase ASCII a-z, digits 0-9, '_' and '-';
      - first character must be a lowercase letter or digit.
*/
inline bool isValidAccountHandle(std::string_view value) noexcept
{
    if (value.size() < AccountHandleMinLength ||
        value.size() > AccountHandleMaxLength)
    {
        return false;
    }

    const auto isAlphaNumeric = [](char c) noexcept
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    };

    if (!isAlphaNumeric(value.front()))
        return false;

    for (const char c : value)
    {
        if (!isAlphaNumeric(c) && c != '_' && c != '-')
            return false;
    }

    return true;
}

inline std::string accountHandleRulesText()
{
    return "account handle must be 3-24 characters: lowercase a-z, digits, _ or -, starting with a letter or digit";
}
}
