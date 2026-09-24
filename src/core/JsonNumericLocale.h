#pragma once

#include <clocale>

namespace core
{

// JSON numbers always use '.', even when the user's text/UI locale uses ','.
// nlohmann's numeric lexer delegates conversion to the C runtime strtod.
// Set this once at process startup, before creating any worker threads.
inline bool useJsonNumericLocale() noexcept
{
    return std::setlocale(LC_NUMERIC, "C") != nullptr;
}

} // namespace core
