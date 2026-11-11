#pragma once

#include <string>

namespace core
{
// Removes the last whole UTF-8 character: its continuation bytes, then its lead byte. (Popping one byte and
// then the continuation bytes before it left a lone lead byte at the end: "Навигационные" cut to fit became
// "Навигационн?" + "?е" - 2026-11-11, map panels.)
inline void utf8PopBack(std::string& s)
{
    while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80)
        s.pop_back();
    if (!s.empty())
        s.pop_back();
}
} // namespace core
